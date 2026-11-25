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

#include "MetadataAliases.h"
#include "Recovery.h"
#include "model/gaming/Game.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QTemporaryDir>


// Pegasus X: Library > Metadata. The keys suggestions use (Nintendo's
// spellings grouped, a division never with its parent), merges as aliases
// (chains folded, loops refused, undo), applied to loaded games (their files'
// spellings kept; one name once), the alias file (saved, read again after a
// restart, a rescrape's old spelling folded in again; recovery's check), a
// big list looked through
class test_MetadataAliases : public QObject {
    Q_OBJECT

private:
    static aliases::Value v(const char* name, int count) {
        aliases::Value out;
        out.name = QString::fromUtf8(name);
        out.count = count;
        return out;
    }
    // the names of a suggestion
    static QStringList names(const std::vector<aliases::Value>& values, const aliases::Suggestion& s) {
        QStringList out;
        for (int i : s.values)
            out.append(values[i].name);
        return out;
    }

private slots:
    void keys_data();
    void keys();
    void divisions_never_maybe();
    void suggestions();
    void chains_and_loops();
    void undo();
    void applied_to_games();
    void file_restart_and_rescrape();
    void recovery_check();
    void big_list();
};

void test_MetadataAliases::keys_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("company");
    QTest::addColumn<QString>("region");
    QTest::newRow("plain") << QStringLiteral("Nintendo") << QStringLiteral("nintendo") << QStringLiteral("nintendo");
    QTest::newRow("co ltd") << QStringLiteral("Nintendo Co., Ltd.") << QStringLiteral("nintendo") << QStringLiteral("nintendo");
    QTest::newRow("of america") << QStringLiteral("Nintendo of America") << QStringLiteral("nintendo of america") << QStringLiteral("nintendo");
    QTest::newRow("inc") << QStringLiteral("Capcom U.S.A., Inc.") << QStringLiteral("capcom usa") << QStringLiteral("capcom");
    QTest::newRow("europe") << QStringLiteral("SEGA Europe") << QStringLiteral("sega europe") << QStringLiteral("sega");
    QTest::newRow("division") << QStringLiteral("Sega AM2") << QStringLiteral("sega am2") << QStringLiteral("sega am2");
    QTest::newRow("ead") << QStringLiteral("Nintendo EAD") << QStringLiteral("nintendo ead") << QStringLiteral("nintendo ead");
    QTest::newRow("the") << QStringLiteral("The 3DO Company") << QStringLiteral("3do company") << QStringLiteral("3do company");
    QTest::newRow("accents and") << QStringLiteral("Pokémon & Friends") << QStringLiteral("pokemon and friends") << QStringLiteral("pokemon and friends");
    QTest::newRow("gmbh") << QStringLiteral("THQ Nordic GmbH") << QStringLiteral("thq nordic") << QStringLiteral("thq nordic");
    QTest::newRow("sa") << QStringLiteral("Ubisoft S.A.") << QStringLiteral("ubisoft") << QStringLiteral("ubisoft");
    QTest::newRow("kk jp") << QStringLiteral("Square Enix K.K. Japan") << QStringLiteral("square enix kk japan") << QStringLiteral("square enix");
    QTest::newRow("only a region") << QStringLiteral("USA") << QStringLiteral("usa") << QStringLiteral("usa");
}

void test_MetadataAliases::keys()
{
    QFETCH(QString, name);
    QFETCH(QString, company);
    QFETCH(QString, region);
    QCOMPARE(aliases::company_key(name), company);
    QCOMPARE(aliases::region_key(name), region);
}

void test_MetadataAliases::divisions_never_maybe()
{
    QVERIFY(!aliases::maybe_same(QStringLiteral("sega am2"), QStringLiteral("sega")));
    QVERIFY(!aliases::maybe_same(QStringLiteral("nintendo ead"), QStringLiteral("nintendo")));
    QVERIFY(!aliases::maybe_same(QStringLiteral("konami digital entertainment"), QStringLiteral("konami")));
    QVERIFY(aliases::maybe_same(QStringLiteral("hudson soft"), QStringLiteral("hudsonsoft")));
    QVERIFY(aliases::maybe_same(QStringLiteral("electronic arts"), QStringLiteral("electronic arst")));
    QVERIFY(aliases::maybe_same(QStringLiteral("bandai namco"), QStringLiteral("bandainamco games")));
    QVERIFY(!aliases::maybe_same(QStringLiteral("sega"), QStringLiteral("saga")));  // too short to be 1 letter off
    QVERIFY(!aliases::maybe_same(QStringLiteral("atlus"), QStringLiteral("atari")));
}

