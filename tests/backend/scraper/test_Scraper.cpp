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

#include "Scraper.h"

#include <QBuffer>
#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>


// Pegasus X: the scraper's waits and downloads (Scraper.cpp), against a
// server on this machine:
//   /never/...      takes the request and never answers
//   /slow/...       answers a picture a little at a time (a piece every 4 s, 28 s in all)
//   /img/<n>?d=<ms> answers picture n after d ms
// It counts the requests per path and the most answered at once.
namespace {
QByteArray png(int n)
{
    QImage image(64, 64, QImage::Format_RGB32);
    image.fill(QColor::fromHsv((n * 37) % 360, 200, 220));
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return out;
}

class FakeServer : public QTcpServer {
public:
    QHash<QString, int> requests;           // path -> how many
    QHash<QString, QList<qint64>> when;     // path -> ms since start, each request
    int busy = 0;                           // /img requests being answered now
    int most_busy = 0;
    QElapsedTimer clock;
    QList<QPointer<QTcpSocket>> held;       // the /never ones, kept open

    FakeServer() {
        clock.start();
        connect(this, &QTcpServer::newConnection, this, [this]{
            while (QTcpSocket* const socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]{ onRequest(socket); });
            }
        });
    }
    QString base() const { return QStringLiteral("http://127.0.0.1:%1").arg(serverPort()); }

private:
    void onRequest(QTcpSocket* socket) {
        if (socket->property("seen").toBool()) {
            socket->readAll();
            return;
        }
        const QByteArray head = socket->readAll();
        if (!head.contains("\r\n\r\n"))
            return;  // (the rest of the request still to come; never in these tests)
        socket->setProperty("seen", true);
        const QString line = QString::fromLatin1(head.left(head.indexOf("\r\n")));
        const QUrl url(QStringLiteral("http://x") + line.section(QLatin1Char(' '), 1, 1));
        const QString path = url.path();
        requests[path]++;
        when[path].append(clock.elapsed());
        if (path.startsWith(QLatin1String("/never"))) {
            held.append(socket);
            return;
        }
        if (path.startsWith(QLatin1String("/slow"))) {
            const QByteArray body = png(99) + QByteArray(7 * 64, '\0');  // (a PNG with padding after it)
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nConnection: close\r\nContent-Length: "
                          + QByteArray::number(body.size()) + "\r\n\r\n");
            const int pieces = 7;
            const int piece = (body.size() + pieces - 1) / pieces;
            for (int i = 0; i < pieces; i++) {
                const QByteArray part = body.mid(i * piece, piece);
                QTimer::singleShot(4000 * (i + 1), socket, [socket, part, i, pieces]{
                    socket->write(part);
                    if (i == pieces - 1)
                        socket->disconnectFromHost();
                });
            }
            return;
        }
        if (path.startsWith(QLatin1String("/img/"))) {
            const int n = path.mid(5).toInt();
            const int delay = QUrlQuery(url).queryItemValue(QStringLiteral("d")).toInt();
            busy++;
            most_busy = qMax(most_busy, busy);
            QTimer::singleShot(delay, socket, [this, socket, n]{
                const QByteArray body = png(n);
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nConnection: close\r\nContent-Length: "
                              + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
                busy--;
            });
            return;
        }
        socket->write("HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
        socket->disconnectFromHost();
    }
};
} // namespace


