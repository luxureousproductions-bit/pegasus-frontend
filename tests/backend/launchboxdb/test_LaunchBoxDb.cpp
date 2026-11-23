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

#include "LaunchBoxDb.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>


// Pegasus X: LaunchBox matching with fallback platforms. A Nintendo Switch 2
// game: the "Nintendo Switch 2" platform first, then "Nintendo Switch"; its
// "Nintendo Switch 2 Edition" counts as nothing. An Android game: the
// "Android" platform first, then every other saved platform; two platforms
// about as sure: a tie, nothing used (Needs your check). Other systems: only
// their own platform, as before.
class test_LaunchBoxDb : public QObject {
    Q_OBJECT

private:
    using Db = scraper::LaunchBoxDb;
    static QJsonObject game(int id, const QString& name, const QStringList& alt = {});
    static void write_platform(const QString& platform, const QJsonArray& games);

private slots:
    void initTestCase();

    void saved_platforms();
    void searched_platforms();
    void switch2_own_platform_first();
    void switch2_falls_back_to_switch();
    void switch2_edition_names_data();
    void switch2_edition_names();
    void edition_only_for_switch2();
    void switch_has_no_fallback();
    void android_own_platform_first();
    void android_falls_back();
    void android_tie_is_not_guessed();
    void picked_game_found_on_fallback();
    void candidates_show_their_platform();
};

QJsonObject test_LaunchBoxDb::game(int id, const QString& name, const QStringList& alt)
{
    return QJsonObject {
        { QStringLiteral("id"), id }, { QStringLiteral("n"), name },
        { QStringLiteral("alt"), QJsonArray::fromStringList(alt) },
        { QStringLiteral("rel"), QString() }, { QStringLiteral("ov"), QString() },
        { QStringLiteral("dev"), QJsonArray() }, { QStringLiteral("pub"), QJsonArray() },
        { QStringLiteral("gen"), QJsonArray() }, { QStringLiteral("pl"), 0 }, { QStringLiteral("rt"), 0.0 },
        { QStringLiteral("img"), QJsonArray() },
    };
}

// as LaunchBoxDb names its index files (launchbox/index/<platform>.json)
void test_LaunchBoxDb::write_platform(const QString& platform, const QJsonArray& games)
{
    QString safe;
    for (const QChar c : platform) {
        if (c.isLetterOrNumber())
            safe += c.toLower();
        else if (c == QLatin1Char(' ') || c == QLatin1Char('-') || c == QLatin1Char('_'))
            safe += QLatin1Char('_');
    }
    QDir().mkpath(Db::index_dir());
    QFile file(Db::index_dir() + QLatin1Char('/') + safe + QStringLiteral(".json"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(games).toJson(QJsonDocument::Compact));
}

void test_LaunchBoxDb::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QDir(Db::dir()).removeRecursively();
    write_platform(QStringLiteral("Nintendo Switch 2"), {
        game(1, QStringLiteral("The Legend of Zelda: Breath of the Wild – Nintendo Switch 2 Edition")),
        game(2, QStringLiteral("Donkey Kong Bananza")),
        game(3, QStringLiteral("Super Mario Party Jamboree - Nintendo Switch 2 Edition")),
    });
    write_platform(QStringLiteral("Nintendo Switch"), {
        game(10, QStringLiteral("Super Mario Odyssey")),
        game(11, QStringLiteral("Donkey Kong Bananza")),
        game(12, QStringLiteral("The Legend of Zelda: Breath of the Wild")),
    });
    write_platform(QStringLiteral("Android"), { game(20, QStringLiteral("Minecraft")) });
    write_platform(QStringLiteral("Nintendo 64"), {
        game(30, QStringLiteral("The Legend of Zelda: Ocarina of Time"), { QStringLiteral("Ship of Harkinian") }),
        game(31, QStringLiteral("Doom")),
        game(32, QStringLiteral("Doom 64")),
    });
    write_platform(QStringLiteral("Sega 32X"), { game(40, QStringLiteral("Doom")) });
    // the platforms saved (what Update LaunchboxDB writes into status.json)
    QJsonObject counts;
    for (const char* p : { "Nintendo Switch 2", "Nintendo Switch", "Android", "Nintendo 64", "Sega 32X" })
        counts.insert(QString::fromLatin1(p), 1);
    QFile status(Db::dir() + QStringLiteral("/status.json"));
    QVERIFY(status.open(QIODevice::WriteOnly | QIODevice::Truncate));
    status.write(QJsonDocument(QJsonObject { { QStringLiteral("platforms"), counts } }).toJson());
}