void test_MetadataAliases::suggestions()
{
    const std::vector<aliases::Value> values {
        v("Nintendo", 40), v("Nintendo Co., Ltd.", 6), v("Nintendo of America", 9), v("Nintendo EAD", 12),
        v("Sega", 20), v("SEGA Europe", 3), v("Sega AM2", 4),
        v("Electronic Arts", 7), v("Electronic Arst", 1), v("Hudson Soft", 5), v("HudsonSoft", 1),
        v("Atlus", 3), v("Atari", 3),
    };
    const std::vector<aliases::Suggestion> found = aliases::suggest(values);
    QVERIFY(!found.empty());
    bool nintendo_near = false, nintendo_regional = false, sega_regional = false, ea = false, hudson = false;
    for (const aliases::Suggestion& s : found) {
        const QStringList n = names(values, s);
        qInfo() << "level" << s.level << s.games << n;
        // a division never with its parent
        QVERIFY2(!(n.contains(QStringLiteral("Nintendo EAD")) && n.contains(QStringLiteral("Nintendo"))), qPrintable(n.join(QStringLiteral(", "))));
        QVERIFY2(!(n.contains(QStringLiteral("Sega AM2")) && n.contains(QStringLiteral("Sega"))), qPrintable(n.join(QStringLiteral(", "))));
        QVERIFY2(!(n.contains(QStringLiteral("Atlus")) && n.contains(QStringLiteral("Atari"))), qPrintable(n.join(QStringLiteral(", "))));
        if (s.level == 0 && n.contains(QStringLiteral("Nintendo")) && n.contains(QStringLiteral("Nintendo Co., Ltd.")))
            nintendo_near = true;
        if (s.level == 1 && n.contains(QStringLiteral("Nintendo")) && n.contains(QStringLiteral("Nintendo of America")))
            nintendo_regional = true;
        if (s.level == 1 && n.contains(QStringLiteral("Sega")) && n.contains(QStringLiteral("SEGA Europe")))
            sega_regional = true;
        if (s.level == 2 && n.contains(QStringLiteral("Electronic Arts")) && n.contains(QStringLiteral("Electronic Arst")))
            ea = true;
        if (s.level == 2 && n.contains(QStringLiteral("Hudson Soft")) && n.contains(QStringLiteral("HudsonSoft")))
            hudson = true;
        // most games first, and the most used first in it
        QCOMPARE(n.first(), values[s.values.front()].name);
    }
    QVERIFY(nintendo_near);
    QVERIFY(nintendo_regional);
    QVERIFY(sega_regional);
    QVERIFY(ea);
    QVERIFY(hudson);
    // levels in order
    for (size_t i = 1; i < found.size(); i++)
        QVERIFY(found[i - 1].level <= found[i].level);
}

void test_MetadataAliases::chains_and_loops()
{
    aliases::Table t;
    const int P = aliases::Publisher;
    QVERIFY(aliases::add(t, P, { QStringLiteral("A") }, QStringLiteral("B")).isEmpty());
    QCOMPARE(t.map[P].value(QStringLiteral("A")), QStringLiteral("B"));
    // B -> C: A -> C, B -> C (never a chain)
    QVERIFY(aliases::add(t, P, { QStringLiteral("B") }, QStringLiteral("C")).isEmpty());
    QCOMPARE(t.map[P].value(QStringLiteral("A")), QStringLiteral("C"));
    QCOMPARE(t.map[P].value(QStringLiteral("B")), QStringLiteral("C"));
    QVERIFY(!t.map[P].contains(QStringLiteral("C")));
    // D -> A: A is shown as C: D -> C
    QVERIFY(aliases::add(t, P, { QStringLiteral("D") }, QStringLiteral("A")).isEmpty());
    QCOMPARE(t.map[P].value(QStringLiteral("D")), QStringLiteral("C"));
    // C -> A: A is shown as C already: a loop, refused, nothing changed
    const auto before = t.map[P];
    const QString error = aliases::add(t, P, { QStringLiteral("C") }, QStringLiteral("A"));
    QVERIFY(error.contains(QStringLiteral("loop")));
    QCOMPARE(t.map[P], before);
    // nothing to do
    QVERIFY(!aliases::add(t, P, { QStringLiteral("C") }, QStringLiteral("C")).isEmpty());
    // every value has an end that isn't itself a spelling with an alias
    for (auto it = t.map[P].cbegin(); it != t.map[P].cend(); ++it)
        QVERIFY(!t.map[P].contains(it.value()));
    // the other fields untouched
    QVERIFY(t.map[aliases::Genre].isEmpty() && t.map[aliases::Developer].isEmpty());
}