class test_Scraper : public QObject {
    Q_OBJECT

private:
    // a scraper ready to fetch, not running a scrape
    static void prepare(scraper::Scraper& s) {
        s.m_net = new QNetworkAccessManager(&s);
    }
    // the nine pictures of a game, the slower ones first (so they arrive out of order)
    static std::vector<scraper::Media> nine(const QString& base) {
        struct K { const char* slot; const char* folder; const char* kind; };
        const std::vector<K> kinds {
            { "assets.box_front", "covers", "2d" },
            { "assets.box_front", "3dboxes", "3d" },
            { "assets.box_back", "backcovers", "" },
            { "assets.cartridge", "physicalmedia", "" },
            { "assets.screenshot", "screenshots", "" },
            { "assets.titlescreen", "titlescreens", "" },
            { "assets.background", "fanart", "" },
            { "assets.logo", "marquees", "" },
            { "assets.steam", "miximages", "" },
        };
        std::vector<scraper::Media> out;
        for (size_t i = 0; i < kinds.size(); i++) {
            scraper::Media m;
            m.setting = QStringLiteral("getScreenshots");
            m.slot = QLatin1String(kinds[i].slot);
            m.folder = QLatin1String(kinds[i].folder);
            m.kind = QLatin1String(kinds[i].kind);
            m.url = QUrl(base + QStringLiteral("/img/%1?d=%2").arg(i).arg(900 - static_cast<int>(i) * 100));
            m.ext = QStringLiteral("png");
            out.push_back(m);
        }
        return out;
    }
    // a game's downloads, to the end (the miximage and saving left out)
    static void downloadAll(scraper::Scraper& s, const QString& data_dir, std::vector<scraper::Media> media, int* most, QVariantMap* changes) {
        scraper::Job job;
        job.collection = QStringLiteral("Nintendo SNES");
        job.system = QStringLiteral("snes");
        job.title = QStringLiteral("Chrono Trigger");
        job.path = QStringLiteral("/roms/snes/Chrono Trigger (USA).sfc");
        s.m_jobs = { job };
        s.m_index = 0;
        s.m_data_dir = data_dir;
        s.m_settings = QVariantMap {
            { QStringLiteral("imageMaxSize"), QStringLiteral("640") },  // (resized off the main thread)
            { QStringLiteral("trimBorders"), QStringLiteral("none") },
        };
        s.m_changes.clear();
        s.m_got.clear();
        s.m_media = media;
        s.m_downloads.clear();
        s.m_dl_begun = false;
        s.m_dl_gen++;
        bool done = false;
        s.m_downloads_done_hook = [&done]{ done = true; };
        s.downloadNext();
        QTRY_VERIFY_WITH_TIMEOUT(done, 30000);
        *most = s.m_dl_most;
        *changes = s.m_changes;
    }

    // Pegasus X: one game's answer read as the scraper reads it (no network): what it would save
    static QVariantMap readScreenScraper(const QByteArray& json, bool has_esrb, bool overwrite, bool details = true) {
        scraper::Scraper s;
        scraper::Job job;
        job.collection = QStringLiteral("Nintendo SNES");
        job.system = QStringLiteral("snes");
        job.title = QStringLiteral("Some Game");
        job.path = QStringLiteral("/roms/snes/Some Game (USA).sfc");
        job.has_esrb = has_esrb;
        s.m_jobs = { job };
        s.m_index = 0;
        s.m_settings = QVariantMap {
            { QStringLiteral("overwrite"), overwrite }, { QStringLiteral("getDetails"), details },
            { QStringLiteral("region"), QStringLiteral("us") }, { QStringLiteral("language"), QStringLiteral("en") },
        };
        s.parseScreenScraper(QJsonDocument::fromJson(json).object());
        return s.m_changes;
    }
    static QVariantMap readLaunchBox(const QString& esrb, bool has_esrb, bool overwrite) {
        scraper::Scraper s;
        scraper::Job job;
        job.collection = QStringLiteral("Nintendo SNES");
        job.system = QStringLiteral("snes");
        job.title = QStringLiteral("Super Mario World");
        job.path = QStringLiteral("/roms/snes/Super Mario World (USA).sfc");
        job.has_esrb = has_esrb;
        s.m_jobs = { job };
        s.m_index = 0;
        s.m_settings = QVariantMap { { QStringLiteral("overwrite"), overwrite } };
        scraper::LaunchBoxDb::Game game;
        game.id = 136;
        game.name = QStringLiteral("Super Mario World");
        game.platform = QStringLiteral("Super Nintendo Entertainment System");
        game.esrb = esrb;
        game.rating = 0.86f;
        s.parseLaunchBox(game, false);
        return s.m_changes;
    }

private slots:
    void esrb_codes_data();
    void esrb_codes();
    void screenscraper_esrb();
    void launchbox_esrb();
    void lookup_with_no_answer_gives_up();
    void download_with_no_answer_is_skipped();
    void slow_download_keeps_going();
    void nine_pictures_at_most_four_at_once();
    void stop_during_downloads();
};

