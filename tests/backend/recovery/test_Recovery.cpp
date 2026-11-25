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

#include "Recovery.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>


// Pegasus X: damaged-file recovery (Recovery.h), on a throwaway folder with
// every kind of file: damaged ones (junk, cut in half) moved to `damaged`
// and replaced by their last good copy (or their defaults), files emptied on
// purpose left alone, files missing after an interrupted save put back. Every
// other file in the folder is checked to be the same, byte for byte.
using recovery::Kind;

class test_Recovery : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmp;
    QString dir() const { return m_tmp.path() + QStringLiteral("/data"); }
    QString damaged() const { return m_tmp.path() + QStringLiteral("/data/damaged"); }
    QString path(const QString& name) const { return dir() + QLatin1Char('/') + name; }
    static const QDateTime NOW;

    static QByteArray read(const QString& p) {
        QFile f(p);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }
    static void write(const QString& p, const QByteArray& bytes) {
        QDir().mkpath(QFileInfo(p).absolutePath());
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(f.write(bytes), qint64(bytes.size()));
    }
    // every file under the folder and its checksum
    QMap<QString, QByteArray> snapshot() const {
        QMap<QString, QByteArray> out;
        QDirIterator it(dir(), QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString p = it.next();
            out.insert(QDir(dir()).relativeFilePath(p), QCryptographicHash::hash(read(p), QCryptographicHash::Sha1));
        }
        return out;
    }
    // the files of `before` that aren't `name`'s (or in `damaged`) are the same
    void others_unchanged(const QMap<QString, QByteArray>& before, const QString& name) {
        const QMap<QString, QByteArray> after = snapshot();
        for (auto it = before.cbegin(); it != before.cend(); ++it) {
            if (it.key().startsWith(name) || it.key().startsWith(QLatin1String("damaged/")))
                continue;
            QVERIFY2(after.value(it.key()) == it.value(), qPrintable(it.key()));
        }
    }
    recovery::Spec spec(const QString& name, Kind kind) const {
        return { dir(), name, kind, name, damaged() };
    }
    QStringList in_damaged() const {
        return QDir(damaged()).entryList(QDir::Files, QDir::Name);
    }
    static QByteArray sqlite_db(const QString& p, int rows) {
        QFile::remove(p);
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("make"));
            db.setDatabaseName(p);
            db.open();
            QSqlQuery q(db);
            q.exec(QStringLiteral("CREATE TABLE plays (id INTEGER PRIMARY KEY, path TEXT, start INTEGER, duration INTEGER)"));
            for (int i = 0; i < rows; i++)
                q.exec(QStringLiteral("INSERT INTO plays (path, start, duration) VALUES ('/roms/game%1.sfc', %1, %2)").arg(i).arg(i * 60));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("make"));
        return read(p);
    }

    // the good versions of every kind
    static QByteArray good_of(Kind kind);

private slots:
    void init();
    void damaged_files_data();
    void damaged_files();
    void damaged_files_without_good_copy_start_at_defaults();
    void damaged_good_copy_too();
    void empty_on_purpose_is_not_damage();
    void rom_folders_emptied();
    void missing_with_pxold();
    void missing_with_pxsave();
    void missing_with_both();
    void missing_with_both_pxsave_cut();
    void missing_with_nothing_whole();
    void fine_file_leftovers_removed();
    void good_copy_only_from_a_file_that_passed();
    void missing_without_leftovers_is_left_alone();
};

const QDateTime test_Recovery::NOW = QDateTime(QDate(2026, 11, 24), QTime(22, 30, 5));

QByteArray test_Recovery::good_of(Kind kind)
{
    switch (kind) {
        case Kind::Settings: return "general.verify-files: true\ngeneral.show-missing-games: false\nkeys.accept: Return,GamepadA\n# a comment\nproviders.steam.enabled: false\n";
        case Kind::Ini: return "[General]\ncheckUpdatesOnStart=true\nscraper\\imageMaxSize=1920\n\n[scraper]\nsource=launchbox\n";
        case Kind::Lines: return "/storage/emulated/0/ROMs/snes/Chrono Trigger (USA).sfc\n/storage/emulated/0/ROMs/gba/Golden Sun (USA).gba\n";
        case Kind::RomFolders: return "/storage/emulated/0/ROMs/snes\tsnes\n/storage/1234-5678/ROMs/gba\tgba\n";
        case Kind::Root: return "/storage/1234-5678/PegasusX\n";
        case Kind::JsonObject: return "{\n    \"collections\": [ \"BIOS\" ],\n    \"games\": [ { \"path\": \"/roms/x.sfc\", \"title\": \"X\", \"collections\": [] } ]\n}\n";
        case Kind::JsonArray: return "[\n    { \"for\": \"snes\", \"label\": \"Snes9x\", \"command\": \"am start -n com.snes9x/.Main\" }\n]\n";
        case Kind::SdlMappings: return "# Pegasus X\n030000005e040000ea02000000007801,Xbox Wireless Controller,a:b0,b:b1,x:b2,y:b3,platform:Android,\n";
        case Kind::Sqlite: return {};
    }
    return {};
}

