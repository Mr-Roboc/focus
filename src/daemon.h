#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QLocalServer>
#include <QObject>
#include <QSet>
#include <QTimer>

#include "enforcer.h"

// The background service (`focus daemon`, run by systemd). It alone writes
// state.json: it starts sessions when their time comes, enforces the allowed
// apps, keeps Do Not Disturb on, and answers the window and the command line
// over a local socket, one JSON object per line each way.
class Daemon : public QObject {
    Q_OBJECT

public:
    explicit Daemon(QObject *parent = nullptr);
    bool listen();

    // The commands the socket accepts; each returns { "ok": bool, "error"? }.
    QJsonObject handle(const QJsonObject &command);

    QJsonObject state() const { return m_state; }
    void tick(const QDateTime &now);

private:
    QJsonObject start(const QJsonObject &plan, const QDateTime &start, const QDateTime &end);
    QJsonObject pause(const QString &reason, int minutes);
    QJsonObject resume();
    QJsonObject finish(const QString &event, const QString &reason);
    QJsonObject setTask(int index, bool done);

    void applyEnforcement();
    void setDnd(bool on);
    QString currentDnd() const;
    void notify(const QString &headline, const QString &body = {});
    void save();
    void log(const QString &event, const QString &reason = {}, const QJsonObject &extra = {});

    QJsonObject m_state;
    QLocalServer m_server;
    QTimer m_timer;
    Enforcer m_enforcer;
    QSet<QString> m_warned;
    QHash<QString, qint64> m_lastBlockNotice;
};