void test_MetadataAliases::undo()
{
    aliases::Table t;
    const int D = aliases::Developer;
    QVERIFY(aliases::add(t, D, { QStringLiteral("Nintendo of America"), QStringLiteral("Nintendo Co., Ltd.") }, QStringLiteral("Nintendo")).isEmpty());
    QVERIFY(aliases::add(t, D, { QStringLiteral("SEGA Europe") }, QStringLiteral("Sega")).isEmpty());
    const QStringList from = aliases::spellings_of(t, D, QStringLiteral("Nintendo"));
    QCOMPARE(from, QStringList({ QStringLiteral("Nintendo Co., Ltd."), QStringLiteral("Nintendo of America") }));
    QCOMPARE(aliases::remove(t, D, from), 2);
    QVERIFY(aliases::spellings_of(t, D, QStringLiteral("Nintendo")).isEmpty());
    QCOMPARE(aliases::shown(t, D, QStringLiteral("Nintendo of America")), QStringLiteral("Nintendo of America"));
    QCOMPARE(aliases::shown(t, D, QStringLiteral("SEGA Europe")), QStringLiteral("Sega"));
}

void test_MetadataAliases::applied_to_games()
{
    aliases::Table t;
    QVERIFY(aliases::add(t, aliases::Publisher, { QStringLiteral("Nintendo of America") }, QStringLiteral("Nintendo")).isEmpty());
    QVERIFY(aliases::add(t, aliases::Genre, { QStringLiteral("Plataform") }, QStringLiteral("Platform")).isEmpty());
    model::Game game;
    game.publisherList() = QStringList { QStringLiteral("Nintendo of America"), QStringLiteral("Nintendo") };
    game.genreList() = QStringList { QStringLiteral("Plataform"), QStringLiteral("Action") };
    game.developerList() = QStringList { QStringLiteral("Nintendo EAD") };
    aliases::apply({ &game }, t);
    // shown: the name once; the files' spellings kept
    QCOMPARE(game.publisherListConst(), QStringList { QStringLiteral("Nintendo") });
    QCOMPARE(game.genreListConst(), QStringList({ QStringLiteral("Platform"), QStringLiteral("Action") }));
    QCOMPARE(game.developerListConst(), QStringList { QStringLiteral("Nintendo EAD") });
    QCOMPARE(aliases::raw_list(game, aliases::Publisher), QStringList({ QStringLiteral("Nintendo of America"), QStringLiteral("Nintendo") }));
    // undone: shown as the files have them again, at once
    aliases::Table none;
    QVERIFY(aliases::reapply(game, none));
    QCOMPARE(game.publisherListConst(), QStringList({ QStringLiteral("Nintendo of America"), QStringLiteral("Nintendo") }));
    QCOMPARE(game.genreListConst(), QStringList({ QStringLiteral("Plataform"), QStringLiteral("Action") }));
    QVERIFY(!aliases::reapply(game, none));
    // a change in place (Change this genre, a scrape): the file's new list, shown through the aliases
    aliases::set_current(t);
    aliases::set_raw(game, aliases::Genre, { QStringLiteral("Plataform"), QStringLiteral("Puzzle") });
    QCOMPARE(game.genreListConst(), QStringList({ QStringLiteral("Platform"), QStringLiteral("Puzzle") }));
    QCOMPARE(aliases::raw_list(game, aliases::Genre), QStringList({ QStringLiteral("Plataform"), QStringLiteral("Puzzle") }));
    aliases::set_current(aliases::Table());
}

