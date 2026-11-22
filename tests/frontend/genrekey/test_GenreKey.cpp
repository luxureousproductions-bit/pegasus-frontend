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
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>


// Pegasus X: Library > Genres counts genres by GenreKey.js. This runs that
// very file (src/frontend/menu/settings/GenreKey.js) in a JS engine.
class test_GenreKey : public QObject {
    Q_OBJECT

private:
    QJSEngine m_js;

    QString key(const QString& name);
    QString call(const char* fn, const QString& a, const QString& b);
    // GenreKey.group() of games (one list of genre names per game)
    QJSValue group(const QList<QStringList>& games, bool skip_apps = false);
    static QList<QStringList> times(const QString& genre, int n)
    {
        QList<QStringList> out;
        for (int i = 0; i < n; i++)
            out.append(QStringList { genre });
        return out;
    }

private slots:
    void initTestCase();

    void the_reported_case();
    void variants_are_one_genre_data();
    void variants_are_one_genre();
    void slash_is_not_split();
    void other_genres_stay_apart();
    void shown_spelling();
    void spellings_and_counts();
    void codes();
    void diff_lines_up();
    void markup_marks_what_differs();
    void markup_is_escaped();
    void note_in_words();
    void replace_data();
    void replace();
    void same();
    void view_search();
    void view_only_ones();
    void view_sort();
};

QString test_GenreKey::key(const QString& name)
{
    const QJSValue out = m_js.globalObject().property(QStringLiteral("key")).call({ name });
    return out.toString();
}

QString test_GenreKey::call(const char* fn, const QString& a, const QString& b)
{
    const QJSValue out = m_js.globalObject().property(QString::fromLatin1(fn)).call({ a, b });
    if (out.isError())
        qWarning() << out.toString();
    return out.toString();
}

QJSValue test_GenreKey::group(const QList<QStringList>& games, bool skip_apps)
{
    QJsonArray all;
    for (const QStringList& g : games)
        all.append(QJsonArray::fromStringList(g));
    const QString json = QString::fromUtf8(QJsonDocument(all).toJson(QJsonDocument::Compact));
    const QJSValue out = m_js.evaluate(QStringLiteral("group(%1, %2)").arg(json,
        skip_apps ? QStringLiteral("function(k) { return k === 'application'; }") : QStringLiteral("null")));
    if (out.isError())
        qWarning() << out.toString();
    return out;
}

void test_GenreKey::initTestCase()
{
    const QString path = QFINDTESTDATA("../../../src/frontend/menu/settings/GenreKey.js");
    QVERIFY2(!path.isEmpty(), "GenreKey.js not found");
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    // ".pragma library" is for QML's import; the rest is plain JS
    QString source = QString::fromUtf8(file.readAll());
    source.replace(QStringLiteral(".pragma library"), QString());
    const QJSValue loaded = m_js.evaluate(source, path);
    QVERIFY2(!loaded.isError(), qPrintable(loaded.toString()));
}

// "Beat’em Up / Fantasy" (1 game, a curly apostrophe) and "Beat'em Up / Fantasy"
// (50 games): counted by their text as it is (what Genres did), two rows that
// look alike; by their key, one row of 51 with 2 spellings
void test_GenreKey::the_reported_case()
{
    const QString curly = QStringLiteral("Beat’em Up / Fantasy");
    const QString straight = QStringLiteral("Beat'em Up / Fantasy");
    QVERIFY(curly.trimmed() != straight.trimmed());   // as Genres keyed them: two genres

    QList<QStringList> games = times(straight, 50);
    games.append({ curly });
    const QJSValue rows = group(games);
    QCOMPARE(rows.property(QStringLiteral("length")).toInt(), 1);
    const QJSValue row = rows.property(0);
    QCOMPARE(row.property(QStringLiteral("count")).toInt(), 51);
    QCOMPARE(row.property(QStringLiteral("name")).toString(), straight);
    QCOMPARE(row.property(QStringLiteral("spellings")).property(QStringLiteral("length")).toInt(), 2);
}