void test_Recovery::init()
{
    QVERIFY(m_tmp.isValid());
    QDir(dir()).removeRecursively();
    QDir().mkpath(dir());
    // a few unrelated files, to be left alone
    write(path(QStringLiteral("theme_settings/xbsx.json")), "{\"accent\":\"green\"}");
    write(path(QStringLiteral("lastrun.log")), "log\n");
    write(path(QStringLiteral("metadata/snes/metadata.pegasus.txt")), "collection: snes\n");
}

// each kind: junk, or cut in half -> moved to damaged/, its last good copy back
void test_Recovery::damaged_files_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("kind");
    QTest::addColumn<QByteArray>("bad");
    const QByteArray zeros(4096, '\0');
    QTest::newRow("settings.txt junk") << QStringLiteral("settings.txt") << int(Kind::Settings) << QByteArray("x\x1b[31m garbage\n%%%\n~~~~\nnot a setting at all\n");
    QTest::newRow("settings.txt zeros") << QStringLiteral("settings.txt") << int(Kind::Settings) << (good_of(Kind::Settings).left(20) + zeros);
    QTest::newRow("pegasusx.ini junk") << QStringLiteral("pegasusx.ini") << int(Kind::Ini) << QByteArray("garbage garbage\nmore garbage\nstill garbage\n");
    QTest::newRow("pegasusx.ini zeros") << QStringLiteral("pegasusx.ini") << int(Kind::Ini) << (good_of(Kind::Ini).left(18) + zeros);
    QTest::newRow("favorites.txt zeros") << QStringLiteral("favorites.txt") << int(Kind::Lines) << (good_of(Kind::Lines).left(30) + zeros);
    QTest::newRow("favorites.txt binary") << QStringLiteral("favorites.txt") << int(Kind::Lines) << QByteArray("\xff\xfe\x01\x02 not text \xc3\x28");
    QTest::newRow("game_dirs.txt zeros") << QStringLiteral("game_dirs.txt") << int(Kind::Lines) << zeros;
    QTest::newRow("rom folders junk") << QStringLiteral("pegasusx-rom-folders.txt") << int(Kind::RomFolders) << QByteArray("garbage\nmore\nno tabs here\n");
    QTest::newRow("hidden half") << QStringLiteral("pegasusx-hidden.json") << int(Kind::JsonObject) << good_of(Kind::JsonObject).left(40);
    QTest::newRow("hidden a list") << QStringLiteral("pegasusx-hidden.json") << int(Kind::JsonObject) << QByteArray("[1,2,3]");
    QTest::newRow("android apps junk") << QStringLiteral("pegasusx-android-apps.json") << int(Kind::JsonObject) << QByteArray("}{ not json");
    QTest::newRow("android kinds zeros") << QStringLiteral("pegasusx-android-kinds.json") << int(Kind::JsonObject) << zeros;
    QTest::newRow("emulators half") << QStringLiteral("pegasusx-emulators.json") << int(Kind::JsonArray) << good_of(Kind::JsonArray).left(30);
    QTest::newRow("sdl junk") << QStringLiteral("sdl_controllers.txt") << int(Kind::SdlMappings) << QByteArray("garbage\nmore garbage\n");
    QTest::newRow("root not a path") << QStringLiteral("pegasusx-root.txt") << int(Kind::Root) << QByteArray("garbage\nmore\n");
    QTest::newRow("private ini junk") << QStringLiteral("pegasusx-private.ini") << int(Kind::Ini) << QByteArray("\x01\x02\x03 token?");
}

void test_Recovery::damaged_files()
{
    QFETCH(QString, name);
    QFETCH(int, kind);
    QFETCH(QByteArray, bad);
    const QByteArray good = good_of(Kind(kind));
    write(path(name), bad);
    write(recovery::good_path(path(name)), good);
    const auto before = snapshot();

    recovery::Report report;
    recovery::recover(spec(name, Kind(kind)), report, NOW);

    QCOMPARE(static_cast<int>(report.fixed.size()), 1);
    QCOMPARE(report.fixed.front().how, QStringLiteral("good copy"));
    QCOMPARE(read(path(name)), good);                                     // its last good copy, in its place
    QCOMPARE(read(recovery::good_path(path(name))), good);                // (kept)
    QCOMPARE(in_damaged(), QStringList { QStringLiteral("2026-11-24_22-30-05 ") + name });
    QCOMPARE(read(damaged() + QStringLiteral("/2026-11-24_22-30-05 ") + name), bad);  // moved as it was
    others_unchanged(before, name);
    for (const QString& line : report.log)
        QVERIFY(line.startsWith(QLatin1String("Recovery: ")));
}