void test_MetadataAliases::file_restart_and_rescrape()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/metadata/aliases.json");
    aliases::Table t;
    QVERIFY(aliases::add(t, aliases::Publisher, { QStringLiteral("Nintendo of America"), QStringLiteral("Nintendo Co., Ltd.") }, QStringLiteral("Nintendo")).isEmpty());
    QVERIFY(aliases::add(t, aliases::Genre, { QStringLiteral("Shoot’em Up") }, QStringLiteral("Shoot'em Up")).isEmpty());
    QVERIFY(aliases::save(path, t));
    // a restart: read from the file
    const aliases::Table read = aliases::load(path);
    QCOMPARE(read.map[aliases::Publisher], t.map[aliases::Publisher]);
    QCOMPARE(read.map[aliases::Genre], t.map[aliases::Genre]);
    // a rescrape writes the old spelling into a game again: it folds in at load
    model::Game rescraped;
    rescraped.publisherList() = QStringList { QStringLiteral("Nintendo Co., Ltd.") };
    rescraped.genreList() = QStringList { QStringLiteral("Shoot’em Up") };
    aliases::apply({ &rescraped }, read);
    QCOMPARE(rescraped.publisherListConst(), QStringList { QStringLiteral("Nintendo") });
    QCOMPARE(rescraped.genreListConst(), QStringList { QStringLiteral("Shoot'em Up") });
    // every alias undone: the file is still a JSON object, never empty
    aliases::Table none;
    QVERIFY(aliases::save(path, none));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray bytes = f.readAll();
    QVERIFY(!bytes.isEmpty());
    QVERIFY(QJsonDocument::fromJson(bytes).isObject());
    QVERIFY(aliases::load(path).empty());
    // a missing file: no aliases
    QVERIFY(aliases::load(tmp.path() + QStringLiteral("/none.json")).empty());
}

void test_MetadataAliases::recovery_check()
{
    using recovery::Kind;
    const QString path = QStringLiteral("aliases.json");
    // empty is fine (no alias), even when its last good copy had some
    QVERIFY(recovery::check(Kind::JsonObjectOrEmpty, QByteArray(), path, QByteArray("{\"publisher\":{\"A\":\"B\"}}")).isEmpty());
    QVERIFY(recovery::check(Kind::JsonObjectOrEmpty, QByteArray("{\"version\":1,\"genre\":{}}"), path).isEmpty());
    // cut in half, not JSON, a list: damaged
    QVERIFY(!recovery::check(Kind::JsonObjectOrEmpty, QByteArray("{\"version\":1,\"genre\":{\"A\":"), path).isEmpty());
    QVERIFY(!recovery::check(Kind::JsonObjectOrEmpty, QByteArray("junk junk"), path).isEmpty());
    QVERIFY(!recovery::check(Kind::JsonObjectOrEmpty, QByteArray("[]"), path).isEmpty());
    QVERIFY(!recovery::check(Kind::JsonObjectOrEmpty, QByteArray("{\"a\":1}\0\0\0", 10), path).isEmpty());
}

void test_MetadataAliases::big_list()
{
    // 5000 publishers: looked through on its own thread in the app; here, timed
    std::vector<aliases::Value> values;
    // (made-up names: syllables, so they differ like real ones do)
    const char* const parts[] = { "ka", "ro", "mi", "zen", "tor", "vel", "quo", "bri", "sun", "dax", "lum", "pex" };
    for (int i = 0; i < 5000; i++) {
        aliases::Value one;
        QString name;
        int x = i * 7919 + 13;
        for (int p = 0; p < 4; p++) {
            name += QLatin1String(parts[x % 12]);
            x /= 12;
        }
        name[0] = name[0].toUpper();
        one.name = name + (i % 3 == 0 ? QStringLiteral(" Games") : i % 3 == 1 ? QStringLiteral(" Soft") : QStringLiteral(" Interactive"));
        one.count = 1 + i % 13;
        values.push_back(one);
    }
    values.push_back(v("Nintendo", 50));
    values.push_back(v("Nintendo of America", 20));
    QElapsedTimer timer;
    timer.start();
    const std::vector<aliases::Suggestion> found = aliases::suggest(values);
    qInfo() << "5002 values looked through in" << timer.elapsed() << "ms," << found.size() << "suggestions";
    bool nintendo = false;
    for (const aliases::Suggestion& s : found)
        nintendo = nintendo || names(values, s).contains(QStringLiteral("Nintendo of America"));
    QVERIFY(nintendo);
}


QTEST_MAIN(test_MetadataAliases)
#include "test_MetadataAliases.moc"
