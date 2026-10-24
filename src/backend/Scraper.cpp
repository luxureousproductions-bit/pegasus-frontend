// Pegasus Frontend
// Copyright (C) 2017-2018  Mátyás Mustoha
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


#include "Scraper.h"

#include "Log.h"
#ifdef WITH_COMPAT_ESDE
#include "providers/esde/EsdeProvider.h"
#endif

#include <QBuffer>
#include <QImage>
#include <QPair>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringBuilder>
#include <QTimer>
#include <QUrlQuery>
#include <QtConcurrent/QtConcurrent>


namespace {
const QString LOG_TAG = QStringLiteral("Scraper");

// ES-DE system -> ScreenScraper system id, from ES-DE's own tables
// (es-app/src/scrapers/ScreenScraper.cpp and PlatformId.cpp, MIT license)
const QHash<QString, int>& screenscraper_systems()
{
    static const QHash<QString, int> map {
    { "3do", 29 },
    { "adam", 89 },
    { "ags", 138 },
    { "amiga", 64 },
    { "amiga1200", 64 },
    { "amiga600", 64 },
    { "amigacd32", 130 },
    { "amstradcpc", 65 },
    { "android", 63 },
    { "androidapps", 63 },
    { "androidgames", 63 },
    { "apple2", 86 },
    { "apple2gs", 217 },
    { "arcade", 75 },
    { "arcadia", 94 },
    { "archimedes", 84 },
    { "arduboy", 263 },
    { "astrocde", 44 },
    { "atari2600", 26 },
    { "atari5200", 40 },
    { "atari7800", 41 },
    { "atari800", 43 },
    { "atarijaguar", 27 },
    { "atarijaguarcd", 171 },
    { "atarilynx", 28 },
    { "atarist", 42 },
    { "atarixe", 43 },
    { "atomiswave", 75 },
    { "bbcmicro", 37 },
    { "c64", 66 },
    { "cdimono1", 133 },
    { "cdtv", 129 },
    { "channelf", 80 },
    { "coco", 144 },
    { "colecovision", 48 },
    { "consolearcade", 75 },
    { "cps", 75 },
    { "cps1", 75 },
    { "cps2", 75 },
    { "cps3", 75 },
    { "crvision", 241 },
    { "daphne", 49 },
    { "desktop", 138 },
    { "doom", 135 },
    { "dos", 135 },
    { "dragon32", 91 },
    { "dreamcast", 23 },
    { "easyrpg", 231 },
    { "electron", 85 },
    { "emulators", 63 },
    { "epic", 63 },
    { "famicom", 3 },
    { "fba", 75 },
    { "fbneo", 75 },
    { "fds", 106 },
    { "fm7", 97 },
    { "fmtowns", 253 },
    { "fpinball", 199 },
    { "gamate", 266 },
    { "gameandwatch", 52 },
    { "gamecom", 121 },
    { "gamegear", 21 },
    { "gb", 9 },
    { "gba", 12 },
    { "gbc", 10 },
    { "gc", 13 },
    { "genesis", 1 },
    { "gmaster", 103 },
    { "gx4000", 87 },
    { "intellivision", 115 },
    { "j2me", 302 },
    { "kodi", 138 },
    { "laserdisc", 49 },
    { "lcdgames", 75 },
    { "lowresnx", 244 },
    { "lutris", 135 },
    { "lutro", 206 },
    { "macintosh", 146 },
    { "mame", 75 },
    { "mame-advmame", 75 },
    { "mark3", 2 },
    { "mastersystem", 2 },
    { "megacd", 20 },
    { "megacdjp", 20 },
    { "megadrive", 1 },
    { "megadrivejp", 1 },
    { "megaduck", 90 },
    { "model2", 75 },
    { "model3", 75 },
    { "moto", 141 },
    { "msx", 113 },
    { "msx1", 113 },
    { "msx2", 116 },
    { "msxturbor", 118 },
    { "multivision", 109 },
    { "n3ds", 17 },
    { "n64", 14 },
    { "n64dd", 14 },
    { "naomi", 75 },
    { "naomi2", 75 },
    { "naomigd", 75 },
    { "nds", 15 },
    { "neogeo", 142 },
    { "neogeocd", 70 },
    { "neogeocdjp", 70 },
    { "nes", 3 },
    { "ngage", 30 },
    { "ngp", 25 },
    { "ngpc", 82 },
    { "odyssey2", 104 },
    { "openbor", 214 },
    { "oric", 131 },
    { "palm", 219 },
    { "pc", 135 },
    { "pc88", 221 },
    { "pc98", 208 },
    { "pcarcade", 75 },
    { "pcengine", 31 },
    { "pcenginecd", 114 },
    { "pcfx", 72 },
    { "pico8", 234 },
    { "plus4", 99 },
    { "pokemini", 211 },
    { "ports", 135 },
    { "ps2", 58 },
    { "ps3", 59 },
    { "ps4", 60 },
    { "psp", 61 },
    { "psvita", 62 },
    { "psx", 57 },
    { "pv1000", 74 },
    { "quake", 135 },
    { "samcoupe", 213 },
    { "satellaview", 107 },
    { "saturn", 22 },
    { "saturnjp", 22 },
    { "scummvm", 123 },
    { "scv", 67 },
    { "sega32x", 19 },
    { "sega32xjp", 19 },
    { "sega32xna", 19 },
    { "segacd", 20 },
    { "sfc", 4 },
    { "sg-1000", 109 },
    { "sgb", 127 },
    { "snes", 4 },
    { "snesna", 4 },
    { "solarus", 223 },
    { "spectravideo", 218 },
    { "steam", 138 },
    { "stv", 75 },
    { "sufami", 108 },
    { "supergrafx", 105 },
    { "supervision", 207 },
    { "supracan", 100 },
    { "switch", 225 },
    { "symbian", 30 },
    { "tanodragon", 91 },
    { "tg-cd", 114 },
    { "tg16", 31 },
    { "ti99", 205 },
    { "tic80", 222 },
    { "to8", 141 },
    { "triforce", 75 },
    { "trs-80", 144 },
    { "type-x", 75 },
    { "uzebox", 216 },
    { "vectrex", 102 },
    { "vic20", 73 },
    { "videopac", 104 },
    { "vircon32", 272 },
    { "virtualboy", 11 },
    { "vpinball", 198 },
    { "vsmile", 120 },
    { "wasm4", 262 },
    { "wii", 16 },
    { "wiiu", 18 },
    { "windows", 138 },
    { "windows3x", 136 },
    { "windows9x", 138 },
    { "wonderswan", 45 },
    { "wonderswancolor", 46 },
    { "x1", 220 },
    { "x68000", 79 },
    { "xbox", 32 },
    { "xbox360", 33 },
    { "xboxone", 34 },
    { "zmachine", 215 },
    { "zx81", 77 },
    { "zxspectrum", 76 },
    };
    return map;
}

// names people give their collections -> ES-DE's names
const QHash<QString, QString>& common_names()
{
    static const QHash<QString, QString> map {
        {"supernintendo", "snes"}, {"supernes", "snes"}, {"superfamicom", "sfc"},
        {"nintendoentertainmentsystem", "nes"}, {"nintendo64", "n64"}, {"gamecube", "gc"},
        {"ngc", "gc"}, {"nintendogamecube", "gc"}, {"nintendowii", "wii"}, {"nintendowiiu", "wiiu"},
        {"nintendoswitch", "switch"}, {"ns", "switch"}, {"gameboy", "gb"}, {"gameboycolor", "gbc"},
        {"gameboyadvance", "gba"}, {"ds", "nds"}, {"nintendods", "nds"}, {"3ds", "n3ds"},
        {"nintendo3ds", "n3ds"}, {"ps1", "psx"}, {"psone", "psx"}, {"playstation", "psx"},
        {"sonyplaystation", "psx"}, {"playstation1", "psx"}, {"playstation2", "ps2"},
        {"sonyplaystation2", "ps2"}, {"playstation3", "ps3"}, {"sonyplaystation3", "ps3"},
        {"playstationportable", "psp"}, {"vita", "psvita"}, {"psv", "psvita"},
        {"playstationvita", "psvita"}, {"sonyplaystationvita", "psvita"}, {"md", "megadrive"},
        {"segamegadrive", "megadrive"}, {"segagenesis", "genesis"}, {"sms", "mastersystem"},
        {"gg", "gamegear"}, {"32x", "sega32x"}, {"dc", "dreamcast"}, {"segadreamcast", "dreamcast"},
        {"turbografx16", "tg16"}, {"pce", "pcengine"}, {"lynx", "atarilynx"}, {"jaguar", "atarijaguar"},
        {"ws", "wonderswan"}, {"wsc", "wonderswancolor"}, {"msdos", "dos"}, {"pc", "windows"},
        {"microsoftwindows", "windows"}, {"x360", "xbox360"}, {"microsoftxbox360", "xbox360"},
        {"microsoftxbox", "xbox"},
    };
    return map;
}

QString normalized(const QString& text)
{
    QString out;
    for (const QChar c : text.toLower()) {
        if (c.isLetterOrNumber())
            out += c;
    }
    return out;
}

// the best entry of a list for the region / language: yours, then world,
// USA, ScreenScraper's own, Europe, Japan, then whatever is first
QJsonObject pick(const QJsonArray& list, const QString& attribute, const QString& wanted)
{
    const QStringList order { wanted, QStringLiteral("wor"), QStringLiteral("us"), QStringLiteral("en"),
                              QStringLiteral("ss"), QStringLiteral("eu"), QStringLiteral("jp") };
    for (const QString& value : order) {
        for (const QJsonValue& item : list) {
            if (item.toObject().value(attribute).toString() == value)
                return item.toObject();
        }
    }
    return list.isEmpty() ? QJsonObject() : list.first().toObject();
}

QString text_of(const QJsonValue& value)
{
    return value.toObject().value(QStringLiteral("text")).toString().trimmed();
}

// "./Crash Bandicoot (USA) (Disc 1).chd" -> "Crash Bandicoot"
QString search_name_of(const QString& path, const QString& title)
{
    if (path.startsWith(QLatin1String("android:")))
        return title;
    QString name = QFileInfo(path).completeBaseName();
    static const QRegularExpression rx_tags(QStringLiteral("\\s*[\\(\\[][^\\)\\]]*[\\)\\]]"));
    name.remove(rx_tags);
    return name.trimmed().isEmpty() ? title : name.trimmed();
}

QString md5_of(const QString& path, qint64 max_bytes)
{
    QFile file(path);
    if (max_bytes <= 0 || file.size() > max_bytes || !file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Md5);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

// the part of a picture that isn't empty border: by transparency when it has
// any, else by difference from the top-left corner's colour
QRect content_rect(const QImage& source)
{
    const QImage img = source.convertToFormat(QImage::Format_ARGB32);
    const bool alpha = source.hasAlphaChannel();
    const QRgb corner = img.pixel(0, 0);
    const auto empty = [alpha, corner](QRgb p) {
        if (alpha)
            return qAlpha(p) < 10;
        return qAbs(qRed(p) - qRed(corner)) < 24 && qAbs(qGreen(p) - qGreen(corner)) < 24
            && qAbs(qBlue(p) - qBlue(corner)) < 24;
    };
    int left = img.width(), right = -1, top = img.height(), bottom = -1;
    for (int y = 0; y < img.height(); y++) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); x++) {
            if (empty(line[x]))
                continue;
            left = qMin(left, x);
            right = qMax(right, x);
            top = qMin(top, y);
            bottom = qMax(bottom, y);
        }
    }
    if (right < left || bottom < top)
        return {};
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

