#include "backend.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcess>

#include "apps.h"
#include "enforcer.h"
#include "store.h"

Backend::Backend(QObject *parent) : QObject(parent) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(60);
    connect(&m_debounce, &QTimer::timeout, this, &Backend::reload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    m_poll.setInterval(3000);
    connect(&m_poll, &QTimer::timeout, this, &Backend::checkService);
    m_poll.start();
    reload();
    checkService();
}

// Saves replace files, which drops them from the watch, so re-arm each time.
void Backend::reload() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    m_watcher.addPath(Store::dataDirectory());
    for (const auto &path : {Store::plansPath(), Store::statePath(), Store::logPath()})
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    const QJsonArray plans = Store::loadPlans();
    if (plans != m_plans) {
        m_plans = plans;
        emit plansChanged();
    }
    const QJsonObject session = Store::loadState();
    if (session != m_session) {
        m_session = session;
        emit sessionChanged();
    }
    const QJsonArray history = Store::readLog(200);
    if (history != m_history) {
        m_history = history;
        emit historyChanged();
    }
}

void Backend::checkService() {
    const bool running = send({{"cmd", "status"}}, 800).value("ok").toBool();
    if (running != m_serviceRunning) {
        m_serviceRunning = running;
        emit serviceChanged();
    }
}

QJsonObject Backend::send(const QJsonObject &command, int timeoutMs) {
    QLocalSocket socket;
    socket.connectToServer(Store::socketPath());
    if (!socket.waitForConnected(timeoutMs))
        return {{"ok", false}, {"error", tr("The focus service isn't running.")}, {"offline", true}};
    socket.write(QJsonDocument(command).toJson(QJsonDocument::Compact) + "\n");
    socket.flush();
    while (!socket.canReadLine())
        if (!socket.waitForReadyRead(timeoutMs))
            return {{"ok", false}, {"error", tr("The focus service didn't answer.")}};
    return QJsonDocument::fromJson(socket.readLine()).object();
}

QString Backend::command(const QJsonObject &command) {
    const QJsonObject reply = send(command);
    reload();
    return reply.value("ok").toBool() ? QString() : reply.value("error").toString();
}

void Backend::commit(const QJsonArray &plans) {
    if (plans == m_plans)
        return;
    m_undo.append(m_plans);
    if (m_undo.size() > 100)
        m_undo.removeFirst();
    m_redo.clear();
    m_plans = plans;
    Store::savePlans(m_plans);
    emit plansChanged();
}

void Backend::editPlan(const QString &id, const std::function<void(QJsonObject &)> &change) {
    QJsonArray plans = m_plans;
    for (qsizetype i = 0; i < plans.size(); ++i) {
        QJsonObject plan = plans[i].toObject();
        if (plan.value("id").toString() != id)
            continue;
        change(plan);
        plans[i] = plan;
        commit(plans);
        return;
    }
}

QString Backend::newPlan(const QString &name) {
    const QJsonObject plan = Store::newPlan(name);
    QJsonArray plans = m_plans;
    plans.append(plan);
    commit(plans);
    return plan.value("id").toString();
}

void Backend::deletePlan(const QString &id) {
    QJsonArray plans;
    for (const auto &plan : m_plans)
        if (plan.toObject().value("id").toString() != id)
            plans.append(plan);
    commit(plans);
}

void Backend::setField(const QString &id, const QString &key, const QVariant &value) {
    static const QStringList editable{"name", "date", "time", "minutes", "dnd", "enabled"};
    if (!editable.contains(key))
        return;
    editPlan(id, [&](QJsonObject &plan) { plan.insert(key, QJsonValue::fromVariant(value)); });
}

void Backend::toggleDay(const QString &id, int day) {
    editPlan(id, [&](QJsonObject &plan) {
        QJsonArray days = plan.value("days").toArray(), next;
        bool had = false;
        for (const auto &d : days) {
            if (d.toInt() == day)
                had = true;
            else
                next.append(d);
        }
        if (!had)
            next.append(day);
        plan.insert("days", next);
    });
}

