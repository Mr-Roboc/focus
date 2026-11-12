#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

// The apps a session can allow: the launcher's entries, and the windows open
// right now (for anything whose launcher entry doesn't name its class).
namespace Apps {

// Every visible .desktop entry as { id, name, icon, classes }, sorted by name.
QJsonArray installed(const QStringList &directories = {});
QJsonObject parseDesktopEntry(const QString &path);

// The window classes a desktop entry's windows are likely to carry.
QStringList classesFor(const QString &desktopId, const QString &startupWmClass, const QString &exec);

} // namespace Apps