void test_GenreKey::variants_are_one_genre_data()
{
    QTest::addColumn<QString>("variant");
    QTest::newRow("curly right ’") << QStringLiteral("Beat’em Up / Fantasy");
    QTest::newRow("curly left ‘") << QStringLiteral("Beat‘em Up / Fantasy");
    QTest::newRow("reversed ‛") << QStringLiteral("Beat‛em Up / Fantasy");
    QTest::newRow("modifier ʼ") << QStringLiteral("Beatʼem Up / Fantasy");
    QTest::newRow("grave `") << QStringLiteral("Beat`em Up / Fantasy");
    QTest::newRow("acute ´") << QStringLiteral("Beat´em Up / Fantasy");
    QTest::newRow("prime ′") << QStringLiteral("Beat′em Up / Fantasy");
    QTest::newRow("non-breaking space") << QStringLiteral("Beat'em Up / Fantasy");
    QTest::newRow("double space") << QStringLiteral("Beat'em  Up / Fantasy");
    QTest::newRow("tab") << QStringLiteral("Beat'em\tUp / Fantasy");
    QTest::newRow("spaces at the ends") << QStringLiteral("  Beat'em Up / Fantasy ");
    QTest::newRow("no spaces around /") << QStringLiteral("Beat'em Up/Fantasy");
    QTest::newRow("space before / only") << QStringLiteral("Beat'em Up /Fantasy");
    QTest::newRow("space after / only") << QStringLiteral("Beat'em Up/ Fantasy");
    QTest::newRow("two spaces around /") << QStringLiteral("Beat'em Up  /  Fantasy");
    QTest::newRow("upper case") << QStringLiteral("BEAT'EM UP / FANTASY");
    QTest::newRow("mixed case") << QStringLiteral("Beat'Em up / fantasy");
}

// each one: the same key as "Beat'em Up / Fantasy", and one row with it
void test_GenreKey::variants_are_one_genre()
{
    QFETCH(QString, variant);
    const QString straight = QStringLiteral("Beat'em Up / Fantasy");
    QCOMPARE(key(variant), key(straight));
    QCOMPARE(key(straight), QStringLiteral("beat'em up / fantasy"));
    const QJSValue rows = group({ { straight }, { variant } });
    QCOMPARE(rows.property(QStringLiteral("length")).toInt(), 1);
    QCOMPARE(rows.property(0).property(QStringLiteral("count")).toInt(), 2);
}

// a genre with "/" stays one genre, not two
void test_GenreKey::slash_is_not_split()
{
    const QJSValue rows = group({ { QStringLiteral("Beat'em Up / Fantasy") } });
    QCOMPARE(rows.property(QStringLiteral("length")).toInt(), 1);
    QCOMPARE(rows.property(0).property(QStringLiteral("name")).toString(), QStringLiteral("Beat'em Up / Fantasy"));
    QVERIFY(key(QStringLiteral("Beat'em Up / Fantasy")) != key(QStringLiteral("Beat'em Up")));
    QVERIFY(key(QStringLiteral("Beat'em Up / Fantasy")) != key(QStringLiteral("Fantasy")));
}

// genres that really differ stay apart: words, letters, a missing "Up"
void test_GenreKey::other_genres_stay_apart()
{
    const QJSValue rows = group({
        { QStringLiteral("Beat'em / Fantasy") },
        { QStringLiteral("Beat'em Up / Fantasy") },
        { QStringLiteral("Beat'em Up") },
        { QStringLiteral("Action") },
        { QStringLiteral("Adventure") },
        { QStringLiteral("Action-Adventure") },
        { QStringLiteral("Application") },
    }, true);
    QCOMPARE(rows.property(QStringLiteral("length")).toInt(), 6);   // Application left out
    QStringList names;
    for (int i = 0; i < 6; i++)
        names << rows.property(i).property(QStringLiteral("name")).toString();
    names.sort();   // (the A to Z of the screen is locale-aware; here only which rows)
    QCOMPARE(names, QStringList({ QStringLiteral("Action"), QStringLiteral("Action-Adventure"), QStringLiteral("Adventure"),
                                  QStringLiteral("Beat'em / Fantasy"), QStringLiteral("Beat'em Up"), QStringLiteral("Beat'em Up / Fantasy") }));
}