// a lookup: given up after 15 s with no data, tried once more after 2 s, then done
void test_Scraper::lookup_with_no_answer_gives_up()
{
    FakeServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    scraper::Scraper s;
    prepare(s);
    int status = -1;
    bool done = false;
    QElapsedTimer t;
    t.start();
    s.get(QUrl(server.base() + QStringLiteral("/never/lookup")), {}, 3,
          [&](int st, const QByteArray&){ status = st; done = true; }, scraper::Scraper::Fetch::Lookup);
    QTRY_VERIFY_WITH_TIMEOUT(done, 60000);
    const qint64 took = t.elapsed();
    const QList<qint64> at = server.when.value(QStringLiteral("/never/lookup"));
    qInfo() << "lookup: gave up after" << took << "ms, requests at" << at;
    QCOMPARE(status, 0);
    QCOMPARE(at.size(), 2);                       // once more, not more
    const qint64 first = at.at(1) - at.at(0);     // the first try's 15 s + the 2 s wait
    QVERIFY2(first >= 16000 && first <= 19500, qPrintable(QString::number(first)));
    QVERIFY2(took >= 31000 && took <= 37000, qPrintable(QString::number(took)));
}

// a picture: given up after 20 s with no data, not tried again
void test_Scraper::download_with_no_answer_is_skipped()
{
    FakeServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    scraper::Scraper s;
    prepare(s);
    int status = -1;
    bool done = false;
    QElapsedTimer t;
    t.start();
    s.get(QUrl(server.base() + QStringLiteral("/never/picture.png")), {}, 3,
          [&](int st, const QByteArray&){ status = st; done = true; }, scraper::Scraper::Fetch::Download);
    QTRY_VERIFY_WITH_TIMEOUT(done, 40000);
    const qint64 took = t.elapsed();
    qInfo() << "picture: skipped after" << took << "ms";
    QCOMPARE(status, 0);
    QCOMPARE(server.requests.value(QStringLiteral("/never/picture.png")), 1);
    QVERIFY2(took >= 19500 && took <= 23500, qPrintable(QString::number(took)));
}

// a picture that keeps coming, slowly (28 s in all, never 20 s with nothing): not cut
void test_Scraper::slow_download_keeps_going()
{
    FakeServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    scraper::Scraper s;
    prepare(s);
    int status = -1;
    QByteArray got;
    bool done = false;
    QElapsedTimer t;
    t.start();
    s.get(QUrl(server.base() + QStringLiteral("/slow/picture.png")), {}, 0,
          [&](int st, const QByteArray& body){ status = st; got = body; done = true; }, scraper::Scraper::Fetch::Download);
    QTRY_VERIFY_WITH_TIMEOUT(done, 60000);
    qInfo() << "slow picture:" << got.size() << "bytes in" << t.elapsed() << "ms";
    QCOMPARE(status, 200);
    QCOMPARE(got.size(), png(99).size() + 7 * 64);
    QVERIFY(t.elapsed() > 25000);
    QCOMPARE(server.requests.value(QStringLiteral("/slow/picture.png")), 1);
}

