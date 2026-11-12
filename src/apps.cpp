#include "apps.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace Apps {

QStringList classesFor(const QString &desktopId, const QString &startupWmClass, const QString &exec) {
    QStringList classes;
    if (!startupWmClass.isEmpty()) {
        classes << startupWmClass;
        // Omarchy's web apps name Chrome's class in their entry, but run in
        // whichever Chromium the user picked (brave-…, chromium-…).
        if (startupWmClass.startsWith("chrome-"))
            classes << "*-" + startupWmClass.mid(7);
    }
    if (!desktopId.isEmpty())
        classes << desktopId;

    const QStringList words = QProcess::splitCommand(exec);
    // Terminal apps launched with an explicit app id carry that id as class.
    for (const auto &word : words) {
        static const QRegularExpression appId("^--(?:app-id|class)=(.+)$");
        const auto match = appId.match(word);
        if (match.hasMatch())
            classes << match.captured(1);
    }
    // The program's own name, skipping env assignments and wrappers.
    for (const auto &word : words) {
        if (word == "env" || word.contains('=') || word.startsWith('-'))
            continue;
        const QString program = QFileInfo(word).fileName();
        if (!program.isEmpty() && !program.startsWith("omarchy-launch") && program != "xdg-terminal-exec" &&
            program != "uwsm" && program != "app")
            classes << program;
        break;
    }
    classes.removeDuplicates();
    return classes;
}

QJsonObject parseDesktopEntry(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    const QString language = QLocale().name();            // pt_BR
    const QString base = language.section('_', 0, 0);     // pt
    QMap<QString, QString> values;
    bool inEntry = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith('[')) {
            inEntry = line == "[Desktop Entry]";
            continue;
        }
        if (!inEntry || line.startsWith('#'))
            continue;
        const qsizetype equals = line.indexOf('=');
        if (equals > 0)
            values.insert(line.left(equals).trimmed(), line.mid(equals + 1).trimmed());
    }

    if (values.value("Type") != "Application" || values.value("NoDisplay") == "true" ||
        values.value("Hidden") == "true" || values.value("Name").isEmpty())
        return {};

    const QString name = values.value("Name[" + language + "]",
                                      values.value("Name[" + base + "]", values.value("Name")));
    const QString id = QFileInfo(path).completeBaseName();
    return QJsonObject{
        {"id", id},
        {"name", name},
        {"icon", values.value("Icon")},
        {"classes", QJsonArray::fromStringList(classesFor(id, values.value("StartupWMClass"), values.value("Exec")))},
    };
}

QJsonArray installed(const QStringList &directories) {
    QStringList dirs = directories;
    if (dirs.isEmpty())
        dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);

    // Earlier directories win, so a user's own entry hides the system one.
    QMap<QString, QJsonObject> byId;
    for (const auto &dir : dirs) {
        for (const auto &info : QDir(dir).entryInfoList({"*.desktop"}, QDir::Files)) {
            const QString id = info.completeBaseName();
            if (byId.contains(id))
                continue;
            const QJsonObject entry = parseDesktopEntry(info.filePath());
            byId.insert(id, entry);
        }
    }

    QList<QJsonObject> entries;
    for (const auto &entry : byId)
        if (!entry.isEmpty())
            entries.append(entry);
    std::sort(entries.begin(), entries.end(), [](const QJsonObject &a, const QJsonObject &b) {
        return QString::localeAwareCompare(a.value("name").toString(), b.value("name").toString()) < 0;
    });
    QJsonArray array;
    for (const auto &entry : entries)
        array.append(entry);
    return array;
}

} // namespace Apps
