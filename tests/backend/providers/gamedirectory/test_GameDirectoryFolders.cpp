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

#include "model/gaming/Collection.h"
#include "model/gaming/Game.h"
#include "model/gaming/GameFile.h"
#include "providers/GameDirectory.h"
#include "providers/LibraryRules.h"
#include "providers/SearchContext.h"
#include "providers/pegasus_metadata/PegasusProvider.h"
#include "Paths.h"


// Pegasus X: a system's ROM folders, one at a time (Settings > Game directory >
// a system > Folders): taking one out leaves the folders next to it and the
// other systems alone, with no line of it left; a listed folder isn't listed
// again, in any spelling; games are matched by path.
class test_GameDirectoryFolders : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_roms;      // the ROM folders
    QString m_snes;            // gamedirectory/snes/metadata.pegasus.txt
    QString m_gba;             // gamedirectory/gba/metadata.pegasus.txt

    QString rom(const QString& rel);
    void write(const QString& path, const QString& text);
    QString read(const QString& path);
    void write_systems();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void identity();
    void clash();
    void keep_games();
    void remove_games();
    void unlisted_folder_changes_nothing();
    void missing_folder_counts();
    void games_by_path();
};

QString test_GameDirectoryFolders::rom(const QString& rel)
{
    const QString path = m_roms.path() + QLatin1Char('/') + rel;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    return path;
}
void test_GameDirectoryFolders::write(const QString& path, const QString& text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(text.toUtf8());
}
QString test_GameDirectoryFolders::read(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(f.readAll()) : QString();
}

// SNES from two folders next to each other (ROMs/snes, ROMs/snes2, the same
// parent) and one inside the first (ROMs/snes/hacks), with your own details
// and launch command on its games; GBA from ROMs/gba
void test_GameDirectoryFolders::write_systems()
{
    const QString r = m_roms.path();
    write(m_snes, QStringLiteral(
        "# Pegasus X game directory: Nintendo SNES\n"
        "# Made by Pegasus X from the ROM folder %1/snes.\n"
        "# folder: %1/snes\n"
        "# folder: %1/snes2\n"
        "# folder: %1/snes/hacks\n"
        "# own metadata: %1/snes\n"
        "\n"
        "collection: Nintendo SNES\n"
        "shortname: snes\n"
        "launch: snes9x \"{file.path}\"\n"
        "\n"
        "game: Chrono Trigger (my own file)\n"
        "file: %1/snes/Chrono Trigger (USA).sfc\n"
        "developer: Square\n"
        "launch: my-emulator \"{file.path}\"\n"
        "\n"
        "game: Super Mario World\n"
        "file: %1/snes/Super Mario World (USA).sfc\n"
        "\n"
        "game: Super Metroid\n"
        "file: %1/snes2/Super Metroid (USA).sfc\n"
        "\n"
        "game: Super Mario World\n"
        "file: %1/snes2/Super Mario World (USA).sfc\n"
        "\n"
        "game: A Hack\n"
        "file: %1/snes/hacks/A Hack.sfc\n").arg(r));
    write(m_gba, QStringLiteral(
        "# Pegasus X game directory: Nintendo Game Boy Advance\n"
        "# folder: %1/gba\n"
        "\n"
        "collection: Nintendo Game Boy Advance\n"
        "shortname: gba\n"
        "\n"
        "game: Metroid Fusion\n"
        "file: %1/gba/Metroid Fusion (USA).gba\n").arg(r));
}

void test_GameDirectoryFolders::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_roms.isValid());
    QDir(providers::gamedir::dir()).removeRecursively();
    QDir(providers::gamedir::backup_dir()).removeRecursively();
    m_snes = providers::gamedir::system_file(QStringLiteral("snes"));
    m_gba = providers::gamedir::system_file(QStringLiteral("gba"));
    for (const char* f : { "snes/Chrono Trigger (USA).sfc", "snes/Super Mario World (USA).sfc",
                           "snes2/Super Metroid (USA).sfc", "snes2/Super Mario World (USA).sfc",
                           "snes/hacks/A Hack.sfc", "gba/Metroid Fusion (USA).gba" })
        rom(QString::fromLatin1(f));
}

