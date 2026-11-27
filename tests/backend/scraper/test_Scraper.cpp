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
#include "EsrbDescriptors.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSignalSpy>
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

// Pegasus X: the ESRB server (server/esrb-worker) as far as the scraper uses
// it: POST /v1/esrb, { games: [...] } -> { results: [...] }. Games by name:
//   "Later Game"  "later" the first time it's asked, "ok" after
//   "No Such Game" "none";  "Wrong Letter" "mismatch" (IGDB: M)
//   "Super Mario World" "ok" with no descriptors
//   "Purchases Game" "ok" with an interactive element
//   anything else "ok" with "Mild Violence"; igdbId 26192 "ok" (two descriptors)
// The first `busy_first` requests get 429 (Retry-After: 1); `always` answers
// every request with that status instead (503: busy).
class FakeEsrbServer : public QTcpServer {
public:
    QList<QJsonArray> batches;       // what each request asked (the answered ones)
    QList<int> statuses;             // what each request got
    QList<qint64> when;              // ms since start, each request
    QElapsedTimer clock;
    int busy_first = 0;
    int always = 0;
    QHash<QString, int> asked;       // name -> times asked

    FakeEsrbServer() {
        clock.start();
        connect(this, &QTcpServer::newConnection, this, [this]{
            while (QTcpSocket* const socket = nextPendingConnection())
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]{ onData(socket); });
        });
    }
    QString url() const { return QStringLiteral("http://127.0.0.1:%1/v1/esrb").arg(serverPort()); }

private:
    void answer(QTcpSocket* socket, int status, const QByteArray& body, const QByteArray& extra = QByteArray()) {
        socket->write("HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: application/json\r\nConnection: close\r\n"
                      + extra + "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        socket->disconnectFromHost();
    }
    void onData(QTcpSocket* socket) {
        QByteArray data = socket->property("data").toByteArray() + socket->readAll();
        socket->setProperty("data", data);
        const int head_end = data.indexOf("\r\n\r\n");
        if (head_end < 0)
            return;
        const QByteArray head = data.left(head_end);
        const QRegularExpressionMatch m = QRegularExpression(QStringLiteral("Content-Length: *(\\d+)"),
            QRegularExpression::CaseInsensitiveOption).match(QString::fromLatin1(head));
        const int length = m.hasMatch() ? m.captured(1).toInt() : 0;
        if (data.size() < head_end + 4 + length || socket->property("seen").toBool())
            return;
        socket->setProperty("seen", true);
        when.append(clock.elapsed());
        if (!head.startsWith("POST /v1/esrb ")) {
            statuses.append(404);
            answer(socket, 404, "{\"error\":\"not found\"}");
            return;
        }
        if (always > 0) {
            statuses.append(always);
            answer(socket, always, "{\"error\":\"later\",\"retryAfter\":1}", "Retry-After: 1\r\n");
            return;
        }
        if (busy_first > 0) {
            busy_first--;
            statuses.append(429);
            answer(socket, 429, "{\"error\":\"later\",\"retryAfter\":1}", "Retry-After: 1\r\n");
            return;
        }
        const QJsonArray games = QJsonDocument::fromJson(data.mid(head_end + 4, length)).object()
            .value(QStringLiteral("games")).toArray();
        batches.append(games);
        statuses.append(200);
        QJsonArray results;
        for (const QJsonValue& v : games) {
            const QJsonObject g = v.toObject();
            const QString name = g.value(QStringLiteral("name")).toString();
            const int id = g.value(QStringLiteral("igdbId")).toInt();
            asked[id > 0 ? QString::number(id) : name]++;
            const QStringList one { QStringLiteral("Mild Violence") };
            QJsonObject r { { QStringLiteral("status"), QStringLiteral("ok") }, { QStringLiteral("igdbId"), 1000 + results.size() },
                            { QStringLiteral("esrb"), g.value(QStringLiteral("esrb")) },
                            { QStringLiteral("descriptors"), QJsonArray::fromStringList(one) },
                            { QStringLiteral("interactive"), QJsonArray() } };
            if (id == 26192)
                r[QStringLiteral("descriptors")] = QJsonArray { QStringLiteral("Blood and Gore"), QStringLiteral("Intense Violence") };
            else if (name == QLatin1String("Later Game") && asked.value(name) == 1)
                r = QJsonObject { { QStringLiteral("status"), QStringLiteral("later") }, { QStringLiteral("retryAfter"), 1 },
                                  { QStringLiteral("reason"), QStringLiteral("IGDB: 4 requests this second") } };
            else if (name == QLatin1String("No Such Game"))
                r = QJsonObject { { QStringLiteral("status"), QStringLiteral("none") } };
            else if (name == QLatin1String("Wrong Letter"))
                r = QJsonObject { { QStringLiteral("status"), QStringLiteral("mismatch") }, { QStringLiteral("igdbId"), 77 },
                                  { QStringLiteral("igdbEsrb"), QStringLiteral("M") } };
            else if (name == QLatin1String("Super Mario World"))
                r[QStringLiteral("descriptors")] = QJsonArray();
            else if (name == QLatin1String("Purchases Game"))
                r[QStringLiteral("interactive")] = QJsonArray { QStringLiteral("In-Game Purchases") };
            results.append(r);
        }
        answer(socket, 200, QJsonDocument(QJsonObject { { QStringLiteral("results"), results } }).toJson(QJsonDocument::Compact));
    }
};

