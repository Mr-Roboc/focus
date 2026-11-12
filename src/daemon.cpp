#include "daemon.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcess>
#include <QStandardPaths>

#include "store.h"

Daemon::Daemon(QObject *parent) : QObject(parent) {
    m_state = Store::loadState();

    connect(&m_enforcer, &Enforcer::blocked, this, [this](const QString &cls, const QString &title) {
        log("blocked", {}, {{"class", cls}, {"title", title}});
        // One notice per app every few seconds, so a relaunch loop doesn't
        // bury the screen.
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastBlockNotice.value(cls) > 8000) {
            m_lastBlockNotice.insert(cls, now);
            notify(tr("%1 isn't part of this session").arg(title.isEmpty() ? cls : title),
                   tr("Focus on: %1").arg(m_state.value("name").toString()));
        }
    });

    m_timer.setInterval(5000);
    connect(&m_timer, &QTimer::timeout, this, [this] { tick(QDateTime::currentDateTime()); });
    m_timer.start();

    // Picks a session that was running before a restart or reboot straight
    // back up, or ends it if its time ran out meanwhile.
    QTimer::singleShot(0, this, [this] {
        applyEnforcement();
        tick(QDateTime::currentDateTime());
    });
}

bool Daemon::listen() {
    QLocalServer::removeServer(Store::socketPath());
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(Store::socketPath()))
        return false;
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server.nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, socket, [this, socket] {
                while (socket->canReadLine()) {
                    const QJsonObject command = QJsonDocument::fromJson(socket->readLine()).object();
                    socket->write(QJsonDocument(handle(command)).toJson(QJsonDocument::Compact) + "\n");
                    socket->flush();
                }
            });
        }
    });
    return true;
}

static QJsonObject failure(const QString &message) { return {{"ok", false}, {"error", message}}; }

QJsonObject Daemon::handle(const QJsonObject &command) {
    const QString name = command.value("cmd").toString();
    if (name == "status") {
        QJsonObject reply = m_state;
        reply.insert("ok", true);
        reply.insert("hyprland", m_enforcer.isConnected());
        return reply;
    }
    if (name == "start") {
        if (m_state.value("status").toString() != "idle")
            return failure(tr("A session is already running."));
        const QJsonObject plan = Store::findPlan(Store::loadPlans(), command.value("id").toString());
        if (plan.isEmpty())
            return failure(tr("No session called “%1”.").arg(command.value("id").toString()));
        const QDateTime now = QDateTime::currentDateTime();
        // Inside its scheduled slot, a manual start takes over that slot so the
        // scheduler doesn't start it a second time.
        const QDateTime scheduled = Store::currentOccurrence(plan, now);
        const QDateTime begin = scheduled.isValid() ? scheduled : now;
        return start(plan, begin, begin.addSecs(60LL * qMax(1, plan.value("minutes").toInt(60))));
    }
    if (name == "pause")
        return pause(command.value("reason").toString(), command.value("minutes").toInt(10));
    if (name == "resume")
        return resume();
    if (name == "end") {
        const QString reason = command.value("reason").toString();
        if (m_state.value("status").toString() == "idle")
            return failure(tr("No session is running."));
        if (!Store::isReasonEnough(reason))
            return failure(tr("Write a reason of at least three words."));
        return finish("ended", reason);
    }
    if (name == "task")
        return setTask(command.value("index").toInt(-1), command.value("done").toBool());
    return failure(tr("Unknown command “%1”.").arg(name));
}

void Daemon::tick(const QDateTime &now) {
    const QString status = m_state.value("status").toString();
    if (status != "idle") {
        if (now >= QDateTime::fromString(m_state.value("end").toString(), Qt::ISODate)) {
            finish("completed", {});
        } else if (status == "paused" &&
                   now >= QDateTime::fromString(m_state.value("pausedUntil").toString(), Qt::ISODate)) {
            resume();
        }
        return;
    }

    const QJsonArray plans = Store::loadPlans();
    const QJsonObject handled = m_state.value("handled").toObject();
    for (const auto &value : plans) {
        const QJsonObject plan = value.toObject();
        const QDateTime begin = Store::currentOccurrence(plan, now);
        if (begin.isValid()) {
            const QString key = Store::occurrenceKey(plan, begin);
            if (handled.value(plan.value("id").toString()).toString() == key)
                continue;
            start(plan, begin, begin.addSecs(60LL * qMax(1, plan.value("minutes").toInt(60))));
            return;
        }
        // A minute's warning, so work in other apps can be saved first.
        const QDateTime next = Store::nextOccurrence(plan, now);
        if (next.isValid() && now.secsTo(next) <= 60) {
            const QString key = Store::occurrenceKey(plan, next);
            if (!m_warned.contains(key)) {
                m_warned.insert(key);
                notify(tr("“%1” starts in a minute").arg(plan.value("name").toString()),
                       tr("Save your work; apps outside the session will close."));
            }
        }
    }
}