// the spelling shown: the one most games have; a tie: A to Z
void test_GenreKey::shown_spelling()
{
    QList<QStringList> games = times(QStringLiteral("Beat’em Up / Fantasy"), 3);
    games += times(QStringLiteral("Beat'em Up / Fantasy"), 2);
    QCOMPARE(group(games).property(0).property(QStringLiteral("name")).toString(), QStringLiteral("Beat’em Up / Fantasy"));

    const QJSValue tie = group({ { QStringLiteral("action") }, { QStringLiteral("Action") } });
    QCOMPARE(tie.property(0).property(QStringLiteral("name")).toString(), QStringLiteral("Action"));
}

// the spellings of a row and their counts; a game with two spellings of one
// genre counts once
void test_GenreKey::spellings_and_counts()
{
    QList<QStringList> games = times(QStringLiteral("Beat'em Up / Fantasy"), 50);
    games.append({ QStringLiteral("Beat’em Up / Fantasy") });
    games.append({ QStringLiteral("beat'em up/fantasy") });
    games.append({ QStringLiteral("Beat'em Up / Fantasy"), QStringLiteral("Beat'em Up / Fantasy") });
    const QJSValue row = group(games).property(0);
    QCOMPARE(row.property(QStringLiteral("count")).toInt(), 53);
    const QJSValue spellings = row.property(QStringLiteral("spellings"));
    QCOMPARE(spellings.property(QStringLiteral("length")).toInt(), 3);
    QCOMPARE(spellings.property(0).property(QStringLiteral("name")).toString(), QStringLiteral("Beat'em Up / Fantasy"));
    QCOMPARE(spellings.property(0).property(QStringLiteral("count")).toInt(), 51);
    // one row, one spelling: no "spellings" note
    QCOMPARE(group({ { QStringLiteral("Action") } }).property(0).property(QStringLiteral("spellings"))
             .property(QStringLiteral("length")).toInt(), 1);
}

// what the log shows of a spelling
void test_GenreKey::codes()
{
    const QJSValue out = m_js.globalObject().property(QStringLiteral("codes")).call({ QStringLiteral("a’ ") });
    QCOMPARE(out.toString(), QStringLiteral("U+0061 U+2019 U+00A0"));
}

// two spellings character by character: only the curly apostrophe differs
void test_GenreKey::diff_lines_up()
{
    const QJSValue d = m_js.globalObject().property(QStringLiteral("diff"))
        .call({ QStringLiteral("Beat’em Up"), QStringLiteral("Beat'em Up") });
    QCOMPARE(d.property(QStringLiteral("onlyA")).property(QStringLiteral("length")).toInt(), 1);
    QCOMPARE(d.property(QStringLiteral("onlyA")).property(0).toInt(), 4);
    QCOMPARE(d.property(QStringLiteral("onlyB")).property(0).toInt(), 4);
    QCOMPARE(d.property(QStringLiteral("caseA")).property(QStringLiteral("length")).toInt(), 0);
    // letter case: lined up, marked as case
    const QJSValue c = m_js.globalObject().property(QStringLiteral("diff"))
        .call({ QStringLiteral("PLatform"), QStringLiteral("Platform") });
    QCOMPARE(c.property(QStringLiteral("onlyA")).property(QStringLiteral("length")).toInt(), 0);
    QCOMPARE(c.property(QStringLiteral("caseA")).property(QStringLiteral("length")).toInt(), 1);
    QCOMPARE(c.property(QStringLiteral("caseA")).property(0).toInt(), 1);
}