// play time: a real database cut in half, then junk
void test_Recovery::damaged_files_without_good_copy_start_at_defaults()
{
    const QByteArray db = sqlite_db(path(QStringLiteral("stats.db")), 400);
    QVERIFY(db.size() > 8192);
    // its last good copy: the whole database
    write(recovery::good_path(path(QStringLiteral("stats.db"))), db);
    write(path(QStringLiteral("stats.db")), db.left(db.size() / 2) + QByteArray(db.size() / 2, '\0'));
    const auto before = snapshot();
    recovery::Report report;
    recovery::recover(spec(QStringLiteral("stats.db"), Kind::Sqlite), report, NOW);
    QCOMPARE(static_cast<int>(report.fixed.size()), 1);
    QVERIFY2(report.fixed.front().reason.contains(QLatin1String("integrity")) || report.fixed.front().reason.contains(QLatin1String("can't")),
             qPrintable(report.fixed.front().reason));
    QCOMPARE(report.fixed.front().how, QStringLiteral("good copy"));
    QCOMPARE(read(path(QStringLiteral("stats.db"))), db);
    others_unchanged(before, QStringLiteral("stats.db"));

    // junk, and no last good copy: moved, and no file (its defaults: a new, empty one)
    QFile::remove(recovery::good_path(path(QStringLiteral("stats.db"))));
    write(path(QStringLiteral("stats.db")), "this is not a database at all");
    recovery::Report report2;
    recovery::recover(spec(QStringLiteral("stats.db"), Kind::Sqlite), report2, NOW.addSecs(1));
    QCOMPARE(report2.fixed.front().how, QStringLiteral("defaults"));
    QCOMPARE(report2.fixed.front().reason, QStringLiteral("not an SQLite database"));
    QVERIFY(!QFileInfo::exists(path(QStringLiteral("stats.db"))));
    QVERIFY(in_damaged().contains(QStringLiteral("2026-11-24_22-30-06 stats.db")));
}

// the file and its last good copy both damaged: both kept in damaged/, defaults
void test_Recovery::damaged_good_copy_too()
{
    write(path(QStringLiteral("pegasusx-hidden.json")), "{ \"collections\": [");
    write(recovery::good_path(path(QStringLiteral("pegasusx-hidden.json"))), "also junk");
    const auto before = snapshot();
    recovery::Report report;
    recovery::recover(spec(QStringLiteral("pegasusx-hidden.json"), Kind::JsonObject), report, NOW);
    QCOMPARE(report.fixed.front().how, QStringLiteral("defaults"));
    QCOMPARE(static_cast<int>(report.fixed.front().moved.size()), 2);
    QVERIFY(!QFileInfo::exists(path(QStringLiteral("pegasusx-hidden.json"))));
    QVERIFY(!QFileInfo::exists(recovery::good_path(path(QStringLiteral("pegasusx-hidden.json")))));
    QCOMPARE(in_damaged(), QStringList({ QStringLiteral("2026-11-24_22-30-05 pegasusx-hidden.json"),
                                         QStringLiteral("2026-11-24_22-30-05 pegasusx-hidden.json.pxgood") }));
    others_unchanged(before, QStringLiteral("pegasusx-hidden.json"));
}

