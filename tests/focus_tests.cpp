#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

#include "apps.h"
#include "backend.h"
#include "daemon.h"
#include "enforcer.h"
#include "store.h"
#include "theme.h"
#include "translator.h"

// Everything runs in a temp directory, with no Hyprland and no real
// notifications, so the tests can never close a window on the desktop.
class FocusTests : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    static QJsonObject weekly(const QString &id, QJsonArray days, const QString &time, int minutes) {
        QJsonObject plan = Store::newPlan(id);
        plan.insert("id", id);
        plan.insert("days", days);
        plan.insert("time", time);
        plan.insert("minutes", minutes);
        return plan;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        qputenv("FOCUS_DATA_DIR", m_dir.filePath("data").toUtf8());
        qputenv("FOCUS_SOCKET", m_dir.filePath("focus.sock").toUtf8());
        qputenv("FOCUS_HYPR_DIR", m_dir.filePath("no-hyprland").toUtf8());
        qputenv("FOCUS_DRY_RUN", "1");
    }

    void init()
    {
        QFile::remove(Store::plansPath());
        QFile::remove(Store::statePath());
        QFile::remove(Store::logPath());
    }

    void weeklyOccurrence()
    {
        // Friday 13 November 2026, 15:00 for an hour.
        const QJsonObject plan = weekly("p", {5}, "15:00", 60);
        const QDate friday(2026, 11, 13);
        QCOMPARE(friday.dayOfWeek(), 5);
        QVERIFY(!Store::currentOccurrence(plan, QDateTime(friday, QTime(14, 59))).isValid());
        QCOMPARE(Store::currentOccurrence(plan, QDateTime(friday, QTime(15, 0))), QDateTime(friday, QTime(15, 0)));
        QCOMPARE(Store::currentOccurrence(plan, QDateTime(friday, QTime(15, 59))), QDateTime(friday, QTime(15, 0)));
        QVERIFY(!Store::currentOccurrence(plan, QDateTime(friday, QTime(16, 0))).isValid());
        QCOMPARE(Store::nextOccurrence(plan, QDateTime(friday, QTime(16, 0))), QDateTime(friday.addDays(7), QTime(15, 0)));
    }

    void overnightOccurrence()
    {
        const QJsonObject plan = weekly("night", {1}, "23:00", 120);
        const QDate monday(2026, 11, 9);
        QCOMPARE(Store::currentOccurrence(plan, QDateTime(monday.addDays(1), QTime(0, 30))), QDateTime(monday, QTime(23, 0)));
        QVERIFY(!Store::currentOccurrence(plan, QDateTime(monday.addDays(1), QTime(1, 0))).isValid());
    }

    void oneTimeAndDisabled()
    {
        QJsonObject plan = weekly("once", {}, "09:30", 30);
        plan.insert("date", "2026-11-20");
        QCOMPARE(Store::currentOccurrence(plan, QDateTime(QDate(2026, 11, 20), QTime(9, 45))), QDateTime(QDate(2026, 11, 20), QTime(9, 30)));
        QVERIFY(!Store::currentOccurrence(plan, QDateTime(QDate(2026, 11, 27), QTime(9, 45))).isValid());
        QCOMPARE(Store::nextOccurrence(plan, QDateTime(QDate(2026, 11, 1), QTime(8, 0))), QDateTime(QDate(2026, 11, 20), QTime(9, 30)));
        plan.insert("enabled", false);
        QVERIFY(!Store::currentOccurrence(plan, QDateTime(QDate(2026, 11, 20), QTime(9, 45))).isValid());
        QVERIFY(!Store::nextOccurrence(plan, QDateTime(QDate(2026, 11, 19), QTime(9, 0))).isValid());
    }

    void classMatching()
    {
        const QStringList patterns{"com.mitchellh.ghostty", "*-web.whatsapp.com__-Default", "code"};
        QVERIFY(Enforcer::isAllowed("com.mitchellh.ghostty", patterns));
        QVERIFY(Enforcer::isAllowed("Code", patterns));
        QVERIFY(Enforcer::isAllowed("brave-web.whatsapp.com__-Default", patterns));
        QVERIFY(!Enforcer::isAllowed("brave-browser", patterns));
        QVERIFY(!Enforcer::isAllowed("code-insiders", patterns));
        QVERIFY(Enforcer::isAllowed("focus", Store::builtinClasses()));
        QVERIFY(Enforcer::isAllowed("xdg-desktop-portal-gtk", Store::builtinClasses()));
    }

    void desktopEntryClasses()
    {
        const QStringList web = Apps::classesFor("WhatsApp", "chrome-web.whatsapp.com__-Default",
                                                 "omarchy-launch-webapp https://web.whatsapp.com/");
        QVERIFY(web.contains("*-web.whatsapp.com__-Default"));
        QVERIFY(!web.contains("omarchy-launch-webapp"));
        const QStringList tui = Apps::classesFor("Docker", "", "xdg-terminal-exec --app-id=TUI.tile -e lazydocker");
        QVERIFY(tui.contains("TUI.tile"));
        const QStringList plain = Apps::classesFor("org.gnome.Nautilus", "", "env GTK_THEME=x /usr/bin/nautilus --new-window %U");
        QVERIFY(plain.contains("org.gnome.Nautilus"));
        QVERIFY(plain.contains("nautilus"));
    }

    void desktopEntryParsing()
    {
        QTemporaryDir apps;
        auto write = [&](const QString &name, const QByteArray &text) {
            QFile file(apps.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(text);
        };
        write("editor.desktop", "[Desktop Entry]\nType=Application\nName=Editor\nExec=editor %F\nIcon=editor\n"
                                "[Desktop Action new]\nName=Ignored\n");
        write("hidden.desktop", "[Desktop Entry]\nType=Application\nName=Hidden\nNoDisplay=true\nExec=hidden\n");
        const QJsonArray entries = Apps::installed({apps.path()});
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].toObject().value("name").toString(), QString("Editor"));
    }

    void reasons()
    {
        QVERIFY(!Store::isReasonEnough(""));
        QVERIFY(!Store::isReasonEnough("tired"));
        QVERIFY(!Store::isReasonEnough("a b c d"));
        QVERIFY(Store::isReasonEnough("lunch with team"));
    }

    void manualStartPauseResumeEnd()
    {
        QJsonObject plan = weekly("w", {}, "15:00", 30);
        plan.insert("name", "Write report");
        plan.insert("tasks", QJsonArray{"outline", "draft"});
        QVERIFY(Store::savePlans({plan}));

        Daemon daemon;
        QVERIFY(daemon.handle({{"cmd", "start"}, {"id", "write report"}}).value("ok").toBool());
        QCOMPARE(daemon.state().value("status").toString(), QString("active"));
        QCOMPARE(daemon.state().value("tasks").toArray().size(), 2);
        QCOMPARE(Store::loadState().value("status").toString(), QString("active"));
        QVERIFY(!daemon.handle({{"cmd", "start"}, {"id", "w"}}).value("ok").toBool());

        QVERIFY(daemon.handle({{"cmd", "task"}, {"index", 1}, {"done", true}}).value("ok").toBool());
        QVERIFY(Store::loadState().value("tasks").toArray()[1].toObject().value("done").toBool());

        QVERIFY(!daemon.handle({{"cmd", "pause"}, {"reason", "bored"}, {"minutes", 5}}).value("ok").toBool());
        QVERIFY(daemon.handle({{"cmd", "pause"}, {"reason", "coffee with a friend"}, {"minutes", 5}}).value("ok").toBool());
        QCOMPARE(daemon.state().value("status").toString(), QString("paused"));
        // The pause ends on its own.
        daemon.tick(QDateTime::currentDateTime().addSecs(6 * 60));
        QCOMPARE(daemon.state().value("status").toString(), QString("active"));

        QVERIFY(!daemon.handle({{"cmd", "end"}, {"reason", ""}}).value("ok").toBool());
        QVERIFY(daemon.handle({{"cmd", "end"}, {"reason", "the deadline moved"}}).value("ok").toBool());
        QCOMPARE(Store::loadState().value("status").toString(), QString("idle"));

        const QJsonArray log = Store::readLog(10);
        QCOMPARE(log[0].toObject().value("event").toString(), QString("ended"));
        QCOMPARE(log[0].toObject().value("reason").toString(), QString("the deadline moved"));
    }

    void scheduledStartRunsOncePerSlot()
    {
        const QDateTime now = QDateTime::currentDateTime();
        const QJsonObject plan = weekly("s", {now.date().dayOfWeek()},
                                        now.addSecs(-60).time().toString("HH:mm"), 30);
        QVERIFY(Store::savePlans({plan}));

        Daemon daemon;
        daemon.tick(now);
        QCOMPARE(daemon.state().value("status").toString(), QString("active"));
        QVERIFY(daemon.handle({{"cmd", "end"}, {"reason", "done a bit early"}}).value("ok").toBool());
        // Ended early: the same slot must not start again.
        daemon.tick(now.addSecs(10));
        QCOMPARE(daemon.state().value("status").toString(), QString("idle"));
    }

    void sessionCompletesAtItsEnd()
    {
        const QDateTime now = QDateTime::currentDateTime();
        const QJsonObject plan = weekly("c", {now.date().dayOfWeek()}, now.addSecs(-60).time().toString("HH:mm"), 30);
        QVERIFY(Store::savePlans({plan}));
        Daemon daemon;
        daemon.tick(now);
        daemon.tick(now.addSecs(31 * 60));
        QCOMPARE(daemon.state().value("status").toString(), QString("idle"));
        QCOMPARE(Store::readLog(1)[0].toObject().value("event").toString(), QString("completed"));
    }

    void serviceAnswersOverTheSocket()
    {
        QVERIFY(Store::savePlans({weekly("sock", {}, "15:00", 30)}));
        Daemon daemon;
        QVERIFY(daemon.listen());
        QJsonObject reply;
        QThread *client = QThread::create([&] { reply = Backend::send({{"cmd", "start"}, {"id", "sock"}}); });
        client->start();
        QTRY_VERIFY(client->isFinished());
        delete client;
        QVERIFY(reply.value("ok").toBool());
        QCOMPARE(daemon.state().value("status").toString(), QString("active"));
        daemon.handle({{"cmd", "end"}, {"reason", "test is over now"}});
    }

    void backendUndoRedo()
    {
        Backend backend;
        const QString id = backend.newPlan("Deep work");
        backend.setField(id, "name", "Deeper work");
        backend.addTask(id, "read");
        QCOMPARE(Store::findPlan(Store::loadPlans(), id).value("tasks").toArray().size(), 1);
        backend.undo();
        QCOMPARE(Store::findPlan(Store::loadPlans(), id).value("tasks").toArray().size(), 0);
        backend.undo();
        QCOMPARE(Store::findPlan(Store::loadPlans(), id).value("name").toString(), QString("Deep work"));
        backend.redo();
        QCOMPARE(Store::findPlan(Store::loadPlans(), id).value("name").toString(), QString("Deeper work"));
        backend.toggleDay(id, 6);
        QVERIFY(Store::findPlan(Store::loadPlans(), id).value("days").toArray().contains(6));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        auto write = [](const QString &path, const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write(response, "{\"int\":0}"));

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(write(response, "{\"int\":6}"));
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(FocusTests)
#include "focus_tests.moc"