scraper::Job esrb_job(const QString& title, const QString& system, int year, const QString& esrb,
                      bool has_descriptors = false, int igdb_id = 0)
{
    scraper::Job job;
    job.collection = system == QLatin1String("snes") ? QStringLiteral("Nintendo SNES") : QStringLiteral("Nintendo 64");
    job.media_folder = system;
    job.system = system;
    job.title = title;
    job.path = QStringLiteral("/roms/%1/%2.rom").arg(system, title);
    job.year = year;
    job.esrb = esrb;
    job.has_esrb = !esrb.isEmpty();
    job.has_esrb_descriptors = has_descriptors;
    job.igdb_id = igdb_id;
    return job;
}
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
    void esrb_platforms_match_the_server();
    void esrb_endpoint();
    void esrb_what_is_asked();
    void esrb_results();
    void esrb_step_against_a_server();
    void esrb_step_busy_server_gives_up();
    void esrb_step_stop_while_waiting();
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


// -- Pegasus X: the ESRB descriptors step --------------------------------------

// the platform table is server/esrb-worker/worker.js's PLATFORMS, name for name
void test_Scraper::esrb_platforms_match_the_server()
{
#ifndef PEGASUSX_WORKER_JS
    QSKIP("PEGASUSX_WORKER_JS isn't set (qmake sets it)");
#else
    QFile file(QStringLiteral(PEGASUSX_WORKER_JS));
    QVERIFY2(file.open(QIODevice::ReadOnly), PEGASUSX_WORKER_JS);
    const QString js = QString::fromUtf8(file.readAll());
    const int from = js.indexOf(QLatin1String("const PLATFORMS = {"));
    QVERIFY(from >= 0);
    const int to = js.indexOf(QLatin1String("};"), from);
    QHash<QString, int> theirs;
    QRegularExpressionMatchIterator it = QRegularExpression(QStringLiteral("\"([a-z0-9-]+)\":\\s*(\\d+)"))
        .globalMatch(js.mid(from, to - from));
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        theirs.insert(m.captured(1), m.captured(2).toInt());
    }
    qInfo() << "worker.js:" << theirs.size() << "platforms";
    QVERIFY(theirs.size() > 100);
    for (auto t = theirs.cbegin(); t != theirs.cend(); ++t)
        QVERIFY2(scraper::esrb::igdb_platform(t.key()) == t.value(), qPrintable(t.key()));
    QCOMPARE(scraper::esrb::platforms().size(), theirs.size());
    QCOMPARE(scraper::esrb::igdb_platform(QStringLiteral("N64")), 4);
    QCOMPARE(scraper::esrb::igdb_platform(QStringLiteral("ps5")), 167);
    QCOMPARE(scraper::esrb::igdb_platform(QStringLiteral("nosuchsystem")), 0);
