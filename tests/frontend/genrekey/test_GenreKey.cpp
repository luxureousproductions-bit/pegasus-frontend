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
};

QString test_GenreKey::key(const QString& name)
{
    const QJSValue out = m_js.globalObject().property(QStringLiteral("key")).call({ name });
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


QTEST_MAIN(test_GenreKey)
#include "test_GenreKey.moc"
