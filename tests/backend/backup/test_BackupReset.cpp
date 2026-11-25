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

#include "BackupReset.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QSettings>
#include <QTemporaryDir>
#include <atomic>


// Pegasus X: Backup & reset, on throwaway folders only: a Pegasus X folder
// with a theme, a ROM folder inside it, links that point out of it; a ROM
// folder and a file outside it; app storage and a cache. A backup holds no
// password, key or token; backup -> restore gives the data back and keeps the
// device's logins; a bad file changes nothing; Start over deletes only its
// list and never a theme, a ROM, a file outside or what a link points to.
class test_BackupReset : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmp;
    backup::Places m_places;
    QString m_outside;
    QString m_roms;

    QString data(const QString& rel) const { return m_places.data + QLatin1Char('/') + rel; }
    static void write(const QString& path, const QByteArray& text);
    static QByteArray read(const QString& path);
    // every file under `dir` -> its md5 (links: where they point)
    static QMap<QString, QByteArray> snapshot(const QString& dir);
    void make_fixture();

private slots:
    void init();

    void backup_has_no_secrets();
    void backup_leaves_out_caches_themes_logs();
    void backup_with_art();
    void stopped_backup_leaves_no_file();
    void restore_round_trip_keeps_device_logins();
    void restore_without_device_logins_asks_to_sign_in();
    void damaged_backup_changes_nothing();
    void not_a_backup_changes_nothing();
    void foreign_path_in_backup_changes_nothing();
    void start_over_deletes_only_its_list();
    void start_over_refuses_a_storage_root();
    void safe_target_checks();
};

void test_BackupReset::write(const QString& path, const QByteArray& text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(text);
}

