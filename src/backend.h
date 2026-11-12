#pragma once

#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVariant>

// The window's side of focus. It edits the planned sessions directly (with
// undo), and asks the background service to start, pause, resume, or end
// one, since only the service enforces and writes the running state.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList plans READ plans NOTIFY plansChanged)
    Q_PROPERTY(QVariantMap session READ session NOTIFY sessionChanged)
    Q_PROPERTY(bool serviceRunning READ serviceRunning NOTIFY serviceChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)
    Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY plansChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY plansChanged)

public:
    explicit Backend(QObject *parent = nullptr);

    QVariantList plans() const { return m_plans.toVariantList(); }
    QVariantMap session() const { return m_session.toVariantMap(); }
    bool serviceRunning() const { return m_serviceRunning; }
    QVariantList history() const { return m_history.toVariantList(); }
    QVariantList apps() const { return m_apps.toVariantList(); }
    QVariantList windows() const { return m_windows.toVariantList(); }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    // Planning.
    Q_INVOKABLE QString newPlan(const QString &name);
    Q_INVOKABLE void deletePlan(const QString &id);
    Q_INVOKABLE void setField(const QString &id, const QString &key, const QVariant &value);
    Q_INVOKABLE void toggleDay(const QString &id, int day);
    Q_INVOKABLE void addTask(const QString &id, const QString &text);
    Q_INVOKABLE void removeTask(const QString &id, int index);
    Q_INVOKABLE void addApp(const QString &id, const QVariantMap &app);
    Q_INVOKABLE void addWindowClass(const QString &id, const QString &windowClass, const QString &title);
    Q_INVOKABLE void removeApp(const QString &id, int index);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE QVariant nextRun(const QString &id) const;
    Q_INVOKABLE void refreshApps();
    Q_INVOKABLE void refreshWindows();

    // Running a session. Each returns an error to show, or an empty string.
    Q_INVOKABLE QString startPlan(const QString &id);
    Q_INVOKABLE QString pause(const QString &reason, int minutes);
    Q_INVOKABLE QString resume();
    Q_INVOKABLE QString endSession(const QString &reason);
    Q_INVOKABLE QString setTaskDone(int index, bool done);
    Q_INVOKABLE bool isReasonEnough(const QString &reason) const;
    Q_INVOKABLE QString enableService();

    // Sends one command to the service; used by the window and the CLI.
    static QJsonObject send(const QJsonObject &command, int timeoutMs = 3000);

signals:
    void plansChanged();
    void sessionChanged();
    void serviceChanged();
    void historyChanged();
    void appsChanged();
    void windowsChanged();

private:
    void reload();
    void commit(const QJsonArray &plans);
    void editPlan(const QString &id, const std::function<void(QJsonObject &)> &change);
    QString command(const QJsonObject &command);
    void checkService();

    QJsonArray m_plans;
    QJsonObject m_session;
    QJsonArray m_history;
    QJsonArray m_apps;
    QJsonArray m_windows;
    QList<QJsonArray> m_undo, m_redo;
    bool m_serviceRunning = false;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce, m_poll;
};