// nine pictures, four at once: all saved, in the same slots and order as
// one at a time (the 2D box before the 3D one), never more than four on their way
void test_Scraper::nine_pictures_at_most_four_at_once()
{
    FakeServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QTemporaryDir one_dir, four_dir;
    QVERIFY(one_dir.isValid() && four_dir.isValid());

    // one at a time (as before)
    QVariantMap one;
    int one_most = 0;
    {
        scraper::Scraper s;
        prepare(s);
        s.m_host_limits.insert(QStringLiteral("127.0.0.1"), 1);
        downloadAll(s, one_dir.path(), nine(server.base()), &one_most, &one);
    }
    QCOMPARE(one_most, 1);
    server.most_busy = 0;

    // four at once
    QVariantMap four;
    int four_most = 0;
    QElapsedTimer t;
    t.start();
    {
        scraper::Scraper s;
        prepare(s);
        s.m_host_limits.insert(QStringLiteral("127.0.0.1"), 4);
        downloadAll(s, four_dir.path(), nine(server.base()), &four_most, &four);
    }
    qInfo() << "nine pictures, four at once:" << t.elapsed() << "ms, at most" << four_most
            << "on their way, the server answering at most" << server.most_busy;
    QVERIFY(four_most <= 4 && four_most >= 2);
    QVERIFY(server.most_busy <= 4 && server.most_busy >= 2);

    // the same slots, files and order (the folders told apart)
    QString four_text = QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(four)).toJson(QJsonDocument::Compact));
    QString one_text = QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(one)).toJson(QJsonDocument::Compact));
    four_text.replace(four_dir.path(), QStringLiteral("<data>"));
    one_text.replace(one_dir.path(), QStringLiteral("<data>"));
    QCOMPARE(four_text, one_text);
    const QStringList boxes = four.value(QStringLiteral("assets.box_front")).toStringList();
    QCOMPARE(boxes.size(), 2);
    QVERIFY(boxes.at(0).contains(QLatin1String("/covers/")));
    QVERIFY(boxes.at(1).contains(QLatin1String("/3dboxes/")));
    int files = 0;
    for (const char* folder : { "covers", "3dboxes", "backcovers", "physicalmedia", "screenshots",
                                "titlescreens", "fanart", "marquees", "miximages" }) {
        const QString path = four_dir.path() + QStringLiteral("/snes/") + QLatin1String(folder)
                           + QStringLiteral("/Chrono Trigger (USA).png");
        QVERIFY2(QFileInfo(path).size() > 0, qPrintable(path));
        files++;
    }
    QCOMPARE(files, 9);
}

// Stop while pictures are on their way: they end at once, and it goes on to saving
void test_Scraper::stop_during_downloads()
{
    FakeServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QTemporaryDir dir;
    scraper::Scraper s;
    prepare(s);
    s.m_host_limits.insert(QStringLiteral("127.0.0.1"), 4);
    std::vector<scraper::Media> media = nine(server.base());
    for (scraper::Media& m : media)
        m.url = QUrl(m.url.toString().replace(QStringLiteral("/img/"), QStringLiteral("/never/")));
    s.m_running = true;
    QElapsedTimer t;
    QTimer::singleShot(500, &s, [&s, &t]{ t.start(); s.stop(); });
    int most = 0;
    QVariantMap changes;
    downloadAll(s, dir.path(), media, &most, &changes);
    qInfo() << "stopped: done" << t.elapsed() << "ms after Stop";
    QVERIFY(t.elapsed() < 3000);
    QVERIFY(changes.isEmpty());
    QCOMPARE(server.requests.size(), 4);  // the first four only: nothing new started after Stop
}


// Pegasus X: a source's own ESRB value -> x-esrb (only the seven it takes)
void test_Scraper::esrb_codes_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<QString>("code");
    // ScreenScraper's (classificationsListe: EC, E, E10+, T, M, AO, KA, NOT RATED)
    QTest::newRow("ss EC") << QStringLiteral("EC") << QStringLiteral("EC");
    QTest::newRow("ss E") << QStringLiteral("E") << QStringLiteral("E");
    QTest::newRow("ss E10+") << QStringLiteral("E10+") << QStringLiteral("E10+");
    QTest::newRow("ss T") << QStringLiteral("T") << QStringLiteral("T");
    QTest::newRow("ss M") << QStringLiteral("M") << QStringLiteral("M");
    QTest::newRow("ss AO") << QStringLiteral("AO") << QStringLiteral("AO");
    QTest::newRow("ss KA") << QStringLiteral("KA") << QString();
    QTest::newRow("ss NOT RATED") << QStringLiteral("NOT RATED") << QString();
    // LaunchBox's (the code, " - ", the name)
    QTest::newRow("lb E") << QStringLiteral("E - Everyone") << QStringLiteral("E");
    QTest::newRow("lb E10+") << QStringLiteral("E10+ - Everyone 10+") << QStringLiteral("E10+");
    QTest::newRow("lb T") << QStringLiteral("T - Teen") << QStringLiteral("T");
    QTest::newRow("lb M") << QStringLiteral("M - Mature") << QStringLiteral("M");
    QTest::newRow("lb AO") << QStringLiteral("AO - Adults Only") << QStringLiteral("AO");
    QTest::newRow("lb EC") << QStringLiteral("EC - Early Childhood") << QStringLiteral("EC");
    QTest::newRow("lb RP") << QStringLiteral("RP - Rating Pending") << QStringLiteral("RP");
    QTest::newRow("lb K-A") << QStringLiteral("K-A - Kids to Adults") << QString();
    QTest::newRow("lb Not Rated") << QStringLiteral("Not Rated") << QString();
    // nothing else: blank, PEGI, ages, a score, a name alone
    QTest::newRow("blank") << QString() << QString();
    QTest::newRow("spaces") << QStringLiteral("   ") << QString();
    QTest::newRow("pegi") << QStringLiteral("PEGI 12") << QString();
    QTest::newRow("age") << QStringLiteral("12") << QString();
    QTest::newRow("age plus") << QStringLiteral("10+") << QString();
    QTest::newRow("name alone") << QStringLiteral("Everyone") << QString();
    QTest::newRow("score") << QStringLiteral("0.85") << QString();
}