QJsonObject Daemon::start(const QJsonObject &plan, const QDateTime &begin, const QDateTime &end) {
    QJsonArray tasks;
    for (const auto &task : plan.value("tasks").toArray())
        tasks.append(QJsonObject{{"text", task.toString()}, {"done", false}});

    QJsonObject handled = m_state.value("handled").toObject();
    handled.insert(plan.value("id").toString(), Store::occurrenceKey(plan, begin));

    const bool dnd = plan.value("dnd").toBool(true);
    m_state = QJsonObject{
        {"status", "active"},
        {"planId", plan.value("id")},
        {"name", plan.value("name")},
        {"start", begin.toString(Qt::ISODate)},
        {"end", end.toString(Qt::ISODate)},
        {"tasks", tasks},
        {"apps", plan.value("apps")},
        {"allowed", QJsonArray::fromStringList(Store::allowedClasses(plan))},
        {"dnd", dnd},
        {"dndBefore", currentDnd()},
        {"handled", handled},
    };
    save();
    log("started");
    notify(tr("Focus: %1").arg(plan.value("name").toString()),
           tr("Until %1. Only this session's apps are open.")
               .arg(QLocale().toString(end.time(), QLocale::ShortFormat)));
    applyEnforcement();
    return {{"ok", true}};
}

QJsonObject Daemon::pause(const QString &reason, int minutes) {
    if (m_state.value("status").toString() != "active")
        return failure(tr("No running session to pause."));
    if (!Store::isReasonEnough(reason))
        return failure(tr("Write a reason of at least three words."));
    if (minutes < 1 || minutes > 60)
        return failure(tr("A pause lasts between 1 and 60 minutes."));
    m_state.insert("status", "paused");
    m_state.insert("pausedUntil", QDateTime::currentDateTime().addSecs(60LL * minutes).toString(Qt::ISODate));
    save();
    log("paused", reason, {{"minutes", minutes}});
    applyEnforcement();
    return {{"ok", true}};
}

QJsonObject Daemon::resume() {
    if (m_state.value("status").toString() != "paused")
        return failure(tr("The session isn't paused."));
    m_state.insert("status", "active");
    m_state.remove("pausedUntil");
    save();
    log("resumed");
    notify(tr("Back to “%1”").arg(m_state.value("name").toString()),
           tr("The pause is over; apps outside the session will close."));
    applyEnforcement();
    return {{"ok", true}};
}

QJsonObject Daemon::finish(const QString &event, const QString &reason) {
    log(event, reason);
    const QString name = m_state.value("name").toString();
    const QJsonObject handled = m_state.value("handled").toObject();
    const QString dndBefore = m_state.value("dndBefore").toString();
    const bool dnd = m_state.value("dnd").toBool();
    m_state = QJsonObject{{"status", "idle"}, {"handled", handled}};
    save();
    m_enforcer.setAllowed({});
    m_enforcer.releaseParked();
    if (dnd)
        setDnd(dndBefore == "on");
    if (event == "completed")
        notify(tr("“%1” is done").arg(name), tr("Nice work. Everything is unlocked again."));
    return {{"ok", true}};
}

QJsonObject Daemon::setTask(int index, bool done) {
    QJsonArray tasks = m_state.value("tasks").toArray();
    if (m_state.value("status").toString() == "idle" || index < 0 || index >= tasks.size())
        return failure(tr("No such task."));
    QJsonObject task = tasks[index].toObject();
    task.insert("done", done);
    tasks[index] = task;
    m_state.insert("tasks", tasks);
    save();
    return {{"ok", true}};
}

// Enforcement and Do Not Disturb follow the state: on while active, off while
// paused or idle, so a crash or restart always lands in the right place.
void Daemon::applyEnforcement() {
    const QString status = m_state.value("status").toString();
    const bool dnd = m_state.value("dnd").toBool();
    if (status == "active") {
        QStringList allowed;
        for (const auto &cls : m_state.value("allowed").toArray())
            allowed.append(cls.toString());
        if (allowed.isEmpty())
            allowed = Store::builtinClasses();
        m_enforcer.setAllowed(allowed);
        if (dnd)
            setDnd(true);
    } else {
        m_enforcer.setAllowed({});
        if (status == "paused") {
            m_enforcer.releaseParked();
            if (dnd)
                setDnd(m_state.value("dndBefore").toString() == "on");
        }
    }
}

// FOCUS_DRY_RUN keeps tests away from the real notifications and settings.
static bool dryRun() { return qEnvironmentVariableIsSet("FOCUS_DRY_RUN"); }

QString Daemon::currentDnd() const {
    if (dryRun())
        return "off";
    QProcess process;
    process.start("omarchy-shell", {"notifications", "isDnd"});
    if (!process.waitForFinished(2000))
        return "off";
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed() == "on" ? "on" : "off";
}

void Daemon::setDnd(bool on) {
    if (dryRun() || QStandardPaths::findExecutable("omarchy-shell").isEmpty())
        return;
    QProcess::execute("omarchy-shell", {"notifications", "setDnd", on ? "on" : "off"});
    QProcess::startDetached("omarchy-shell", {"-q", "omarchy.indicators", "refresh"});
}

// Omarchy's own sender gets through Do Not Disturb, which matters because the
// session itself turned it on.
void Daemon::notify(const QString &headline, const QString &body) {
    if (dryRun())
        return;
    if (!QStandardPaths::findExecutable("omarchy-notification-send").isEmpty()) {
        QStringList args{"--app-name", "omarchy-action", headline};
        if (!body.isEmpty())
            args << body;
        args << "--exec" << "focus";
        QProcess::startDetached("omarchy-notification-send", args);
    } else {
        QProcess::startDetached("notify-send", {"-u", "critical", "-a", "focus", headline, body});
    }
}

void Daemon::save() { Store::saveState(m_state); }

void Daemon::log(const QString &event, const QString &reason, const QJsonObject &extra) {
    QJsonObject entry = extra;
    entry.insert("event", event);
    entry.insert("plan", m_state.value("planId"));
    entry.insert("name", m_state.value("name"));
    if (!reason.isEmpty())
        entry.insert("reason", reason);
    Store::appendLog(entry);
}