#endif
}

void test_Scraper::esrb_endpoint()
{
    using scraper::esrb::endpoint;
    QCOMPARE(endpoint(QString()).toString(), QStringLiteral("https://pegasusx-esrb.pegasusx.workers.dev/v1/esrb"));
    QCOMPARE(endpoint(QStringLiteral(" https://pegasusx-esrb.pegasusx.workers.dev/v1/esrb ")).toString(),
             QStringLiteral("https://pegasusx-esrb.pegasusx.workers.dev/v1/esrb"));
    QCOMPARE(endpoint(QStringLiteral("https://esrb.example.dev")).toString(), QStringLiteral("https://esrb.example.dev/v1/esrb"));
    QCOMPARE(endpoint(QStringLiteral("https://esrb.example.dev/?x=1#y")).toString(), QStringLiteral("https://esrb.example.dev/v1/esrb"));
    QVERIFY(!endpoint(QStringLiteral("http://esrb.example.dev/v1/esrb")).isValid());   // not https
    QVERIFY(!endpoint(QStringLiteral("ftp://esrb.example.dev")).isValid());
    QVERIFY(!endpoint(QStringLiteral("https://user:pw@esrb.example.dev")).isValid());
    QVERIFY(!endpoint(QStringLiteral("not an address")).isValid());
    QVERIFY(endpoint(QStringLiteral("http://127.0.0.1:8080/v1/esrb")).isValid());       // (tests)
}

void test_Scraper::esrb_what_is_asked()
{
    using namespace scraper::esrb;
    Game g;
    g.title = g.name = QStringLiteral("GoldenEye 007");
    g.letter = scraper::Scraper::esrbCode(QStringLiteral("T - Teen"));
    g.platform = igdb_platform(QStringLiteral("n64"));
    g.year = 1997;
    QCOMPARE(why_not(g), QString());
    const QJsonObject by_name = request_entry(g);
    QCOMPARE(by_name.value(QStringLiteral("name")).toString(), QStringLiteral("GoldenEye 007"));
    QCOMPARE(by_name.value(QStringLiteral("platform")).toInt(), 4);
    QCOMPARE(by_name.value(QStringLiteral("year")).toInt(), 1997);
    QCOMPARE(by_name.value(QStringLiteral("esrb")).toString(), QStringLiteral("T"));
    QVERIFY(!by_name.contains(QStringLiteral("igdbId")));
    // IGDB's id when known: that and the letter only
    g.igdb_id = 1638;
    const QJsonObject by_id = request_entry(g);
    QCOMPARE(by_id.value(QStringLiteral("igdbId")).toInt(), 1638);
    QCOMPARE(by_id.keys(), (QStringList { QStringLiteral("esrb"), QStringLiteral("igdbId") }));
    g.igdb_id = 0;
    // what can't be asked
    Game no_year = g; no_year.year = 0;
    QVERIFY(!why_not(no_year).isEmpty());
    Game no_platform = g; no_platform.platform = 0;
    QVERIFY(!why_not(no_platform).isEmpty());
    Game quote = g; quote.name = QStringLiteral("Say \"Hi\"");
    QVERIFY(!why_not(quote).isEmpty());
    Game no_letter = g; no_letter.letter = scraper::Scraper::esrbCode(QStringLiteral("Mature"));
    QVERIFY(!why_not(no_letter).isEmpty());
    Game known = no_year; known.igdb_id = 5;  // by id: no year or platform needed
    QCOMPARE(why_not(known), QString());
}