// a downloaded picture made smaller and/or trimmed, as the Scraper settings
// say; untouched (same bytes) when nothing needs doing
QPair<QByteArray, QString> process_image(const QByteArray& data, const QString& ext, int max_w, int max_h, bool trim)
{
    QImage img;
    if (!img.loadFromData(data))
        return { data, ext };
    bool changed = false;
    if (trim) {
        const QRect box = content_rect(img);
        // only a real border (a few pixels at least), never an empty picture
        if (box.isValid() && box != img.rect() && box.width() > 16 && box.height() > 16) {
            img = img.copy(box);
            changed = true;
        }
    }
    if (max_w > 0 && (img.width() > max_w || img.height() > max_h)) {
        img = img.scaled(max_w, max_h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        changed = true;
    }
    if (!changed)
        return { data, ext };
    const QString out_ext = (img.hasAlphaChannel() || ext == QLatin1String("png")) ? QStringLiteral("png") : QStringLiteral("jpg");
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    if (!img.save(&buffer, out_ext == QLatin1String("png") ? "PNG" : "JPG", out_ext == QLatin1String("jpg") ? 90 : -1))
        return { data, ext };
    return { out, out_ext };
}

QString safe_file_name(QString name)
{
    static const QRegularExpression rx_bad(QStringLiteral("[\\\\/:*?\"<>|]"));
    name.replace(rx_bad, QStringLiteral("_"));
    return name.trimmed().isEmpty() ? QStringLiteral("game") : name.trimmed();
}
} // namespace


