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
#include "providers/GameDirectory.h"
#include "providers/esde/EsdeProvider.h"
#endif

#include "PegasusAssets.h"
#include <QBuffer>
#include <algorithm>
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

// a name without its (tags) and [tags], normalized: "Chrono Trigger (USA)" -> "chronotrigger"
QString plain_key(const QString& name)
{
    static const QRegularExpression rx_tags(QStringLiteral("\\s*[\\(\\[][^\\)\\]]*[\\)\\]]"));
    QString t = name;
    t.remove(rx_tags);
    return normalized(t);
}

// how sure a match is, 0..100: the same name is 100, else the share of
// words in common (so "Mario Kart 64" vs "Mario Kart 8" stays low)
int name_score(const QString& wanted, const QString& found)
{
    if (plain_key(wanted) == plain_key(found))
        return 100;
    const auto words = [](const QString& text) {
        static const QRegularExpression rx_tags(QStringLiteral("\\s*[\\(\\[][^\\)\\]]*[\\)\\]]"));
        QString t = text.toLower();
        t.remove(rx_tags);
        QSet<QString> out;
        QString cur;
        for (const QChar c : t + QLatin1Char(' ')) {
            if (c.isLetterOrNumber()) {
                cur += c;
            }
            else if (!cur.isEmpty()) {
                if (cur != QLatin1String("the") && cur != QLatin1String("of") && cur != QLatin1String("a"))
                    out.insert(cur);
                cur.clear();
            }
        }
        return out;
    };
    const QSet<QString> a = words(wanted);
    const QSet<QString> b = words(found);
    if (a.isEmpty() || b.isEmpty())
        return 0;
    const int common = QSet<QString>(a).intersect(b).size();
    return static_cast<int>(90.0 * common / qMax(a.size(), b.size()));
}

// libretro's thumbnail folder of a system (ES-DE's names), empty if none
QString libretro_system(const QString& system)
{
    static const QHash<QString, QString> map {
        { "snes", "Nintendo - Super Nintendo Entertainment System" },
        { "snesna", "Nintendo - Super Nintendo Entertainment System" },
        { "sfc", "Nintendo - Super Nintendo Entertainment System" },
        { "nes", "Nintendo - Nintendo Entertainment System" },
        { "famicom", "Nintendo - Nintendo Entertainment System" },
        { "fds", "Nintendo - Family Computer Disk System" },
        { "n64", "Nintendo - Nintendo 64" },
        { "gb", "Nintendo - Game Boy" },
        { "gbc", "Nintendo - Game Boy Color" },
        { "gba", "Nintendo - Game Boy Advance" },
        { "nds", "Nintendo - Nintendo DS" },
        { "n3ds", "Nintendo - Nintendo 3DS" },
        { "gc", "Nintendo - GameCube" },
        { "wii", "Nintendo - Wii" },
        { "virtualboy", "Nintendo - Virtual Boy" },
        { "pokemini", "Nintendo - Pokemon Mini" },
        { "satellaview", "Nintendo - Satellaview" },
        { "sufami", "Nintendo - Sufami Turbo" },
        { "megadrive", "Sega - Mega Drive - Genesis" },
        { "genesis", "Sega - Mega Drive - Genesis" },
        { "mastersystem", "Sega - Master System - Mark III" },
        { "gamegear", "Sega - Game Gear" },
        { "segacd", "Sega - Mega-CD - Sega CD" },
        { "megacd", "Sega - Mega-CD - Sega CD" },
        { "sega32x", "Sega - 32X" },
        { "saturn", "Sega - Saturn" },
        { "dreamcast", "Sega - Dreamcast" },
        { "sg-1000", "Sega - SG-1000" },
        { "psx", "Sony - PlayStation" },
        { "ps2", "Sony - PlayStation 2" },
        { "psp", "Sony - PlayStation Portable" },
        { "psvita", "Sony - PlayStation Vita" },
        { "pcengine", "NEC - PC Engine - TurboGrafx 16" },
        { "tg16", "NEC - PC Engine - TurboGrafx 16" },
        { "pcenginecd", "NEC - PC Engine CD - TurboGrafx-CD" },
        { "tg-cd", "NEC - PC Engine CD - TurboGrafx-CD" },
        { "supergrafx", "NEC - PC Engine SuperGrafx" },
        { "pcfx", "NEC - PC-FX" },
        { "atari2600", "Atari - 2600" },
        { "atari5200", "Atari - 5200" },
        { "atari7800", "Atari - 7800" },
        { "atarilynx", "Atari - Lynx" },
        { "atarijaguar", "Atari - Jaguar" },
        { "atarist", "Atari - ST" },
        { "ngp", "SNK - Neo Geo Pocket" },
        { "ngpc", "SNK - Neo Geo Pocket Color" },
        { "neogeocd", "SNK - Neo Geo CD" },
        { "wonderswan", "Bandai - WonderSwan" },
        { "wonderswancolor", "Bandai - WonderSwan Color" },
        { "colecovision", "Coleco - ColecoVision" },
        { "intellivision", "Mattel - Intellivision" },
        { "vectrex", "GCE - Vectrex" },
        { "msx", "Microsoft - MSX" },
        { "msx2", "Microsoft - MSX2" },
        { "3do", "The 3DO Company - 3DO" },
        { "c64", "Commodore - 64" },
        { "amiga", "Commodore - Amiga" },
        { "dos", "DOS" },
        { "scummvm", "ScummVM" },
    };
    return map.value(system);
}

// libretro's file name for a game name: & * / : ` < > ? \ | are _
QString libretro_file_name(const QString& name)
{
    QString out = name;
    for (const QChar c : QStringLiteral("&*/:`<>?\\|"))
        out.replace(c, QLatin1Char('_'));
    return out;
}