void test_Scraper::esrb_results()
{
    using namespace scraper::esrb;
    Game g;
    g.title = g.name = QStringLiteral("The Last of Us Part II");
    g.letter = QStringLiteral("M");
    const auto read = [&g](const QByteArray& json, bool overwrite) {
        return read_result(QJsonDocument::fromJson(json).object(), g, overwrite);
    };
    // ok: the descriptors, joined with ", "; no interactive list: no x-esrb-interactive
    Outcome o = read(R"({"status":"ok","igdbId":26192,"esrb":"M","descriptors":["Blood and Gore","Intense Violence","Strong Language"],"interactive":[]})", false);
    QCOMPARE(o.kind, int(Outcome::Write));
    QCOMPARE(o.changes.value(QStringLiteral("x-esrb-descriptors")).toString(), QStringLiteral("Blood and Gore, Intense Violence, Strong Language"));
    QCOMPARE(o.changes.keys(), QStringList { QStringLiteral("x-esrb-descriptors") });   // never the letter or anything else
    QVERIFY(o.log.contains(QLatin1String("26192")));
    // ...with interactive elements: both lines
    o = read(R"({"status":"ok","igdbId":1,"descriptors":["Violence"],"interactive":["In-Game Purchases","Users Interact"]})", false);
    QCOMPARE(o.changes.value(QStringLiteral("x-esrb-interactive")).toString(), QStringLiteral("In-Game Purchases, Users Interact"));
    // ok with nothing in it: nothing written
    o = read(R"({"status":"ok","igdbId":1070,"descriptors":[],"interactive":[]})", false);
    QCOMPARE(o.kind, int(Outcome::Nothing));
    QVERIFY(o.changes.isEmpty());
    // none, mismatch, invalid, something unknown: nothing, the reason in the log
    o = read(R"({"status":"none"})", false);
    QCOMPARE(o.kind, int(Outcome::Nothing));
    QVERIFY(o.changes.isEmpty() && o.log.startsWith(QLatin1String("none")));
    o = read(R"({"status":"mismatch","igdbId":5,"igdbEsrb":"T","descriptors":["Blood"]})", false);
    QCOMPARE(o.kind, int(Outcome::Nothing));
    QVERIFY(o.changes.isEmpty() && o.log.contains(QLatin1String("T")) && o.log.contains(QLatin1String("M")));
    o = read(R"({"status":"invalid","reason":"year"})", false);
    QVERIFY(o.changes.isEmpty() && o.log.contains(QLatin1String("year")));
    o = read(R"({"status":"maybe","descriptors":["Blood"]})", false);
    QVERIFY(o.changes.isEmpty() && o.kind == Outcome::Nothing);
    // later: asked again, after the wait it says
    o = read(R"({"status":"later","retryAfter":7,"reason":"IGDB: 8 requests open"})", false);
    QCOMPARE(o.kind, int(Outcome::Later));
    QCOMPARE(o.retry_after, 7);
    QVERIFY(o.changes.isEmpty());
    o = read(R"({"status":"later","retryAfter":9999})", false);
    QCOMPARE(o.retry_after, int(MAX_WAIT_S));
    // a line the game has (typed by hand): kept...
    g.has_descriptors = true;
    g.has_interactive = true;
    o = read(R"({"status":"ok","descriptors":["Blood"],"interactive":["Users Interact"]})", false);
    QCOMPARE(o.kind, int(Outcome::Nothing));
    QVERIFY(o.changes.isEmpty());
    QVERIFY(o.log.contains(QLatin1String("kept")));
    // ...unless Overwrite
    o = read(R"({"status":"ok","descriptors":["Blood"],"interactive":["Users Interact"]})", true);
    QCOMPARE(o.changes.value(QStringLiteral("x-esrb-descriptors")).toString(), QStringLiteral("Blood"));
    QCOMPARE(o.changes.value(QStringLiteral("x-esrb-interactive")).toString(), QStringLiteral("Users Interact"));
    // one typed, the other not: only the empty one
    g.has_interactive = false;
    o = read(R"({"status":"ok","descriptors":["Blood"],"interactive":["Users Interact"]})", false);
    QCOMPARE(o.changes.keys(), QStringList { QStringLiteral("x-esrb-interactive") });
    // nothing that could break a line of the file
    g.has_descriptors = g.has_interactive = false;
    o = read("{\"status\":\"ok\",\"descriptors\":[\"Blood\\nx-esrb: AO\",\"  Violence  \",\"\",\"Violence\"]}", false);
    QCOMPARE(o.changes.value(QStringLiteral("x-esrb-descriptors")).toString(), QStringLiteral("Blood x-esrb: AO, Violence"));
    // the server's wait: its header first, then its body, at most MAX_WAIT_S
    QCOMPARE(retry_after("12", QJsonObject { { QStringLiteral("retryAfter"), 3 } }, 5), 12);
    QCOMPARE(retry_after(QByteArray(), QJsonObject { { QStringLiteral("retryAfter"), 3 } }, 5), 3);
    QCOMPARE(retry_after("soon", QJsonObject(), 5), 5);
    QCOMPARE(retry_after("100000", QJsonObject(), 5), int(MAX_WAIT_S));
}