namespace scraper {

Scraper::Scraper(QObject* parent)
    : QObject(parent)
{}

QString Scraper::system_of(const QString& collection_name, const QString& short_name)
{
    const auto& systems = screenscraper_systems();
    for (const QString& candidate : { short_name.toLower(), normalized(short_name), normalized(collection_name) }) {
        if (candidate.isEmpty())
            continue;
        if (systems.contains(candidate))
            return candidate;
        const auto alias = common_names().find(candidate);
        if (alias != common_names().cend() && systems.contains(alias.value()))
            return alias.value();
    }
    return {};
}

QVariantMap Scraper::state() const
{
    return QVariantMap {
        { QStringLiteral("running"), m_running },
        { QStringLiteral("done"), static_cast<int>(m_index) },
        { QStringLiteral("total"), static_cast<int>(m_jobs.size()) },
        { QStringLiteral("current"), m_current },
        { QStringLiteral("title"), m_last_title },
        { QStringLiteral("result"), m_last_result },
        { QStringLiteral("ok"), m_last_ok },
        { QStringLiteral("saved"), m_saved },
        { QStringLiteral("notFound"), m_not_found },
        { QStringLiteral("failed"), m_failed },
        { QStringLiteral("stopReason"), m_stop_reason },
        { QStringLiteral("changes"), m_last_changes },
    };
}

void Scraper::start(std::vector<Job> jobs, const QVariantMap& settings,
                    const QString& dev_id, const QString& dev_password, const QString& data_dir)
{
    if (m_running)
        return;
    if (!m_net)
        m_net = new QNetworkAccessManager(this);
    m_jobs = std::move(jobs);
    for (Job& job : m_jobs)
        job.search_name = search_name_of(job.path, job.title);
    m_settings = settings;
    m_dev_id = dev_id;
    m_dev_password = dev_password;
    m_data_dir = data_dir;
    m_index = 0;
    m_saved = m_not_found = m_failed = 0;
    m_last_title.clear();
    m_last_result.clear();
    m_last_changes.clear();
    m_stop_reason.clear();
    m_stop = false;
    m_running = true;
    Log::info(LOG_TAG, LOGMSG("Scraping %1 games into `%2`").arg(m_jobs.size()).arg(m_data_dir));
    report();
    next();
}

void Scraper::stop()
{
    if (!m_running)
        return;
    m_stop = true;
    // waiting for a pick: nothing else will move it on
    if (m_waiting) {
        m_waiting = false;
        next();
    }
}

bool Scraper::interactive() const
{
    return m_settings.value(QStringLiteral("interactive")).toBool();
}

// the matches for a name: ScreenScraper's (with details and art), else
// SteamGridDB's (art only)
void Scraper::search(const QString& name)
{
    const Job& job = m_jobs[m_index];
    const QString source = m_settings.value(QStringLiteral("source")).toString();
    const bool ss_ok = source == QLatin1String("screenscraper") && !m_dev_id.isEmpty()
        && screenscraper_systems().contains(job.system);
    if (ss_ok)
        ssSearch(name);
    else if (!m_settings.value(QStringLiteral("sgdbKey")).toString().trimmed().isEmpty())
        sgdbSearch(name);
    else {
        m_not_found++;
        finishGame(tr("ScreenScraper isn't in this build yet"), false);
    }
}

void Scraper::ssSearch(const QString& name)
{
    const Job& job = m_jobs[m_index];
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("devid"), m_dev_id);
    query.addQueryItem(QStringLiteral("devpassword"), m_dev_password);
    query.addQueryItem(QStringLiteral("softname"), QStringLiteral("PegasusX"));
    query.addQueryItem(QStringLiteral("output"), QStringLiteral("json"));
    const QString user = m_settings.value(QStringLiteral("ssUser")).toString();
    if (m_settings.value(QStringLiteral("ssUse")).toBool() && !user.isEmpty()) {
        query.addQueryItem(QStringLiteral("ssid"), user);
        query.addQueryItem(QStringLiteral("sspassword"), m_settings.value(QStringLiteral("ssPassword")).toString());
    }
    query.addQueryItem(QStringLiteral("systemeid"), QString::number(screenscraper_systems().value(job.system)));
    query.addQueryItem(QStringLiteral("recherche"), name);
    QUrl url(QStringLiteral("https://api.screenscraper.fr/api2/jeuRecherche.php"));
    url.setQuery(query);
    get(url, {}, m_settings.value(QStringLiteral("retries")).toInt(), [this](int status, const QByteArray& body){
        if (m_stop) {
            next();
            return;
        }
        if (status == 430 || status == 403 || status == 426 || body.contains("Erreur de login")) {
            m_stop_reason = status == 430 ? tr("Your ScreenScraper games for today are used up. Try again tomorrow.")
                          : status == 426 ? tr("ScreenScraper needs a newer Pegasus X.")
                          : tr("ScreenScraper didn't accept the login: check Account settings.");
            m_stop = true;
            finishGame(m_stop_reason, false);
            return;
        }
        m_ss_results = QJsonDocument::fromJson(body).object()
            .value(QStringLiteral("response")).toObject().value(QStringLiteral("jeux")).toArray();
        const QString region = m_settings.value(QStringLiteral("region")).toString();
        const QString language = m_settings.value(QStringLiteral("language")).toString();
        QVariantList candidates;
        QJsonArray kept;
        for (const QJsonValue& value : qAsConst(m_ss_results)) {
            const QJsonObject game = value.toObject();
            const QString name = text_of(pick(game.value(QStringLiteral("noms")).toArray(), QStringLiteral("region"), region));
            if (name.isEmpty())
                continue;  // ScreenScraper ends its list with an empty entry
            kept.append(game);
            const QString date = text_of(pick(game.value(QStringLiteral("dates")).toArray(), QStringLiteral("region"), region));
            QStringList genres;
            for (const QJsonValue& genre : game.value(QStringLiteral("genres")).toArray()) {
                const QString g = text_of(pick(genre.toObject().value(QStringLiteral("noms")).toArray(), QStringLiteral("langue"), language));
                if (!g.isEmpty())
                    genres.append(g);
            }
            QJsonArray boxes;
            for (const QJsonValue& m : game.value(QStringLiteral("medias")).toArray()) {
                const QString type = m.toObject().value(QStringLiteral("type")).toString();
                if (type == QLatin1String("box-2D") || type == QLatin1String("box-3D") || type == QLatin1String("ss"))
                    boxes.append(m);
            }
            const QString system = game.value(QStringLiteral("systeme")).toObject().value(QStringLiteral("text")).toString();
            const double note = text_of(game.value(QStringLiteral("note"))).toDouble();
            candidates.append(QVariantMap {
                { QStringLiteral("name"), name },
                { QStringLiteral("info"), QStringList { system, date.left(4) }.filter(QRegularExpression(QStringLiteral("."))).join(QStringLiteral(" · ")) },
                { QStringLiteral("description"), text_of(pick(game.value(QStringLiteral("synopsis")).toArray(), QStringLiteral("langue"), language)) },
                { QStringLiteral("thumb"), pick(boxes, QStringLiteral("region"), region).value(QStringLiteral("url")).toString() },
                { QStringLiteral("developer"), text_of(game.value(QStringLiteral("developpeur"))) },
                { QStringLiteral("publisher"), text_of(game.value(QStringLiteral("editeur"))) },
                { QStringLiteral("genre"), genres.join(QStringLiteral(", ")) },
                { QStringLiteral("players"), text_of(game.value(QStringLiteral("joueurs"))) },
                { QStringLiteral("release"), date.left(10) },
                { QStringLiteral("rating"), note > 0 ? QString::number(qRound(note * 5)) + QLatin1Char('%') : QString() },
            });
        }
        m_ss_results = kept;
        m_choice_source = QStringLiteral("ss");
        offerChoice(candidates);
    });
}