// the region of a ROM name, as No-Intro writes it: "(USA)", "(Europe)", ...
QString region_of(const QString& name)
{
    static const QRegularExpression rx(QStringLiteral("\\((USA|Europe|Japan|World|[A-Z][a-z]+(?:, [A-Z][a-z]+)*)\\)"));
    const auto m = rx.match(name);
    return m.hasMatch() ? m.captured(1) : QString();
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

// the file's MD5, read in 1 MiB parts so Stop can end it (`cancel`)
QString md5_of(const QString& path, qint64 max_bytes, const std::atomic<bool>* cancel)
{
    QFile file(path);
    if (max_bytes <= 0 || file.size() > max_bytes || !file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Md5);
    QByteArray part;
    while (!file.atEnd()) {
        if (cancel && cancel->load())
            return {};
        part = file.read(1 << 20);
        if (part.isEmpty())
            return {};
        hash.addData(part);
    }
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
// fan art stretched to 16:9: nothing cut, everything made taller (or wider)
QImage stretch_16_9(const QImage& src)
{
    const double ratio = static_cast<double>(src.width()) / static_cast<double>(src.height());
    const double wanted = 16.0 / 9.0;
    if (ratio > wanted * 0.97 && ratio < wanted * 1.03)
        return src;
    // the longer side kept, the other one changed to fit
    if (ratio > wanted)
        return src.scaled(src.width(), qRound(src.width() / wanted), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return src.scaled(qRound(src.height() * wanted), src.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage fit_16_9(const QImage& src)
{
    // 16:9 by cropping, never by padding: the middle of a taller picture,
    // the middle of a wider one (fan art is often banner-wide);
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

// fanart_mode: 0 as downloaded, 1 cropped to 16:9, 2 stretched to 16:9
QPair<QByteArray, QString> process_image(const QByteArray& data, const QString& ext, int max_w, int max_h, bool trim, int fanart_mode)
{
    QImage img;
    if (!img.loadFromData(data))
        return { data, ext };
    bool changed = false;
    if (fanart_mode > 0) {
        const QImage shaped = fanart_mode == 2 ? stretch_16_9(img) : fit_16_9(img);
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

// a URL as it's logged: ScreenScraper's media links carry the developer
// and user passwords in their query, so the query never goes to the log
QString safe_url(const QUrl& url)
{
    return url.toString(QUrl::RemoveQuery | QUrl::RemoveUserInfo);
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
    m_cancel_hash = false;
    m_transport_streak = 0;
    m_libretro_lists.clear();
    m_backed_up.clear();
    m_running = true;
    m_run++;  // answers still on their way from the last run are ignored
    note(LOGMSG("Scraping %1 games into `%2` (source: %3, interactive: %4, overwrite: %5, ScreenScraper login: %6, LaunchBox database: %7)")
        .arg(m_jobs.size()).arg(m_data_dir)
        .arg(m_settings.value(QStringLiteral("source")).toString())
        .arg(interactive() ? QStringLiteral("yes") : QStringLiteral("no"))
        .arg(m_settings.value(QStringLiteral("overwrite")).toBool() ? QStringLiteral("yes") : QStringLiteral("no"))
        .arg(m_dev_id.isEmpty() ? QStringLiteral("missing") : QStringLiteral("yes"))
        .arg(lbReady() ? QStringLiteral("yes") : QStringLiteral("missing")));
    // which system each collection is looked up as
    QMap<QString, QString> systems;
    for (const Job& job : m_jobs)
        systems.insert(job.collection, job.system);
    for (auto it = systems.cbegin(); it != systems.cend(); ++it) {
        const bool known = screenscraper_systems().contains(it.value());
        note(LOGMSG("Collection `%1`: system `%2`%3").arg(it.key(),
            it.value().isEmpty() ? QStringLiteral("?") : it.value(),
            known ? QString() : QStringLiteral(" (not a ScreenScraper system: LaunchBox and libretro only)")));
    }
    report();
    next();
}

void Scraper::stop()
{
    if (!m_running)
        return;
    m_stop = true;
    // what's on its way ends now (a download, a hash), not when it's done
    m_cancel_hash = true;
    const QSet<QNetworkReply*> replies = m_replies;
    for (QNetworkReply* const reply : replies)
        reply->abort();
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

// the matches for a name: the source's (ScreenScraper or the LaunchBox
// database, with details and art), else the other's
void Scraper::search(const QString& name)
{
    const Job& job = m_jobs[m_index];
    const QString source = m_settings.value(QStringLiteral("source")).toString();
    const bool ss_ok = source == QLatin1String("screenscraper") && !m_dev_id.isEmpty()
        && screenscraper_systems().contains(job.system);
    const bool lb_ok = lbReady() && m_lb->hasSystem(job.system);
    if (ss_ok)
        ssSearch(name);
    else if (lb_ok)
        lbSearch(name);
    else {
        m_not_found++;
        QString why;
        if (source == QLatin1String("screenscraper") && m_dev_id.isEmpty())
            why = tr("ScreenScraper isn't in this build yet");
        else if (source == QLatin1String("screenscraper"))
            why = tr("ScreenScraper doesn't know the system `%1`").arg(job.system);
        else if (source == QLatin1String("launchbox") && !lbReady())
            why = tr("the LaunchBox database isn't downloaded yet (Scraper > Account settings)");
        else if (source == QLatin1String("launchbox"))
            why = tr("LaunchBox has no platform for the system `%1`").arg(job.system);
        else
            why = tr("no source is set up");
        note(LOGMSG("%1: %2").arg(name, why));
        if (!libretro_system(job.system).isEmpty() && m_settings.value(QStringLiteral("useLibretro"), true).toBool()) {
            // still: the free pictures (libretro's thumbnails), by checksum or name
            m_not_found--;
            identify([this]{ fillMore(); });
            return;
        }
        finishGame(why, false);
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

void Scraper::offerChoice(const QVariantList& candidates)
{
    // one match and Auto-accept on: no need to ask (scraping one game always asks)
    if (candidates.size() == 1 && m_settings.value(QStringLiteral("autoAcceptSingle")).toBool()
            && m_jobs.size() > 1) {
        m_waiting = true;
        choose(0);
        return;
    }
    // the picker at once; the pictures follow one at a time (downloaded here,
    // so they show even where the picker couldn't load them itself), the
    // highlighted one first: a burst of requests is what ScreenScraper's
    // one-thread accounts refuse
    m_waiting = true;
    const Job& job = m_jobs[m_index];
    QVariantList list = candidates;
    m_thumb_queue.clear();
    for (int i = 0; i < list.size(); i++) {
        QVariantMap c = list[i].toMap();
        const QString url = c.value(QStringLiteral("thumb")).toString();
        if (url.startsWith(QLatin1String("http"))) {
            c.insert(QStringLiteral("remoteThumb"), url);
            c.insert(QStringLiteral("thumb"), QString());
            m_thumb_queue.push_back(i);
        }
        list[i] = c;
    }
    m_choice = QVariantMap {
        { QStringLiteral("title"), job.title },
        { QStringLiteral("collection"), job.collection },
        { QStringLiteral("searchName"), job.title_is_file_name ? job.search_name : job.title },
        { QStringLiteral("source"), m_choice_source == QLatin1String("ss") ? QStringLiteral("ScreenScraper")
                                    : QStringLiteral("LaunchBox") },
        { QStringLiteral("candidates"), list },
    };
    note(LOGMSG("%1: %2 matches for \"%3\"").arg(m_choice.value(QStringLiteral("source")).toString())
         .arg(list.size()).arg(m_choice.value(QStringLiteral("searchName")).toString()));
    report();
    emit choiceNeeded(m_choice);
    m_thumb_busy = false;
    nextThumb();
}

void Scraper::thumbFirst(int index)
{
    const auto it = std::find(m_thumb_queue.begin(), m_thumb_queue.end(), index);
    if (it == m_thumb_queue.end() || it == m_thumb_queue.begin())
        return;
    m_thumb_queue.erase(it);
    m_thumb_queue.insert(m_thumb_queue.begin(), index);
}

void Scraper::nextThumb()
{
    if (m_thumb_busy || m_thumb_queue.empty() || !m_waiting)
        return;
    const int index = m_thumb_queue.front();
    m_thumb_queue.erase(m_thumb_queue.begin());
    QVariantList list = m_choice.value(QStringLiteral("candidates")).toList();
    if (index < 0 || index >= list.size()) {
        nextThumb();
        return;
    }
    const QString url = list[index].toMap().value(QStringLiteral("remoteThumb")).toString();
    const QString dir = paths::writableCacheDir() + QStringLiteral("/scraper-pictures");
    QDir().mkpath(dir);
    const QString name = QString::fromLatin1(QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Md5).toHex());
    const QString path = dir + QLatin1Char('/') + name + QStringLiteral(".img");
    const auto deliver = [this, index, path](int error) {
        QVariantList now = m_choice.value(QStringLiteral("candidates")).toList();
        if (index < now.size()) {
            QVariantMap c = now[index].toMap();
            if (error == 0)
                c.insert(QStringLiteral("thumb"), QUrl::fromLocalFile(path).toString());
            else
                c.insert(QStringLiteral("thumbError"), error);
            now[index] = c;
            m_choice.insert(QStringLiteral("candidates"), now);
        }
        emit thumbReady(index, error == 0 ? QUrl::fromLocalFile(path).toString() : QString(), error);
    };
    // fetched before (an earlier search of the same game)
    if (QFileInfo(path).size() > 0) {
        deliver(0);
        nextThumb();
        return;
    }
    m_thumb_busy = true;
    const int run = m_run;
    get(QUrl(url), {}, 0, [this, run, url, path, deliver](int status, const QByteArray& body){
        m_thumb_busy = false;
        if (run != m_run || !m_waiting)
            return;  // picked, skipped or stopped meanwhile
        QBuffer buffer;
        buffer.setData(body);
        buffer.open(QIODevice::ReadOnly);
        int error = status == 200 ? 0 : (status == 0 ? -2 : status);
        if (error == 0 && QImageReader(&buffer).canRead()) {
            QFile file(path);
            if (!(file.open(QIODevice::WriteOnly) && file.write(body) == body.size()))
                error = -3;
        }
        else if (error == 0) {
            error = -4;  // not a picture
        }
        if (error != 0)
            Log::warning(LOG_TAG, LOGMSG("Picture `%1`: %2").arg(safe_url(QUrl(url))).arg(status));
        deliver(error);
        nextThumb();
    });
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
    if (index < 0 || index >= static_cast<int>(m_lb_results.size())) {
        m_waiting = true;
        skip();
        return;
    }
    m_lb_done = true;
    parseLaunchBox(m_lb_results[static_cast<size_t>(index)], false);
    fillMore();
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
    m_match.clear();
    m_md5.clear();
    m_hash_name.clear();
    m_known_lb_id = job.launchbox_id;
    m_identified = job.launchbox_id > 0;  // picked by you: no need to ask by checksum
    m_review_hint.clear();
    m_libretro_done = false;
    m_got.clear();
    m_lb_done = false;
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
    if (!ss_ok) {
        // who the game is by its checksum first (keyless), then LaunchBox's
        // database (details and art, no login), then libretro's pictures
        identify([this]{
            const Job& job = m_jobs[m_index];
            if (m_known_lb_id > 0 && lbReady()) {
                // the match you picked in Needs your check
                const LaunchBoxDb::Game game = m_lb->game(job.system, m_known_lb_id);
                m_lb_done = true;
                if (game.id > 0)
                    parseLaunchBox(game, false);
                fillMore();
            }
            else if (lbReady() && m_lb->hasSystem(job.system))
                lbAuto(false);
            else
                fillMore();
        });
        return;
    }

    hashThen([this](const QString& md5){ screenScraper(md5); });
}

// the file's MD5, if it may be made (Use checksums, a file small enough, not
// an archive: the sources know the ROM inside, not the .zip), off the main
// thread; empty otherwise
void Scraper::hashThen(std::function<void(const QString&)> then)
{
    const Job& job = m_jobs[m_index];
    static const QStringList archives { QStringLiteral("zip"), QStringLiteral("7z"), QStringLiteral("rar") };
    const bool use_hash = m_settings.value(QStringLiteral("useHashes")).toBool()
        && !job.path.startsWith(QLatin1String("android:")) && QFileInfo(job.path).isFile()
        && !archives.contains(QFileInfo(job.path).suffix().toLower());
    if (!use_hash) {
        then(QString());
        return;
    }
    const qint64 max_bytes = static_cast<qint64>(m_settings.value(QStringLiteral("hashMaxMiB")).toInt()) * 1024 * 1024;
    const QString path = job.path;
    const int run = m_run;
    const size_t index = m_index;
    auto* const watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, then, run, index]{
        const QString md5 = watcher->result();
        watcher->deleteLater();
        if (run != m_run || index != m_index)
            return;
        then(md5);
    });
    const std::atomic<bool>* cancel = &m_cancel_hash;
    watcher->setFuture(QtConcurrent::run([path, max_bytes, cancel]{ return md5_of(path, max_bytes, cancel); }));
}

// Hasheous (no key): the game's name by its checksum, so LaunchBox and the
// thumbnails are looked up by its real name instead of a guessed one
void Scraper::identify(std::function<void()> then)
{
    if (m_identified || !m_settings.value(QStringLiteral("useHasheous"), true).toBool()) {
        then();
        return;
    }
    m_identified = true;
    hashThen([this, then](QString md5){
#ifdef PEGASUSX_TESTENV
        // the test environment's ROMs are made up: a real game's checksum
        if (!qEnvironmentVariable("PEGASUSX_TEST_MD5").isEmpty())
            md5 = qEnvironmentVariable("PEGASUSX_TEST_MD5");
#endif
        if (md5.isEmpty() || m_stop) {
            then();
            return;
        }
        m_md5 = md5;
        const int run = m_run;
        const size_t index = m_index;
        const QUrl url(QStringLiteral("https://hasheous.org/api/v1/Lookup/ByHash/md5/") + md5);
        get(url, {}, 1, [this, then, run, index](int status, const QByteArray& body){
            if (run != m_run || index != m_index)
                return;
            const QJsonDocument doc = QJsonDocument::fromJson(body);
            const QJsonObject game = doc.isArray() ? doc.array().at(0).toObject() : doc.object();
            if (status == 200 && !game.isEmpty()) {
                m_hash_name = game.value(QStringLiteral("name")).toString().trimmed();
                QStringList sources;
                for (const QJsonValue& v : game.value(QStringLiteral("metadata")).toArray())
                    sources.append(v.toObject().value(QStringLiteral("source")).toString());
                note(LOGMSG("Hasheous: \"%1\" (sources: %2)")
                     .arg(m_hash_name)
                     .arg(sources.isEmpty() ? QStringLiteral("none") : sources.join(QStringLiteral(", "))));
                if (!m_hash_name.isEmpty())
                    setMatch(QStringLiteral("hasheous"), game.value(QStringLiteral("id")).toVariant().toString(), m_hash_name);
            }
            else {
                note(LOGMSG("Hasheous: answered %1, %2").arg(status)
                     .arg(status == 404 ? QStringLiteral("checksum not known") : QStringLiteral("no match")));
            }
            then();
        });
    });
}

// libretro's thumbnails (no key): the box, screenshot and title screen by the
// game's No-Intro name (by its checksum, else its file name), from the
// system's list of names (read once a run)
void Scraper::libretroArt()
{
    m_libretro_done = true;
    const Job& job = m_jobs[m_index];
    const QString system = libretro_system(job.system);
    struct Want { const char* folder_on_server; const char* setting; const char* slot; const char* folder; const char* box; };
    static const std::vector<Want> wants_all {
        { "Named_Boxarts", "getCovers", "assets.box_front", "covers", "2d" },
        { "Named_Snaps", "getScreenshots", "assets.screenshot", "screenshots", "" },
        { "Named_Titles", "getTitlescreens", "assets.titlescreen", "titlescreens", "" },
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
    if (system.isEmpty() || todo->empty()) {
        delete todo;
        fillMore();
        return;
    }
    // the names to look for: by checksum, the file's own name, the title
    const QString file_part = job.path.contains(QLatin1String("://"))
        ? QUrl::fromPercentEncoding(job.path.toUtf8()).section(QLatin1Char('/'), -1) : job.path;
    QStringList names;
    if (!m_hash_name.isEmpty())
        names.append(m_hash_name);
    if (!job.path.startsWith(QLatin1String("android:")))
        names.append(QFileInfo(file_part).completeBaseName());
    names.append(m_changes.value(QStringLiteral("title"), job.title).toString());
    names.removeDuplicates();

    const int run = m_run;
    const size_t index = m_index;
    auto* const pending = new int(static_cast<int>(todo->size()));
    const auto one_done = [this, pending, todo, run, index]{
        if (--(*pending) > 0)
            return;
        delete pending;
        delete todo;
        if (run != m_run || index != m_index)
            return;
        fillMore();
    };
    const QString base = QStringLiteral("https://thumbnails.libretro.com/") + system;
    for (const Want& w : *todo) {
        const Want want = w;
        const QString key = system + QLatin1Char('/') + QLatin1String(want.folder_on_server);
        const auto pick = [this, want, names, base, one_done, run, index](const QStringList& list){
            if (run != m_run || index != m_index) {
                one_done();
                return;
            }
            // the exact name first, else the same name in another region
            // (the ROM's region preferred), else the closest one if it's sure
            QString found;
            for (const QString& name : names) {
                const QString file = libretro_file_name(name) + QStringLiteral(".png");
                if (list.contains(file)) {
                    found = file;
                    break;
                }
            }
            if (found.isEmpty()) {
                // the region of the first name that has one (Hasheous' names have none)
                QString region;
                for (const QString& name : names) {
                    region = region_of(name);
                    if (!region.isEmpty())
                        break;
                }
                int best = 0;
                for (const QString& file : list) {
                    const QString stem = file.left(file.size() - 4);
                    int score = 0;
                    for (const QString& name : names)
                        score = qMax(score, name_score(name, stem));
                    if (score < 85)
                        continue;
                    if (!region.isEmpty() && region_of(stem) == region)
                        score += 5;
                    if (stem.contains(QLatin1String("(Beta")) || stem.contains(QLatin1String("(Proto")))
                        score -= 10;
                    if (score > best) {
                        best = score;
                        found = file;
                    }
                }
            }
            note(LOGMSG("libretro: %1: %2").arg(QLatin1String(want.folder_on_server),
                found.isEmpty() ? QStringLiteral("no match among %1 names").arg(list.size()) : found));
            if (!found.isEmpty()) {
                Media media;
                media.setting = QLatin1String(want.setting);
                media.slot = QLatin1String(want.slot);
                media.folder = QLatin1String(want.folder);
                media.kind = QLatin1String(want.box);
                media.url = QUrl(base + QLatin1Char('/') + QLatin1String(want.folder_on_server) + QLatin1Char('/')
                                 + QString::fromLatin1(QUrl::toPercentEncoding(found)));
                media.ext = QStringLiteral("png");
                m_media.push_back(media);
            }
            one_done();
        };
        const auto cached = m_libretro_lists.constFind(key);
        if (cached != m_libretro_lists.constEnd()) {
            pick(cached.value());
            continue;
        }
        const QUrl list_url(base + QLatin1Char('/') + QLatin1String(want.folder_on_server) + QLatin1Char('/'));
        get(list_url, {}, 1, [this, key, pick](int status, const QByteArray& body){
            // the server's folder page: <a href="Name%20(USA).png">
            QStringList list;
            if (status == 200) {
                static const QRegularExpression rx_href(QStringLiteral("href=\"([^\"?/]+\\.png)\""));
                auto it = rx_href.globalMatch(QString::fromUtf8(body));
                while (it.hasNext())
                    list.append(QUrl::fromPercentEncoding(it.next().captured(1).toUtf8()));
            }
            else {
                note(LOGMSG("libretro: the list of `%1` answered %2").arg(key).arg(status));
            }
            m_libretro_lists.insert(key, list);
            pick(list);
        });
    }
}

// the library index (v1): what each game was matched to, by its file: the
// source, its id for the game, the name it has there, the file's MD5 when one
// was made, and when. Read by later runs and by the Library page.
QString Scraper::indexPath()
{
    return paths::writableConfigDir() + QStringLiteral("/metadata/library-index.json");
}

void Scraper::setMatch(const QString& source, const QString& id, const QString& name)
{
    if (id.isEmpty() || id == QLatin1String("0"))
        return;
    m_match = QVariantMap {
        { QStringLiteral("source"), source },
        { QStringLiteral("id"), id },
        { QStringLiteral("name"), name },
    };
}

void Scraper::recordMatch(const Job& job)
{
    if (job.path.isEmpty() || (m_match.isEmpty() && m_md5.isEmpty()))
        return;
    QJsonObject root;
    {
        QFile file(indexPath());
        if (file.open(QIODevice::ReadOnly))
            root = QJsonDocument::fromJson(file.readAll()).object();
    }
    root[QStringLiteral("version")] = 1;
    QJsonObject games = root.value(QStringLiteral("games")).toObject();
    QJsonObject entry = games.value(job.path).toObject();
    entry[QStringLiteral("collection")] = job.collection;
    if (!m_md5.isEmpty())
        entry[QStringLiteral("md5")] = m_md5;
    if (!m_match.isEmpty()) {
        // one entry per source: a LaunchBox match doesn't drop the ScreenScraper one
        QJsonObject ids = entry.value(QStringLiteral("ids")).toObject();
        ids[m_match.value(QStringLiteral("source")).toString()] = QJsonObject {
            { QStringLiteral("id"), m_match.value(QStringLiteral("id")).toString() },
            { QStringLiteral("name"), m_match.value(QStringLiteral("name")).toString() },
        };
        entry[QStringLiteral("ids")] = ids;
        entry[QStringLiteral("source")] = m_match.value(QStringLiteral("source")).toString();
    }
    entry[QStringLiteral("scraped")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    games[job.path] = entry;
    root[QStringLiteral("games")] = games;
    QDir().mkpath(QFileInfo(indexPath()).absolutePath());
    QSaveFile file(indexPath());
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
            || !file.commit())
        note(LOGMSG("Couldn't write `%1`").arg(indexPath()));
}

// -- Needs your check ----------------------------------------------------------

namespace {
QString review_path() { return paths::writableConfigDir() + QStringLiteral("/metadata/review.json"); }

QJsonObject read_review()
{
    QFile file(review_path());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

void write_review(QJsonObject root)
{
    root[QStringLiteral("version")] = 1;
    QDir().mkpath(QFileInfo(review_path()).absolutePath());
    QSaveFile file(review_path());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.commit();
    }
}
} // namespace

void Scraper::reviewAdd(const Job& job)
{
    if (job.path.isEmpty())
        return;
    QJsonObject root = read_review();
    QJsonObject games = root.value(QStringLiteral("games")).toObject();
    QJsonObject entry {
        { QStringLiteral("title"), job.title },
        { QStringLiteral("collection"), job.collection },
        { QStringLiteral("system"), job.system },
        { QStringLiteral("date"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
    };
    if (!m_review_hint.isEmpty()) {
        entry[QStringLiteral("closest")] = m_review_hint.value(QStringLiteral("closest")).toString();
        entry[QStringLiteral("info")] = m_review_hint.value(QStringLiteral("info")).toString();
        entry[QStringLiteral("score")] = m_review_hint.value(QStringLiteral("score")).toInt();
        entry[QStringLiteral("launchbox")] = m_review_hint.value(QStringLiteral("launchbox")).toInt();
    }
    games[job.path] = entry;
    root[QStringLiteral("games")] = games;
    write_review(root);
}

void Scraper::reviewRemove(const QString& path)
{
    QJsonObject root = read_review();
    QJsonObject games = root.value(QStringLiteral("games")).toObject();
    if (!games.contains(path))
        return;
    games.remove(path);
    root[QStringLiteral("games")] = games;
    write_review(root);
}

QVariantList Scraper::reviewList()
{
    const QJsonObject games = read_review().value(QStringLiteral("games")).toObject();
    QVariantList out;
    for (auto it = games.constBegin(); it != games.constEnd(); ++it) {
        QVariantMap entry = it.value().toObject().toVariantMap();
        entry.insert(QStringLiteral("path"), it.key());
        out.append(entry);
    }
    std::sort(out.begin(), out.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value(QStringLiteral("title")).toString()
            .compare(b.toMap().value(QStringLiteral("title")).toString(), Qt::CaseInsensitive) < 0;
    });
    return out;
}

void Scraper::setNeverScrape(const QString& path, const QString& title, bool never)
{
    QJsonObject root = read_review();
    QJsonObject excluded = root.value(QStringLiteral("neverScrape")).toObject();
    if (never) {
        excluded[path] = title;
        QJsonObject games = root.value(QStringLiteral("games")).toObject();
        games.remove(path);
        root[QStringLiteral("games")] = games;
    }
    else {
        excluded.remove(path);
    }
    root[QStringLiteral("neverScrape")] = excluded;
    write_review(root);
}

bool Scraper::hasFreeArt(const QString& system)
{
    return !libretro_system(system).isEmpty();
}

QSet<QString> Scraper::neverScrape()
{
    QSet<QString> out;
    const QJsonObject excluded = read_review().value(QStringLiteral("neverScrape")).toObject();
    for (auto it = excluded.constBegin(); it != excluded.constEnd(); ++it)
        out.insert(it.key());
    return out;
}

// several requests in a row without an answer (no connection, servers
// down): the run stops with the reason, instead of going through every
// game as "not found"
void Scraper::countTransport(int status)
{
    if (m_stop || !m_running)
        return;
    if (status == 0 || status >= 500) {
        m_transport_streak++;
        if (m_transport_streak >= 3) {
            note(LOGMSG("%1 requests in a row got no answer: stopping").arg(m_transport_streak));
            m_stop_reason = status == 0
                ? tr("No connection: scraping stopped. Start it again to go on from where it was.")
                : tr("The scraping service isn't answering (error %1): scraping stopped.").arg(status);
            m_stop = true;
        }
        return;
    }
    m_transport_streak = 0;
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
    m_replies.insert(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, auth, tries_left, done]{
        m_replies.remove(reply);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        const bool retry = (status == 0 || status == 429 || status >= 500) && status != 430;
        if (!retry || tries_left <= 0 || m_stop)
            countTransport(status);
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
    m_md5 = md5;
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
    note(LOGMSG("ScreenScraper: looking up \"%1\" (system `%2`, id %3%4)")
         .arg(by_title ? job.title : job.search_name, job.system)
         .arg(screenscraper_systems().value(job.system))
         .arg(md5.isEmpty() ? QString() : QStringLiteral(", by its MD5")));

    get(url, {}, m_settings.value(QStringLiteral("retries")).toInt(), [this](int status, const QByteArray& body){
        if (m_stop) {
            next();
            return;
        }
        {
            const QJsonObject found = QJsonDocument::fromJson(body).object()
                .value(QStringLiteral("response")).toObject().value(QStringLiteral("jeu")).toObject();
            QString name;
            for (const QJsonValue& n : found.value(QStringLiteral("noms")).toArray()) {
                name = n.toObject().value(QStringLiteral("text")).toString();
                if (!name.isEmpty())
                    break;
            }
            note(LOGMSG("ScreenScraper: answered %1%2").arg(status)
                 .arg(!name.isEmpty() ? QStringLiteral(", \"%1\" (id %2)").arg(name, found.value(QStringLiteral("id")).toString())
                      : status == 200 || status == 404 ? QStringLiteral(", no match") : QString()));
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
            // not on ScreenScraper: the LaunchBox database may still have it
            if (lbReady() && m_lb->hasSystem(job.system)) {
                fillMore();
                return;
            }
            // no answer at all is a failure, not "not found"
            if (status == 404 || status == 200)
                m_not_found++;
            else
                m_failed++;
            finishGame(status == 404 || status == 200 ? tr("not found")
                       : status == 0 ? tr("ScreenScraper: no connection") : tr("ScreenScraper error %1").arg(status), false);
            return;
        }
        parseScreenScraper(game);
        // what ScreenScraper doesn't have, from LaunchBox and libretro
        fillMore();
    });
}

void Scraper::parseScreenScraper(const QJsonObject& game)
{
    const Job& job = m_jobs[m_index];
    setMatch(QStringLiteral("screenscraper"), game.value(QStringLiteral("id")).toVariant().toString(),
             text_of(game.value(QStringLiteral("noms")).toArray().at(0).toObject()));
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

bool Scraper::lbReady() const
{
    return m_lb && m_lb->ready() && m_settings.value(QStringLiteral("useLaunchBox"), true).toBool();
}

QVariantMap Scraper::lbCandidate(const LaunchBoxDb::Game& game) const
{
    QStringList info { game.platform };
    if (game.year > 0)
        info.append(QString::number(game.year));
    QString thumb;
    for (const LaunchBoxDb::Image& image : game.images) {
        if (image.type == QLatin1String("Box - Front")) {
            thumb = LaunchBoxDb::image_url(image.file);
            break;
        }
    }
    return QVariantMap {
        { QStringLiteral("name"), game.name },
        { QStringLiteral("info"), info.join(QStringLiteral(" · ")) },
        { QStringLiteral("description"), game.overview },
        { QStringLiteral("thumb"), thumb },
        { QStringLiteral("developer"), game.developers.join(QStringLiteral(", ")) },
        { QStringLiteral("publisher"), game.publishers.join(QStringLiteral(", ")) },
        { QStringLiteral("genre"), game.genres.join(QStringLiteral(", ")) },
        { QStringLiteral("players"), game.players > 0 ? QString::number(game.players) : QString() },
        { QStringLiteral("release"), game.release.isValid() ? game.release.toString(Qt::ISODate) : QString() },
        { QStringLiteral("rating"), game.rating > 0.f ? QString::number(qRound(game.rating * 100)) + QLatin1Char('%') : QString() },
    };
}

// interactive: the closest games, to pick from
void Scraper::lbSearch(const QString& name)
{
    const Job& job = m_jobs[m_index];
    QString wanted = name;
    if (!m_hash_name.isEmpty())
        wanted = search_name_of(m_hash_name + QStringLiteral(".x"), m_hash_name);
    m_lb_results = m_lb->candidates(job.system, wanted, 10);
    note(LOGMSG("LaunchBox: %1 matches for \"%2\" (%3)").arg(m_lb_results.size()).arg(wanted, LaunchBoxDb::platform_for(job.system)));
    QVariantList candidates;
    for (const LaunchBoxDb::Game& game : m_lb_results)
        candidates.append(lbCandidate(game));
    m_choice_source = QStringLiteral("launchbox");
    offerChoice(candidates);
}

// without asking: the surest match; a weak one is left for Needs your check
void Scraper::lbAuto(bool after_other)
{
    m_lb_done = true;
    const Job& job = m_jobs[m_index];
    const bool by_title = m_settings.value(QStringLiteral("useMetadataNames")).toBool() && !job.title_is_file_name;
    QString name = by_title ? job.title : job.search_name;
    if (!m_hash_name.isEmpty() && !by_title)
        name = search_name_of(m_hash_name + QStringLiteral(".x"), m_hash_name);
    if (after_other && m_changes.contains(QStringLiteral("title")))
        name = m_changes.value(QStringLiteral("title")).toString();
    const std::vector<LaunchBoxDb::Game> found = m_lb->candidates(job.system, name, 5);
    const int needed = m_settings.value(QStringLiteral("minMatch"), 60).toInt();
    if (found.empty() || found.front().score < needed) {
        note(LOGMSG("LaunchBox: no sure match for \"%1\" on %2%3").arg(name, LaunchBoxDb::platform_for(job.system))
             .arg(found.empty() ? QString() : QStringLiteral(" (closest \"%1\", %2 of %3 needed)")
                                      .arg(found.front().name).arg(found.front().score).arg(needed)));
        if (!found.empty() && !after_other) {
            m_review_hint = QVariantMap {
                { QStringLiteral("closest"), found.front().name },
                { QStringLiteral("info"), lbCandidate(found.front()).value(QStringLiteral("info")) },
                { QStringLiteral("score"), found.front().score },
                { QStringLiteral("launchbox"), found.front().id },
            };
        }
        fillMore();
        return;
    }
    parseLaunchBox(found.front(), after_other);
    fillMore();
}

void Scraper::parseLaunchBox(const LaunchBoxDb::Game& game, bool after_other)
{
    const Job& job = m_jobs[m_index];
    if (!after_other)
        setMatch(QStringLiteral("launchbox"), QString::number(game.id), game.name);
    note(LOGMSG("LaunchBox: using \"%1\" (%2, id %3)%4").arg(game.name, game.platform).arg(game.id)
         .arg(after_other ? QStringLiteral(" for what's still missing") : QString()));
    const bool overwrite = m_settings.value(QStringLiteral("overwrite")).toBool();
    const auto open_key = [this, overwrite](const char* key, bool has) {
        return !m_changes.contains(QLatin1String(key)) && (overwrite || !has);
    };
    if (!after_other && wants(QStringLiteral("getNames")) && (overwrite || job.title_is_file_name)) {
        if (!game.name.isEmpty() && game.name != job.title)
            m_changes.insert(QStringLiteral("title"), game.name);
    }
    if (wants(QStringLiteral("getRatings")) && open_key("rating", job.has_rating) && game.rating > 0.f)
        m_changes.insert(QStringLiteral("rating"), QString::number(qRound(game.rating * 100)) + QLatin1Char('%'));
    if (wants(QStringLiteral("getDetails"))) {
        if (!game.overview.isEmpty() && open_key("description", job.has_description)) {
            m_changes.insert(QStringLiteral("description"), game.overview);
            m_changes.insert(QStringLiteral("summary"), QString());
        }
        if (!game.developers.isEmpty() && open_key("developer", job.has_developer))
            m_changes.insert(QStringLiteral("developer"), game.developers);
        if (!game.publishers.isEmpty() && open_key("publisher", job.has_publisher))
            m_changes.insert(QStringLiteral("publisher"), game.publishers);
        if (game.players > 0 && open_key("players", job.has_players))
            m_changes.insert(QStringLiteral("players"), QString::number(game.players));
        if (game.release.isValid() && open_key("release", job.has_release))
            m_changes.insert(QStringLiteral("release"), game.release.toString(Qt::ISODate));
        if (!game.genres.isEmpty() && open_key("genre", job.has_genre))
            m_changes.insert(QStringLiteral("genre"), game.genres);
    }

    // the art: one picture per kind, the region you prefer first
    const QString region = m_settings.value(QStringLiteral("region")).toString();
    const QStringList region_order = region == QLatin1String("eu")
        ? QStringList { "Europe", "World", "North America", "United States", "Japan" }
        : region == QLatin1String("jp")
        ? QStringList { "Japan", "World", "North America", "United States", "Europe" }
        : QStringList { "North America", "United States", "World", "Europe", "Japan" };
    const auto pick = [&game, &region_order](const QStringList& types) {
        int best = 1000;
        QString url;
        for (const QString& type : types) {
            for (const LaunchBoxDb::Image& image : game.images) {
                if (image.type != type)
                    continue;
                int rank = region_order.indexOf(image.region);
                if (rank < 0)
                    rank = region_order.size() + (image.region.isEmpty() ? 0 : 1);
                if (rank < best) {
                    best = rank;
                    url = LaunchBoxDb::image_url(image.file);
                }
            }
            if (!url.isEmpty())
                return url;  // the first type that has one
        }
        return url;
    };
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
        media.ext = QFileInfo(media.url.path()).suffix().isEmpty() ? QStringLiteral("jpg") : QFileInfo(media.url.path()).suffix();
        for (const Media& m : m_media) {
            if (m.slot == media.slot && m.kind == media.kind)
                return;  // the first source had it
        }
        if (slotFree(job, media))
            m_media.push_back(media);
    };
    const size_t before = m_media.size();
    add(pick({ "Box - Front", "Box - Front - Reconstructed", "Fanart - Box - Front" }), "getCovers", "assets.box_front", "covers", "2d");
    add(pick({ "Box - 3D" }), "getCovers", "assets.box_front", "3dboxes", "3d");
    add(pick({ "Box - Back", "Box - Back - Reconstructed" }), "getBackcovers", "assets.box_back", "backcovers", "");
    add(pick({ "Cart - Front", "Disc", "Cart - 3D", "Fanart - Cart - Front", "Fanart - Disc" }), "getPhysical", "assets.cartridge", "physicalmedia", "");
    add(pick({ "Screenshot - Gameplay", "Screenshot - Game Select", "Screenshot - Game Over" }), "getScreenshots", "assets.screenshot", "screenshots", "");
    add(pick({ "Screenshot - Game Title" }), "getTitlescreens", "assets.titlescreen", "titlescreens", "");
    add(pick({ "Fanart - Background" }), "getFanart", "assets.background", "fanart", "");
    add(pick({ "Clear Logo" }), "getLogos", "assets.logo", "marquees", "");
    add(pick({ "Banner", "Steam Banner" }), "getBanners", "assets.banner", "banners", "");
    add(pick({ "Box - Spine" }), "getSpines", "assets.box_spine", "spines", "");
    note(LOGMSG("LaunchBox: %1 pictures to download (of %2 it lists)").arg(m_media.size() - before).arg(game.images.size()));
}

void Scraper::fillMore()
{
    if (!m_lb_done && lbReady() && m_settings.value(QStringLiteral("launchboxFill"), true).toBool()
            && m_lb->hasSystem(m_jobs[m_index].system)) {
        lbAuto(true);
        return;
    }
    if (!m_libretro_done && m_settings.value(QStringLiteral("useLibretro"), true).toBool()) {
        libretroArt();
        return;
    }
    if (m_media.empty() && m_changes.isEmpty()) {
        m_not_found++;
        finishGame(tr("not found"), false);
        return;
    }
    downloadNext();
}

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
            note(LOGMSG("Download failed (%1): %2").arg(status).arg(safe_url(media.url)));
            downloadNext();
            return;
        }
        QString ext = media.ext.toLower();
        if (ext.isEmpty())
            ext = QStringLiteral("png");
        // what came back must be what was asked for: a server's error page
        // answered with 200 is not a picture (saved, it would fill the slot
        // with a file no theme can show)
        const bool is_video = media.folder == QLatin1String("videos");
        const bool is_manual = media.folder == QLatin1String("manuals");
        bool valid = true;
        if (is_manual) {
            valid = body.startsWith("%PDF");
        }
        else if (is_video) {
            const QByteArray head = body.left(64).trimmed().toLower();
            valid = !head.startsWith("<") && !head.startsWith("{");
        }
        else {
            QBuffer buffer;
            buffer.setData(body);
            buffer.open(QIODevice::ReadOnly);
            valid = QImageReader(&buffer).canRead();
        }
        if (!valid) {
            m_picture_failures++;
            m_picture_error = -4;
            note(LOGMSG("Not a %1 (%2 bytes, starts \"%3\"): %4").arg(is_video ? QStringLiteral("video") : is_manual ? QStringLiteral("PDF") : QStringLiteral("picture"))
                 .arg(body.size()).arg(QString::fromLatin1(body.left(24).simplified())).arg(safe_url(media.url)));
            downloadNext();
            return;
        }
        note(LOGMSG("Downloaded %1 (%2 KB) -> %3").arg(media.folder).arg(body.size() / 1024).arg(safe_url(media.url)));

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
        const QString fit = m_settings.value(QStringLiteral("fanartFit"), QStringLiteral("original")).toString();
        const int fanart_mode = media.folder != QLatin1String("fanart") ? 0
            : fit == QLatin1String("16:9") ? 1 : fit == QLatin1String("stretch") ? 2 : 0;
        if (picture && (max_w > 0 || trim || fanart_mode > 0)) {
            auto* const watcher = new QFutureWatcher<QPair<QByteArray, QString>>(this);
            connect(watcher, &QFutureWatcher<QPair<QByteArray, QString>>::finished, this, [this, watcher, media, job]{
                const QPair<QByteArray, QString> result = watcher->result();
                watcher->deleteLater();
                storeMedia(media, job, result.first, result.second);
            });
            watcher->setFuture(QtConcurrent::run([body, ext, max_w, max_h, trim, fanart_mode]{
                return process_image(body, ext, max_w, max_h, trim, fanart_mode);
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
    // the system's folder (as ES-DE: downloaded_media/snes/...), else the
    // collection's short name, else its name
    const QString system_folder = !job.system.isEmpty() ? job.system
                                : !job.media_folder.isEmpty() ? job.media_folder : job.collection;
    const QString dir = m_data_dir % QLatin1Char('/') % safe_file_name(system_folder)
                      % QLatin1Char('/') % media.folder;
    if (!QDir().mkpath(dir)) {
        // the art folder can't be made (SD card missing, no permission):
        // nothing else will work either, so the run stops with the reason
        note(LOGMSG("Couldn't make `%1`").arg(dir));
        m_stop_reason = tr("Couldn't write to the art folder %1 (is the SD card there?)").arg(m_data_dir);
        m_stop = true;
        downloadNext();
        return;
    }
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
        m_picture_error = -3;  // couldn't write
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
    if (m_settings.value(QStringLiteral("overwrite")).toBool()) {
        // replaced: the old files of this kind go, every time a file is
        // stored (the front box holds two kinds: the 2D one stored first
        // must not keep an old 3D box ahead of the new one)
        QStringList kept;
        for (const QString& f : list) {
            const bool is3d = pegasus_assets::is_3d_box(f);
            const bool same_kind = media.kind == QLatin1String("3d") ? is3d
                                 : media.kind == QLatin1String("2d") ? !is3d
                                 : true;
            if (!same_kind)
                kept.append(f);
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
        // (that file only, by its whole path: hiding works by path too)
        const QString wanted = QDir::cleanPath(file_path).toLower();
        QStringList hidden = m_changes.contains(QStringLiteral("x-pegasusx-hidden-art"))
            ? m_changes.value(QStringLiteral("x-pegasusx-hidden-art")).toStringList() : job.hidden_art;
        const int before = hidden.size();
        hidden.erase(std::remove_if(hidden.begin(), hidden.end(), [&kind, &wanted](const QString& h) {
            const int bar = h.indexOf(QLatin1Char('|'));
            return bar > 0 && h.left(bar) == kind && QDir::cleanPath(h.mid(bar + 1)).toLower() == wanted;
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
            if (!result.error.isEmpty())
                note(LOGMSG("Miximage: %1").arg(result.error));
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
    // the collection's file backed up once a run, before its first change
    if (m_settings.value(QStringLiteral("overwrite")).toBool()) {
        const QString file = providers::esde::metadata_location(job.collection, QString()).file;
        if (!m_backed_up.contains(file)) {
            m_backed_up.insert(file);
            providers::gamedir::backup(file, QStringLiteral("scrape"));
        }
    }
    const QString error = providers::esde::save_game_metadata(job.collection, job.path, title, m_changes, true);
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
    recordMatch(job);
    QStringList what;
    // which source the game came from
    const QString from = m_match.value(QStringLiteral("source")).toString();
    if (from == QLatin1String("launchbox")) what.append(QStringLiteral("LaunchBox"));
    else if (from == QLatin1String("screenscraper")) what.append(QStringLiteral("ScreenScraper"));
    if (m_changes.contains(QStringLiteral("description")) || m_changes.contains(QStringLiteral("developer")))
        what.append(tr("details"));
    for (const QString& folder : m_got) {
        if (!what.contains(folder))
            what.append(folder);
    }
    // a miximage that couldn't be made is said, not dropped
    const bool mix_wanted = m_settings.value(QStringLiteral("getMiximages")).toBool();
    if (mix_wanted && !m_mix_error.isEmpty() && !m_got.contains(QStringLiteral("miximages")))
        what.append(tr("miximage: %1").arg(m_mix_error));
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
            .arg(m_picture_error == 0 ? tr("no connection")
                 : m_picture_error == -3 ? tr("couldn't write to the art folder")
                 : m_picture_error == -4 ? tr("not a picture")
                 : tr("error %1").arg(m_picture_error));
        result = result.isEmpty() ? failed : result + QStringLiteral(" · ") + failed;
    }
    if (m_index < m_jobs.size()) {
        m_last_title = m_jobs[m_index].title;
        m_preview = previewOf(m_jobs[m_index]);
        // Needs your check: a game it wasn't sure about (or found nothing
        // for) waits there; one that was saved leaves it. Not for a stop or
        // a lost connection.
        if (m_settings.value(QStringLiteral("reviewQueue")).toBool()) {
            if (ok)
                reviewRemove(m_jobs[m_index].path);
            else if (!m_stop && m_transport_streak == 0)
                reviewAdd(m_jobs[m_index]);
        }
        else if (ok) {
            reviewRemove(m_jobs[m_index].path);
        }
    }
    m_last_result = result;
    m_last_ok = ok;
    m_index++;
    report();
    // a short pause between games keeps within ScreenScraper's pace
    QTimer::singleShot(300, this, [this]{ next(); });
}

} // namespace scraper
