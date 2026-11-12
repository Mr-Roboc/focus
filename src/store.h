#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// Everything focus keeps on disk lives in one directory:
//
//   sessions.json  the planned sessions, written by the window
//   state.json     the running session, written only by the background service
//   log.jsonl      one line per start, pause, resume, end, and blocked window
//
// A plan is a JSON object:
//   { "id", "name", "tasks": [text], "days": [1..7, Monday first],
//     "date": "yyyy-MM-dd" (a one-time session; empty when it repeats),
//     "time": "HH:mm", "minutes", "apps": [{ "id", "name", "icon", "classes" }],
//     "dnd": bool, "enabled": bool }
namespace Store {

QString dataDirectory();
QString plansPath();
QString statePath();
QString logPath();
QString socketPath();

QJsonArray readArray(const QString &path);
QJsonObject readObject(const QString &path);
// Writes beside the target and renames over it, so a reader never sees half
// a file and a crash never loses the previous version.
bool writeJson(const QString &path, const QJsonValue &value);

QJsonArray loadPlans();
bool savePlans(const QJsonArray &plans);
QJsonObject findPlan(const QJsonArray &plans, const QString &idOrName);
QJsonObject newPlan(const QString &name);

QJsonObject loadState();
bool saveState(const QJsonObject &state);

void appendLog(const QJsonObject &entry);
QJsonArray readLog(int limit);

// The occurrence of a plan that contains `now`, or an invalid date when the
// plan isn't scheduled right now. Sessions may cross midnight.
QDateTime currentOccurrence(const QJsonObject &plan, const QDateTime &now);
// The first occurrence starting after `now` (repeating sessions look a week ahead).
QDateTime nextOccurrence(const QJsonObject &plan, const QDateTime &now);
// Identifies one occurrence, so a session that was ended early isn't started
// again inside the same slot.
QString occurrenceKey(const QJsonObject &plan, const QDateTime &start);

// Window classes allowed by a plan: every app's classes plus the ones focus
// always needs (itself, portals, password prompts).
QStringList allowedClasses(const QJsonObject &plan);
QStringList builtinClasses();

// A pause or an early end needs a reason of a few real words.
bool isReasonEnough(const QString &reason);

} // namespace Store