void Scraper::sgdbSearch(const QString& name)
{
    const QByteArray auth = "Bearer " + m_settings.value(QStringLiteral("sgdbKey")).toString().trimmed().toUtf8();
    QUrl url(QStringLiteral("https://www.steamgriddb.com/api/v2/search/autocomplete/")
             + QString::fromLatin1(QUrl::toPercentEncoding(name)));
    get(url, auth, m_settings.value(QStringLiteral("retries")).toInt(), [this, auth](int status, const QByteArray& body){
        if (m_stop) {
            next();
            return;
        }
        if (status == 401) {
            m_stop_reason = tr("SteamGridDB didn't accept the key: check Account settings.");
            m_stop = true;
            finishGame(m_stop_reason, false);
            return;
        }
        const QJsonArray found = QJsonDocument::fromJson(body).object().value(QStringLiteral("data")).toArray();
        m_sgdb_ids.clear();
        auto* const candidates = new QVariantList();
        for (const QJsonValue& v : found) {
            const QJsonObject game = v.toObject();
            m_sgdb_ids.push_back(game.value(QStringLiteral("id")).toInt());
            const qint64 released = static_cast<qint64>(game.value(QStringLiteral("release_date")).toDouble());
            candidates->append(QVariantMap {
                { QStringLiteral("name"), game.value(QStringLiteral("name")).toString() },
                { QStringLiteral("info"), released > 0 ? QString::number(QDateTime::fromSecsSinceEpoch(released).date().year()) : QString() },
                { QStringLiteral("release"), released > 0 ? QDateTime::fromSecsSinceEpoch(released).date().toString(Qt::ISODate) : QString() },
            });
        }
        m_choice_source = QStringLiteral("sgdb");
        // a cover for the first ones, to tell them apart
        const int with_thumbs = qMin(6, static_cast<int>(m_sgdb_ids.size()));
        if (with_thumbs == 0) {
            offerChoice(*candidates);
            delete candidates;
            return;
        }
        auto* const pending = new int(with_thumbs);
        for (int i = 0; i < with_thumbs; i++) {
            const QUrl grid(QStringLiteral("https://www.steamgriddb.com/api/v2/grids/game/%1?dimensions=600x900").arg(m_sgdb_ids[static_cast<size_t>(i)]));
            get(grid, auth, 0, [this, i, pending, candidates](int st, const QByteArray& b){
                const QJsonArray images = QJsonDocument::fromJson(b).object().value(QStringLiteral("data")).toArray();
                if (st == 200 && !images.isEmpty()) {
                    QVariantMap c = (*candidates)[i].toMap();
                    c.insert(QStringLiteral("thumb"), images.first().toObject().value(QStringLiteral("thumb")).toString());
                    (*candidates)[i] = c;
                }
                if (--(*pending) == 0) {
                    delete pending;
                    const QVariantList list = *candidates;
                    delete candidates;
                    if (m_stop)
                        next();
                    else
                        offerChoice(list);
                }
            });
        }
    });
}

void Scraper::offerChoice(const QVariantList& candidates)
{
    const Job& job = m_jobs[m_index];
    // one match and Auto-accept on: no need to ask (scraping one game always asks)
    if (candidates.size() == 1 && m_settings.value(QStringLiteral("autoAcceptSingle")).toBool()
            && m_jobs.size() > 1) {
        m_waiting = true;
        choose(0);
        return;
    }
    m_choice = QVariantMap {
        { QStringLiteral("title"), job.title },
        { QStringLiteral("collection"), job.collection },
        { QStringLiteral("searchName"), job.title_is_file_name ? job.search_name : job.title },
        { QStringLiteral("source"), m_choice_source == QLatin1String("ss") ? QStringLiteral("ScreenScraper") : QStringLiteral("SteamGridDB") },
        { QStringLiteral("candidates"), candidates },
    };
    m_waiting = true;
    report();
    emit choiceNeeded(m_choice);
}

void Scraper::choose(int index)
{
    if (!m_waiting)
        return;
    m_waiting = false;
    if (m_choice_source == QLatin1String("ss")) {
        if (index < 0 || index >= m_ss_results.size()) {
            m_waiting = true;
            skip();
            return;
        }
        parseScreenScraper(m_ss_results.at(index).toObject());
        if (!m_settings.value(QStringLiteral("sgdbKey")).toString().trimmed().isEmpty())
            steamGridDb();
        else
            downloadNext();
        return;
    }
    if (index < 0 || index >= static_cast<int>(m_sgdb_ids.size())) {
        m_waiting = true;
        skip();
        return;
    }
    steamGridDb(m_sgdb_ids[static_cast<size_t>(index)]);
}