void test_Scraper::esrb_codes()
{
    QFETCH(QString, value);
    QFETCH(QString, code);
    QCOMPARE(scraper::Scraper::esrbCode(value), code);
}

// ScreenScraper's jeuInfos answer (jeu.classifications: [{ type, text }])
void test_Scraper::screenscraper_esrb()
{
    const QByteArray e10 = R"({ "id": "3456", "noms": [ { "region": "ss", "text": "Some Game" } ], "note": { "text": "16" },
        "classifications": [ { "type": "PEGI", "text": "7" }, { "type": "ESRB", "text": "E10+" } ], "medias": [] })";
    // E10+ -> x-esrb: E10+; the review score as before (16/20 -> 80%)
    QVariantMap c = readScreenScraper(e10, false, false);
    QCOMPARE(c.value(QStringLiteral("x-esrb")).toString(), QStringLiteral("E10+"));
    QCOMPARE(c.value(QStringLiteral("rating")).toString(), QStringLiteral("80%"));
    // KA -> nothing
    c = readScreenScraper(R"({ "id": "1", "classifications": [ { "type": "ESRB", "text": "KA" } ], "medias": [] })", false, false);
    QVERIFY(!c.contains(QStringLiteral("x-esrb")));
    // no ESRB entry (a PEGI one only) -> nothing, never from PEGI
    c = readScreenScraper(R"({ "id": "1", "classifications": [ { "type": "PEGI", "text": "18" } ], "medias": [] })", false, false);
    QVERIFY(!c.contains(QStringLiteral("x-esrb")));
    // no classifications at all, and a score: nothing from the score
    c = readScreenScraper(R"({ "id": "1", "note": { "text": "19" }, "medias": [] })", false, false);
    QVERIFY(!c.contains(QStringLiteral("x-esrb")));
    QCOMPARE(c.value(QStringLiteral("rating")).toString(), QStringLiteral("95%"));
    // an x-esrb the game has (typed by hand) is kept...
    c = readScreenScraper(e10, true, false);
    QVERIFY(!c.contains(QStringLiteral("x-esrb")));
    // ...unless Overwrite
    c = readScreenScraper(e10, true, true);
    QCOMPARE(c.value(QStringLiteral("x-esrb")).toString(), QStringLiteral("E10+"));
    // Details off: nothing
    c = readScreenScraper(e10, false, false, false);
    QVERIFY(!c.contains(QStringLiteral("x-esrb")));
}

// LaunchBox's Metadata.xml ESRB element, from its index
void test_Scraper::launchbox_esrb()
{
    QVariantMap c = readLaunchBox(QStringLiteral("E - Everyone"), false, false);
    QCOMPARE(c.value(QStringLiteral("x-esrb")).toString(), QStringLiteral("E"));
    QCOMPARE(c.value(QStringLiteral("rating")).toString(), QStringLiteral("86%"));  // (as before)
    QVERIFY(!readLaunchBox(QStringLiteral("Not Rated"), false, false).contains(QStringLiteral("x-esrb")));
    QVERIFY(!readLaunchBox(QString(), false, false).contains(QStringLiteral("x-esrb")));
    QVERIFY(!readLaunchBox(QStringLiteral("M - Mature"), true, false).contains(QStringLiteral("x-esrb")));
    QCOMPARE(readLaunchBox(QStringLiteral("M - Mature"), true, true).value(QStringLiteral("x-esrb")).toString(), QStringLiteral("M"));
}


QTEST_MAIN(test_Scraper)
#include "test_Scraper.moc"