// what differs from the first spelling in amber and underlined; a space as ·
// where it differs or isn't a plain one; plain spaces kept as &nbsp;
void test_GenreKey::markup_marks_what_differs()
{
    const QString hot = QStringLiteral("<font color=\"#e8b06a\"><u>%1</u></font>");
    QCOMPARE(call("markup", QStringLiteral("Beat’em Up"), QStringLiteral("Beat'em Up")),
             QStringLiteral("‹Beat") + hot.arg(QStringLiteral("’")) + QStringLiteral("em&nbsp;Up›"));
    // the first spelling: nothing marked
    QCOMPARE(call("markup", QStringLiteral("Beat'em Up"), QString()), QStringLiteral("‹Beat'em&nbsp;Up›"));
    // a doubled space: the extra one, as ·
    QCOMPARE(call("markup", QStringLiteral("Up  Down"), QStringLiteral("Up Down")),
             QStringLiteral("‹Up&nbsp;") + hot.arg(QStringLiteral("·")) + QStringLiteral("Down›"));
    // a non-breaking space: shown as ⍽ even in the first spelling
    QCOMPARE(call("markup", QStringLiteral("Up\u00A0Down"), QString()),
             QStringLiteral("‹Up") + hot.arg(QStringLiteral("⍽")) + QStringLiteral("Down›"));
    // letter case
    QCOMPARE(call("markup", QStringLiteral("PLatform"), QStringLiteral("Platform")),
             QStringLiteral("‹P") + hot.arg(QStringLiteral("L")) + QStringLiteral("atform›"));
}

// a genre's text can't make markup of its own
void test_GenreKey::markup_is_escaped()
{
    QCOMPARE(call("markup", QStringLiteral("<b>A&B</b>"), QString()), QStringLiteral("‹&lt;b&gt;A&amp;B&lt;/b&gt;›"));
}

void test_GenreKey::note_in_words()
{
    QCOMPARE(call("note", QStringLiteral("Beat’em Up"), QStringLiteral("Beat'em Up")),
             QStringLiteral("extra ’ U+2019 · missing ' U+0027"));
    QCOMPARE(call("note", QStringLiteral("beat'em  up/fantasy"), QStringLiteral("Beat'em Up / Fantasy")),
             QStringLiteral("extra · U+0020 · missing · U+0020 ×2 · letter case"));
    QCOMPARE(call("note", QStringLiteral("Action"), QStringLiteral("Action")), QString());
}