// the step alone (Get ESRB descriptors now) against a server here: 10 games a
// request, spaced; a 429 waits and asks again; "later" is asked again; a line
// the game has, no letter, a letter it doesn't take: not asked; only
// x-esrb-descriptors / x-esrb-interactive saved
void test_Scraper::esrb_step_against_a_server()
{
    FakeEsrbServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.busy_first = 1;
    scraper::Scraper s;
    s.m_esrb_spacing_ms = 300;
    s.m_esrb_second_ms = 100;
    QHash<QString, QVariantMap> saved;
    QStringList saved_collections;
    s.m_esrb_save_hook = [&](const QString& collection, const std::vector<scraper::EsrbEdit>& edits) {
        saved_collections.append(collection);
        for (const scraper::EsrbEdit& e : edits)
            saved.insert(e.title, e.changes);
        return QString();
    };
    std::vector<scraper::Job> jobs {
        esrb_job(QStringLiteral("The Last of Us Part II"), QStringLiteral("n64"), 0, QStringLiteral("M"), false, 26192),
        esrb_job(QStringLiteral("GoldenEye 007"), QStringLiteral("n64"), 1997, QStringLiteral("T - Teen")),
        esrb_job(QStringLiteral("Super Mario World"), QStringLiteral("snes"), 1991, QStringLiteral("E")),
        esrb_job(QStringLiteral("Wrong Letter"), QStringLiteral("snes"), 1993, QStringLiteral("T")),
        esrb_job(QStringLiteral("No Such Game"), QStringLiteral("snes"), 1993, QStringLiteral("E")),
        esrb_job(QStringLiteral("Later Game"), QStringLiteral("snes"), 1994, QStringLiteral("E")),
        esrb_job(QStringLiteral("Typed By Hand"), QStringLiteral("snes"), 1994, QStringLiteral("E"), true),
        esrb_job(QStringLiteral("No Letter"), QStringLiteral("snes"), 1994, QString()),
        esrb_job(QStringLiteral("Odd Letter"), QStringLiteral("snes"), 1994, QStringLiteral("Mature")),
        esrb_job(QStringLiteral("No Year"), QStringLiteral("snes"), 0, QStringLiteral("E")),
        esrb_job(QStringLiteral("Purchases Game"), QStringLiteral("n64"), 1999, QStringLiteral("E")),
    };
    for (int i = 1; i <= 4; i++)
        jobs.push_back(esrb_job(QStringLiteral("Game %1").arg(i), QStringLiteral("n64"), 1998, QStringLiteral("E10+")));
    const QVariantMap settings {
        { QStringLiteral("esrbOnly"), true }, { QStringLiteral("esrbServer"), server.url() },
        { QStringLiteral("overwrite"), false },
    };
    QSignalSpy finished(&s, &scraper::Scraper::finished);
    QElapsedTimer t;
    t.start();
    s.start(jobs, settings, QString(), QString(), QDir::tempPath());
    QTRY_VERIFY_WITH_TIMEOUT(!s.running(), 30000);
    qInfo() << "ESRB step:" << t.elapsed() << "ms," << server.statuses << "at" << server.when;
    QCOMPARE(finished.count(), 1);

    // asked: 11 games (not the typed one, no letter, an odd letter, no year), 10 at most a request
    QStringList asked = server.asked.keys();
    asked.sort();
    QCOMPARE(asked, (QStringList { QStringLiteral("26192"), QStringLiteral("Game 1"), QStringLiteral("Game 2"), QStringLiteral("Game 3"),
                                   QStringLiteral("Game 4"), QStringLiteral("GoldenEye 007"), QStringLiteral("Later Game"),
                                   QStringLiteral("No Such Game"), QStringLiteral("Purchases Game"), QStringLiteral("Super Mario World"),
                                   QStringLiteral("Wrong Letter") }));
    QCOMPARE(server.asked.value(QStringLiteral("Later Game")), 2);
    for (const QJsonArray& b : server.batches)
        QVERIFY(b.size() <= 10);
    QCOMPARE(server.batches.first().size(), 10);
    // the 429 first, then the same games again; never two requests closer than the spacing
    QCOMPARE(server.statuses.first(), 429);
    for (int i = 1; i < server.when.size(); i++)
        QVERIFY2(server.when.at(i) - server.when.at(i - 1) >= 280, qPrintable(QString::number(server.when.at(i) - server.when.at(i - 1))));
    // the letter as the server takes it ("T - Teen" -> T), by name + IGDB platform + year
    bool goldeneye = false;
    for (const QJsonArray& b : server.batches) {
        for (const QJsonValue& v : b) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("name")).toString() == QLatin1String("GoldenEye 007")) {
                goldeneye = true;
                QCOMPARE(o.value(QStringLiteral("esrb")).toString(), QStringLiteral("T"));
                QCOMPARE(o.value(QStringLiteral("platform")).toInt(), 4);
                QCOMPARE(o.value(QStringLiteral("year")).toInt(), 1997);
            }
        }
    }
    QVERIFY(goldeneye);

    // saved: ok with descriptors only; nothing but the two lines
    QCOMPARE(saved.value(QStringLiteral("The Last of Us Part II")).value(QStringLiteral("x-esrb-descriptors")).toString(),
             QStringLiteral("Blood and Gore, Intense Violence"));
    QCOMPARE(saved.value(QStringLiteral("GoldenEye 007")).value(QStringLiteral("x-esrb-descriptors")).toString(), QStringLiteral("Mild Violence"));
    QCOMPARE(saved.value(QStringLiteral("Later Game")).value(QStringLiteral("x-esrb-descriptors")).toString(), QStringLiteral("Mild Violence"));
    QCOMPARE(saved.value(QStringLiteral("Purchases Game")).value(QStringLiteral("x-esrb-interactive")).toString(), QStringLiteral("In-Game Purchases"));
    QVERIFY(!saved.value(QStringLiteral("GoldenEye 007")).contains(QStringLiteral("x-esrb-interactive")));
    for (const char* none : { "Super Mario World", "Wrong Letter", "No Such Game", "Typed By Hand", "No Letter", "Odd Letter", "No Year" })
        QVERIFY2(!saved.contains(QLatin1String(none)), none);
    for (auto it = saved.cbegin(); it != saved.cend(); ++it) {
        for (const QString& key : it.value().keys())
            QVERIFY2(key == QLatin1String("x-esrb-descriptors") || key == QLatin1String("x-esrb-interactive"), qPrintable(key));
    }
    QVERIFY(saved_collections.contains(QStringLiteral("Nintendo 64")));
    QCOMPARE(saved.size(), 8);   // TLOU, GoldenEye, Later, Purchases, Game 1-4

    // the counts (alone: the run's)
    const QVariantMap state = s.state();
    QCOMPARE(state.value(QStringLiteral("saved")).toInt(), 8);
    QCOMPARE(state.value(QStringLiteral("notFound")).toInt(), 3);   // Super Mario World, Wrong Letter, No Such Game
    QCOMPARE(state.value(QStringLiteral("failed")).toInt(), 0);
    QCOMPARE(state.value(QStringLiteral("done")).toInt(), 11);
    QCOMPARE(state.value(QStringLiteral("total")).toInt(), 11);
    QVERIFY(state.value(QStringLiteral("stopReason")).toString().isEmpty());
    // the log says each game's result
    const QString log = s.log().join(QLatin1Char('\n'));
    QVERIFY(log.contains(QLatin1String("\"No Such Game\": none")));
    QVERIFY(log.contains(QLatin1String("\"Wrong Letter\": mismatch")));
    QVERIFY(log.contains(QLatin1String("\"Later Game\": later")));
    QVERIFY(log.contains(QLatin1String("have an x-esrb-descriptors line already")));
}

