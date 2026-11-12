#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QLocalSocket>
#include <QObject>
#include <QStringList>
#include <QTimer>

// Keeps only allowed windows open while a session runs. It listens on
// Hyprland's event socket for new windows and also sweeps every few seconds,
// because some apps (Chromium web apps) settle on their class after mapping.
class Enforcer : public QObject {
    Q_OBJECT

public:
    explicit Enforcer(QObject *parent = nullptr);

    // An empty list switches enforcement off.
    void setAllowed(const QStringList &classes);
    bool isActive() const { return !m_allowed.isEmpty(); }
    bool isConnected() const;

    // Class patterns use shell wildcards and ignore case.
    static bool isAllowed(const QString &windowClass, const QStringList &patterns);

    // Sends one request on Hyprland's command socket and returns the reply.
    static QByteArray request(const QByteArray &command);

    // Brings back windows that refused to close and were parked out of sight.
    void releaseParked();

signals:
    void blocked(const QString &windowClass, const QString &title);
    void hyprlandMissing();

private:
    void connectEvents();
    void readEvents();
    void sweep();
    void check(const QString &address, const QString &windowClass, const QString &title);
    static QString hyprDirectory();

    QStringList m_allowed;
    QLocalSocket m_events;
    QByteArray m_buffer;
    QTimer m_sweep;
    QTimer m_reconnect;
    // When each window was asked to close, so one that ignores the request
    // gets parked on a hidden workspace instead of asked forever.
    QHash<QString, QElapsedTimer> m_closing;
};
