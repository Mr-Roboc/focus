#include "cli.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include "backend.h"
#include "daemon.h"
#include "store.h"

namespace Cli {

static const QStringList kCommands{"daemon", "status", "list", "start", "pause", "resume", "end", "skill", "help",
                                   "--help", "-h", "--version"};

static const char *kUsage = R"(focus — scheduled focus sessions that only let chosen apps run.

Usage:
  focus [open]                       Open the window
  focus daemon                       Run the background service (systemd does this)
  focus status [--json]              Show the running session
  focus list [--json]                List planned sessions and when they run next
  focus start <id|name>              Start a planned session now
  focus pause --reason "…" [--minutes 5|10|15]
                                     Pause the running session (reason required)
  focus resume                       End a pause early
  focus end --reason "…"             End the running session early (reason required)
  focus skill install                Install a short skill so coding agents can drive focus
)";

static const char *kSkill = R"(---
name: focus
description: Check, start, pause, or end the user's scheduled focus sessions with the `focus` command. Use when asked about focus sessions, what they're working on now, or to start/pause/end one.
---

# focus

`focus` runs scheduled focus sessions on Omarchy: during one, only the apps the
user picked stay open and Do Not Disturb is on.

- `focus status --json` — the running session (`status`: idle, active, paused; `end`; `tasks`).
- `focus list --json` — planned sessions with their next start.
- `focus start <id|name>` — start one now.
- `focus pause --reason "<why, 3+ words>" --minutes 10` and `focus end --reason "<why>"`.

Pausing or ending needs the user's own reason. Never invent one; ask the user.
Exit status is 0 on success and 1 on failure, with the error on stderr.
)";

bool handles(const QStringList &arguments) { return arguments.size() > 1 && kCommands.contains(arguments.at(1)); }

static QString option(const QStringList &args, const QString &name, const QString &fallback = {}) {
    const qsizetype index = args.indexOf(name);
    return index >= 0 && index + 1 < args.size() ? args.at(index + 1) : fallback;
}

static int fail(const QString &message) {
    QTextStream(stderr) << "focus: " << message << "\n";
    return 1;
}

static int reply(const QJsonObject &answer) {
    if (!answer.value("ok").toBool())
        return fail(answer.value("error").toString());
    return 0;
}

int run(const QStringList &arguments) {
    QTextStream out(stdout);
    const QString command = arguments.at(1);
    const bool json = arguments.contains("--json");

    if (command == "help" || command == "--help" || command == "-h") {
        out << kUsage;
        return 0;
    }
    if (command == "--version") {
        out << "focus " << QCoreApplication::applicationVersion() << "\n";
        return 0;
    }
    if (command == "daemon") {
        Daemon daemon;
        if (!daemon.listen())
            return fail(QString("can't listen on %1").arg(Store::socketPath()));
        return QCoreApplication::exec();
    }
    if (command == "status") {
        const QJsonObject answer = Backend::send({{"cmd", "status"}});
        // Without the service, the last state on disk is still worth showing.
        QJsonObject state = answer.value("ok").toBool() ? answer : Store::loadState();
        state.insert("service", answer.value("ok").toBool());
        state.remove("ok");
        state.remove("handled");
        if (json) {
            out << QJsonDocument(state).toJson(QJsonDocument::Indented);
        } else if (state.value("status").toString() == "idle") {
            out << "No session running.\n";
        } else {
            out << state.value("name").toString() << " — " << state.value("status").toString() << " until "
                << QDateTime::fromString(state.value("end").toString(), Qt::ISODate).toString("HH:mm") << "\n";
            for (const auto &task : state.value("tasks").toArray())
                out << (task.toObject().value("done").toBool() ? "  [x] " : "  [ ] ")
                    << task.toObject().value("text").toString() << "\n";
        }
        if (!state.value("service").toBool())
            QTextStream(stderr) << "focus: the background service isn't running (systemctl --user start focus)\n";
        return 0;
    }
    if (command == "list") {
        const QDateTime now = QDateTime::currentDateTime();
        QJsonArray rows;
        for (const auto &value : Store::loadPlans()) {
            QJsonObject plan = value.toObject();
            QDateTime next = Store::currentOccurrence(plan, now);
            if (!next.isValid())
                next = Store::nextOccurrence(plan, now);
            plan.insert("next", next.isValid() ? QJsonValue(next.toString(Qt::ISODate)) : QJsonValue());
            rows.append(plan);
        }
        if (json) {
            out << QJsonDocument(rows).toJson(QJsonDocument::Indented);
        } else {
            for (const auto &row : rows) {
                const QJsonObject plan = row.toObject();
                out << plan.value("id").toString() << "  " << plan.value("name").toString() << "  "
                    << (plan.value("next").isNull()
                            ? QString("not scheduled")
                            : QDateTime::fromString(plan.value("next").toString(), Qt::ISODate)
                                  .toString("ddd d MMM HH:mm"))
                    << "  (" << plan.value("minutes").toInt() << " min)\n";
            }
        }
        return 0;
    }
    if (command == "start") {
        if (arguments.size() < 3)
            return fail("start needs a session id or name");
        return reply(Backend::send({{"cmd", "start"}, {"id", arguments.at(2)}}));
    }
    if (command == "pause")
        return reply(Backend::send({{"cmd", "pause"},
                                    {"reason", option(arguments, "--reason")},
                                    {"minutes", option(arguments, "--minutes", "10").toInt()}}));
    if (command == "resume")
        return reply(Backend::send({{"cmd", "resume"}}));
    if (command == "end")
        return reply(Backend::send({{"cmd", "end"}, {"reason", option(arguments, "--reason")}}));
    if (command == "skill") {
        if (arguments.value(2) != "install")
            return fail("usage: focus skill install");
        const QString dir = QDir::homePath() + "/.claude/skills/focus";
        QDir().mkpath(dir);
        QFile file(dir + "/SKILL.md");
        if (!file.open(QIODevice::WriteOnly))
            return fail("can't write " + file.fileName());
        file.write(kSkill);
        out << "Installed " << file.fileName() << "\n";
        return 0;
    }
    return fail("unknown command " + command);
}

} // namespace Cli