// a server that's always busy: asked again a few times, longer each time, then
// the step ends (the games left for the next run), the run not failed
void test_Scraper::esrb_step_busy_server_gives_up()
{
    FakeEsrbServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.always = 503;
    scraper::Scraper s;
    s.m_esrb_spacing_ms = 50;
    s.m_esrb_second_ms = 10;
    bool saved = false;
    s.m_esrb_save_hook = [&](const QString&, const std::vector<scraper::EsrbEdit>&) { saved = true; return QString(); };
    std::vector<scraper::Job> jobs;
    for (int i = 1; i <= 3; i++)
        jobs.push_back(esrb_job(QStringLiteral("Game %1").arg(i), QStringLiteral("n64"), 1998, QStringLiteral("T")));
    s.start(jobs, QVariantMap { { QStringLiteral("esrbOnly"), true }, { QStringLiteral("esrbServer"), server.url() } },
            QString(), QString(), QDir::tempPath());
    QTRY_VERIFY_WITH_TIMEOUT(!s.running(), 30000);
    qInfo() << "busy server:" << server.statuses.size() << "requests at" << server.when;
    QCOMPARE(server.statuses.size(), int(scraper::esrb::FAILURES));
    // the waits grow (5, 10, 20... server seconds)
    QVERIFY(server.when.last() - server.when.at(server.when.size() - 2) > server.when.at(1) - server.when.at(0));
    QVERIFY(!saved);
    const QVariantMap state = s.state();
    QCOMPARE(state.value(QStringLiteral("failed")).toInt(), 3);   // left for the next run
    QVERIFY(!state.value(QStringLiteral("esrbMessage")).toString().isEmpty());
    QVERIFY(state.value(QStringLiteral("stopReason")).toString().isEmpty());
}

// Stop while it waits to ask again: it ends now, not after the wait
void test_Scraper::esrb_step_stop_while_waiting()
{
    FakeEsrbServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.always = 429;
    scraper::Scraper s;
    s.m_esrb_spacing_ms = 50;   // (a second of the server's: a real one, 60 s waits)
    std::vector<scraper::Job> jobs { esrb_job(QStringLiteral("Game"), QStringLiteral("n64"), 1998, QStringLiteral("T")) };
    s.start(jobs, QVariantMap { { QStringLiteral("esrbOnly"), true }, { QStringLiteral("esrbServer"), server.url() } },
            QString(), QString(), QDir::tempPath());
    QTRY_COMPARE_WITH_TIMEOUT(server.statuses.size(), 1, 10000);
    QTest::qWait(200);
    QElapsedTimer t;
    t.start();
    s.stop();
    QTRY_VERIFY_WITH_TIMEOUT(!s.running(), 5000);
    QVERIFY(t.elapsed() < 2000);
    QCOMPARE(server.statuses.size(), 1);
    QCOMPARE(s.state().value(QStringLiteral("failed")).toInt(), 1);
}


QTEST_MAIN(test_Scraper)
#include "test_Scraper.moc"
