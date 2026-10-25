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
#include "Paths.h"
#include "Miximage.h"
#ifdef WITH_COMPAT_ESDE
#include "providers/esde/EsdeProvider.h"
#endif

#include "PegasusAssets.h"
#include <QBuffer>
#include <QImageReader>
#include <QImage>
#include <QColor>
#include <QPainter>
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
// some image servers turn away requests that don't say who's asking
const QByteArray USER_AGENT = QByteArrayLiteral("PegasusX/1.0 (Pegasus frontend)");

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
// ES-DE system -> IGDB platform id (nothing found on that platform: searched
// again on every platform)
const QHash<QString, int>& igdb_platforms()
{
    static const QHash<QString, int> map {
    { "3do", 50 }, { "amiga", 16 }, { "amiga1200", 16 }, { "amiga600", 16 }, { "amigacd32", 114 },
    { "amstradcpc", 25 }, { "android", 34 }, { "androidapps", 34 }, { "androidgames", 34 },
    { "apple2", 75 }, { "apple2gs", 115 }, { "arcade", 52 }, { "atomiswave", 52 }, { "cps", 52 },
    { "cps1", 52 }, { "cps2", 52 }, { "cps3", 52 }, { "fba", 52 }, { "fbneo", 52 }, { "mame", 52 },
    { "mame-advmame", 52 }, { "model2", 52 }, { "model3", 52 }, { "naomi", 52 }, { "naomi2", 52 },
    { "naomigd", 52 }, { "atari2600", 59 }, { "atari5200", 66 }, { "atari7800", 60 },
    { "atari800", 65 }, { "atarixe", 65 }, { "atarist", 63 }, { "atarijaguar", 62 },
    { "atarilynx", 61 }, { "c64", 15 }, { "colecovision", 68 }, { "dos", 13 }, { "dreamcast", 23 },
    { "famicom", 99 }, { "fds", 51 }, { "gamegear", 35 }, { "gb", 33 }, { "gba", 24 }, { "gbc", 22 },
    { "gc", 21 }, { "genesis", 29 }, { "megadrive", 29 }, { "megadrivejp", 29 }, { "intellivision", 67 },
    { "macintosh", 14 }, { "mark3", 64 }, { "mastersystem", 64 }, { "megacd", 78 }, { "megacdjp", 78 },
    { "segacd", 78 }, { "msx", 27 }, { "msx1", 27 }, { "msx2", 53 }, { "n3ds", 37 }, { "n64", 4 },
    { "n64dd", 416 }, { "nds", 20 }, { "neogeo", 80 }, { "neogeocd", 136 }, { "neogeocdjp", 136 },
    { "nes", 18 }, { "ngage", 42 }, { "ngp", 119 }, { "ngpc", 120 }, { "odyssey2", 133 },
    { "videopac", 133 }, { "pc", 6 }, { "windows", 6 }, { "steam", 6 }, { "pc88", 125 },
    { "pc98", 149 }, { "pcengine", 86 }, { "tg16", 86 }, { "pcenginecd", 150 }, { "tg-cd", 150 },
    { "pcfx", 274 }, { "ps2", 8 }, { "ps3", 9 }, { "ps4", 48 }, { "psp", 38 }, { "psvita", 46 },
    { "psx", 7 }, { "saturn", 32 }, { "saturnjp", 32 }, { "sega32x", 30 }, { "sega32xjp", 30 },
    { "sega32xna", 30 }, { "sfc", 58 }, { "sg-1000", 84 }, { "snes", 19 }, { "snesna", 19 },
    { "supergrafx", 128 }, { "switch", 130 }, { "vectrex", 70 }, { "vic20", 71 },
    { "virtualboy", 87 }, { "wii", 5 }, { "wiiu", 41 }, { "wonderswan", 57 },
    { "wonderswancolor", 123 }, { "x68000", 121 }, { "xbox", 11 }, { "xbox360", 12 },
    { "xboxone", 49 }, { "zxspectrum", 26 },
    };
    return map;
}

// an IGDB picture's address, in one of its sizes
QString igdb_image(const QJsonValue& image, const char* size)
{
    const QString id = image.toObject().value(QStringLiteral("image_id")).toString();
    return id.isEmpty() ? QString()
        : QStringLiteral("https://images.igdb.com/igdb/image/upload/t_%1/%2.jpg").arg(QLatin1String(size), id);
}

