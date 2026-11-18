// Pegasus Frontend
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.


#include <QtTest/QtTest>

#include "Paths.h"
#include "ScriptRunner.h"


// Pegasus X: event scripts in the settings folder's scripts/<event>/ folders.
// On Android they're run by /system/bin/sh (any readable file); this checks
// the desktop side, which keeps running executable files only, as before
class test_ScriptRunner : public QObject {
    Q_OBJECT

private:
    QString m_scripts;
    QString m_out;

    void write_script(const QString& name, bool executable);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void game_start_script();
    void no_scripts();
};

void test_ScriptRunner::initTestCase()
{
#ifdef Q_OS_WIN
    QSKIP("shell scripts");
#endif
    QStandardPaths::setTestModeEnabled(true);
    m_scripts = paths::writableConfigDir() + QStringLiteral("/scripts");
    m_out = paths::writableConfigDir() + QStringLiteral("/script-ran.txt");
    QDir(m_scripts).removeRecursively();
    QFile::remove(m_out);
    QVERIFY(QDir().mkpath(m_scripts + QStringLiteral("/game-start")));
}

void test_ScriptRunner::cleanupTestCase()
{
    if (!m_scripts.isEmpty())
        QDir(m_scripts).removeRecursively();
    if (!m_out.isEmpty())
        QFile::remove(m_out);
}

void test_ScriptRunner::write_script(const QString& name, bool executable)
{
    QFile file(m_scripts + QStringLiteral("/game-start/") + name);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(QStringLiteral("#!/bin/sh\necho \"%1 $1\" >> \"%2\"\n").arg(name, m_out).toUtf8());
    file.close();
    QFileDevice::Permissions perms = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    if (executable)
        perms |= QFileDevice::ExeOwner;
    QVERIFY(file.setPermissions(perms));
}

// an executable script runs with the game's path; a file that isn't
// executable doesn't run on desktop
void test_ScriptRunner::game_start_script()
{
    write_script(QStringLiteral("10-runs.sh"), true);
    write_script(QStringLiteral("20-not-executable.sh"), false);

    ScriptRunner::run(ScriptEvent::PROCESS_STARTED, { QStringLiteral("/roms/snes/game.sfc") });

    QFile out(m_out);
    QVERIFY(out.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(out.readAll()), QStringLiteral("10-runs.sh /roms/snes/game.sfc\n"));
}

// no folder for an event: nothing runs, nothing breaks
void test_ScriptRunner::no_scripts()
{
    QFile::remove(m_out);
    ScriptRunner::run(ScriptEvent::QUIT);
    QVERIFY(!QFile::exists(m_out));
}


QTEST_MAIN(test_ScriptRunner)
#include "test_ScriptRunner.moc"