void Scraper::refine(const QString& search_name)
{
    if (!m_waiting || search_name.trimmed().isEmpty())
        return;
    m_waiting = false;
    Job& job = m_jobs[m_index];
    job.search_name = search_name.trimmed();
    job.title_is_file_name = true;  // searched by this name from now on
    search(job.search_name);
}

void Scraper::skip()
{
    if (!m_waiting)
        return;
    m_waiting = false;
    m_not_found++;
    finishGame(tr("skipped"), false);
}

void Scraper::report()
{
    emit progress(state());
}

bool Scraper::wants(const QString& setting) const
{
    return m_settings.value(setting, true).toBool();
}

// fill-only: a slot that already has a file of that kind keeps it
bool Scraper::slotFree(const Job& job, const Media& media) const
{
    if (m_settings.value(QStringLiteral("overwrite")).toBool())
        return true;
    if (media.kind == QLatin1String("2d"))
        return !job.has_box2d;
    if (media.kind == QLatin1String("3d"))
        return !job.has_box3d;
    return job.slot_files.value(media.slot).toStringList().isEmpty();
}

void Scraper::next()
{
    if (m_stop || m_index >= m_jobs.size()) {
        m_running = false;
        m_current.clear();
        if (m_stop && m_stop_reason.isEmpty())
            m_stop_reason = tr("Stopped");
        Log::info(LOG_TAG, LOGMSG("Done: %1 saved, %2 not found, %3 failed").arg(m_saved).arg(m_not_found).arg(m_failed));
        report();
        emit finished(state());
        return;
    }
    const Job& job = m_jobs[m_index];
    m_current = job.title;
    m_changes.clear();
    m_media.clear();
    m_got.clear();
    report();

    // interactive mode: the matches are shown, you pick one
    if (interactive()) {
        search(job.title_is_file_name ? job.search_name : job.title);
        return;
    }

    const QString source = m_settings.value(QStringLiteral("source")).toString();
    const bool ss_ok = source == QLatin1String("screenscraper") && !m_dev_id.isEmpty()
        && screenscraper_systems().contains(job.system);
    if (!ss_ok) {
        steamGridDb();
        return;
    }

    // a checksum first, if the file is small enough (off the main thread)
    const bool use_hash = m_settings.value(QStringLiteral("useHashes")).toBool()
        && !job.path.startsWith(QLatin1String("android:")) && QFileInfo(job.path).isFile();
    if (!use_hash) {
        screenScraper(QString());
        return;
    }
    const qint64 max_bytes = static_cast<qint64>(m_settings.value(QStringLiteral("hashMaxMiB")).toInt()) * 1024 * 1024;
    const QString path = job.path;
    auto* const watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]{
        const QString md5 = watcher->result();
        watcher->deleteLater();
        screenScraper(md5);
    });
    watcher->setFuture(QtConcurrent::run([path, max_bytes]{ return md5_of(path, max_bytes); }));
}

void Scraper::get(const QUrl& url, const QByteArray& auth, int tries_left,
                  std::function<void(int, const QByteArray&)> done)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(120000);
    if (!auth.isEmpty())
        request.setRawHeader("Authorization", auth);
    QNetworkReply* const reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, auth, tries_left, done]{
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        const bool retry = (status == 0 || status == 429 || status >= 500) && status != 430;
        if (retry && tries_left > 0 && !m_stop) {
            const int delay = qMax(1, m_settings.value(QStringLiteral("retryDelay")).toInt()) * 1000;
            QTimer::singleShot(delay, this, [this, url, auth, tries_left, done]{
                get(url, auth, tries_left - 1, done);
            });
            return;
        }
        done(status, body);
    });
}

void Scraper::screenScraper(const QString& md5)
{
    const Job& job = m_jobs[m_index];
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("devid"), m_dev_id);
    query.addQueryItem(QStringLiteral("devpassword"), m_dev_password);
    query.addQueryItem(QStringLiteral("softname"), QStringLiteral("PegasusX"));
    query.addQueryItem(QStringLiteral("output"), QStringLiteral("json"));
    const QString user = m_settings.value(QStringLiteral("ssUser")).toString();
    if (m_settings.value(QStringLiteral("ssUse")).toBool() && !user.isEmpty()) {
        query.addQueryItem(QStringLiteral("ssid"), user);
        query.addQueryItem(QStringLiteral("sspassword"), m_settings.value(QStringLiteral("ssPassword")).toString());
    }
    query.addQueryItem(QStringLiteral("systemeid"), QString::number(screenscraper_systems().value(job.system)));
    query.addQueryItem(QStringLiteral("romtype"), QStringLiteral("rom"));
    // your metadata's title, or the file's name
    const bool by_title = m_settings.value(QStringLiteral("useMetadataNames")).toBool() && !job.title_is_file_name;
    query.addQueryItem(QStringLiteral("romnom"), by_title ? job.title : job.search_name);
    if (!md5.isEmpty()) {
        query.addQueryItem(QStringLiteral("md5"), md5);
        query.addQueryItem(QStringLiteral("romtaille"), QString::number(QFileInfo(job.path).size()));
    }
    QUrl url(QStringLiteral("https://api.screenscraper.fr/api2/jeuInfos.php"));
    url.setQuery(query);

    get(url, {}, m_settings.value(QStringLiteral("retries")).toInt(), [this](int status, const QByteArray& body){
        if (m_stop) {
            next();
            return;
        }
        if (status == 430 || status == 431) {
            m_stop_reason = tr("Your ScreenScraper games for today are used up. Try again tomorrow.");
            m_stop = true;
            finishGame(m_stop_reason, false);
            return;
        }
        if (status == 403 || body.contains("Erreur de login")) {
            m_stop_reason = tr("ScreenScraper didn't accept the login: check Account settings.");
            m_stop = true;
            finishGame(m_stop_reason, false);
            return;
        }
        if (status == 426) {
            m_stop_reason = tr("ScreenScraper needs a newer Pegasus X.");
            m_stop = true;
            finishGame(m_stop_reason, false);
            return;
        }
        const QJsonObject game = QJsonDocument::fromJson(body).object()
            .value(QStringLiteral("response")).toObject().value(QStringLiteral("jeu")).toObject();
        if (game.isEmpty()) {
            // not on ScreenScraper: SteamGridDB may still have art
            if (!m_settings.value(QStringLiteral("sgdbKey")).toString().isEmpty()) {
                steamGridDb();
                return;
            }
            m_not_found++;
            finishGame(status == 404 || status == 200 ? tr("not found") : tr("ScreenScraper error %1").arg(status), false);
            return;
        }
        parseScreenScraper(game);
        // what ScreenScraper doesn't have, from SteamGridDB
        if (!m_settings.value(QStringLiteral("sgdbKey")).toString().isEmpty())
            steamGridDb();
        else
            downloadNext();
    });
}