// Update LaunchboxDB: Switch 2 brings Nintendo Switch along; nothing else changes
void test_LaunchBoxDb::saved_platforms()
{
    QCOMPARE(Db::saved_platforms_for(QStringLiteral("switch2")),
             QStringList({ QStringLiteral("Nintendo Switch 2"), QStringLiteral("Nintendo Switch") }));
    QCOMPARE(Db::saved_platforms_for(QStringLiteral("switch")), QStringList { QStringLiteral("Nintendo Switch") });
    QCOMPARE(Db::saved_platforms_for(QStringLiteral("n64")), QStringList { QStringLiteral("Nintendo 64") });
    QCOMPARE(Db::saved_platforms_for(QStringLiteral("androidgames")), QStringList { QStringLiteral("Android") });
    QVERIFY(Db::saved_platforms_for(QStringLiteral("nosuchsystem")).isEmpty());
}

void test_LaunchBoxDb::searched_platforms()
{
    Db db;
    QCOMPARE(db.search_platforms(QStringLiteral("switch2")),
             QStringList({ QStringLiteral("Nintendo Switch 2"), QStringLiteral("Nintendo Switch") }));
    QCOMPARE(db.search_platforms(QStringLiteral("n64")), QStringList { QStringLiteral("Nintendo 64") });
    // Android: its own, then every other saved platform
    const QStringList android = db.search_platforms(QStringLiteral("android"));
    QCOMPARE(android.first(), QStringLiteral("Android"));
    QCOMPARE(android.size(), 5);
    QCOMPARE(db.search_platforms(QStringLiteral("androidgames")), android);
}

// on both platforms: the Switch 2 one
void test_LaunchBoxDb::switch2_own_platform_first()
{
    Db db;
    const Db::Match m = db.best(QStringLiteral("switch2"), QStringLiteral("Donkey Kong Bananza"), 60);
    QCOMPARE(m.game.id, 2);
    QCOMPARE(m.game.platform, QStringLiteral("Nintendo Switch 2"));
    QVERIFY(!m.tie);
}

// only on Nintendo Switch (a Switch game with a Switch 2 upgrade)
void test_LaunchBoxDb::switch2_falls_back_to_switch()
{
    Db db;
    const Db::Match m = db.best(QStringLiteral("switch2"), QStringLiteral("Super Mario Odyssey"), 60);
    QCOMPARE(m.game.id, 10);
    QCOMPARE(m.game.platform, QStringLiteral("Nintendo Switch"));
    // nowhere: no match, the closest kept for Needs your check
    const Db::Match none = db.best(QStringLiteral("switch2"), QStringLiteral("Metroid Prime 4"), 60);
    QCOMPARE(none.game.id, 0);
    QVERIFY(!none.tie);
}

void test_LaunchBoxDb::switch2_edition_names_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("id");
    QTest::newRow("without it") << QStringLiteral("The Legend of Zelda Breath of the Wild") << 1;
    QTest::newRow("en dash") << QStringLiteral("The Legend of Zelda: Breath of the Wild – Nintendo Switch 2 Edition") << 1;
    QTest::newRow("hyphen") << QStringLiteral("The Legend of Zelda - Breath of the Wild - Nintendo Switch 2 Edition") << 1;
    QTest::newRow("Switch 2 Edition") << QStringLiteral("The Legend of Zelda Breath of the Wild Switch 2 Edition") << 1;
    QTest::newRow("colon, lower case") << QStringLiteral("the legend of zelda breath of the wild: switch 2 edition") << 1;
    QTest::newRow("found with a hyphen") << QStringLiteral("Super Mario Party Jamboree") << 3;
}