void test_GameDirectoryFolders::cleanupTestCase()
{
    QDir(providers::gamedir::dir()).removeRecursively();
    QDir(providers::gamedir::backup_dir()).removeRecursively();
}

// one folder, written in different ways
void test_GameDirectoryFolders::identity()
{
    using providers::gamedir::folder_identity;
    QCOMPARE(folder_identity(QStringLiteral("/storage/emulated/0/ROMs/snes/"), false), QStringLiteral("/storage/emulated/0/ROMs/snes"));
    QCOMPARE(folder_identity(QStringLiteral("/sdcard/ROMs/snes"), false), QStringLiteral("/storage/emulated/0/ROMs/snes"));
    QCOMPARE(folder_identity(QStringLiteral("/storage/self/primary/ROMs//snes"), false), QStringLiteral("/storage/emulated/0/ROMs/snes"));
    QCOMPARE(folder_identity(QStringLiteral("/storage/ABCD-1234/ROMs/SNES"), true), QStringLiteral("/storage/abcd-1234/roms/snes"));
    QVERIFY(folder_identity(QStringLiteral("/a/SNES"), false) != folder_identity(QStringLiteral("/a/snes"), false));
}

// a folder listed already, in another spelling, inside or around one: refused;
// one next to it isn't
void test_GameDirectoryFolders::clash()
{
    using providers::gamedir::folder_clash;
    const std::vector<std::pair<QString, QString>> listed {
        { QStringLiteral("/storage/emulated/0/ROMs/snes"), QStringLiteral("Nintendo SNES") },
        { QStringLiteral("/storage/ABCD-1234/ROMs/gba"), QStringLiteral("Nintendo Game Boy Advance") },
    };
    auto c = folder_clash(QStringLiteral("/sdcard/ROMs/snes/"), listed, true);
    QCOMPARE(c.kind, QStringLiteral("same"));
    QCOMPARE(c.system, QStringLiteral("Nintendo SNES"));
    c = folder_clash(QStringLiteral("/storage/abcd-1234/roms/GBA"), listed, true);
    QCOMPARE(c.kind, QStringLiteral("same"));
    QCOMPARE(c.system, QStringLiteral("Nintendo Game Boy Advance"));
    c = folder_clash(QStringLiteral("/storage/emulated/0/ROMs/snes/hacks"), listed, true);
    QCOMPARE(c.kind, QStringLiteral("inside"));
    c = folder_clash(QStringLiteral("/storage/emulated/0/ROMs"), listed, true);
    QCOMPARE(c.kind, QStringLiteral("contains"));
    QCOMPARE(c.folder, QStringLiteral("/storage/emulated/0/ROMs/snes"));
    // next to it (the same start of the name isn't inside)
    QVERIFY(folder_clash(QStringLiteral("/storage/emulated/0/ROMs/snes2"), listed, true).kind.isEmpty());
    QVERIFY(folder_clash(QStringLiteral("/storage/ABCD-1234/ROMs/snes"), listed, true).kind.isEmpty());
}

// stop watching one folder: its line goes, every game stays; the folder next
// to it, the one inside it and the other system are as they were
void test_GameDirectoryFolders::keep_games()
{
    write_systems();
    const QString gba_before = read(m_gba);
    const QString snes2 = m_roms.path() + QStringLiteral("/snes2");
    QCOMPARE(providers::gamedir::remove_system_folder(m_snes, snes2, false), 0);
    const QString after = read(m_snes);
    QVERIFY(!after.contains(QStringLiteral("# folder: ") + snes2 + QLatin1Char('\n')));
    QVERIFY(after.contains(QStringLiteral("# folder: ") + m_roms.path() + QStringLiteral("/snes\n")));
    QVERIFY(after.contains(QStringLiteral("# folder: ") + m_roms.path() + QStringLiteral("/snes/hacks\n")));
    QCOMPARE(after.count(QStringLiteral("game: ")), 5);
    QCOMPARE(read(m_gba), gba_before);
    QCOMPARE(providers::gamedir::system_folders(m_snes).size(), 2);
}