void Scraper::parseScreenScraper(const QJsonObject& game)
{
    const Job& job = m_jobs[m_index];
    const bool overwrite = m_settings.value(QStringLiteral("overwrite")).toBool();
    const QString region = m_settings.value(QStringLiteral("region")).toString();
    const QString language = m_settings.value(QStringLiteral("language")).toString();

    if (wants(QStringLiteral("getNames")) && (overwrite || job.title_is_file_name)) {
        const QString name = text_of(pick(game.value(QStringLiteral("noms")).toArray(), QStringLiteral("region"), region));
        if (!name.isEmpty() && name != job.title)
            m_changes.insert(QStringLiteral("title"), name);
    }
    if (wants(QStringLiteral("getRatings")) && (overwrite || !job.has_rating)) {
        const double note = text_of(game.value(QStringLiteral("note"))).toDouble();
        if (note > 0)
            m_changes.insert(QStringLiteral("rating"), QString::number(qRound(note * 5)) + QLatin1Char('%'));
    }
    if (wants(QStringLiteral("getDetails"))) {
        const QString synopsis = text_of(pick(game.value(QStringLiteral("synopsis")).toArray(), QStringLiteral("langue"), language));
        if (!synopsis.isEmpty() && (overwrite || !job.has_description)) {
            m_changes.insert(QStringLiteral("description"), synopsis);
            m_changes.insert(QStringLiteral("summary"), QString());
        }
        const QString developer = text_of(game.value(QStringLiteral("developpeur")));
        if (!developer.isEmpty() && (overwrite || !job.has_developer))
            m_changes.insert(QStringLiteral("developer"), QStringList { developer });
        const QString publisher = text_of(game.value(QStringLiteral("editeur")));
        if (!publisher.isEmpty() && (overwrite || !job.has_publisher))
            m_changes.insert(QStringLiteral("publisher"), QStringList { publisher });
        const QString players = text_of(game.value(QStringLiteral("joueurs")));
        if (!players.isEmpty() && (overwrite || !job.has_players)) {
            // "1-4" -> 4
            const QString most = players.section(QLatin1Char('-'), -1).trimmed();
            if (most.toInt() > 0)
                m_changes.insert(QStringLiteral("players"), most);
        }
        const QString date = text_of(pick(game.value(QStringLiteral("dates")).toArray(), QStringLiteral("region"), region));
        if (!date.isEmpty() && (overwrite || !job.has_release))
            m_changes.insert(QStringLiteral("release"), date.left(10));
        if (overwrite || !job.has_genre) {
            QStringList genres;
            for (const QJsonValue& genre : game.value(QStringLiteral("genres")).toArray()) {
                const QString name = text_of(pick(genre.toObject().value(QStringLiteral("noms")).toArray(), QStringLiteral("langue"), language));
                if (!name.isEmpty() && !genres.contains(name))
                    genres.append(name);
            }
            if (!genres.isEmpty())
                m_changes.insert(QStringLiteral("genre"), genres);
        }
    }

    // the art: ScreenScraper's media types -> Pegasus' slots (the folders are ES-DE's names)
    struct Kind { const char* types; const char* setting; const char* slot; const char* folder; const char* box; };
    const QString mix = m_settings.value(QStringLiteral("miximageStyle")).toString();
    const std::vector<Kind> kinds {
        { "box-2D", "getCovers", "assets.box_front", "covers", "2d" },
        { "box-3D", "getBox3d", "assets.box_front", "3dboxes", "3d" },
        { "box-2D-back", "getBackcovers", "assets.box_back", "backcovers", "" },
        { "support-2D", "getPhysical", "assets.cartridge", "physicalmedia", "" },
        { "ss", "getScreenshots", "assets.screenshot", "screenshots", "" },
        { "sstitle", "getTitlescreens", "assets.titlescreen", "titlescreens", "" },
        { "fanart", "getFanart", "assets.background", "fanart", "" },
        { "wheel-hd,wheel", "getLogos", "assets.logo", "marquees", "" },
        { "video-normalized,video", "getVideos", "assets.video", "videos", "" },
        { "manuel", "getManuals", "x-manual", "manuals", "" },
        { "", "getMiximages", "assets.steam", "miximages", "" },
    };
    const QJsonArray medias = game.value(QStringLiteral("medias")).toArray();
    for (const Kind& kind : kinds) {
        const QString setting = QLatin1String(kind.setting);
        if (!wants(setting))
            continue;
        const QStringList types = kind.types[0] ? QString::fromLatin1(kind.types).split(QLatin1Char(','))
                                                : QStringList { mix.isEmpty() ? QStringLiteral("mixrbv2") : mix };
        for (const QString& type : types) {
            QJsonArray of_type;
            for (const QJsonValue& m : medias) {
                if (m.toObject().value(QStringLiteral("type")).toString() == type)
                    of_type.append(m);
            }
            if (of_type.isEmpty())
                continue;
            const QJsonObject chosen = pick(of_type, QStringLiteral("region"), region);
            Media media;
            media.setting = setting;
            media.slot = QLatin1String(kind.slot);
            media.folder = QLatin1String(kind.folder);
            media.kind = QLatin1String(kind.box);
            media.url = QUrl(chosen.value(QStringLiteral("url")).toString());
            media.ext = chosen.value(QStringLiteral("format")).toString();
            if (media.url.isValid() && slotFree(job, media))
                m_media.push_back(media);
            break;
        }
    }
}

