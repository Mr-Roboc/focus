#include "store.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace Store {

// FOCUS_DATA_DIR and FOCUS_SOCKET let tests and a second copy run without
// touching the real sessions.
QString dataDirectory() {
    const QString overridden = qEnvironmentVariable("FOCUS_DATA_DIR");
    const QString dir = !overridden.isEmpty()
        ? overridden
        : QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/focus";
    QDir().mkpath(dir);
    return dir;
}

QString plansPath() { return dataDirectory() + "/sessions.json"; }
QString statePath() { return dataDirectory() + "/state.json"; }
QString logPath() { return dataDirectory() + "/log.jsonl"; }

QString socketPath() {
    const QString overridden = qEnvironmentVariable("FOCUS_SOCKET");
    if (!overridden.isEmpty())
        return overridden;
    QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtime.isEmpty())
        runtime = QDir::tempPath();
    return runtime + "/focus.sock";
}

QJsonArray readArray(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).array();
}

QJsonObject readObject(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool writeJson(const QString &path, const QJsonValue &value) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QJsonDocument document = value.isArray() ? QJsonDocument(value.toArray()) : QJsonDocument(value.toObject());
    file.write(document.toJson(QJsonDocument::Indented));
    return file.commit();
}

QJsonArray loadPlans() { return readArray(plansPath()); }
bool savePlans(const QJsonArray &plans) { return writeJson(plansPath(), plans); }

QJsonObject findPlan(const QJsonArray &plans, const QString &idOrName) {
    for (const auto &value : plans)
        if (value.toObject().value("id").toString() == idOrName)
            return value.toObject();
    for (const auto &value : plans)
        if (value.toObject().value("name").toString().compare(idOrName, Qt::CaseInsensitive) == 0)
            return value.toObject();
    return {};
}

QJsonObject newPlan(const QString &name) {
    const QDateTime now = QDateTime::currentDateTime();
    return QJsonObject{
        {"id", QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)},
        {"name", name},
        {"tasks", QJsonArray()},
        {"days", QJsonArray{now.date().dayOfWeek()}},
        {"date", ""},
        {"time", QTime(now.time().hour() + (now.time().hour() < 23 ? 1 : 0), 0).toString("HH:mm")},
        {"minutes", 60},
        {"apps", QJsonArray()},
        {"dnd", true},
        {"enabled", true},
    };
}

QJsonObject loadState() {
    QJsonObject state = readObject(statePath());
    if (!state.contains("status"))
        state.insert("status", "idle");
    return state;
}

bool saveState(const QJsonObject &state) { return writeJson(statePath(), state); }

void appendLog(const QJsonObject &entry) {
    QJsonObject line = entry;
    line.insert("at", QDateTime::currentDateTime().toString(Qt::ISODate));
    QFile file(logPath());
    if (file.open(QIODevice::Append))
        file.write(QJsonDocument(line).toJson(QJsonDocument::Compact) + "\n");
}

QJsonArray readLog(int limit) {
    QFile file(logPath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QList<QByteArray> lines = file.readAll().split('\n');
    QJsonArray entries;
    for (auto it = lines.crbegin(); it != lines.crend() && entries.size() < limit; ++it) {
        const QJsonObject object = QJsonDocument::fromJson(*it).object();
        if (!object.isEmpty())
            entries.append(object);
    }
    return entries;
}

static QDateTime startOn(const QJsonObject &plan, const QDate &date) {
    const QTime time = QTime::fromString(plan.value("time").toString(), "HH:mm");
    if (!time.isValid() || !date.isValid())
        return {};
    const QString oneTime = plan.value("date").toString();
    if (!oneTime.isEmpty()) {
        if (QDate::fromString(oneTime, Qt::ISODate) != date)
            return {};
    } else if (!plan.value("days").toArray().contains(date.dayOfWeek())) {
        return {};
    }
    return QDateTime(date, time);
}

static int minutesOf(const QJsonObject &plan) { return qMax(1, plan.value("minutes").toInt(60)); }

QDateTime currentOccurrence(const QJsonObject &plan, const QDateTime &now) {
    if (!plan.value("enabled").toBool(true))
        return {};
    // Yesterday's start still counts when the session runs past midnight.
    for (int offset : {0, -1}) {
        const QDateTime start = startOn(plan, now.date().addDays(offset));
        if (start.isValid() && start <= now && now < start.addSecs(60LL * minutesOf(plan)))
            return start;
    }
    return {};
}

QDateTime nextOccurrence(const QJsonObject &plan, const QDateTime &now) {
    if (!plan.value("enabled").toBool(true))
        return {};
    // A one-time session can be planned any distance ahead.
    const QString oneTime = plan.value("date").toString();
    if (!oneTime.isEmpty()) {
        const QDateTime start = startOn(plan, QDate::fromString(oneTime, Qt::ISODate));
        return start.isValid() && start > now ? start : QDateTime();
    }
    for (int offset = 0; offset <= 8; ++offset) {
        const QDateTime start = startOn(plan, now.date().addDays(offset));
        if (start.isValid() && start > now)
            return start;
    }
    return {};
}

QString occurrenceKey(const QJsonObject &plan, const QDateTime &start) {
    return plan.value("id").toString() + "@" + start.toString(Qt::ISODate);
}

QStringList builtinClasses() {
    return {
        "focus",
        "xdg-desktop-portal-*",
        "hyprland-share-picker",
        "hyprpolkitagent",
        "*polkit*",
        "pinentry*",
        "gcr-prompter",
    };
}

QStringList allowedClasses(const QJsonObject &plan) {
    QStringList classes = builtinClasses();
    for (const auto &app : plan.value("apps").toArray())
        for (const auto &cls : app.toObject().value("classes").toArray())
            if (!cls.toString().isEmpty())
                classes.append(cls.toString());
    classes.removeDuplicates();
    return classes;
}

bool isReasonEnough(const QString &reason) {
    static const QRegularExpression word("\\w{2,}", QRegularExpression::UseUnicodePropertiesOption);
    int words = 0;
    auto it = word.globalMatch(reason);
    while (it.hasNext()) {
        it.next();
        ++words;
    }
    return words >= 3;
}

} // namespace Store