// no favorites, nothing hidden: fine, and their last good copy follows
void test_Recovery::empty_on_purpose_is_not_damage()
{
    write(path(QStringLiteral("favorites.txt")), "");
    write(recovery::good_path(path(QStringLiteral("favorites.txt"))), good_of(Kind::Lines));
    write(path(QStringLiteral("pegasusx-hidden.json")), "{\n    \"collections\": [\n    ],\n    \"games\": [\n    ]\n}\n");
    write(recovery::good_path(path(QStringLiteral("pegasusx-hidden.json"))), good_of(Kind::JsonObject));
    recovery::Report report;
    recovery::recover(spec(QStringLiteral("favorites.txt"), Kind::Lines), report, NOW);
    recovery::recover(spec(QStringLiteral("pegasusx-hidden.json"), Kind::JsonObject), report, NOW);
    QVERIFY(report.fixed.empty());
    QVERIFY(in_damaged().isEmpty());
    QCOMPARE(read(path(QStringLiteral("favorites.txt"))), QByteArray());
    QCOMPARE(read(recovery::good_path(path(QStringLiteral("favorites.txt")))), QByteArray());  // (the good copy is the empty list now)
    QVERIFY(read(recovery::good_path(path(QStringLiteral("pegasusx-hidden.json")))).contains("\"games\": ["));

    // pegasusx.ini emptied by QSettings (its last entry removed), no gamepad
    // layouts: fine too (seen in the test env: the first start removes an old
    // [genres] entry, and the ini was put back from its good copy each start)
    const QStringList names { QStringLiteral("pegasusx.ini"), QStringLiteral("pegasusx-private.ini"), QStringLiteral("sdl_controllers.txt") };
    const Kind kinds[] = { Kind::Ini, Kind::Ini, Kind::SdlMappings };
    for (int i = 0; i < names.size(); i++) {
        write(path(names[i]), "");
        write(recovery::good_path(path(names[i])), good_of(kinds[i]));
        recovery::Report r;
        recovery::recover(spec(names[i], kinds[i]), r, NOW);
        QVERIFY2(r.fixed.empty(), qPrintable(names[i]));
        QCOMPARE(read(path(names[i])), QByteArray());
        QCOMPARE(read(recovery::good_path(path(names[i]))), QByteArray());
    }
    QVERIFY(in_damaged().isEmpty());
}

// the ROM folder list: empty while its last good copy has folders is damage;
// emptied on purpose (saved_empty) it isn't
void test_Recovery::rom_folders_emptied()
{
    const QString name = QStringLiteral("pegasusx-rom-folders.txt");
    write(path(name), "");
    write(recovery::good_path(path(name)), good_of(Kind::RomFolders));
    recovery::Report report;
    recovery::recover(spec(name, Kind::RomFolders), report, NOW);
    QCOMPARE(static_cast<int>(report.fixed.size()), 1);
    QCOMPARE(read(path(name)), good_of(Kind::RomFolders));

    // the last folder taken out in Game directory: the good copy emptied with it
    write(path(name), "");
    recovery::saved_empty(path(name));
    recovery::Report report2;
    recovery::recover(spec(name, Kind::RomFolders), report2, NOW.addSecs(1));
    QVERIFY(report2.fixed.empty());
    QCOMPARE(read(path(name)), QByteArray());
}

// missing, .pxold beside it (cut after the old file was moved, with no new one written)
void test_Recovery::missing_with_pxold()
{
    write(path(QStringLiteral("favorites.txt.pxold")), good_of(Kind::Lines));
    const auto before = snapshot();
    recovery::Report report;
    recovery::recover(spec(QStringLiteral("favorites.txt"), Kind::Lines), report, NOW);
    QCOMPARE(report.fixed.front().how, QStringLiteral("put back"));
    QCOMPARE(read(path(QStringLiteral("favorites.txt"))), good_of(Kind::Lines));
    QVERIFY(!QFileInfo::exists(path(QStringLiteral("favorites.txt.pxold"))));
    QCOMPARE(read(recovery::good_path(path(QStringLiteral("favorites.txt")))), good_of(Kind::Lines));
    others_unchanged(before, QStringLiteral("favorites.txt"));
}

// missing, only .pxsave (a new file's save cut before its rename)
void test_Recovery::missing_with_pxsave()
{
    write(path(QStringLiteral("pegasusx-emulators.json.pxsave")), good_of(Kind::JsonArray));
    recovery::Report report;
    recovery::recover(spec(QStringLiteral("pegasusx-emulators.json"), Kind::JsonArray), report, NOW);
    QCOMPARE(report.fixed.front().how, QStringLiteral("put back"));
    QCOMPARE(read(path(QStringLiteral("pegasusx-emulators.json"))), good_of(Kind::JsonArray));
    QVERIFY(!QFileInfo::exists(path(QStringLiteral("pegasusx-emulators.json.pxsave"))));
}

// missing, both (cut between the two renames): the new one (.pxsave), the old one goes
void test_Recovery::missing_with_both()
{
    const QByteArray newer = good_of(Kind::Settings) + "general.mouse-support: false\n";
    write(path(QStringLiteral("settings.txt.pxold")), good_of(Kind::Settings));
    write(path(QStringLiteral("settings.txt.pxsave")), newer);
    recovery::Report report;
    recovery::recover(spec(QStringLiteral("settings.txt"), Kind::Settings), report, NOW);
    QCOMPARE(report.fixed.front().how, QStringLiteral("put back"));
    QCOMPARE(read(path(QStringLiteral("settings.txt"))), newer);
    QVERIFY(!QFileInfo::exists(path(QStringLiteral("settings.txt.pxold"))));
    QVERIFY(!QFileInfo::exists(path(QStringLiteral("settings.txt.pxsave"))));
}