// IGDB game -> its developers / publishers
QStringList igdb_companies(const QJsonObject& game, const QString& role)
{
    QStringList out;
    for (const QJsonValue& v : game.value(QStringLiteral("involved_companies")).toArray()) {
        const QJsonObject ic = v.toObject();
        if (!ic.value(role).toBool())
            continue;
        const QString name = ic.value(QStringLiteral("company")).toObject().value(QStringLiteral("name")).toString();
        if (!name.isEmpty() && !out.contains(name))
            out.append(name);
    }
    return out;
}
QStringList igdb_names(const QJsonObject& game, const QString& field)
{
    QStringList out;
    for (const QJsonValue& v : game.value(field).toArray()) {
        const QString name = v.toObject().value(QStringLiteral("name")).toString();
        if (!name.isEmpty() && !out.contains(name))
            out.append(name);
    }
    return out;
}
// most players on one machine (single player only: 1)
int igdb_players(const QJsonObject& game)
{
    int most = 0;
    for (const QJsonValue& v : game.value(QStringLiteral("multiplayer_modes")).toArray())
        most = qMax(most, v.toObject().value(QStringLiteral("offlinemax")).toInt());
    if (most == 0 && igdb_names(game, QStringLiteral("game_modes")).contains(QStringLiteral("Single player")))
        most = 1;
    return most;
}
// 0-100: the critics' and users' rating
int igdb_rating(const QJsonObject& game)
{
    for (const char* key : { "total_rating", "aggregated_rating", "rating" }) {
        const double value = game.value(QLatin1String(key)).toDouble();
        if (value > 0)
            return qRound(value);
    }
    return 0;
}
QDate igdb_release(const QJsonObject& game)
{
    const qint64 secs = static_cast<qint64>(game.value(QStringLiteral("first_release_date")).toDouble());
    return secs != 0 ? QDateTime::fromSecsSinceEpoch(secs, Qt::UTC).date() : QDate();
}
QString igdb_description(const QJsonObject& game)
{
    const QString summary = game.value(QStringLiteral("summary")).toString().trimmed();
    return summary.isEmpty() ? game.value(QStringLiteral("storyline")).toString().trimmed() : summary;
}

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
// fan art cropped to 16:9 (Scraper > Fan art shape; off by default: kept as
// downloaded, like ES-DE)
QImage fit_16_9(const QImage& src)
{
    // 16:9 by cropping, never by padding: the middle of a taller picture,
    // the middle of a wider one (IGDB's artworks are often banner-wide);
    // padding showed as bars above and below
    const double ratio = static_cast<double>(src.width()) / static_cast<double>(src.height());
    const double wanted = 16.0 / 9.0;
    if (ratio > wanted * 0.97 && ratio < wanted * 1.03)
        return src;
    if (ratio < wanted) {
        const int h = qRound(src.width() / wanted);
        return src.copy(0, (src.height() - h) / 2, src.width(), h);
    }
    const int w = qRound(src.height() * wanted);
    return src.copy((src.width() - w) / 2, 0, w, src.height());
}