void Scraper::steamGridDb(int known_id)
{
    const QString key = m_settings.value(QStringLiteral("sgdbKey")).toString().trimmed();
    if (key.isEmpty()) {
        if (m_media.empty() && m_changes.isEmpty()) {
            m_not_found++;
            finishGame(m_dev_id.isEmpty() && m_settings.value(QStringLiteral("source")).toString() == QLatin1String("screenscraper")
                ? tr("ScreenScraper isn't in this build yet") : tr("not found"), false);
        }
        else {
            downloadNext();
        }
        return;
    }
    const Job& job = m_jobs[m_index];
    // only what's still free and not found on ScreenScraper
    struct Want { const char* path; const char* setting; const char* slot; const char* folder; const char* box; };
    const std::vector<Want> wants_all {
        { "grids", "getCovers", "assets.box_front", "covers", "2d" },
        { "heroes", "getFanart", "assets.background", "fanart", "" },
        { "logos", "getLogos", "assets.logo", "marquees", "" },
    };
    auto* const todo = new std::vector<Want>();
    for (const Want& w : wants_all) {
        Media probe;
        probe.slot = QLatin1String(w.slot);
        probe.kind = QLatin1String(w.box);
        bool already = false;
        for (const Media& m : m_media)
            already = already || (m.slot == probe.slot && m.kind == probe.kind);
        if (wants(QLatin1String(w.setting)) && !already && slotFree(job, probe))
            todo->push_back(w);
    }
    if (todo->empty()) {
        delete todo;
        downloadNext();
        return;
    }
    const QByteArray auth = "Bearer " + key.toUtf8();
    const QString name = job.title_is_file_name ? job.search_name : job.title;
    // the art of a SteamGridDB game: only what's still free
    const auto fetch_art = [this, todo, auth](const QString& id) {
        auto* const pending = new int(static_cast<int>(todo->size()));
        for (const Want& w : *todo) {
            QString path = QStringLiteral("https://www.steamgriddb.com/api/v2/%1/game/%2").arg(QLatin1String(w.path), id);
            if (QLatin1String(w.path) == QLatin1String("grids"))
                path += QStringLiteral("?dimensions=600x900");
            const Want want = w;
            get(QUrl(path), auth, 1, [this, want, pending, todo](int st, const QByteArray& b){
                const QJsonArray images = QJsonDocument::fromJson(b).object().value(QStringLiteral("data")).toArray();
                if (st == 200 && !images.isEmpty()) {
                    Media media;
                    media.setting = QLatin1String(want.setting);
                    media.slot = QLatin1String(want.slot);
                    media.folder = QLatin1String(want.folder);
                    media.kind = QLatin1String(want.box);
                    media.url = QUrl(images.first().toObject().value(QStringLiteral("url")).toString());
                    media.ext = QFileInfo(media.url.path()).suffix();
                    if (media.url.isValid())
                        m_media.push_back(media);
                }
                if (--(*pending) == 0) {
                    delete pending;
                    delete todo;
                    if (m_media.empty() && m_changes.isEmpty()) {
                        m_not_found++;
                        finishGame(tr("no art on SteamGridDB"), false);
                    }
                    else {
                        downloadNext();
                    }
                }
            });
        }
    };
    if (known_id > 0) {
        fetch_art(QString::number(known_id));
        return;
    }
    QUrl url(QStringLiteral("https://www.steamgriddb.com/api/v2/search/autocomplete/")
             + QString::fromLatin1(QUrl::toPercentEncoding(name)));
    get(url, auth, m_settings.value(QStringLiteral("retries")).toInt(), [this, todo, name, fetch_art](int status, const QByteArray& body){
        const QJsonArray found = QJsonDocument::fromJson(body).object().value(QStringLiteral("data")).toArray();
        if (m_stop || status != 200 || found.isEmpty()) {
            delete todo;
            if (m_media.empty() && m_changes.isEmpty()) {
                m_not_found++;
                finishGame(status == 401 ? tr("SteamGridDB didn't accept the key") : tr("not found"), false);
            }
            else {
                downloadNext();
            }
            return;
        }
        // the exact name if it's there, else SteamGridDB's best match
        QJsonObject game = found.first().toObject();
        for (const QJsonValue& v : found) {
            if (normalized(v.toObject().value(QStringLiteral("name")).toString()) == normalized(name)) {
                game = v.toObject();
                break;
            }
        }
        fetch_art(QString::number(game.value(QStringLiteral("id")).toInt()));
    });
}

// one file at a time into <data>/<collection>/<folder>/<title>~<time>.<ext>
void Scraper::downloadNext()
{
    if (m_stop) {
        save();
        return;
    }
    if (m_media.empty()) {
        save();
        return;
    }
    const Media media = m_media.front();
    m_media.erase(m_media.begin());
    const Job& job = m_jobs[m_index];
    get(media.url, {}, 1, [this, media, job](int status, const QByteArray& body){
        if (status != 200 || body.isEmpty()) {
            downloadNext();
            return;
        }
        QString ext = media.ext.toLower();
        if (ext.isEmpty())
            ext = QStringLiteral("png");

        // pictures: the size and trimming of the Scraper settings, worked out
        // off the main thread
        const bool picture = media.folder != QLatin1String("videos") && media.folder != QLatin1String("manuals")
            && (ext == QLatin1String("png") || ext == QLatin1String("jpg") || ext == QLatin1String("jpeg") || ext == QLatin1String("webp"));
        const QString size = m_settings.value(QStringLiteral("imageMaxSize")).toString();
        int max_w = size == QLatin1String("1920") ? 1920 : size == QLatin1String("1280") ? 1280
                  : size == QLatin1String("640") ? 640 : size == QLatin1String("original") ? 0 : 2560;
        int max_h = max_w == 1920 ? 1080 : max_w == 1280 ? 720 : max_w == 640 ? 480 : max_w == 0 ? 0 : 1440;
        const bool logo = media.folder == QLatin1String("marquees");
        if (logo && max_w > 1000) {
            max_w = 1000;  // logos don't need more (as ES-DE)
            max_h = 600;
        }
        const QString trim_setting = m_settings.value(QStringLiteral("trimBorders")).toString();
        const bool trim = trim_setting == QLatin1String("all") || (trim_setting == QLatin1String("logos") && logo);
        if (picture && (max_w > 0 || trim)) {
            auto* const watcher = new QFutureWatcher<QPair<QByteArray, QString>>(this);
            connect(watcher, &QFutureWatcher<QPair<QByteArray, QString>>::finished, this, [this, watcher, media, job]{
                const QPair<QByteArray, QString> result = watcher->result();
                watcher->deleteLater();
                storeMedia(media, job, result.first, result.second);
            });
            watcher->setFuture(QtConcurrent::run([body, ext, max_w, max_h, trim]{
                return process_image(body, ext, max_w, max_h, trim);
            }));
            return;
        }
        storeMedia(media, job, body, ext);
    });
}