void Backend::addTask(const QString &id, const QString &text) {
    if (text.trimmed().isEmpty())
        return;
    editPlan(id, [&](QJsonObject &plan) {
        QJsonArray tasks = plan.value("tasks").toArray();
        tasks.append(text.trimmed());
        plan.insert("tasks", tasks);
    });
}

void Backend::removeTask(const QString &id, int index) {
    editPlan(id, [&](QJsonObject &plan) {
        QJsonArray tasks = plan.value("tasks").toArray();
        if (index >= 0 && index < tasks.size())
            tasks.removeAt(index);
        plan.insert("tasks", tasks);
    });
}

void Backend::addApp(const QString &id, const QVariantMap &app) {
    const QJsonObject entry = QJsonObject::fromVariantMap(app);
    editPlan(id, [&](QJsonObject &plan) {
        QJsonArray apps = plan.value("apps").toArray();
        for (const auto &existing : apps)
            if (existing.toObject().value("id") == entry.value("id"))
                return;
        apps.append(QJsonObject{{"id", entry.value("id")},
                                {"name", entry.value("name")},
                                {"icon", entry.value("icon")},
                                {"classes", entry.value("classes")}});
        plan.insert("apps", apps);
    });
}

void Backend::addWindowClass(const QString &id, const QString &windowClass, const QString &title) {
    Q_UNUSED(title)
    addApp(id, {{"id", "class:" + windowClass},
                {"name", windowClass},
                {"icon", ""},
                {"classes", QStringList{windowClass}}});
}

void Backend::removeApp(const QString &id, int index) {
    editPlan(id, [&](QJsonObject &plan) {
        QJsonArray apps = plan.value("apps").toArray();
        if (index >= 0 && index < apps.size())
            apps.removeAt(index);
        plan.insert("apps", apps);
    });
}

void Backend::undo() {
    if (m_undo.isEmpty())
        return;
    m_redo.append(m_plans);
    m_plans = m_undo.takeLast();
    Store::savePlans(m_plans);
    emit plansChanged();
}

void Backend::redo() {
    if (m_redo.isEmpty())
        return;
    m_undo.append(m_plans);
    m_plans = m_redo.takeLast();
    Store::savePlans(m_plans);
    emit plansChanged();
}

QVariant Backend::nextRun(const QString &id) const {
    const QJsonObject plan = Store::findPlan(m_plans, id);
    const QDateTime now = QDateTime::currentDateTime();
    QDateTime when = Store::currentOccurrence(plan, now);
    if (!when.isValid())
        when = Store::nextOccurrence(plan, now);
    return when.isValid() ? QVariant(when) : QVariant();
}

void Backend::refreshApps() {
    m_apps = Apps::installed();
    emit appsChanged();
}

void Backend::refreshWindows() {
    QJsonArray windows;
    QStringList seen;
    for (const auto &value : QJsonDocument::fromJson(Enforcer::request("j/clients")).array()) {
        const QJsonObject client = value.toObject();
        const QString cls = client.value("class").toString();
        if (cls.isEmpty() || cls == "focus" || seen.contains(cls))
            continue;
        seen.append(cls);
        windows.append(QJsonObject{{"class", cls}, {"title", client.value("title")}});
    }
    m_windows = windows;
    emit windowsChanged();
}

QString Backend::startPlan(const QString &id) { return command({{"cmd", "start"}, {"id", id}}); }
QString Backend::pause(const QString &reason, int minutes) {
    return command({{"cmd", "pause"}, {"reason", reason}, {"minutes", minutes}});
}
QString Backend::resume() { return command({{"cmd", "resume"}}); }
QString Backend::endSession(const QString &reason) { return command({{"cmd", "end"}, {"reason", reason}}); }
QString Backend::setTaskDone(int index, bool done) {
    return command({{"cmd", "task"}, {"index", index}, {"done", done}});
}
bool Backend::isReasonEnough(const QString &reason) const { return Store::isReasonEnough(reason); }

QString Backend::enableService() {
    QProcess process;
    process.start("systemctl", {"--user", "enable", "--now", "focus.service"});
    process.waitForFinished(10000);
    checkService();
    if (process.exitCode() != 0)
        return QString::fromUtf8(process.readAllStandardError()).trimmed();
    return {};
}