// remove one folder and its games: only its own (not those of the folder next
// to it with the same start of the name, nor of the listed folder inside it),
// your details and launch command on the others stay, no line of it left
void test_GameDirectoryFolders::remove_games()
{
    write_systems();
    const QString gba_before = read(m_gba);
    const QString snes = m_roms.path() + QStringLiteral("/snes");
    QCOMPARE(providers::gamedir::folder_game_count(m_snes, snes), 2);
    QCOMPARE(providers::gamedir::remove_system_folder(m_snes, snes, true), 2);
    const QString after = read(m_snes);
    QVERIFY(!after.contains(QStringLiteral("# folder: ") + snes + QLatin1Char('\n')));
    QVERIFY(!after.contains(QStringLiteral("# own metadata: ") + snes + QLatin1Char('\n')));
    QVERIFY(!after.contains(QStringLiteral("Chrono Trigger")));
    QVERIFY(!after.contains(snes + QStringLiteral("/Super Mario World")));
    QVERIFY(after.contains(QStringLiteral("snes2/Super Mario World (USA).sfc")));
    QVERIFY(after.contains(QStringLiteral("snes2/Super Metroid (USA).sfc")));
    QVERIFY(after.contains(QStringLiteral("snes/hacks/A Hack.sfc")));
    QVERIFY(after.contains(QStringLiteral("launch: snes9x \"{file.path}\"")));
    QCOMPARE(read(m_gba), gba_before);
    // a copy first
    QVERIFY(!QDir(providers::gamedir::backup_dir(QStringLiteral("snes"))).entryList({ QStringLiteral("*.remove.*") }, QDir::Files).isEmpty());
}

// a folder no system lists: no file is touched, byte for byte
void test_GameDirectoryFolders::unlisted_folder_changes_nothing()
{
    write_systems();
    const QString snes_before = read(m_snes);
    const QString gba_before = read(m_gba);
    QCOMPARE(providers::gamedir::remove_system_folder(m_snes, m_roms.path() + QStringLiteral("/psx"), true), 0);
    QCOMPARE(providers::gamedir::remove_system_folder(m_snes, m_roms.path(), false), 0);
    QCOMPARE(read(m_snes), snes_before);
    QCOMPARE(read(m_gba), gba_before);
}

// a folder that's gone (an SD card out): still listed, its games still counted
void test_GameDirectoryFolders::missing_folder_counts()
{
    write_systems();
    const QString gone = m_roms.path() + QStringLiteral("/snes2");
    QVERIFY(QDir().rename(gone, gone + QStringLiteral(".out")));
    QVERIFY(providers::gamedir::system_folders(m_snes).contains(gone));
    QCOMPARE(providers::gamedir::folder_game_count(m_snes, gone), 2);
    QVERIFY(QDir().rename(gone + QStringLiteral(".out"), gone));
}

// the same file name in two folders: two games (and noted as a duplicate);
// the same path from two systems' files: one game
void test_GameDirectoryFolders::games_by_path()
{
    write_systems();
    // GBA lists the SNES folder's Super Mario World too (an overlapping folder)
    write(m_gba, read(m_gba) + QStringLiteral("\ngame: Super Mario World\nfile: %1/snes/Super Mario World (USA).sfc\n").arg(m_roms.path()));
    providers::SearchContext sctx({
        QFileInfo(m_snes).absolutePath(),
        QFileInfo(m_gba).absolutePath(),
    });
    providers::pegasus::PegasusProvider().run(sctx);
    const auto [collections, games] = sctx.finalize(this);
    int smw = 0;
    const model::Game* shared = nullptr;
    for (const model::Game* g : games) {
        for (const model::GameFile* f : g->filesModel()->entries()) {
            if (f->fileinfo().fileName() == QLatin1String("Super Mario World (USA).sfc")) {
                smw++;
                if (f->path().contains(QLatin1String("/snes/")))
                    shared = g;
            }
        }
    }
    QCOMPARE(smw, 2);
    QVERIFY(shared);
    QCOMPARE(shared->collectionsModel()->count(), 2);
    const auto dups = providers::library::find_duplicate_files(games);
    QCOMPARE(static_cast<int>(dups.size()), 1);
    QCOMPARE(dups.front().name, QStringLiteral("Super Mario World (USA).sfc"));
}


QTEST_MAIN(test_GameDirectoryFolders)
#include "test_GameDirectoryFolders.moc"