// missing, both, but .pxsave doesn't pass: the old one
void test_Recovery::missing_with_both_pxsave_cut()
{
    write(path(QStringLiteral("pegasusx-hidden.json.pxold")), good_of(Kind::JsonObject));
    write(path(QStringLiteral("pegasusx-hidden.json.pxsave")), good_of(Kind::JsonObject).left(25));
    recovery::Report report;
    recovery::recover(spec(QStringLiteral("pegasusx-hidden.json"), Kind::JsonObject), report, NOW);
    QCOMPARE(report.fixed.front().how, QStringLiteral("put back"));
    QCOMPARE(read(path(QStringLiteral("pegasusx-hidden.json"))), good_of(Kind::JsonObject));
}

// missing, and nothing whole beside it: kept in damaged/, its last good copy
void test_Recovery::missing_with_nothing_whole()
{
    write(path(QStringLiteral("pegasusx-hidden.json.pxsave")), "{ \"coll");
    write(recovery::good_path(path(QStringLiteral("pegasusx-hidden.json"))), good_of(Kind::JsonObject));
    recovery::Report report;
    recovery::recover(spec(QStringLiteral("pegasusx-hidden.json"), Kind::JsonObject), report, NOW);
    QCOMPARE(report.fixed.front().how, QStringLiteral("good copy"));
    QCOMPARE(read(path(QStringLiteral("pegasusx-hidden.json"))), good_of(Kind::JsonObject));
    QCOMPARE(in_damaged(), QStringList { QStringLiteral("2026-11-24_22-30-05 pegasusx-hidden.json.pxsave") });
}

// fine, with leftovers of an interrupted save beside it: they go, nothing reported
void test_Recovery::fine_file_leftovers_removed()
{
    write(path(QStringLiteral("game_dirs.txt")), good_of(Kind::Lines));
    write(path(QStringLiteral("game_dirs.txt.pxold")), "/old\n");
    write(path(QStringLiteral("game_dirs.txt.pxsave")), "/half");
    const auto before = snapshot();
    recovery::Report report;
    recovery::recover(spec(QStringLiteral("game_dirs.txt"), Kind::Lines), report, NOW);
    QVERIFY(report.fixed.empty());
    QCOMPARE(read(path(QStringLiteral("game_dirs.txt"))), good_of(Kind::Lines));
    QVERIFY(!QFileInfo::exists(path(QStringLiteral("game_dirs.txt.pxold"))));
    QVERIFY(!QFileInfo::exists(path(QStringLiteral("game_dirs.txt.pxsave"))));
    QCOMPARE(read(recovery::good_path(path(QStringLiteral("game_dirs.txt")))), good_of(Kind::Lines));
    others_unchanged(before, QStringLiteral("game_dirs.txt"));
}

// a damaged file never becomes a last good copy
void test_Recovery::good_copy_only_from_a_file_that_passed()
{
    write(path(QStringLiteral("sdl_controllers.txt")), "junk junk\njunk\n");
    recovery::Report report;
    recovery::recover(spec(QStringLiteral("sdl_controllers.txt"), Kind::SdlMappings), report, NOW);
    QCOMPARE(report.fixed.front().how, QStringLiteral("defaults"));
    QVERIFY(!QFileInfo::exists(recovery::good_path(path(QStringLiteral("sdl_controllers.txt")))));
    QVERIFY(!QFileInfo::exists(path(QStringLiteral("sdl_controllers.txt"))));
    // the next start: nothing there, nothing to do
    recovery::Report again;
    recovery::recover(spec(QStringLiteral("sdl_controllers.txt"), Kind::SdlMappings), again, NOW.addSecs(60));
    QVERIFY(again.fixed.empty());
    QVERIFY(again.log.isEmpty());
}

// missing with no leftovers (a first start, or removed on purpose): nothing done
void test_Recovery::missing_without_leftovers_is_left_alone()
{
    write(recovery::good_path(path(QStringLiteral("favorites.txt"))), good_of(Kind::Lines));
    const auto before = snapshot();
    recovery::Report report;
    recovery::recover(spec(QStringLiteral("favorites.txt"), Kind::Lines), report, NOW);
    QVERIFY(report.fixed.empty());
    QVERIFY(!QFileInfo::exists(path(QStringLiteral("favorites.txt"))));
    QCOMPARE(snapshot(), before);
}


QTEST_MAIN(test_Recovery)
#include "test_Recovery.moc"