// Change this genre...: only the chosen genre (any spelling of it) is
// replaced; the game's other genres stay, in their order; the new one once
void test_GenreKey::replace_data()
{
    QTest::addColumn<QStringList>("genres");
    QTest::addColumn<QString>("from");
    QTest::addColumn<QString>("to");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("the others stay, in order") << QStringList { QStringLiteral("Action"), QStringLiteral("Platform"), QStringLiteral("RPG") } << QStringLiteral("Platform") << QStringLiteral("Platformer")
        << QStringList { QStringLiteral("Action"), QStringLiteral("Platformer"), QStringLiteral("RPG") };
    QTest::newRow("any spelling of it") << QStringList { QStringLiteral("platform"), QStringLiteral("Action") } << QStringLiteral("Platform") << QStringLiteral("Platformer")
        << QStringList { QStringLiteral("Platformer"), QStringLiteral("Action") };
    QTest::newRow("two spellings of it: once") << QStringList { QStringLiteral("Platform"), QStringLiteral("PLATFORM"), QStringLiteral("Action") } << QStringLiteral("Platform") << QStringLiteral("Platformer")
        << QStringList { QStringLiteral("Platformer"), QStringLiteral("Action") };
    QTest::newRow("has the new one already") << QStringList { QStringLiteral("Platformer"), QStringLiteral("Platform") } << QStringLiteral("Platform") << QStringLiteral("Platformer")
        << QStringList { QStringLiteral("Platformer") };
    QTest::newRow("has the new one after it") << QStringList { QStringLiteral("Platform"), QStringLiteral("platformer") } << QStringLiteral("Platform") << QStringLiteral("Platformer")
        << QStringList { QStringLiteral("Platformer") };
    QTest::newRow("has the new one before it: kept as written") << QStringList { QStringLiteral("platformer"), QStringLiteral("Platform") } << QStringLiteral("Platform") << QStringLiteral("Platformer")
        << QStringList { QStringLiteral("platformer") };
    QTest::newRow("one spelling for all") << QStringList { QStringLiteral("Shoot’em Up"), QStringLiteral("Action") } << QStringLiteral("Shoot'em Up") << QStringLiteral("Shoot'em Up")
        << QStringList { QStringLiteral("Shoot'em Up"), QStringLiteral("Action") };
    QTest::newRow("other duplicates untouched") << QStringList { QStringLiteral("Action"), QStringLiteral("Action"), QStringLiteral("Platform") } << QStringLiteral("Platform") << QStringLiteral("RPG")
        << QStringList { QStringLiteral("Action"), QStringLiteral("Action"), QStringLiteral("RPG") };
    QTest::newRow("Application stays") << QStringList { QStringLiteral("Application"), QStringLiteral("Platform") } << QStringLiteral("Platform") << QStringLiteral("RPG")
        << QStringList { QStringLiteral("Application"), QStringLiteral("RPG") };
    QTest::newRow("the new one trimmed") << QStringList { QStringLiteral("Platform") } << QStringLiteral("Platform") << QStringLiteral("  RPG ")
        << QStringList { QStringLiteral("RPG") };
    QTest::newRow("a game without it: as it is") << QStringList { QStringLiteral("Action"), QStringLiteral("RPG") } << QStringLiteral("Platform") << QStringLiteral("Platformer")
        << QStringList { QStringLiteral("Action"), QStringLiteral("RPG") };
    QTest::newRow("a genre with a slash stays whole") << QStringList { QStringLiteral("Beat'em Up / Fantasy"), QStringLiteral("Fantasy") } << QStringLiteral("Fantasy") << QStringLiteral("Magic")
        << QStringList { QStringLiteral("Beat'em Up / Fantasy"), QStringLiteral("Magic") };
}

void test_GenreKey::replace()
{
    QFETCH(QStringList, genres);
    QFETCH(QString, from);
    QFETCH(QString, to);
    QFETCH(QStringList, expected);
    QJSValue fn = m_js.globalObject().property(QStringLiteral("replace"));
    const QJSValue out = fn.call({ m_js.toScriptValue(genres), key(from), to });
    QVERIFY2(!out.isError(), qPrintable(out.toString()));
    QCOMPARE(out.toVariant().toStringList(), expected);
}

void test_GenreKey::same()
{
    QJSValue fn = m_js.globalObject().property(QStringLiteral("same"));
    QVERIFY(fn.call({ m_js.toScriptValue(QStringList { QStringLiteral("A"), QStringLiteral("B") }), m_js.toScriptValue(QStringList { QStringLiteral("A"), QStringLiteral("B") }) }).toBool());
    QVERIFY(!fn.call({ m_js.toScriptValue(QStringList { QStringLiteral("A"), QStringLiteral("B") }), m_js.toScriptValue(QStringList { QStringLiteral("B"), QStringLiteral("A") }) }).toBool());
    QVERIFY(!fn.call({ m_js.toScriptValue(QStringList { QStringLiteral("a") }), m_js.toScriptValue(QStringList { QStringLiteral("A") }) }).toBool());
}

