#include "enforcer.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

static const char *kParkedWorkspace = "special:focus-blocked";

Enforcer::Enforcer(QObject *parent) : QObject(parent) {
    connect(&m_events, &QLocalSocket::readyRead, this, &Enforcer::readEvents);
    connect(&m_events, &QLocalSocket::disconnected, this, [this] { m_reconnect.start(); });
    connect(&m_events, &QLocalSocket::errorOccurred, this, [this] { m_reconnect.start(); });
    m_reconnect.setSingleShot(true);
    m_reconnect.setInterval(2000);
    connect(&m_reconnect, &QTimer::timeout, this, &Enforcer::connectEvents);
    m_sweep.setInterval(3000);
    connect(&m_sweep, &QTimer::timeout, this, &Enforcer::sweep);
    connectEvents();
}

bool Enforcer::isConnected() const { return m_events.state() == QLocalSocket::ConnectedState; }

void Enforcer::setAllowed(const QStringList &classes) {
    m_allowed = classes;
    m_closing.clear();
    if (isActive()) {
        m_sweep.start();
        sweep();
    } else {
        m_sweep.stop();
    }
}

bool Enforcer::isAllowed(const QString &windowClass, const QStringList &patterns) {
    for (const auto &pattern : patterns) {
        const QRegularExpression expression(
            QRegularExpression::wildcardToRegularExpression(pattern, QRegularExpression::UnanchoredWildcardConversion),
            QRegularExpression::CaseInsensitiveOption);
        const auto match = expression.match(windowClass);
        if (match.hasMatch() && match.capturedLength() == windowClass.length())
            return true;
    }
    return false;
}

// Hyprland's sockets live under $XDG_RUNTIME_DIR/hypr/<signature>. The service
// may start before the session exports the signature, or outlive a Hyprland
// restart, so fall back to the newest instance on disk.
QString Enforcer::hyprDirectory() {
    // Tests point this at an empty directory so they never touch the desktop.
    if (qEnvironmentVariableIsSet("FOCUS_HYPR_DIR")) {
        const QString overridden = qEnvironmentVariable("FOCUS_HYPR_DIR");
        return QFileInfo::exists(overridden + "/.socket2.sock") ? overridden : QString();
    }
    const QString base = qEnvironmentVariable("XDG_RUNTIME_DIR") + "/hypr";
    const QString signature = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
    if (!signature.isEmpty() && QFileInfo::exists(base + "/" + signature + "/.socket2.sock"))
        return base + "/" + signature;
    QString newest;
    QDateTime newestTime;
    for (const auto &info : QDir(base).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!QFileInfo::exists(info.filePath() + "/.socket2.sock"))
            continue;
        if (newest.isEmpty() || info.lastModified() > newestTime) {
            newest = info.filePath();
            newestTime = info.lastModified();
        }
    }
    return newest;
}

void Enforcer::connectEvents() {
    const QString directory = hyprDirectory();
    if (directory.isEmpty()) {
        emit hyprlandMissing();
        m_reconnect.start();
        return;
    }
    m_events.abort();
    m_buffer.clear();
    m_events.connectToServer(directory + "/.socket2.sock");
}

QByteArray Enforcer::request(const QByteArray &command) {
    const QString directory = hyprDirectory();
    if (directory.isEmpty())
        return {};
    QLocalSocket socket;
    socket.connectToServer(directory + "/.socket.sock");
    if (!socket.waitForConnected(1000))
        return {};
    socket.write(command);
    socket.flush();
    QByteArray reply;
    while (socket.state() == QLocalSocket::ConnectedState && socket.waitForReadyRead(1500))
        reply += socket.readAll();
    reply += socket.readAll();
    return reply;
}

void Enforcer::readEvents() {
    m_buffer += m_events.readAll();
    qsizetype newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_buffer.left(newline));
        m_buffer.remove(0, newline + 1);
        if (!isActive() || !line.startsWith("openwindow>>"))
            continue;
        // openwindow>>ADDRESS,WORKSPACE,CLASS,TITLE (the title may hold commas).
        const QStringList parts = line.mid(12).split(',');
        if (parts.size() < 3)
            continue;
        QString address = parts[0];
        if (!address.startsWith("0x"))
            address.prepend("0x");
        check(address, parts[2], parts.mid(3).join(','));
    }
}

void Enforcer::sweep() {
    if (!isActive())
        return;
    const auto clients = QJsonDocument::fromJson(request("j/clients")).array();
    for (const auto &value : clients) {
        const QJsonObject client = value.toObject();
        if (!client.value("mapped").toBool(true))
            continue;
        if (client.value("workspace").toObject().value("name").toString() == kParkedWorkspace)
            continue;
        check(client.value("address").toString(), client.value("class").toString(),
              client.value("title").toString());
    }
}

void Enforcer::check(const QString &address, const QString &windowClass, const QString &title) {
    // A window without a class yet is left for the next sweep to judge.
    if (windowClass.isEmpty() || isAllowed(windowClass, m_allowed))
        return;

    const QByteArray target = "window = \"address:" + address.toUtf8() + "\"";
    auto closing = m_closing.find(address);
    if (closing == m_closing.end()) {
        request("/dispatch hl.dsp.window.close({ " + target + " })");
        QElapsedTimer timer;
        timer.start();
        m_closing.insert(address, timer);
        emit blocked(windowClass, title);
    } else if (closing->elapsed() > 2500) {
        // It asked to stay (an unsaved-changes prompt, say). Don't kill it and
        // lose work; move it out of sight until the session pauses or ends.
        request("/dispatch hl.dsp.window.move({ workspace = \"" + QByteArray(kParkedWorkspace) +
                "\", follow = false, " + target + " })");
    }
}

void Enforcer::releaseParked() {
    const auto active = QJsonDocument::fromJson(request("j/activeworkspace")).object();
    const QByteArray workspace = QByteArray::number(active.value("id").toInt(1));
    for (const auto &value : QJsonDocument::fromJson(request("j/clients")).array()) {
        const QJsonObject client = value.toObject();
        if (client.value("workspace").toObject().value("name").toString() != kParkedWorkspace)
            continue;
        request("/dispatch hl.dsp.window.move({ workspace = \"" + workspace + "\", follow = false, window = \"address:" +
                client.value("address").toString().toUtf8() + "\" })");
    }
}
