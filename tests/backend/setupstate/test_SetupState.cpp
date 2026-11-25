// Pegasus X
// Copyright (C) 2026  Esteban (MisterE88)
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

#include "SetupState.h"

#include <QTemporaryDir>


// Pegasus X: setup done or not, once at the start: the marker, a step to go
// on from (an older build's too), an older build's finished setup, data
// found after the app's own data was cleared
class test_SetupState : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmp;
    setupstate::Dirs m_dirs;

    static void write(const QString& p, const QByteArray& bytes) {
        QDir().mkpath(QFileInfo(p).absolutePath());
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(bytes);
    }
    static QByteArray read(const QString& p) {
        QFile f(p);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }
    QString priv(const QString& name) const { return m_dirs.priv + QLatin1Char('/') + name; }
    QString data(const QString& name) const { return m_dirs.data + QLatin1Char('/') + name; }

private slots:
    void init();
    void new_install_starts_at_step_1();
    void marker_means_done();
    void goes_on_at_its_step();
    void older_builds_step_moved();
    void older_builds_finished_setup_is_done();
    void cleared_app_data_asks_about_data_found();
    void settings_defaults_are_not_data();
    void finish_and_reopen();
};

void test_SetupState::init()
{
    QVERIFY(m_tmp.isValid());
    for (const char* sub : { "files", "config", "PegasusX", "cache" })
        QDir(m_tmp.path() + QLatin1Char('/') + QLatin1String(sub)).removeRecursively();
    m_dirs.app_data = m_tmp.path() + QStringLiteral("/files");
    m_dirs.priv = m_tmp.path() + QStringLiteral("/config");
    m_dirs.data = m_tmp.path() + QStringLiteral("/PegasusX");
    m_dirs.cache = m_tmp.path() + QStringLiteral("/cache");
    for (const QString& d : { m_dirs.app_data, m_dirs.priv, m_dirs.data, m_dirs.cache })
        QDir().mkpath(d);
}

void test_SetupState::new_install_starts_at_step_1()
{
    const setupstate::State st = setupstate::decide(m_dirs);
    QVERIFY(!st.done);
    QCOMPARE(st.step, 1);
    QVERIFY(st.found.isEmpty());
    QCOMPARE(setupstate::step(m_dirs), 1);   // (written: a restart goes on there)
    QVERIFY(st.log.startsWith(QLatin1String("Setup: ")));
}

void test_SetupState::marker_means_done()
{
    write(setupstate::done_path(m_dirs), "done\n");
    // (even with an older build's step left: the marker decides)
    write(priv(QStringLiteral("pegasusx-wizard")), "2\n");
    const setupstate::State st = setupstate::decide(m_dirs);
    QVERIFY(st.done);
}

void test_SetupState::goes_on_at_its_step()
{
    setupstate::set_step(m_dirs, 3);
    const setupstate::State st = setupstate::decide(m_dirs);
    QVERIFY(!st.done);
    QCOMPARE(st.step, 3);
    // a step the file can't hold: not a step
    write(setupstate::step_path(m_dirs), "9\n");
    QCOMPARE(setupstate::step(m_dirs), 0);
}

void test_SetupState::older_builds_step_moved()
{
    write(priv(QStringLiteral("pegasusx-wizard")), "2\n");
    write(priv(QStringLiteral("pegasusx-root-asked")), "asked\n");
    const setupstate::State st = setupstate::decide(m_dirs);
    QVERIFY(!st.done);
    QCOMPARE(st.step, 2);
    QCOMPARE(setupstate::step(m_dirs), 2);
    QVERIFY(!QFileInfo::exists(priv(QStringLiteral("pegasusx-wizard"))));
}

void test_SetupState::older_builds_finished_setup_is_done()
{
    write(priv(QStringLiteral("pegasusx-root.txt")), m_dirs.data.toUtf8() + '\n');
    write(data(QStringLiteral("game_dirs.txt")), "/roms/snes\n");
    write(m_dirs.cache + QStringLiteral("/gameindex-v1.json"), "{}");
    const setupstate::State st = setupstate::decide(m_dirs);
    QVERIFY(st.done);
    QVERIFY(QFileInfo::exists(setupstate::done_path(m_dirs)));
    QVERIFY(setupstate::decide(m_dirs).done);
}

void test_SetupState::cleared_app_data_asks_about_data_found()
{
    // Android's Clear data: the app's storage and cache empty; the private
    // folder (on shared storage) and the Pegasus X folder still there
    write(priv(QStringLiteral("pegasusx-root-asked")), "asked\n");
    write(data(QStringLiteral("gamedirectory/snes/metadata.pegasus.txt")), "collection: SNES\n");
    write(data(QStringLiteral("theme_settings/XBSX.json")), "{}");
    const setupstate::State st = setupstate::decide(m_dirs);
    QVERIFY(!st.done);
    QCOMPARE(st.step, 1);
    QCOMPARE(st.found, m_dirs.data);
    QVERIFY(!QFileInfo::exists(setupstate::done_path(m_dirs)));
}

void test_SetupState::settings_defaults_are_not_data()
{
    write(data(QStringLiteral("settings.txt")), "general.theme: :/themes/pegasus-theme-grid/\n");
    write(data(QStringLiteral("gamedirectory/.migrated")), "");
    write(data(QStringLiteral("favorites.txt")), "");
    QVERIFY(!setupstate::holds_data(m_dirs.data));
    QVERIFY(setupstate::decide(m_dirs).found.isEmpty());
    write(data(QStringLiteral("favorites.txt")), "/roms/a.sfc\n");
    QVERIFY(setupstate::holds_data(m_dirs.data));
}

void test_SetupState::finish_and_reopen()
{
    setupstate::set_step(m_dirs, 4);
    write(priv(QStringLiteral("pegasusx-wizard")), "4\n");
    QVERIFY(setupstate::finish(m_dirs));
    QCOMPARE(read(setupstate::done_path(m_dirs)), QByteArray("done\n"));
    QCOMPARE(setupstate::step(m_dirs), 0);
    QVERIFY(!QFileInfo::exists(priv(QStringLiteral("pegasusx-wizard"))));
    // three restarts: done each time
    for (int i = 0; i < 3; i++)
        QVERIFY(setupstate::decide(m_dirs).done);
    // Run setup again / Start over: from step 1, no marker
    setupstate::reopen(m_dirs, 1);
    QVERIFY(!QFileInfo::exists(setupstate::done_path(m_dirs)));
    const setupstate::State st = setupstate::decide(m_dirs);
    QVERIFY(!st.done);
    QCOMPARE(st.step, 1);
}


QTEST_MAIN(test_SetupState)
#include "test_SetupState.moc"