// "Nintendo Switch 2 Edition" at the end counts as nothing, on both sides
void test_LaunchBoxDb::switch2_edition_names()
{
    QFETCH(QString, name);
    QFETCH(int, id);
    Db db;
    const Db::Match m = db.best(QStringLiteral("switch2"), name, 60);
    QCOMPARE(m.game.id, id);
    QCOMPARE(m.game.score, 100);
}

// other systems' names are compared as before
void test_LaunchBoxDb::edition_only_for_switch2()
{
    Db db;
    const std::vector<Db::Game> found = db.candidates(QStringLiteral("switch"),
        QStringLiteral("The Legend of Zelda: Breath of the Wild – Nintendo Switch 2 Edition"), 1);
    QVERIFY(!found.empty());
    QVERIFY(found.front().score < 60);
}

// the original Switch: its own platform only, no fallback
void test_LaunchBoxDb::switch_has_no_fallback()
{
    Db db;
    QCOMPARE(db.search_platforms(QStringLiteral("switch")), QStringList { QStringLiteral("Nintendo Switch") });
    QCOMPARE(db.best(QStringLiteral("switch"), QStringLiteral("Super Mario Odyssey"), 60).game.id, 10);
    QCOMPARE(db.best(QStringLiteral("switch"), QStringLiteral("Donkey Kong Bananza"), 60).game.id, 11);
}

void test_LaunchBoxDb::android_own_platform_first()
{
    Db db;
    const Db::Match m = db.best(QStringLiteral("android"), QStringLiteral("Minecraft"), 60);
    QCOMPARE(m.game.id, 20);
    QCOMPARE(m.game.platform, QStringLiteral("Android"));
}

// a recomp: not on Android, its original game is (by an alternate name)
void test_LaunchBoxDb::android_falls_back()
{
    Db db;
    const Db::Match m = db.best(QStringLiteral("androidgames"), QStringLiteral("Ship of Harkinian"), 60);
    QCOMPARE(m.game.id, 30);
    QCOMPARE(m.game.platform, QStringLiteral("Nintendo 64"));
    QVERIFY(!m.tie);
}

// as sure on Nintendo 64 and Sega 32X: none is guessed, both are kept
void test_LaunchBoxDb::android_tie_is_not_guessed()
{
    Db db;
    const Db::Match m = db.best(QStringLiteral("android"), QStringLiteral("Doom"), 60);
    QCOMPARE(m.game.id, 0);
    QVERIFY(m.tie);
    QCOMPARE(static_cast<int>(m.closest.size()), 2);
    QStringList platforms;
    for (const Db::Game& g : m.closest)
        platforms.append(g.platform);
    platforms.sort();
    QCOMPARE(platforms, QStringList({ QStringLiteral("Nintendo 64"), QStringLiteral("Sega 32X") }));
}

// a match you picked (Needs your check) is found on a fallback platform too
void test_LaunchBoxDb::picked_game_found_on_fallback()
{
    Db db;
    QCOMPARE(db.game(QStringLiteral("switch2"), 10).name, QStringLiteral("Super Mario Odyssey"));
    QCOMPARE(db.game(QStringLiteral("android"), 40).platform, QStringLiteral("Sega 32X"));
    QCOMPARE(db.game(QStringLiteral("switch"), 1).id, 0);  // not the original Switch's
}

// Choose (interactive): the matches of every searched platform, each with its platform
void test_LaunchBoxDb::candidates_show_their_platform()
{
    Db db;
    const std::vector<Db::Game> found = db.candidates(QStringLiteral("android"), QStringLiteral("Doom"), 10);
    QStringList platforms;
    for (const Db::Game& g : found) {
        if (g.score == 100)
            platforms.append(g.platform);
    }
    platforms.sort();
    QCOMPARE(platforms, QStringList({ QStringLiteral("Nintendo 64"), QStringLiteral("Sega 32X") }));
    // the same score: the system's own platform first
    const std::vector<Db::Game> both = db.candidates(QStringLiteral("switch2"), QStringLiteral("Donkey Kong Bananza"), 10);
    QVERIFY(both.size() >= 2);
    QCOMPARE(both.front().platform, QStringLiteral("Nintendo Switch 2"));
    QCOMPARE(both.at(1).platform, QStringLiteral("Nintendo Switch"));
}


QTEST_MAIN(test_LaunchBoxDb)
#include "test_LaunchBoxDb.moc"