QByteArray test_BackupReset::read(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QMap<QString, QByteArray> test_BackupReset::snapshot(const QString& dir)
{
    QMap<QString, QByteArray> out;
    QDirIterator it(dir, QDir::Files | QDir::Hidden | QDir::System | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString rel = QDir(dir).relativeFilePath(it.filePath());
        out.insert(rel, it.fileInfo().isDir() ? QByteArray("<dir>")
                        : QCryptographicHash::hash(read(it.filePath()), QCryptographicHash::Md5).toHex());
    }
    QDirIterator links(dir, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (links.hasNext()) {
        links.next();
        if (links.fileInfo().isSymLink())
            out.insert(QDir(dir).relativeFilePath(links.filePath()), "-> " + links.fileInfo().symLinkTarget().toUtf8());
    }
    return out;
}

void test_BackupReset::make_fixture()
{
    const QString root = m_tmp.path();
    m_places.data = root + QStringLiteral("/PegasusX");
    m_places.priv = root + QStringLiteral("/appstorage/config");
    m_places.app_data = root + QStringLiteral("/appstorage/files");
    m_places.cache = root + QStringLiteral("/appstorage/cache");
    m_places.never = QStringList { root + QStringLiteral("/storage") };
    m_outside = root + QStringLiteral("/outside");
    m_roms = root + QStringLiteral("/storage/ROMs");

    // Pegasus X's own data
    write(data("pegasusx.ini"),
          "[library]\nmergeAndroid=true\n\n"
          "[scraper]\nssUser=me\nssPassword=SECRETPASS\ngetCovers=true\n\n"
          "[retroachievements]\nuser=raMe\nkey=SECRETKEY\n\n"
          "[github]\ntoken=SECRETTOKEN\n");
    write(data("settings.txt"), "general.theme: themes/XBSX\n");
    write(data("game_dirs.txt"), m_roms.toUtf8() + "/snes\n");
    write(data("favorites.txt"), "/roms/snes/Super Mario World.sfc\n");
    write(data("stats.db"), QByteArray(4096, 'S'));
    write(data("pegasusx-hidden.json"), "{\"games\":[]}");
    write(data("pegasusx-android-apps.json"), "{\"apps\":[\"com.test.notes\"]}");
    write(data("pegasusx-themes/links.json"), "[\"https://github.com/x/xbsx\"]");
    write(data("pegasusx-themes/cache.json"), "{}");
    write(data("gamedirectory/snes/metadata.pegasus.txt"), "collection: SNES\n");
    write(data("metadata/review.json"), "{}");
    // Library > Metadata's merged names
    write(data("metadata/aliases.json"), "{\"version\":1,\"publisher\":{\"Nintendo of America\":\"Nintendo\"}}");
    write(data("metadata/library-index.json"), "{}");
    write(data("theme_settings/XBSX.json"), "{\"accent\":\"green\"}");
    write(data("downloaded_media/snes/covers/Super Mario World.png"), QByteArray(2048, 'P'));
    write(data("backup/old.tar"), "old");
    write(data("launchbox/status.json"), "{}");
    write(data("lastrun.log"), "log");
    // not Pegasus X's data: a theme, a ROM folder inside the Pegasus X folder, a note
    write(data("themes/XBSX/theme.qml"), "Item {}");
    write(data("MyROMs/snes/Zelda.sfc"), "rom");
    write(data("notes.txt"), "mine");
    // app storage and caches
    write(m_places.priv + QStringLiteral("/pegasusx-root.txt"), m_places.data.toUtf8());
    write(m_places.priv + QStringLiteral("/pegasusx-wizard"), "2\n");
    // the copy left in the app's own folder when the App Data Folder was picked
    write(m_places.priv + QStringLiteral("/settings.txt"), "general.theme: themes/OLD\n");
    write(m_places.priv + QStringLiteral("/theme_settings/XBSX.json"), "{\"accent\":\"red\"}");
    write(m_places.priv + QStringLiteral("/gamedirectory/snes/metadata.pegasus.txt"), "collection: SNES\n");
    write(m_places.priv + QStringLiteral("/themes/Old/theme.qml"), "Item {}");
    write(m_places.app_data + QStringLiteral("/pegasusx-setup-done"), "done\n");
    write(m_places.priv + QStringLiteral("/pegasusx-root-asked"), "asked\n");
    write(m_places.app_data + QStringLiteral("/pegasusx-private.ini"), "[github]\ntoken=SECRETTOKEN\n");
    write(m_places.cache + QStringLiteral("/gameindex-v1.json"), "{}");
    write(m_places.cache + QStringLiteral("/netcache/a"), "x");
    write(m_places.cache + QStringLiteral("/someone-else.txt"), "x");
    // outside: ROMs with ES-DE's files, a file, and folders links point to
    write(m_roms + QStringLiteral("/snes/Super Mario World.sfc"), "rom");
    write(m_roms + QStringLiteral("/snes/gamelist.xml"), "<gameList/>");
    write(m_roms + QStringLiteral("/snes/media/covers/Super Mario World.png"), "art");
    write(m_outside + QStringLiteral("/important.txt"), "keep me");
    write(m_outside + QStringLiteral("/launchbox-real/status.json"), "{}");
    write(m_outside + QStringLiteral("/linked/keep.txt"), "keep me too");
    // links out of the Pegasus X folder: a deleted name that's a link, a
    // link inside a deleted folder, a link inside a backed-up folder
    QVERIFY(QFile::link(m_outside + QStringLiteral("/linked"), data("metadata/outlink")));
    QDir(data("launchbox")).removeRecursively();
    QVERIFY(QFile::link(m_outside + QStringLiteral("/launchbox-real"), data("launchbox")));
    QVERIFY(QFile::link(m_outside + QStringLiteral("/important.txt"), data("gamedirectory/snes/linked.txt")));
}

void test_BackupReset::init()
{
    QVERIFY(m_tmp.isValid());
    QDir(m_tmp.path()).removeRecursively();
    QDir().mkpath(m_tmp.path());
    make_fixture();
}

void test_BackupReset::backup_has_no_secrets()
{
    const QString file = m_tmp.path() + QStringLiteral("/Download/b.tar");
    QCOMPARE(backup::make(m_places, file, true, 2253), QString());
    const QByteArray all = read(file);
    QVERIFY(!all.isEmpty());
    QVERIFY(!all.contains("SECRETPASS"));
    QVERIFY(!all.contains("SECRETKEY"));
    QVERIFY(!all.contains("SECRETTOKEN"));
    QVERIFY(!all.contains("ssPassword"));
    // the names stay
    QVERIFY(all.contains("ssUser=me"));
    QVERIFY(all.contains("user=raMe"));
    QVERIFY(all.contains("getCovers=true"));
    QVERIFY(backup::is_secret_key(QStringLiteral("scraper/ssPassword")));
    QVERIFY(backup::is_secret_key(QStringLiteral("retroachievements/key")));
    QVERIFY(backup::is_secret_key(QStringLiteral("github/token")));
    QVERIFY(!backup::is_secret_key(QStringLiteral("scraper/ssUser")));
}

void test_BackupReset::backup_leaves_out_caches_themes_logs()
{
    const QString file = m_tmp.path() + QStringLiteral("/Download/b.tar");
    QCOMPARE(backup::make(m_places, file, false, 2253), QString());
    const QByteArray all = read(file);
    for (const char* in : { "data/pegasusx.ini", "data/settings.txt", "data/game_dirs.txt", "data/favorites.txt",
                            "data/stats.db", "data/pegasusx-hidden.json", "data/pegasusx-android-apps.json",
                            "data/pegasusx-themes/links.json", "data/gamedirectory/snes/metadata.pegasus.txt",
                            "data/metadata/review.json", "data/theme_settings/XBSX.json", "data/metadata/aliases.json" })
        QVERIFY2(all.contains(in), in);
    for (const char* out : { "downloaded_media", "theme.qml", "lastrun.log", "library-index.json", "backup/old.tar",
                             "launchbox", "cache.json", "MyROMs", "notes.txt", "keep me", "linked.txt", "outlink" })
        QVERIFY2(!all.contains(out), out);
    const backup::Info info = backup::read_info(file);
    QCOMPARE(info.error, QString());
    QCOMPARE(info.build, 2253);
    QCOMPARE(info.art, false);
    QCOMPARE(info.files, 12);
}

void test_BackupReset::backup_with_art()
{
    const QString file = m_tmp.path() + QStringLiteral("/Download/b.tar");
    QCOMPARE(backup::make(m_places, file, true, 2253), QString());
    QVERIFY(read(file).contains("data/downloaded_media/snes/covers/Super Mario World.png"));
    QCOMPARE(backup::read_info(file).art, true);
    QCOMPARE(backup::art_bytes(m_places), qint64(2048));
}

void test_BackupReset::stopped_backup_leaves_no_file()
{
    const QString file = m_tmp.path() + QStringLiteral("/Download/b.tar");
    std::atomic<bool> stop { true };
    backup::Progress progress;
    progress.cancel = &stop;
    QCOMPARE(backup::make(m_places, file, true, 2253, progress), QStringLiteral("Stopped"));
    QVERIFY(!QFileInfo::exists(file));
    QVERIFY(!QFileInfo::exists(file + QStringLiteral(".part")));
}

void test_BackupReset::restore_round_trip_keeps_device_logins()
{
    const QString file = m_tmp.path() + QStringLiteral("/Download/b.tar");
    QCOMPARE(backup::make(m_places, file, false, 2253), QString());
    // changes after the backup; the device's logins are new
    write(data("game_dirs.txt"), "/elsewhere\n");
    QFile::remove(data("favorites.txt"));
    write(data("gamedirectory/n64/metadata.pegasus.txt"), "collection: N64\n");
    write(data("metadata/aliases.json"), "{\"version\":1}");
    {
        QSettings ini(data("pegasusx.ini"), QSettings::IniFormat);
        ini.setValue(QStringLiteral("scraper/ssUser"), QStringLiteral("device"));
        ini.setValue(QStringLiteral("scraper/ssPassword"), QStringLiteral("DEVICEPASS"));
        ini.setValue(QStringLiteral("retroachievements/key"), QStringLiteral("DEVICEKEY"));
    }
    const backup::RestoreResult result = backup::restore(m_places, file, 2253);
    QCOMPARE(result.error, QString());
    QVERIFY(QFileInfo::exists(result.saved_first));
    QVERIFY(result.saved_first.startsWith(data("backup/before-restore-")));
    QCOMPARE(backup::read_info(result.saved_first).error, QString());
    QVERIFY(result.sign_in.isEmpty());
    QCOMPARE(read(data("game_dirs.txt")), m_roms.toUtf8() + "/snes\n");
    QCOMPARE(read(data("favorites.txt")), QByteArray("/roms/snes/Super Mario World.sfc\n"));
    QVERIFY(!QFileInfo::exists(data("gamedirectory/n64/metadata.pegasus.txt")));
    QCOMPARE(read(data("theme_settings/XBSX.json")), QByteArray("{\"accent\":\"green\"}"));
    // the merged names as the backup has them
    QCOMPARE(read(data("metadata/aliases.json")), QByteArray("{\"version\":1,\"publisher\":{\"Nintendo of America\":\"Nintendo\"}}"));
    const QSettings ini(data("pegasusx.ini"), QSettings::IniFormat);
    QCOMPARE(ini.value(QStringLiteral("scraper/ssUser")).toString(), QStringLiteral("device"));
    QCOMPARE(ini.value(QStringLiteral("scraper/ssPassword")).toString(), QStringLiteral("DEVICEPASS"));
    QCOMPARE(ini.value(QStringLiteral("retroachievements/key")).toString(), QStringLiteral("DEVICEKEY"));
    QCOMPARE(ini.value(QStringLiteral("library/mergeAndroid")).toString(), QStringLiteral("true"));
    // never touched: the theme, the ROMs, the art, the file outside, what links point to
    QCOMPARE(read(data("themes/XBSX/theme.qml")), QByteArray("Item {}"));
    QCOMPARE(read(data("MyROMs/snes/Zelda.sfc")), QByteArray("rom"));
    QCOMPARE(read(data("downloaded_media/snes/covers/Super Mario World.png")), QByteArray(2048, 'P'));
    QCOMPARE(read(m_outside + QStringLiteral("/important.txt")), QByteArray("keep me"));
    QCOMPARE(read(m_outside + QStringLiteral("/linked/keep.txt")), QByteArray("keep me too"));
}

void test_BackupReset::restore_without_device_logins_asks_to_sign_in()
{
    const QString file = m_tmp.path() + QStringLiteral("/Download/b.tar");
    QCOMPARE(backup::make(m_places, file, false, 2253), QString());
    {
        QSettings ini(data("pegasusx.ini"), QSettings::IniFormat);
        ini.remove(QStringLiteral("scraper/ssPassword"));
        ini.remove(QStringLiteral("retroachievements/key"));
    }
    const backup::RestoreResult result = backup::restore(m_places, file, 2253);
    QCOMPARE(result.error, QString());
    QCOMPARE(result.sign_in, QStringList({ QStringLiteral("ScreenScraper"), QStringLiteral("RetroAchievements") }));
    const QSettings ini(data("pegasusx.ini"), QSettings::IniFormat);
    QCOMPARE(ini.value(QStringLiteral("scraper/ssUser")).toString(), QStringLiteral("me"));
    QVERIFY(ini.value(QStringLiteral("scraper/ssPassword")).toString().isEmpty());
}

void test_BackupReset::damaged_backup_changes_nothing()
{
    const QString file = m_tmp.path() + QStringLiteral("/Download/b.tar");
    QCOMPARE(backup::make(m_places, file, false, 2253), QString());
    // one byte of a file changed (its checksum no longer matches)
    QByteArray bytes = read(file);
    const int at = bytes.indexOf("collection: SNES");
    QVERIFY(at > 0);
    bytes[at] = 'K';
    write(file, bytes);
    const QMap<QString, QByteArray> before = snapshot(m_tmp.path());
    const backup::RestoreResult result = backup::restore(m_places, file, 2253);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(result.saved_first.isEmpty());
    QCOMPARE(snapshot(m_tmp.path()), before);
    // cut short (the file itself changes here: compared from after it's cut)
    write(file, bytes.left(bytes.size() / 2));
    const QMap<QString, QByteArray> before_cut = snapshot(m_tmp.path());
    QVERIFY(!backup::read_info(file).error.isEmpty());
    QVERIFY(!backup::restore(m_places, file, 2253).error.isEmpty());
    QCOMPARE(snapshot(m_tmp.path()), before_cut);
}

void test_BackupReset::not_a_backup_changes_nothing()
{
    const QString file = m_tmp.path() + QStringLiteral("/Download/photo.tar");
    write(file, QByteArray(3000, 'z'));
    const QMap<QString, QByteArray> before = snapshot(m_tmp.path());
    QVERIFY(!backup::read_info(file).error.isEmpty());
    QVERIFY(!backup::restore(m_places, file, 2253).error.isEmpty());
    QVERIFY(!backup::restore(m_places, m_tmp.path() + QStringLiteral("/nothing.tar"), 2253).error.isEmpty());
    QCOMPARE(snapshot(m_tmp.path()), before);
}

// a tar made by hand: a good manifest, then a file outside what a backup may hold
void test_BackupReset::foreign_path_in_backup_changes_nothing()
{
    const auto header = [](const QByteArray& name, qint64 size) {
        QByteArray h(512, '\0');
        const auto put = [&h](int at, const QByteArray& v) { for (int i = 0; i < v.size(); i++) h[at + i] = v[i]; };
        put(0, name);
        put(100, "0000644");
        put(108, "0000000");
        put(116, "0000000");
        put(124, QByteArray::number(size, 8).rightJustified(11, '0'));
        put(136, "00000000000");
        put(148, QByteArray(8, ' '));
        h[156] = '0';
        put(257, QByteArray("ustar\0" "00", 8));
        quint32 sum = 0;
        for (const char c : h) sum += static_cast<uchar>(c);
        put(148, QByteArray::number(sum, 8).rightJustified(6, '0') + QByteArray("\0 ", 2));
        return h;
    };
    const auto entry = [&header](const QByteArray& name, const QByteArray& body) {
        QByteArray out = header(name, body.size()) + body;
        if (body.size() % 512)
            out += QByteArray(512 - body.size() % 512, '\0');
        return out;
    };
    for (const QByteArray& evil : { QByteArray("data/../../outside/important.txt"), QByteArray("data/themes/XBSX/theme.qml"),
                                    QByteArray("/etc/passwd"), QByteArray("data/MyROMs/snes/Zelda.sfc") }) {
        const QByteArray manifest = "{\"format\":1,\"app\":\"Pegasus X\",\"build\":2253,\"art\":false}";
        const QByteArray list = "{\"files\":{}}";
        const QString file = m_tmp.path() + QStringLiteral("/Download/evil.tar");
        write(file, entry("pegasusx-backup.json", manifest) + entry(evil, "pwned")
                    + entry("pegasusx-backup-files.json", list) + QByteArray(1024, '\0'));
        const QMap<QString, QByteArray> before = snapshot(m_tmp.path());
        QVERIFY2(!backup::read_info(file).error.isEmpty(), evil.constData());
        QVERIFY2(!backup::restore(m_places, file, 2253).error.isEmpty(), evil.constData());
        QCOMPARE(snapshot(m_tmp.path()), before);
    }
}

void test_BackupReset::start_over_deletes_only_its_list()
{
    // damaged-file recovery's last good copies and damaged folders go too;
    // pegasusx-root.txt's good copy stays with the folder setting
    write(data("favorites.txt.pxgood"), "/roms/a.sfc\n");
    write(data("settings.txt.pxgood"), "general.theme: x\n");
    write(data("damaged/2026-11-24_22-30-05 favorites.txt"), "junk");
    write(m_places.app_data + QStringLiteral("/pegasusx-private.ini.pxgood"), "[github]\ntoken=SECRETTOKEN\n");
    write(m_places.app_data + QStringLiteral("/damaged/2026-11-24_22-30-05 pegasusx-private.ini"), "junk");
    write(m_places.priv + QStringLiteral("/damaged/2026-11-24_22-30-05 pegasusx-root.txt"), "junk");
    write(m_places.priv + QStringLiteral("/pegasusx-root.txt.pxgood"), m_places.data.toUtf8());

    const backup::WipePlan plan = backup::wipe_plan(m_places);
    QStringList planned;
    for (const backup::WipeItem& item : plan.remove)
        planned.append(item.path);
    QVERIFY(planned.contains(data("downloaded_media")));
    QVERIFY(planned.contains(m_places.priv + QStringLiteral("/pegasusx-wizard")));
    QVERIFY(!planned.contains(data("themes")));
    QVERIFY(!planned.contains(m_places.priv + QStringLiteral("/pegasusx-root.txt")));

    QStringList removed;
    QCOMPARE(backup::wipe(m_places, &removed), QString());
    // gone
    for (const QString rel : { "pegasusx.ini", "settings.txt", "game_dirs.txt", "favorites.txt", "stats.db",
                                "pegasusx-hidden.json", "pegasusx-android-apps.json", "pegasusx-themes", "gamedirectory",
                                "metadata", "theme_settings", "downloaded_media", "backup", "launchbox" })
        QVERIFY2(!QFileInfo(data(rel)).exists() && !QFileInfo(data(rel)).isSymLink(), qPrintable(rel));
    for (const QString& path : { m_places.priv + QStringLiteral("/pegasusx-wizard"),
                                 m_places.priv + QStringLiteral("/pegasusx-root-asked"),
                                 m_places.app_data + QStringLiteral("/pegasusx-private.ini"),
                                 m_places.cache + QStringLiteral("/gameindex-v1.json"),
                                 m_places.cache + QStringLiteral("/netcache"),
                                 data("favorites.txt.pxgood"), data("settings.txt.pxgood"), data("damaged"),
                                 data("metadata/aliases.json"),
                                 m_places.app_data + QStringLiteral("/pegasusx-private.ini.pxgood"),
                                 m_places.app_data + QStringLiteral("/damaged"),
                                 m_places.priv + QStringLiteral("/damaged") })
        QVERIFY2(!QFileInfo::exists(path), qPrintable(path));
    QCOMPARE(read(m_places.priv + QStringLiteral("/pegasusx-root.txt.pxgood")), m_places.data.toUtf8());
    // the older copy in the app's own folder went too (Pegasus X's data, the
    // theme settings, the setup marker); its themes stay
    for (const QString& path : { m_places.priv + QStringLiteral("/settings.txt"),
                                 m_places.priv + QStringLiteral("/theme_settings"),
                                 m_places.priv + QStringLiteral("/gamedirectory"),
                                 m_places.app_data + QStringLiteral("/pegasusx-setup-done") })
        QVERIFY2(!QFileInfo::exists(path), qPrintable(path));
    QCOMPARE(read(m_places.priv + QStringLiteral("/themes/Old/theme.qml")), QByteArray("Item {}"));
    // kept: the folder itself, the theme, the ROMs inside and outside, other files,
    // the folder setting, the log, what the links pointed to
    QVERIFY(QFileInfo(m_places.data).isDir());
    QCOMPARE(read(data("themes/XBSX/theme.qml")), QByteArray("Item {}"));
    QCOMPARE(read(data("MyROMs/snes/Zelda.sfc")), QByteArray("rom"));
    QCOMPARE(read(data("notes.txt")), QByteArray("mine"));
    QCOMPARE(read(data("lastrun.log")), QByteArray("log"));
    QCOMPARE(read(m_places.priv + QStringLiteral("/pegasusx-root.txt")), m_places.data.toUtf8());
    QCOMPARE(read(m_places.cache + QStringLiteral("/someone-else.txt")), QByteArray("x"));
    QCOMPARE(read(m_roms + QStringLiteral("/snes/Super Mario World.sfc")), QByteArray("rom"));
    QCOMPARE(read(m_roms + QStringLiteral("/snes/gamelist.xml")), QByteArray("<gameList/>"));
    QCOMPARE(read(m_roms + QStringLiteral("/snes/media/covers/Super Mario World.png")), QByteArray("art"));
    QCOMPARE(read(m_outside + QStringLiteral("/important.txt")), QByteArray("keep me"));
    QCOMPARE(read(m_outside + QStringLiteral("/launchbox-real/status.json")), QByteArray("{}"));
    QCOMPARE(read(m_outside + QStringLiteral("/linked/keep.txt")), QByteArray("keep me too"));
}

void test_BackupReset::start_over_refuses_a_storage_root()
{
    backup::Places places = m_places;
    places.never.append(m_places.data);  // as if the Pegasus X folder were a storage root
    const QMap<QString, QByteArray> before = snapshot(m_tmp.path());
    QVERIFY(!backup::wipe(places).isEmpty());
    QCOMPARE(snapshot(m_tmp.path()), before);
    places = m_places;
    places.data.clear();
    places.priv.clear();
    places.app_data.clear();
    places.cache.clear();
    QVERIFY(!backup::wipe(places).isEmpty());
    QCOMPARE(snapshot(m_tmp.path()), before);
}

void test_BackupReset::safe_target_checks()
{
    QVERIFY(backup::safe_target(m_places, m_places.data, data("metadata")));
    QVERIFY(!backup::safe_target(m_places, m_places.data, QString()));
    QVERIFY(!backup::safe_target(m_places, QString(), data("metadata")));
    QVERIFY(!backup::safe_target(m_places, m_places.data, m_places.data));
    QVERIFY(!backup::safe_target(m_places, m_places.data, data("../outside")));
    QVERIFY(!backup::safe_target(m_places, m_places.data, data("metadata/review.json")));  // not directly in it
    QVERIFY(!backup::safe_target(m_places, m_tmp.path() + QStringLiteral("/storage"), m_roms));
    // a base that is a link to somewhere else: its real folder isn't the base's
    QVERIFY(QFile::link(m_outside, m_tmp.path() + QStringLiteral("/linkbase")));
    QVERIFY(backup::safe_target(m_places, m_tmp.path() + QStringLiteral("/linkbase"),
                                m_tmp.path() + QStringLiteral("/linkbase/important.txt")));  // (resolved the same both sides)
    QVERIFY(!backup::safe_target(m_places, m_places.data, data("metadata/outlink/keep.txt")));
}


QTEST_MAIN(test_BackupReset)
#include "test_BackupReset.moc"