QPair<QByteArray, QString> process_image(const QByteArray& data, const QString& ext, int max_w, int max_h, bool trim, bool fanart_16_9)
{
    QImage img;
    if (!img.loadFromData(data))
        return { data, ext };
    bool changed = false;
    if (fanart_16_9) {
        const QImage shaped = fit_16_9(img);
        if (shaped.size() != img.size()) {
            img = shaped;
            changed = true;
        }
    }
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
    const bool alpha = img.hasAlphaChannel() && img.format() != QImage::Format_RGB32;
    const QString out_ext = (alpha || ext == QLatin1String("png")) ? QStringLiteral("png") : QStringLiteral("jpg");
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

// a line for the log page (and the log file), with the time
void Scraper::note(const QString& line)
{
    m_log.append(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss ")) + line);
    while (m_log.size() > 400)
        m_log.removeFirst();
    Log::info(LOG_TAG, line);
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
        { QStringLiteral("preview"), m_preview },
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
    m_preview.clear();
    m_stop_reason.clear();
    m_stop = false;
    m_running = true;
    m_run++;  // answers still on their way from the last run are ignored
    note(LOGMSG("Scraping %1 games into `%2` (source: %3, interactive: %4, overwrite: %5, ScreenScraper login: %6, IGDB login: %7, SteamGridDB key: %8)")
        .arg(m_jobs.size()).arg(m_data_dir)
        .arg(m_settings.value(QStringLiteral("source")).toString())
        .arg(interactive() ? QStringLiteral("yes") : QStringLiteral("no"))
        .arg(m_settings.value(QStringLiteral("overwrite")).toBool() ? QStringLiteral("yes") : QStringLiteral("no"))
        .arg(m_dev_id.isEmpty() ? QStringLiteral("missing") : QStringLiteral("yes"))
        .arg(igdbReady() ? QStringLiteral("yes") : QStringLiteral("missing"))
        .arg(m_settings.value(QStringLiteral("sgdbKey")).toString().trimmed().isEmpty() ? QStringLiteral("missing") : QStringLiteral("yes")));
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

// the matches for a name: the source's (IGDB or ScreenScraper, with details
// and art), else IGDB's, else SteamGridDB's (art only)
void Scraper::search(const QString& name)
{
    const Job& job = m_jobs[m_index];
    const QString source = m_settings.value(QStringLiteral("source")).toString();
    const bool ss_ok = source == QLatin1String("screenscraper") && !m_dev_id.isEmpty()
        && screenscraper_systems().contains(job.system);
    if (source == QLatin1String("igdb") && igdbReady())
        igdbSearch(name);
    else if (ss_ok)
        ssSearch(name);
    else if (source != QLatin1String("steamgriddb") && igdbReady())
        igdbSearch(name);
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
        const int run = m_run;
        for (int i = 0; i < with_thumbs; i++) {
            const QUrl grid(QStringLiteral("https://www.steamgriddb.com/api/v2/grids/game/%1?dimensions=600x900").arg(m_sgdb_ids[static_cast<size_t>(i)]));
            get(grid, auth, 0, [this, i, pending, candidates, run](int st, const QByteArray& b){
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
                    if (run != m_run)
                        return;  // a later run took over
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
    // one match and Auto-accept on: no need to ask (scraping one game always asks)
    if (candidates.size() == 1 && m_settings.value(QStringLiteral("autoAcceptSingle")).toBool()
            && m_jobs.size() > 1) {
        m_waiting = true;
        choose(0);
        return;
    }
    // the pictures first (downloaded here, so they show even where the
    // picker couldn't load them itself), then the pick
    m_choice.clear();
    m_waiting = true;
    const int run = m_run;
    prefetchThumbs(candidates, [this, run](const QVariantList& with_pictures){
        if (run != m_run || !m_waiting || m_index >= m_jobs.size())
            return;  // stopped meanwhile
        const Job& job = m_jobs[m_index];
        m_choice = QVariantMap {
            { QStringLiteral("title"), job.title },
            { QStringLiteral("collection"), job.collection },
            { QStringLiteral("searchName"), job.title_is_file_name ? job.search_name : job.title },
            { QStringLiteral("source"), m_choice_source == QLatin1String("ss") ? QStringLiteral("ScreenScraper")
                                        : m_choice_source == QLatin1String("igdb") ? QStringLiteral("IGDB")
                                        : QStringLiteral("SteamGridDB") },
            { QStringLiteral("candidates"), with_pictures },
        };
        report();
        emit choiceNeeded(m_choice);
    });
}

void Scraper::prefetchThumbs(const QVariantList& candidates, std::function<void(const QVariantList&)> done)
{
    auto* const list = new QVariantList(candidates);
    auto* const pending = new int(0);
    const QString dir = paths::writableCacheDir() + QStringLiteral("/scraper-pictures");
    QDir().mkpath(dir);
    for (int i = 0; i < list->size(); i++) {
        const QString url = (*list)[i].toMap().value(QStringLiteral("thumb")).toString();
        if (url.startsWith(QLatin1String("http")))
            (*pending)++;
    }
    if (*pending == 0) {
        delete pending;
        const QVariantList out = *list;
        delete list;
        done(out);
        return;
    }
    for (int i = 0; i < list->size(); i++) {
        const QString url = (*list)[i].toMap().value(QStringLiteral("thumb")).toString();
        if (!url.startsWith(QLatin1String("http")))
            continue;
        get(QUrl(url), {}, 0, [this, i, url, dir, list, pending, done](int status, const QByteArray& body){
            QVariantMap c = (*list)[i].toMap();
            QBuffer buffer;
            buffer.setData(body);
            buffer.open(QIODevice::ReadOnly);
            if (status == 200 && QImageReader(&buffer).canRead()) {
                const QString name = QString::fromLatin1(QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Md5).toHex());
                const QString path = dir + QLatin1Char('/') + name + QStringLiteral(".img");
                QFile file(path);
                if (file.open(QIODevice::WriteOnly) && file.write(body) == body.size()) {
                    file.close();
                    c.insert(QStringLiteral("thumb"), QUrl::fromLocalFile(path).toString());
                }
            }
            else if (status != 200) {
                c.insert(QStringLiteral("thumbError"), status);
                Log::warning(LOG_TAG, LOGMSG("Picture `%1`: %2").arg(url).arg(status));
            }
            (*list)[i] = c;
            if (--(*pending) == 0) {
                delete pending;
                const QVariantList out = *list;
                delete list;
                done(out);
            }
        });
    }
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
        fillMore();
        return;
    }
    if (m_choice_source == QLatin1String("igdb")) {
        if (index < 0 || index >= m_igdb_results.size()) {
            m_waiting = true;
            skip();
            return;
        }
        m_igdb_done = true;
        parseIgdb(m_igdb_results.at(index).toObject(), false);
        fillMore();
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
    m_igdb_done = false;
    m_mix_done = false;
    m_mix_error.clear();
    m_picture_failures = 0;
    m_picture_error = 0;
    report();

    // the offline generator: only the miximage, from the art the game has
    if (offline()) {
        downloadNext();
        return;
    }

    // interactive mode: the matches are shown, you pick one
    if (interactive()) {
        search(job.title_is_file_name ? job.search_name : job.title);
        return;
    }

    const QString source = m_settings.value(QStringLiteral("source")).toString();
    const bool ss_ok = source == QLatin1String("screenscraper") && !m_dev_id.isEmpty()
        && screenscraper_systems().contains(job.system);
    if (source == QLatin1String("igdb") && igdbReady()) {
        igdbAuto(false);
        return;
    }
    if (!ss_ok) {
        // ScreenScraper can't: IGDB has details too, SteamGridDB only art
        if (source != QLatin1String("steamgriddb") && igdbReady())
            igdbAuto(false);
        else
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
    request.setRawHeader("User-Agent", USER_AGENT);
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
            // not on ScreenScraper: IGDB and SteamGridDB may still have it
            const bool more = igdbReady() || !m_settings.value(QStringLiteral("sgdbKey")).toString().isEmpty();
            if (more) {
                fillMore();
                return;
            }
            m_not_found++;
            finishGame(status == 404 || status == 200 ? tr("not found") : tr("ScreenScraper error %1").arg(status), false);
            return;
        }
        parseScreenScraper(game);
        // what ScreenScraper doesn't have, from IGDB and SteamGridDB
        fillMore();
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
    };
    const QJsonArray medias = game.value(QStringLiteral("medias")).toArray();
    for (const Kind& kind : kinds) {
        const QString setting = QLatin1String(kind.setting);
        if (!wants(setting))
            continue;
        const QStringList types = QString::fromLatin1(kind.types).split(QLatin1Char(','));
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

// -- IGDB ---------------------------------------------------------------------------
// The login is your own Twitch app's Client ID and secret (dev.twitch.tv >
// Console > Register your application): a token from Twitch, then one POST
// per search to api.igdb.com. Its limit (4 requests a second) is well above
// what scraping one game at a time asks.

bool Scraper::igdbReady() const
{
    return !m_settings.value(QStringLiteral("igdbClientId")).toString().trimmed().isEmpty()
        && !m_settings.value(QStringLiteral("igdbClientSecret")).toString().trimmed().isEmpty();
}

void Scraper::igdbToken(std::function<void(int)> done)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), m_settings.value(QStringLiteral("igdbClientId")).toString().trimmed());
    query.addQueryItem(QStringLiteral("client_secret"), m_settings.value(QStringLiteral("igdbClientSecret")).toString().trimmed());
    query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("client_credentials"));
    // the login goes in the body, as Twitch documents it
    QNetworkRequest request{QUrl(QStringLiteral("https://id.twitch.tv/oauth2/token"))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    request.setTransferTimeout(60000);
    request.setRawHeader("User-Agent", USER_AGENT);
    QNetworkReply* const reply = m_net->post(request, query.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, done]{
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const QJsonObject answer = QJsonDocument::fromJson(body).object();
        reply->deleteLater();
        const QString token = answer.value(QStringLiteral("access_token")).toString();
        note(LOGMSG("IGDB: Twitch login answered %1%2").arg(status == 0 ? QStringLiteral("nothing (no connection?)") : QString::number(status))
             .arg(status == 200 && !token.isEmpty() ? QStringLiteral(", token received")
                  : QStringLiteral(": ") + QString::fromUtf8(body.left(200))));
        if (status == 200 && !token.isEmpty()) {
            m_igdb_token = token;
            // a minute early, so it doesn't run out mid-search
            m_igdb_expiry = QDateTime::currentDateTimeUtc().addSecs(qMax(60, answer.value(QStringLiteral("expires_in")).toInt() - 60));
        }
        done(status == 200 && token.isEmpty() ? 500 : status);
    });
}

// status -1: Twitch didn't accept the Client ID / secret
void Scraper::igdbPost(const QByteArray& query, std::function<void(int, const QByteArray&)> done,
                       int tries_left, bool retried)
{
    if (m_igdb_token.isEmpty() || QDateTime::currentDateTimeUtc() >= m_igdb_expiry) {
        igdbToken([this, query, done, tries_left, retried](int status){
            if (status != 200) {
                done(status == 0 || status >= 500 ? status : -1, {});
                return;
            }
            igdbPost(query, done, tries_left, retried);
        });
        return;
    }
    QNetworkRequest request(QUrl(QStringLiteral("https://api.igdb.com/v4/games")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/plain"));
    request.setRawHeader("Client-ID", m_settings.value(QStringLiteral("igdbClientId")).toString().trimmed().toUtf8());
    request.setRawHeader("Authorization", "Bearer " + m_igdb_token.toUtf8());
    request.setRawHeader("User-Agent", USER_AGENT);
    request.setTransferTimeout(60000);
    QNetworkReply* const reply = m_net->post(request, query);
    connect(reply, &QNetworkReply::finished, this, [this, reply, query, done, tries_left, retried]{
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        // the token ran out (or was revoked): a new one, once
        if (status == 401 && !retried) {
            m_igdb_token.clear();
            igdbPost(query, done, tries_left, true);
            return;
        }
        if ((status == 0 || status == 429 || status >= 500) && tries_left > 0 && !m_stop) {
            const int delay = status == 429 ? 1000 : qMax(1, m_settings.value(QStringLiteral("retryDelay")).toInt()) * 1000;
            QTimer::singleShot(delay, this, [this, query, done, tries_left, retried]{
                igdbPost(query, done, tries_left - 1, retried);
            });
            return;
        }
        done(status == 401 ? -1 : status, body);
    });
}

// the games of a name, on the collection's platform (else on any)
namespace {
// IGDB answers errors as a JSON list of {title, status, cause} objects
QJsonArray igdb_games(int status, const QByteArray& body)
{
    return status == 200 ? QJsonDocument::fromJson(body).array() : QJsonArray();
}
QString igdb_answer(int status, const QByteArray& body)
{
    if (status == 200)
        return LOGMSG("IGDB: answered 200, %1 games").arg(QJsonDocument::fromJson(body).array().size());
    const QJsonObject err = QJsonDocument::fromJson(body).array().at(0).toObject();
    const QString why = err.isEmpty()
        ? QString::fromUtf8(body.left(200))
        : err.value(QStringLiteral("title")).toString() + QLatin1String(": ") + err.value(QStringLiteral("cause")).toString();
    return LOGMSG("IGDB: answered %1, %2").arg(status).arg(why);
}
} // namespace

void Scraper::igdbFind(const QString& name, std::function<void(int, const QJsonArray&)> done)
{
    const Job& job = m_jobs[m_index];
    const int platform = igdb_platforms().value(job.system, 0);
    QString clean = name;
    clean.remove(QLatin1Char('"'));
    clean.remove(QLatin1Char('\\'));
    const auto query_for = [clean](int on_platform) {
        QString q = QStringLiteral("search \"%1\"; fields name,summary,storyline,first_release_date,"
                                   "total_rating,aggregated_rating,rating,genres.name,game_modes.name,"
                                   "multiplayer_modes.offlinemax,platforms.name,"
                                   "involved_companies.developer,involved_companies.publisher,"
                                   "involved_companies.company.name,cover.image_id,screenshots.image_id,"
                                   "artworks.image_id,artworks.width,artworks.height;").arg(clean);
        if (on_platform > 0)
            q += QStringLiteral(" where platforms = (%1);").arg(on_platform);
        q += QStringLiteral(" limit 10;");
        return q.toUtf8();
    };
    const int tries = m_settings.value(QStringLiteral("retries")).toInt();
    note(LOGMSG("IGDB: searching \"%1\" (system `%2`, platform %3)").arg(clean, job.system).arg(platform));
    igdbPost(query_for(platform), [this, platform, query_for, tries, done, clean](int status, const QByteArray& body){
        // an error comes back as a list too (of error objects), so only a 200 is a list of games
        const QJsonArray games = igdb_games(status, body);
        note(igdb_answer(status, body));
        if (status == 200 && games.isEmpty() && platform > 0 && !m_stop) {
            note(LOGMSG("IGDB: nothing on that platform, searching \"%1\" on every platform").arg(clean));
            igdbPost(query_for(0), [this, done](int st, const QByteArray& b){
                note(igdb_answer(st, b));
                done(st, igdb_games(st, b));
            }, tries);
            return;
        }
        done(status, games);
    }, tries);
}

QVariantMap Scraper::igdbCandidate(const QJsonObject& game) const
{
    const QDate released = igdb_release(game);
    QStringList info = igdb_names(game, QStringLiteral("platforms")).mid(0, 2);
    if (released.isValid())
        info.append(QString::number(released.year()));
    const int rating = igdb_rating(game);
    const int players = igdb_players(game);
    return QVariantMap {
        { QStringLiteral("name"), game.value(QStringLiteral("name")).toString() },
        { QStringLiteral("info"), info.join(QStringLiteral(" · ")) },
        { QStringLiteral("description"), igdb_description(game) },
        { QStringLiteral("thumb"), igdb_image(game.value(QStringLiteral("cover")), "cover_big") },
        { QStringLiteral("developer"), igdb_companies(game, QStringLiteral("developer")).join(QStringLiteral(", ")) },
        { QStringLiteral("publisher"), igdb_companies(game, QStringLiteral("publisher")).join(QStringLiteral(", ")) },
        { QStringLiteral("genre"), igdb_names(game, QStringLiteral("genres")).join(QStringLiteral(", ")) },
        { QStringLiteral("players"), players > 0 ? QString::number(players) : QString() },
        { QStringLiteral("release"), released.isValid() ? released.toString(Qt::ISODate) : QString() },
        { QStringLiteral("rating"), rating > 0 ? QString::number(rating) + QLatin1Char('%') : QString() },
    };
}

void Scraper::igdbSearch(const QString& name)
{
    igdbFind(name, [this](int status, const QJsonArray& games){
        if (m_stop) {
            next();
            return;
        }
        if (status == -1) {
            m_stop_reason = tr("IGDB didn't accept the Client ID or secret: check Account settings.");
            m_stop = true;
            finishGame(m_stop_reason, false);
            return;
        }
        if (status != 200) {
            m_failed++;
            finishGame(status == 0
                ? tr("IGDB: no connection")
                : tr("IGDB error %1: see Scraper > Log").arg(status), false);
            return;
        }
        m_igdb_results = games;
        QVariantList candidates;
        for (const QJsonValue& v : games)
            candidates.append(igdbCandidate(v.toObject()));
        m_choice_source = QStringLiteral("igdb");
        offerChoice(candidates);
    });
}

// without asking: the game of the same name, else IGDB's best match
void Scraper::igdbAuto(bool after_other)
{
    m_igdb_done = true;
    const Job& job = m_jobs[m_index];
    const bool by_title = m_settings.value(QStringLiteral("useMetadataNames")).toBool() && !job.title_is_file_name;
    QString name = by_title ? job.title : job.search_name;
    if (after_other && m_changes.contains(QStringLiteral("title")))
        name = m_changes.value(QStringLiteral("title")).toString();  // the name the first source found
    igdbFind(name, [this, name, after_other](int status, const QJsonArray& games){
        if (m_stop) {
            next();
            return;
        }
        if (status == -1 && !after_other) {
            m_stop_reason = tr("IGDB didn't accept the Client ID or secret: check Account settings.");
            m_stop = true;
            finishGame(m_stop_reason, false);
            return;
        }
        if (status != 200)
            Log::warning(LOG_TAG, LOGMSG("IGDB: %1 for `%2`").arg(status).arg(name));
        if (status == 200 && !games.isEmpty()) {
            QJsonObject game = games.first().toObject();
            for (const QJsonValue& v : games) {
                if (normalized(v.toObject().value(QStringLiteral("name")).toString()) == normalized(name)) {
                    game = v.toObject();
                    break;
                }
            }
            parseIgdb(game, after_other);
        }
        fillMore();
    });
}

// IGDB's game into the changes; after another source, only what's still missing
void Scraper::parseIgdb(const QJsonObject& game, bool after_other)
{
    const Job& job = m_jobs[m_index];
    note(LOGMSG("IGDB: using \"%1\"%2").arg(game.value(QStringLiteral("name")).toString())
         .arg(after_other ? QStringLiteral(" for what's still missing") : QString()));
    const bool overwrite = m_settings.value(QStringLiteral("overwrite")).toBool();
    const auto open_key = [this, overwrite](const char* key, bool has) {
        return !m_changes.contains(QLatin1String(key)) && (overwrite || !has);
    };

    if (!after_other && wants(QStringLiteral("getNames")) && (overwrite || job.title_is_file_name)) {
        const QString name = game.value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty() && name != job.title)
            m_changes.insert(QStringLiteral("title"), name);
    }
    if (wants(QStringLiteral("getRatings")) && open_key("rating", job.has_rating)) {
        const int rating = igdb_rating(game);
        if (rating > 0)
            m_changes.insert(QStringLiteral("rating"), QString::number(rating) + QLatin1Char('%'));
    }
    if (wants(QStringLiteral("getDetails"))) {
        const QString description = igdb_description(game);
        if (!description.isEmpty() && open_key("description", job.has_description)) {
            m_changes.insert(QStringLiteral("description"), description);
            m_changes.insert(QStringLiteral("summary"), QString());
        }
        const QStringList developers = igdb_companies(game, QStringLiteral("developer"));
        if (!developers.isEmpty() && open_key("developer", job.has_developer))
            m_changes.insert(QStringLiteral("developer"), developers);
        const QStringList publishers = igdb_companies(game, QStringLiteral("publisher"));
        if (!publishers.isEmpty() && open_key("publisher", job.has_publisher))
            m_changes.insert(QStringLiteral("publisher"), publishers);
        const int players = igdb_players(game);
        if (players > 0 && open_key("players", job.has_players))
            m_changes.insert(QStringLiteral("players"), QString::number(players));
        const QDate released = igdb_release(game);
        if (released.isValid() && open_key("release", job.has_release))
            m_changes.insert(QStringLiteral("release"), released.toString(Qt::ISODate));
        const QStringList genres = igdb_names(game, QStringLiteral("genres"));
        if (!genres.isEmpty() && open_key("genre", job.has_genre))
            m_changes.insert(QStringLiteral("genre"), genres);
    }

    // the art: the cover, a screenshot, an artwork (as fan art)
    const auto add = [this, &job](const QString& url, const char* setting, const char* slot,
                                  const char* folder, const char* kind) {
        if (url.isEmpty() || !wants(QLatin1String(setting)))
            return;
        Media media;
        media.setting = QLatin1String(setting);
        media.slot = QLatin1String(slot);
        media.folder = QLatin1String(folder);
        media.kind = QLatin1String(kind);
        media.url = QUrl(url);
        media.ext = QStringLiteral("jpg");
        for (const Media& m : m_media) {
            if (m.slot == media.slot && m.kind == media.kind)
                return;  // the first source had it
        }
        if (media.url.isValid() && slotFree(job, media))
            m_media.push_back(media);
    };
    const size_t media_before = m_media.size();
    add(igdb_image(game.value(QStringLiteral("cover")), "cover_big_2x"), "getCovers", "assets.box_front", "covers", "2d");
    add(igdb_image(game.value(QStringLiteral("screenshots")).toArray().at(0), "1080p"), "getScreenshots", "assets.screenshot", "screenshots", "");
    // the artwork closest to 16:9 (IGDB has them in every shape)
    QJsonValue best_art;
    double best_off = 1e9;
    for (const QJsonValue& v : game.value(QStringLiteral("artworks")).toArray()) {
        const QJsonObject a = v.toObject();
        const double w = a.value(QStringLiteral("width")).toDouble();
        const double h = a.value(QStringLiteral("height")).toDouble();
        const double off = (w > 0 && h > 0) ? qAbs(w / h - 16.0 / 9.0) : 0.5;
        if (off < best_off) {
            best_off = off;
            best_art = v;
        }
    }
    add(igdb_image(best_art, "1080p"), "getFanart", "assets.background", "fanart", "");
    QStringList kept;
    for (auto it = m_changes.cbegin(); it != m_changes.cend(); ++it)
        kept.append(it.key());
    note(LOGMSG("IGDB: details to save: %1; pictures to download: %2 (cover %3, screenshots %4, artworks %5)%6")
         .arg(kept.isEmpty() ? QStringLiteral("none") : kept.join(QStringLiteral(", ")))
         .arg(m_media.size() - media_before)
         .arg(game.value(QStringLiteral("cover")).isObject() ? QStringLiteral("yes") : QStringLiteral("no"))
         .arg(game.value(QStringLiteral("screenshots")).toArray().size())
         .arg(game.value(QStringLiteral("artworks")).toArray().size())
         .arg(!overwrite && m_media.size() == media_before ? QStringLiteral(" (Overwrite is off: slots that have a file keep it)") : QString()));
}

void Scraper::fillMore()
{
    const QString source = m_settings.value(QStringLiteral("source")).toString();
    if (!m_igdb_done && igdbReady() && source != QLatin1String("steamgriddb")
            && m_settings.value(QStringLiteral("igdbFill"), true).toBool()) {
        igdbAuto(true);
        return;
    }
    if (!m_settings.value(QStringLiteral("sgdbKey")).toString().trimmed().isEmpty()) {
        steamGridDb();
        return;
    }
    if (m_media.empty() && m_changes.isEmpty()) {
        m_not_found++;
        finishGame(tr("not found"), false);
        return;
    }
    downloadNext();
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
        // all downloaded: the miximage made from them
        if (!m_mix_done && wantsMiximage()) {
            m_mix_done = true;
            makeMiximage();
            return;
        }
        save();
        return;
    }
    const Media media = m_media.front();
    m_media.erase(m_media.begin());
    const Job& job = m_jobs[m_index];
    get(media.url, {}, 1, [this, media, job](int status, const QByteArray& body){
        if (status != 200 || body.isEmpty()) {
            m_picture_failures++;
            m_picture_error = status;
            note(LOGMSG("Download failed (%1): %2").arg(status).arg(media.url.toString()));
            downloadNext();
            return;
        }
        note(LOGMSG("Downloaded %1 (%2 KB) -> %3").arg(media.folder).arg(body.size() / 1024).arg(media.url.toString()));
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
        const bool fanart_16_9 = media.folder == QLatin1String("fanart")
            && m_settings.value(QStringLiteral("fanartFit"), QStringLiteral("original")).toString() == QLatin1String("16:9");
        if (picture && (max_w > 0 || trim || fanart_16_9)) {
            auto* const watcher = new QFutureWatcher<QPair<QByteArray, QString>>(this);
            connect(watcher, &QFutureWatcher<QPair<QByteArray, QString>>::finished, this, [this, watcher, media, job]{
                const QPair<QByteArray, QString> result = watcher->result();
                watcher->deleteLater();
                storeMedia(media, job, result.first, result.second);
            });
            watcher->setFuture(QtConcurrent::run([body, ext, max_w, max_h, trim, fanart_16_9]{
                return process_image(body, ext, max_w, max_h, trim, fanart_16_9);
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
    const QString dir = m_data_dir % QLatin1Char('/') % safe_file_name(job.media_folder.isEmpty() ? job.collection : job.media_folder)
                      % QLatin1Char('/') % media.folder;
    QDir().mkpath(dir);
    // the art folder is no place for the gallery app
    QFile nomedia(m_data_dir % QStringLiteral("/.nomedia"));
    if (!nomedia.exists() && nomedia.open(QIODevice::WriteOnly))
        nomedia.close();
    const bool has_file = !job.path.isEmpty() && !job.path.startsWith(QLatin1String("android:"));
    // (a content:// entry: its last part, decoded)
    const QString file_part = job.path.contains(QLatin1String("://"))
        ? QUrl::fromPercentEncoding(job.path.toUtf8()).section(QLatin1Char('/'), -1)
        : job.path;
    const QString base = safe_file_name(has_file ? QFileInfo(file_part).completeBaseName() : title);
    const QString file_path = dir % QLatin1Char('/') % base % QLatin1Char('.') % ext;
    QSaveFile file(file_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(body) != body.size() || !file.commit()) {
        m_picture_failures++;
        note(LOGMSG("Couldn't write `%1`: %2").arg(file_path, file.errorString()));
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
            const bool is3d = pegasus_assets::is_3d_box(f);
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

bool Scraper::offline() const
{
    return m_settings.value(QStringLiteral("offlineMix")).toBool();
}

bool Scraper::wantsMiximage() const
{
    return offline() || wants(QStringLiteral("getMiximages"));
}

// the game's miximage from its screenshot, logo, box and disc (the ones just
// scraped, else the ones it had)
void Scraper::makeMiximage()
{
    const Job& job = m_jobs[m_index];
    const auto files_of = [this, &job](const QString& key) {
        return m_changes.contains(key) ? m_changes.value(key).toStringList() : job.slot_files.value(key).toStringList();
    };
    if (!files_of(QStringLiteral("assets.steam")).isEmpty()
            && !m_settings.value(QStringLiteral("mixOverwrite"), true).toBool()) {
        m_mix_error = tr("has a miximage already");
        save();
        return;
    }
    // the first local file of a kind (boxes: 3D or 2D)
    const auto first_local = [&files_of](const QString& key, int want3d) {
        for (const QString& f : files_of(key)) {
            const QString path = f.startsWith(QLatin1String("file:")) ? QUrl(f).toLocalFile() : f;
            if (path.isEmpty() || path.contains(QLatin1String("://")))
                continue;
            const bool is3d = pegasus_assets::is_3d_box(path);
            if ((want3d == 0 && is3d) || (want3d == 1 && !is3d))
                continue;
            if (QFileInfo(path).isFile())
                return path;
        }
        return QString();
    };
    miximage::Sources sources;
    sources.screenshot = first_local(QStringLiteral("assets.screenshot"), -1);
    sources.marquee = first_local(QStringLiteral("assets.logo"), -1);
    sources.box3d = first_local(QStringLiteral("assets.box_front"), 1);
    sources.cover = first_local(QStringLiteral("assets.box_front"), 0);
    sources.physical = first_local(QStringLiteral("assets.cartridge"), -1);
    if (sources.screenshot.isEmpty()) {
        m_mix_error = tr("no screenshot for a miximage");
        save();
        return;
    }
    const miximage::Options options = miximage::Options::from(m_settings);
    auto* const watcher = new QFutureWatcher<miximage::Result>(this);
    connect(watcher, &QFutureWatcher<miximage::Result>::finished, this, [this, watcher]{
        const miximage::Result result = watcher->result();
        watcher->deleteLater();
        if (m_stop || result.data.isEmpty()) {
            m_mix_error = result.error;
            save();
            return;
        }
        Media media;
        media.setting = QStringLiteral("getMiximages");
        media.slot = QStringLiteral("assets.steam");
        media.folder = QStringLiteral("miximages");
        media.ext = result.ext;
        storeMedia(media, m_jobs[m_index], result.data, result.ext);  // then saves
    });
    watcher->setFuture(QtConcurrent::run([sources, options]{ return miximage::make(sources, options); }));
}

void Scraper::save()
{
    const Job& job = m_jobs[m_index];
    if (m_changes.isEmpty() && !m_mix_error.isEmpty()) {
        m_not_found++;
        finishGame(m_mix_error, false);
        return;
    }
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
    note(LOGMSG("Saving `%1` (%2) into the metadata of `%3`: %4").arg(title, job.path, job.collection)
         .arg(error.isEmpty() ? QStringLiteral("ok") : error));
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

// the game as the Scraper page shows it: its picture (the new one, else the
// one it had) and the details found
QVariantMap Scraper::previewOf(const Job& job) const
{
    const auto files_of = [this, &job](const QString& key) {
        return m_changes.contains(key) ? m_changes.value(key).toStringList() : job.slot_files.value(key).toStringList();
    };
    QString picture;
    for (const char* key : { "assets.box_front", "assets.screenshot", "assets.steam", "assets.logo" }) {
        for (const QString& f : files_of(QLatin1String(key))) {
            if (f.startsWith(QLatin1String("image://")) || f.startsWith(QLatin1String("file:"))) {
                picture = f;
                break;
            }
            if (!f.contains(QLatin1String("://")) && QFileInfo(f).isFile()) {
                picture = QUrl::fromLocalFile(f).toString();
                break;
            }
        }
        if (!picture.isEmpty())
            break;
    }
    const auto text = [this](const char* key) {
        const QVariant v = m_changes.value(QLatin1String(key));
        return v.type() == QVariant::StringList ? v.toStringList().join(QStringLiteral(", ")) : v.toString();
    };
    return QVariantMap {
        { QStringLiteral("title"), m_changes.value(QStringLiteral("title"), job.title).toString() },
        { QStringLiteral("collection"), job.collection },
        { QStringLiteral("picture"), picture },
        { QStringLiteral("description"), text("description") },
        { QStringLiteral("developer"), text("developer") },
        { QStringLiteral("publisher"), text("publisher") },
        { QStringLiteral("genre"), text("genre") },
        { QStringLiteral("release"), text("release") },
        { QStringLiteral("players"), text("players") },
        { QStringLiteral("rating"), text("rating") },
    };
}

void Scraper::finishGame(const QString& given_result, bool ok)
{
    QString result = given_result;
    if (m_picture_failures > 0) {
        const QString failed = tr("%n picture(s) didn't download (%1)", "", m_picture_failures)
            .arg(m_picture_error == 0 ? tr("no connection") : tr("error %1").arg(m_picture_error));
        result = result.isEmpty() ? failed : result + QStringLiteral(" · ") + failed;
    }
    if (m_index < m_jobs.size()) {
        m_last_title = m_jobs[m_index].title;
        m_preview = previewOf(m_jobs[m_index]);
    }
    m_last_result = result;
    m_last_ok = ok;
    m_index++;
    report();
    // a short pause between games keeps within ScreenScraper's pace
    QTimer::singleShot(300, this, [this]{ next(); });
}

} // namespace scraper