void Scraper::storeMedia(const Media& media, const Job& job, const QByteArray& body, const QString& ext)
{
    // one file per game and kind, named after the game's file (as ES-DE does):
    // <data>/<collection>/<folder>/<game file name>.<ext>, written over on the
    // next scrape and the same whatever the title becomes. Android apps (no
    // file, an android: entry) use the title.
    const QString title = m_changes.value(QStringLiteral("title"), job.title).toString();
    const QString dir = m_data_dir % QLatin1Char('/') % safe_file_name(job.collection) % QLatin1Char('/') % media.folder;
    QDir().mkpath(dir);
    const bool has_file = !job.path.isEmpty() && !job.path.startsWith(QLatin1String("android:"));
    // (a content:// entry: its last part, decoded)
    const QString file_part = job.path.contains(QLatin1String("://"))
        ? QUrl::fromPercentEncoding(job.path.toUtf8()).section(QLatin1Char('/'), -1)
        : job.path;
    const QString base = safe_file_name(has_file ? QFileInfo(file_part).completeBaseName() : title);
    const QString file_path = dir % QLatin1Char('/') % base % QLatin1Char('.') % ext;
    QSaveFile file(file_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(body) != body.size() || !file.commit()) {
        downloadNext();
        return;
    }

    // the game's other copies of this kind in Pegasus X's data folder (another
    // extension, or the time-stamped copies of older scrapes) go
    QStringList removed;
    // exactly "<name>.<ext>" or "<name>~<14 digits>.<ext>", so another game
    // ("Game Deluxe", "Game.Remake") never matches
    static const QRegularExpression rx_stamp(QStringLiteral("^~\\d{14}$"));
    const QFileInfoList others = QDir(dir).entryInfoList(QDir::Files);
    for (const QFileInfo& other : others) {
        const QString path = other.absoluteFilePath();
        if (path == QFileInfo(file_path).absoluteFilePath())
            continue;
        const QString stem = other.completeBaseName();
        const bool same = stem == base
            || (stem.startsWith(base) && rx_stamp.match(stem.mid(base.length())).hasMatch());
        if (same && QFile::remove(path))
            removed.append(path);
    }

    // into the slot: the new file first (the 3D box after the 2D one), never
    // twice, without the copies just removed; the others stay unless you overwrite
    QStringList list = m_changes.contains(media.slot) ? m_changes.value(media.slot).toStringList()
                                                       : job.slot_files.value(media.slot).toStringList();
    if (m_settings.value(QStringLiteral("overwrite")).toBool() && !m_changes.contains(media.slot)) {
        // replaced: the old files of that kind go
        QStringList kept;
        for (const QString& f : list) {
            const bool is3d = f.contains(QLatin1String("3dbox"), Qt::CaseInsensitive) || f.contains(QLatin1String("box3d"), Qt::CaseInsensitive);
            if (media.kind == QLatin1String("2d") && is3d) kept.append(f);
            if (media.kind == QLatin1String("3d") && !is3d) kept.append(f);
        }
        list = kept;
    }
    // a file of this game's in the same scraper folder under another name
    // (scraped before, named after its title) is replaced by this one too
    const QString dir_abs = QDir(dir).absolutePath() + QLatin1Char('/');
    QStringList clean;
    for (const QString& f : list) {
        const QString path = f.startsWith(QLatin1String("file:")) ? QUrl(f).toLocalFile() : f;
        if (path == file_path || removed.contains(path) || clean.contains(f))
            continue;
        const QFileInfo info(path);
        if (info.absolutePath() + QLatin1Char('/') == dir_abs && info.isFile()) {
            if (QFile::remove(info.absoluteFilePath()))
                removed.append(path);
            continue;
        }
        clean.append(f);
    }
    if (media.kind == QLatin1String("3d"))
        clean.append(file_path);
    else
        clean.prepend(file_path);
    m_changes.insert(media.slot, clean);
    m_got.append(media.folder);

    // a file of this name removed in Library before: scraped again, it's back
    if (media.slot.startsWith(QLatin1String("assets."))) {
        const QString kind = media.slot.mid(7);
        const QString name = QFileInfo(file_path).fileName().toLower();
        QStringList hidden = m_changes.contains(QStringLiteral("x-pegasusx-hidden-art"))
            ? m_changes.value(QStringLiteral("x-pegasusx-hidden-art")).toStringList() : job.hidden_art;
        const int before = hidden.size();
        hidden.erase(std::remove_if(hidden.begin(), hidden.end(), [&kind, &name](const QString& h) {
            const int bar = h.indexOf(QLatin1Char('|'));
            return bar > 0 && h.left(bar) == kind && QFileInfo(h.mid(bar + 1)).fileName().toLower() == name;
        }), hidden.end());
        if (hidden.size() != before)
            m_changes.insert(QStringLiteral("x-pegasusx-hidden-art"), hidden);
    }
    downloadNext();
}

void Scraper::save()
{
    const Job& job = m_jobs[m_index];
    if (m_changes.isEmpty()) {
        m_not_found++;
        finishGame(tr("nothing new"), false);
        return;
    }
    const QString title = m_changes.value(QStringLiteral("title"), job.title).toString();
#ifdef WITH_COMPAT_ESDE
    const QString error = providers::esde::save_game_metadata(job.collection, job.path, title, m_changes);
#else
    // metadata editing is Android only for now
    const QString error = tr("saving isn't supported on this platform yet");
#endif
    if (!error.isEmpty()) {
        m_failed++;
        finishGame(error, false);
        return;
    }
    m_saved++;
    m_last_changes = m_changes;
    QStringList what;
    if (m_changes.contains(QStringLiteral("description")) || m_changes.contains(QStringLiteral("developer")))
        what.append(tr("details"));
    for (const QString& folder : m_got) {
        if (!what.contains(folder))
            what.append(folder);
    }
    finishGame(what.join(QStringLiteral(", ")), true);
}

void Scraper::finishGame(const QString& result, bool ok)
{
    if (m_index < m_jobs.size())
        m_last_title = m_jobs[m_index].title;
    m_last_result = result;
    m_last_ok = ok;
    m_index++;
    report();
    // a short pause between games keeps within ScreenScraper's pace
    QTimer::singleShot(300, this, [this]{ next(); });
}

} // namespace scraper