// the list as shown: rows of group(), searched, filtered, sorted (names only)
static QStringList view_names(QJSEngine& js, const QJSValue& rows, const QString& query, bool only_ones, const QString& order)
{
    QJSValue fn = js.globalObject().property(QStringLiteral("view"));
    const QJSValue out = fn.call({ rows, query, only_ones, order });
    QStringList names;
    for (int i = 0; i < out.property(QStringLiteral("length")).toInt(); i++)
        names << out.property(i).property(QStringLiteral("name")).toString()
                 + QLatin1Char(':') + QString::number(out.property(i).property(QStringLiteral("count")).toInt());
    return names;
}
static QList<QStringList> view_games()
{
    return {
        { QStringLiteral("Action") }, { QStringLiteral("Action") }, { QStringLiteral("Action") },
        { QStringLiteral("Platform") }, { QStringLiteral("platform") },
        { QStringLiteral("Shoot’em Up") }, { QStringLiteral("RPG") }, { QStringLiteral("Beat'em Up / Fantasy") },
    };
}

// part of a name, any case, apostrophe or spacing; any spelling counts
void test_GenreKey::view_search()
{
    const QJSValue rows = group(view_games());
    QCOMPARE(view_names(m_js, rows, QString(), false, QStringLiteral("az")).size(), 5);
    QCOMPARE(view_names(m_js, rows, QStringLiteral("plat"), false, QStringLiteral("az")),
             QStringList { QStringLiteral("Platform:2") });
    QCOMPARE(view_names(m_js, rows, QStringLiteral("shoot'em"), false, QStringLiteral("az")),
             QStringList { QStringLiteral("Shoot’em Up:1") });
    QCOMPARE(view_names(m_js, rows, QStringLiteral("  EM up"), false, QStringLiteral("az")),
             QStringList({ QStringLiteral("Beat'em Up / Fantasy:1"), QStringLiteral("Shoot’em Up:1") }));
    QVERIFY(view_names(m_js, rows, QStringLiteral("zzz"), false, QStringLiteral("az")).isEmpty());
    // the rows themselves aren't changed
    QCOMPARE(rows.property(QStringLiteral("length")).toInt(), 5);
}

void test_GenreKey::view_only_ones()
{
    const QJSValue rows = group(view_games());
    QCOMPARE(view_names(m_js, rows, QString(), true, QStringLiteral("az")),
             QStringList({ QStringLiteral("Beat'em Up / Fantasy:1"), QStringLiteral("RPG:1"), QStringLiteral("Shoot’em Up:1") }));
    QCOMPARE(view_names(m_js, rows, QStringLiteral("rpg"), true, QStringLiteral("az")),
             QStringList { QStringLiteral("RPG:1") });
}

// by game count both ways; the same count stays A to Z
void test_GenreKey::view_sort()
{
    const QJSValue rows = group(view_games());
    QCOMPARE(view_names(m_js, rows, QString(), false, QStringLiteral("az")),
             QStringList({ QStringLiteral("Action:3"), QStringLiteral("Beat'em Up / Fantasy:1"), QStringLiteral("Platform:2"),
                           QStringLiteral("RPG:1"), QStringLiteral("Shoot’em Up:1") }));
    QCOMPARE(view_names(m_js, rows, QString(), false, QStringLiteral("most")),
             QStringList({ QStringLiteral("Action:3"), QStringLiteral("Platform:2"), QStringLiteral("Beat'em Up / Fantasy:1"),
                           QStringLiteral("RPG:1"), QStringLiteral("Shoot’em Up:1") }));
    QCOMPARE(view_names(m_js, rows, QString(), false, QStringLiteral("fewest")),
             QStringList({ QStringLiteral("Beat'em Up / Fantasy:1"), QStringLiteral("RPG:1"), QStringLiteral("Shoot’em Up:1"),
                           QStringLiteral("Platform:2"), QStringLiteral("Action:3") }));
}


QTEST_MAIN(test_GenreKey)
#include "test_GenreKey.moc"
