// Pegasus Frontend
// Copyright (C) 2017-2020  Mátyás Mustoha
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


#include "LaunchBoxDb.h"

#include "Log.h"
#include "Paths.h"
#include "utils/ZipReader.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStringBuilder>
#include <QXmlStreamReader>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <unordered_map>


namespace {
constexpr auto LOG_TAG = "LaunchBox";
const QString ZIP_URL = QStringLiteral("https://gamesdb.launchbox-app.com/Metadata.zip");
constexpr qint64 FRESH_FOR_DAYS = 7;

QString safe_name(const QString& name)
{
    QString out;
    for (const QChar c : name) {
        if (c.isLetterOrNumber())
            out += c.toLower();
        else if (c == QLatin1Char(' ') || c == QLatin1Char('-') || c == QLatin1Char('_'))
            out += QLatin1Char('_');
    }
    return out.isEmpty() ? QStringLiteral("other") : out;
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

// "Chrono Trigger (USA)" -> "chronotrigger"; ", The" moved to the front
QString plain_key(const QString& name)
{
    static const QRegularExpression rx_tags(QStringLiteral("\\s*[\\(\\[][^\\)\\]]*[\\)\\]]"));
    QString t = name;
    t.remove(rx_tags);
    if (t.endsWith(QLatin1String(", The"), Qt::CaseInsensitive))
        t = QStringLiteral("The ") % t.left(t.size() - 5);
    return normalized(t);
}

QSet<QString> words_of(const QString& text)
{
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
}

int name_score(const QString& wanted, const QString& found)
{
    if (plain_key(wanted) == plain_key(found))
        return 100;
    const QSet<QString> a = words_of(wanted);
    const QSet<QString> b = words_of(found);
    if (a.isEmpty() || b.isEmpty())
        return 0;
    const int common = QSet<QString>(a).intersect(b).size();
    return static_cast<int>(90.0 * common / std::max(a.size(), b.size()));
}

QString status_path() { return scraper::LaunchBoxDb::dir() % QStringLiteral("/status.json"); }
QString platforms_path() { return scraper::LaunchBoxDb::dir() % QStringLiteral("/platforms.json"); }
QString index_path(const QString& platform) { return scraper::LaunchBoxDb::index_dir() % QLatin1Char('/') % safe_name(platform) % QStringLiteral(".json"); }

QJsonObject read_json(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

bool write_json(const QString& path, const QJsonDocument& doc)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(doc.toJson(QJsonDocument::Compact)) >= 0 && file.commit();
}

QStringList split_list(const QString& value)
{
    QStringList out;
    for (const QString& part : value.split(QLatin1Char(';')))
        if (!part.trimmed().isEmpty())
            out.append(part.trimmed());
    return out;
}

// -- reading Metadata.xml ---------------------------------------------------------

struct Build {
    QSet<QString> wanted_platforms;
    std::unordered_map<int, scraper::LaunchBoxDb::Game> games;   // the wanted platforms' games, by id
    QHash<QString, int> platform_counts;                          // every platform in the file
    int unknown_extras = 0;   // alternate names / images for games not kept (or not seen yet)
    int kept_images = 0;
    bool games_only = false;  // the first pass: games; the second (if needed): names and images
    bool extras_only = false;
};

// one top-level element (Game, GameAlternateName, GameImage) with its children's text
void take_element(Build& b, const QString& element, const QHash<QString, QString>& fields)
{
    using Game = scraper::LaunchBoxDb::Game;
    if (element == QLatin1String("Game")) {
        const QString platform = fields.value(QStringLiteral("Platform"));
        b.platform_counts[platform]++;
        if (b.extras_only || !b.wanted_platforms.contains(platform))
            return;
        Game game;
        game.id = fields.value(QStringLiteral("DatabaseID")).toInt();
        game.name = fields.value(QStringLiteral("Name")).trimmed();
        if (game.id <= 0 || game.name.isEmpty())
            return;
        game.platform = platform;
        game.release = QDate::fromString(fields.value(QStringLiteral("ReleaseDate")).left(10), Qt::ISODate);
        game.year = fields.value(QStringLiteral("ReleaseYear")).toInt();
        if (!game.release.isValid() && game.year > 0)
            game.release = QDate(game.year, 1, 1);
        game.overview = fields.value(QStringLiteral("Overview")).trimmed();
        game.developers = split_list(fields.value(QStringLiteral("Developer")));
        game.publishers = split_list(fields.value(QStringLiteral("Publisher")));
        game.genres = split_list(fields.value(QStringLiteral("Genres")));
        game.players = fields.value(QStringLiteral("MaxPlayers")).toInt();
        const double rating = fields.value(QStringLiteral("CommunityRating")).toDouble();
        game.rating = rating > 0 ? static_cast<float>(std::min(1.0, rating / 5.0)) : 0.f;
        b.games[game.id] = std::move(game);
        return;
    }
    if (b.games_only)
        return;
    const int id = fields.value(QStringLiteral("DatabaseID")).toInt();
    const auto it = b.games.find(id);
    if (it == b.games.end()) {
        b.unknown_extras++;
        return;
    }
    if (element == QLatin1String("GameAlternateName")) {
        const QString alt = fields.value(QStringLiteral("AlternateName")).trimmed();
        if (!alt.isEmpty() && !it->second.alt_names.contains(alt))
            it->second.alt_names.append(alt);
    }
    else if (element == QLatin1String("GameImage")) {
        scraper::LaunchBoxDb::Image image;
        image.type = fields.value(QStringLiteral("Type"));
        image.file = fields.value(QStringLiteral("FileName"));
        image.region = fields.value(QStringLiteral("Region"));
        if (!image.file.isEmpty() && !image.type.isEmpty()) {
            it->second.images.push_back(std::move(image));
            b.kept_images++;
        }
    }
}

// the zip's Metadata.xml, in a stream, through the reader; returns an error
QString read_xml(const utils::ZipReader& zip, Build& b, const std::atomic<bool>& cancel, std::atomic<int>& progress)
{
    const utils::ZipReader::Entry* entry = nullptr;
    for (const utils::ZipReader::Entry& e : zip.entries()) {
        if (e.path.compare(QLatin1String("Metadata.xml"), Qt::CaseInsensitive) == 0 || e.path.endsWith(QLatin1String("/Metadata.xml")))
            entry = &e;
    }
    if (!entry)
        return QStringLiteral("Metadata.xml isn't in the download");

    QXmlStreamReader xml;
    int depth = 0;
    QString element;             // the top-level element being read
    QString child;               // its child element being read
    QString text;
    QHash<QString, QString> fields;
    QString error;
    quint64 seen = 0;
    const auto handle = [&]() {
        while (!xml.atEnd()) {
            const QXmlStreamReader::TokenType token = xml.readNext();
            if (token == QXmlStreamReader::Invalid) {
                if (xml.error() == QXmlStreamReader::PrematureEndOfDocumentError)
                    return true;  // more data needed
                error = xml.errorString();
                return false;
            }
            if (token == QXmlStreamReader::StartElement) {
                depth++;
                if (depth == 2) {
                    element = xml.name().toString();
                    fields.clear();
                }
                else if (depth == 3) {
                    child = xml.name().toString();
                    text.clear();
                }
            }
            else if (token == QXmlStreamReader::Characters) {
                if (depth == 3)
                    text += xml.text();
            }
            else if (token == QXmlStreamReader::EndElement) {
                if (depth == 3) {
                    fields.insert(child, text);
                }
                else if (depth == 2) {
                    take_element(b, element, fields);
                    element.clear();
                }
                depth--;
            }
        }
        return true;
    };
    const bool ok = zip.extract(*entry, [&](const char* data, int size) {
        if (cancel)
            return false;
        seen += static_cast<quint64>(size);
        progress = entry->size > 0 ? static_cast<int>(seen * 100 / entry->size) : 0;
        xml.addData(QByteArray(data, size));
        return handle();
    });
    if (cancel)
        return QStringLiteral("cancelled");
    if (!ok)
        return error.isEmpty() ? QStringLiteral("the download is damaged (couldn't be unpacked): download it again") : error;
    return {};
}
} // namespace


namespace scraper {

LaunchBoxDb::LaunchBoxDb(QObject* parent)
    : QObject(parent)
{}

QString LaunchBoxDb::dir()
{
    return paths::writableConfigDir() % QStringLiteral("/launchbox");
}

QString LaunchBoxDb::zip_path()
{
    return dir() % QStringLiteral("/Metadata.zip");
}

QString LaunchBoxDb::index_dir()
{
    return dir() % QStringLiteral("/index");
}

QString LaunchBoxDb::image_url(const QString& file)
{
    return QStringLiteral("https://images.launchbox-app.com/") % file;
}

QString LaunchBoxDb::platform_for(const QString& esde_system)
{
    static const QHash<QString, QString> map {
        { "nes", "Nintendo Entertainment System" }, { "famicom", "Nintendo Entertainment System" },
        { "fds", "Nintendo Famicom Disk System" },
        { "snes", "Super Nintendo Entertainment System" }, { "snesna", "Super Nintendo Entertainment System" },
        { "sfc", "Super Nintendo Entertainment System" }, { "satellaview", "Nintendo Satellaview" },
        { "n64", "Nintendo 64" }, { "n64dd", "Nintendo 64DD" }, { "gc", "Nintendo GameCube" }, { "wii", "Nintendo Wii" },
        { "wiiu", "Nintendo Wii U" }, { "switch", "Nintendo Switch" },
        { "gb", "Nintendo Game Boy" }, { "gbc", "Nintendo Game Boy Color" }, { "gba", "Nintendo Game Boy Advance" },
        { "nds", "Nintendo DS" }, { "n3ds", "Nintendo 3DS" }, { "virtualboy", "Nintendo Virtual Boy" },
        { "pokemini", "Nintendo Pokemon Mini" },
        { "megadrive", "Sega Genesis" }, { "genesis", "Sega Genesis" }, { "mastersystem", "Sega Master System" },
        { "gamegear", "Sega Game Gear" }, { "segacd", "Sega CD" }, { "megacd", "Sega CD" }, { "sega32x", "Sega 32X" },
        { "saturn", "Sega Saturn" }, { "dreamcast", "Sega Dreamcast" }, { "sg-1000", "Sega SG-1000" },
        { "naomi", "Sega Naomi" }, { "atomiswave", "Sammy Atomiswave" },
        { "psx", "Sony Playstation" }, { "ps2", "Sony Playstation 2" }, { "ps3", "Sony Playstation 3" },
        { "psp", "Sony PSP" }, { "psvita", "Sony Playstation Vita" }, { "ps4", "Sony Playstation 4" },
        { "xboxone", "Microsoft Xbox One" }, { "switch2", "Nintendo Switch 2" }, { "gameandwatch", "Nintendo Game & Watch" },
        { "zx81", "Sinclair ZX-81" }, { "vic20", "Commodore VIC-20" }, { "c128", "Commodore 128" }, { "cdtv", "Commodore CDTV" },
        { "model2", "Sega Model 2" }, { "model3", "Sega Model 3" }, { "segastv", "Sega ST-V" }, { "triforce", "Sega Triforce" },
        { "xbox", "Microsoft Xbox" }, { "xbox360", "Microsoft Xbox 360" },
        { "pcengine", "NEC TurboGrafx-16" }, { "tg16", "NEC TurboGrafx-16" }, { "pcenginecd", "NEC TurboGrafx-CD" },
        { "tg-cd", "NEC TurboGrafx-CD" }, { "pcfx", "NEC PC-FX" },
        { "atari2600", "Atari 2600" }, { "atari5200", "Atari 5200" }, { "atari7800", "Atari 7800" },
        { "atarijaguar", "Atari Jaguar" }, { "atarijaguarcd", "Atari Jaguar CD" }, { "atarilynx", "Atari Lynx" },
        { "atarist", "Atari ST" }, { "atari800", "Atari 800" }, { "atarixe", "Atari XEGS" },
        { "neogeo", "SNK Neo Geo AES" }, { "neogeocd", "SNK Neo Geo CD" }, { "ngp", "SNK Neo Geo Pocket" },
        { "ngpc", "SNK Neo Geo Pocket Color" },
        { "wonderswan", "WonderSwan" }, { "wonderswancolor", "WonderSwan Color" },
        { "colecovision", "ColecoVision" }, { "intellivision", "Mattel Intellivision" }, { "vectrex", "GCE Vectrex" },
        { "msx", "Microsoft MSX" }, { "msx1", "Microsoft MSX" }, { "msx2", "Microsoft MSX2" },
        { "3do", "3DO Interactive Multiplayer" }, { "c64", "Commodore 64" }, { "amiga", "Commodore Amiga" },
        { "amigacd32", "Commodore Amiga CD32" }, { "dos", "MS-DOS" }, { "pc", "Windows" }, { "windows", "Windows" },
        { "scummvm", "ScummVM" }, { "arcade", "Arcade" }, { "mame", "Arcade" }, { "fbneo", "Arcade" },
        { "android", "Android" }, { "zxspectrum", "Sinclair ZX Spectrum" }, { "amstradcpc", "Amstrad CPC" },
        { "apple2", "Apple II" }, { "channelf", "Fairchild Channel F" }, { "odyssey2", "Magnavox Odyssey 2" },
        { "gx4000", "Amstrad GX4000" }, { "x68000", "Sharp X68000" }, { "pc98", "NEC PC-9801" }, { "pc88", "NEC PC-8801" },
        { "cdimono1", "Philips CD-i" }, { "supervision", "Watara Supervision" }, { "gamecom", "Tiger Game.com" },
    };
    return map.value(esde_system);
}

QVariantMap LaunchBoxDb::status() const
{
    const QJsonObject saved = read_json(status_path());
    QVariantMap out;
    const QFileInfo zip(zip_path());
    out.insert(QStringLiteral("present"), !saved.isEmpty());
    out.insert(QStringLiteral("zip"), zip.exists());
    out.insert(QStringLiteral("date"), saved.value(QStringLiteral("date")).toString());
    out.insert(QStringLiteral("size"), zip.exists() ? static_cast<double>(zip.size()) / 1048576.0 : 0.0);
    out.insert(QStringLiteral("platforms"), saved.value(QStringLiteral("platforms")).toObject().toVariantMap());
    out.insert(QStringLiteral("games"), saved.value(QStringLiteral("games")).toInt());
    out.insert(QStringLiteral("working"), m_working);
    out.insert(QStringLiteral("stage"), m_stage);
    out.insert(QStringLiteral("progress"), m_progress.load());
    out.insert(QStringLiteral("error"), m_error);
    return out;
}

bool LaunchBoxDb::hasSystem(const QString& esde_system) const
{
    const QString platform = platform_for(esde_system);
    return !platform.isEmpty() && QFileInfo::exists(index_path(platform));
}

bool LaunchBoxDb::ready() const
{
    return !QDir(index_dir()).entryList({ QStringLiteral("*.json") }, QDir::Files).isEmpty();
}

void LaunchBoxDb::update(const QStringList& esde_systems, bool force)
{
    if (m_working)
        return;
    m_error.clear();
    m_cancel = false;
    m_working = true;
    const QFileInfo zip(zip_path());
    const bool fresh = zip.exists() && zip.lastModified().daysTo(QDateTime::currentDateTime()) < FRESH_FOR_DAYS;
    if (zip.exists() && (fresh || !force) && zip.size() > 0)
        build(esde_systems);
    else
        download(esde_systems);
}

void LaunchBoxDb::cancel()
{
    m_cancel = true;
    if (m_reply)
        m_reply->abort();
}

void LaunchBoxDb::download(const QStringList& esde_systems)
{
    if (!m_net)
        m_net = new QNetworkAccessManager(this);
    QDir().mkpath(dir());
    m_download.reset(new QFile(zip_path() % QStringLiteral(".part")));
    if (!m_download->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_error = QStringLiteral("Couldn't write to %1").arg(dir());
        m_working = false;
        emit finished(status());
        return;
    }
    m_stage = QStringLiteral("downloading");
    m_progress = 0;
    emit progress(status());
    Log::info(LOG_TAG, LOGMSG("Downloading `%1`").arg(ZIP_URL));
    QNetworkRequest request{QUrl(ZIP_URL)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", "PegasusX");
    m_reply = m_net->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this]{
        if (m_download)
            m_download->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total){
        m_progress = total > 0 ? static_cast<int>(got * 100 / total) : 0;
        emit progress(status());
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, esde_systems]{
        const int http = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = m_reply->error() == QNetworkReply::NoError && http == 200;
        const QString why = m_reply->errorString();
        m_reply->deleteLater();
        m_reply = nullptr;
        if (m_download) {
            m_download->write(QByteArray());
            m_download->close();
        }
        if (!ok || m_cancel) {
            QFile::remove(zip_path() % QStringLiteral(".part"));
            m_download.reset();
            m_error = m_cancel ? QStringLiteral("Stopped") : (http > 0 ? QStringLiteral("The download failed (%1)").arg(http) : why);
            Log::warning(LOG_TAG, LOGMSG("Download failed: %1").arg(m_error));
            m_stage.clear();
            m_working = false;
            emit finished(status());
            return;
        }
        QFile::remove(zip_path());
        m_download->rename(zip_path());
        m_download.reset();
        Log::info(LOG_TAG, LOGMSG("Downloaded `%1` (%2 MB)").arg(zip_path()).arg(QFileInfo(zip_path()).size() / 1048576));
        build(esde_systems);
    });
}

void LaunchBoxDb::build(const QStringList& esde_systems)
{
    m_stage = QStringLiteral("reading");
    m_progress = 0;
    m_loaded.clear();
    emit progress(status());
    QSet<QString> platforms;
    for (const QString& system : esde_systems) {
        const QString platform = platform_for(system);
        if (platform.isEmpty())
            Log::info(LOG_TAG, LOGMSG("No LaunchBox platform known for the system `%1`").arg(system));
        else
            platforms.insert(platform);
    }
    // the platforms indexed before stay indexed
    const QJsonObject saved = read_json(status_path()).value(QStringLiteral("platforms")).toObject();
    for (auto it = saved.constBegin(); it != saved.constEnd(); ++it)
        platforms.insert(it.key());
    if (platforms.isEmpty()) {
        m_error = QStringLiteral("None of your systems is one LaunchBox knows");
        m_stage.clear();
        m_working = false;
        emit finished(status());
        return;
    }
    const std::atomic<bool>* cancel = &m_cancel;
    std::atomic<int>* progress_out = &m_progress;
    if (!m_watcher) {
        m_watcher = new QFutureWatcher<QString>(this);
        connect(m_watcher, &QFutureWatcher<QString>::finished, this, [this]{
            m_error = m_watcher->result();
            m_stage.clear();
            m_working = false;
            if (m_error.isEmpty())
                Log::info(LOG_TAG, LOGMSG("The database is ready"));
            else
                Log::warning(LOG_TAG, LOGMSG("Reading the database: %1").arg(m_error));
            emit finished(status());
        });
    }
    m_watcher->setFuture(QtConcurrent::run([platforms, cancel, progress_out]() -> QString {
        const utils::ZipReader zip(zip_path());
        if (!zip.isReadable())
            return QStringLiteral("The download can't be read: download it again");
        Build b;
        b.wanted_platforms = platforms;
        QString error = read_xml(zip, b, *cancel, *progress_out);
        if (!error.isEmpty())
            return error;
        if (b.kept_images == 0 && b.unknown_extras > 0 && !b.games.empty()) {
            // the names and pictures came before their games: once more for those
            Log::info(LOG_TAG, LOGMSG("Reading the names and pictures again (%1 came before their games)").arg(b.unknown_extras));
            b.extras_only = true;
            b.unknown_extras = 0;
            error = read_xml(zip, b, *cancel, *progress_out);
            if (!error.isEmpty())
                return error;
        }
        // one file per platform
        QHash<QString, QJsonArray> per_platform;
        for (const auto& pair : b.games) {
            const Game& g = pair.second;
            QJsonArray images;
            for (const Image& i : g.images)
                images.append(QJsonArray { i.type, i.file, i.region });
            per_platform[g.platform].append(QJsonObject {
                { QStringLiteral("id"), g.id }, { QStringLiteral("n"), g.name },
                { QStringLiteral("alt"), QJsonArray::fromStringList(g.alt_names) },
                { QStringLiteral("rel"), g.release.isValid() ? g.release.toString(Qt::ISODate) : QString() },
                { QStringLiteral("ov"), g.overview },
                { QStringLiteral("dev"), QJsonArray::fromStringList(g.developers) },
                { QStringLiteral("pub"), QJsonArray::fromStringList(g.publishers) },
                { QStringLiteral("gen"), QJsonArray::fromStringList(g.genres) },
                { QStringLiteral("pl"), g.players }, { QStringLiteral("rt"), static_cast<double>(g.rating) },
                { QStringLiteral("img"), images },
            });
        }
        QJsonObject counts;
        int total = 0;
        for (const QString& platform : platforms) {
            const QJsonArray games = per_platform.value(platform);
            if (!write_json(index_path(platform), QJsonDocument(games)))
                return QStringLiteral("Couldn't write to %1").arg(index_dir());
            counts.insert(platform, games.size());
            total += games.size();
            Log::info(LOG_TAG, LOGMSG("%1: %2 games").arg(platform).arg(games.size()));
        }
        QJsonObject all;
        for (auto it = b.platform_counts.cbegin(); it != b.platform_counts.cend(); ++it)
            all.insert(it.key(), it.value());
        write_json(platforms_path(), QJsonDocument(all));
        write_json(status_path(), QJsonDocument(QJsonObject {
            { QStringLiteral("date"), QDateTime::currentDateTime().toString(Qt::ISODate) },
            { QStringLiteral("platforms"), counts },
            { QStringLiteral("games"), total },
            { QStringLiteral("images"), b.kept_images },
        }));
        return {};
    }));
}

const std::vector<LaunchBoxDb::Game>& LaunchBoxDb::platform_games(const QString& platform)
{
    auto it = m_loaded.find(platform);
    if (it != m_loaded.end())
        return it.value();
    std::vector<Game> games;
    QFile file(index_path(platform));
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonArray list = QJsonDocument::fromJson(file.readAll()).array();
        games.reserve(static_cast<size_t>(list.size()));
        for (const QJsonValue& v : list) {
            const QJsonObject o = v.toObject();
            Game g;
            g.id = o.value(QStringLiteral("id")).toInt();
            g.name = o.value(QStringLiteral("n")).toString();
            for (const QJsonValue& a : o.value(QStringLiteral("alt")).toArray())
                g.alt_names.append(a.toString());
            g.platform = platform;
            g.release = QDate::fromString(o.value(QStringLiteral("rel")).toString(), Qt::ISODate);
            g.year = g.release.isValid() ? g.release.year() : 0;
            g.overview = o.value(QStringLiteral("ov")).toString();
            for (const QJsonValue& a : o.value(QStringLiteral("dev")).toArray()) g.developers.append(a.toString());
            for (const QJsonValue& a : o.value(QStringLiteral("pub")).toArray()) g.publishers.append(a.toString());
            for (const QJsonValue& a : o.value(QStringLiteral("gen")).toArray()) g.genres.append(a.toString());
            g.players = o.value(QStringLiteral("pl")).toInt();
            g.rating = static_cast<float>(o.value(QStringLiteral("rt")).toDouble());
            for (const QJsonValue& a : o.value(QStringLiteral("img")).toArray()) {
                const QJsonArray i = a.toArray();
                g.images.push_back({ i.at(0).toString(), i.at(1).toString(), i.at(2).toString() });
            }
            games.push_back(std::move(g));
        }
    }
    return m_loaded.insert(platform, std::move(games)).value();
}

std::vector<LaunchBoxDb::Game> LaunchBoxDb::candidates(const QString& esde_system, const QString& name, int limit)
{
    std::vector<Game> out;
    const QString platform = platform_for(esde_system);
    if (platform.isEmpty())
        return out;
    const QString key = plain_key(name);
    for (const Game& g : platform_games(platform)) {
        int score = name_score(name, g.name);
        for (const QString& alt : g.alt_names) {
            if (score >= 100)
                break;
            score = std::max(score, plain_key(alt) == key ? 98 : name_score(name, alt) - 2);
        }
        if (score <= 0)
            continue;
        Game copy = g;
        copy.score = score;
        out.push_back(std::move(copy));
    }
    std::stable_sort(out.begin(), out.end(), [](const Game& a, const Game& b) { return a.score > b.score; });
    if (static_cast<int>(out.size()) > limit)
        out.resize(static_cast<size_t>(limit));
    return out;
}

LaunchBoxDb::Game LaunchBoxDb::game(const QString& esde_system, int id)
{
    const QString platform = platform_for(esde_system);
    if (!platform.isEmpty()) {
        for (const Game& g : platform_games(platform)) {
            if (g.id == id)
                return g;
        }
    }
    return Game();
}

} // namespace scraper
