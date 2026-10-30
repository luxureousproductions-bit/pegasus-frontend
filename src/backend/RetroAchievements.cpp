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


#include "RetroAchievements.h"

#include "Log.h"
#include "Paths.h"
#include "providers/esde/EsdeProvider.h"
#include "utils/ZipReader.h"

#include "rc_hash.h"

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
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QtConcurrent/QtConcurrent>


namespace {
const QString LOG_TAG = QStringLiteral("RetroAchievements");
const char* const API_BASE = "https://retroachievements.org/API/";
const char* const MEDIA_BASE = "https://media.retroachievements.org";
const int KEEP_DAYS = 7;
const int PROGRESS_BATCH = 50;

// ES-DE's system names -> RA's console ids (rc_consoles.h)
const QHash<QString, int>& consoles()
{
    static const QHash<QString, int> map {
        { "nes", 7 }, { "famicom", 7 }, { "fds", 81 }, { "snes", 3 }, { "snesna", 3 }, { "sfc", 3 }, { "satellaview", 3 }, { "sufami", 3 },
        { "n64", 2 }, { "n64dd", 2 }, { "gb", 4 }, { "gbc", 6 }, { "gba", 5 }, { "nds", 18 }, { "dsi", 78 }, { "n3ds", 62 }, { "3ds", 62 },
        { "gc", 16 }, { "gamecube", 16 }, { "wii", 19 }, { "wiiu", 20 }, { "virtualboy", 28 }, { "pokemini", 24 }, { "gameandwatch", 60 },
        { "megadrive", 1 }, { "genesis", 1 }, { "megadrivejp", 1 }, { "mastersystem", 11 }, { "gamegear", 15 }, { "segacd", 9 }, { "megacd", 9 },
        { "megacdjp", 9 }, { "sega32x", 10 }, { "sega32xna", 10 }, { "sega32xjp", 10 }, { "saturn", 39 }, { "saturnjp", 39 }, { "dreamcast", 40 },
        { "sg-1000", 33 }, { "sg1000", 33 }, { "pico", 68 },
        { "psx", 12 }, { "ps2", 21 }, { "ps3", 82 }, { "psp", 41 },
        { "pcengine", 8 }, { "tg16", 8 }, { "supergrafx", 8 }, { "pcenginecd", 76 }, { "tg-cd", 76 }, { "pcfx", 49 },
        { "arcade", 27 }, { "mame", 27 }, { "fbneo", 27 }, { "neogeo", 27 }, { "cps", 27 }, { "cps1", 27 }, { "cps2", 27 }, { "cps3", 27 },
        { "naomi", 27 }, { "naomi2", 27 }, { "atomiswave", 27 }, { "model2", 27 }, { "model3", 27 }, { "neogeocd", 56 }, { "neogeocdjp", 56 },
        { "atari2600", 25 }, { "atari5200", 50 }, { "atari7800", 51 }, { "atarilynx", 13 }, { "lynx", 13 }, { "atarijaguar", 17 }, { "jaguar", 17 },
        { "atarijaguarcd", 77 }, { "atarist", 36 }, { "ngp", 14 }, { "ngpc", 14 }, { "wonderswan", 53 }, { "wonderswancolor", 53 },
        { "colecovision", 44 }, { "intellivision", 45 }, { "vectrex", 46 }, { "msx", 29 }, { "msx1", 29 }, { "msx2", 29 }, { "msxturbor", 29 },
        { "c64", 30 }, { "amiga", 35 }, { "amiga600", 35 }, { "amiga1200", 35 }, { "amigacd32", 35 }, { "apple2", 38 }, { "3do", 43 },
        { "cdimono1", 42 }, { "dos", 26 }, { "pc", 26 }, { "zxspectrum", 59 }, { "zx81", 31 }, { "channelf", 57 }, { "supervision", 63 },
        { "odyssey2", 23 }, { "videopac", 23 }, { "arcadia", 73 }, { "vc4000", 74 }, { "oric", 32 }, { "vic20", 34 }, { "amstradcpc", 37 },
        { "x68000", 52 }, { "pc88", 47 }, { "pc98", 48 }, { "fmtowns", 58 }, { "scv", 55 }, { "cassettevision", 54 }, { "megaduck", 69 },
        { "ngage", 61 }, { "ti83", 79 }, { "zeebo", 70 }, { "wasm4", 72 }, { "arduboy", 71 }, { "tic80", 65 }, { "uzebox", 80 },
        { "x1", 64 }, { "pc6000", 67 }, { "to8", 66 }, { "elektor", 75 },
    };
    return map;
}

QString plain_title(QString title)
{
    static const QRegularExpression rx_tags(QStringLiteral("\\s*[\\(\\[][^\\)\\]]*[\\)\\]]"));
    title.remove(rx_tags);
    QString out;
    for (const QChar c : title.toLower()) {
        if (c.isLetterOrNumber())
            out += c;
    }
    return out;
}

// the ROM inside a .zip: the first file that isn't itself an archive
QByteArray rom_in_zip(const QString& path, QString& name)
{
    utils::ZipReader zip(path);
    if (!zip.isReadable())
        return {};
    static const QStringList archives { "zip", "7z", "rar" };
    for (const utils::ZipReader::Entry& entry : zip.entries()) {
        if (entry.is_dir || entry.size == 0)
            continue;
        if (archives.contains(QFileInfo(entry.path).suffix().toLower()))
            continue;
        name = entry.path;
        bool ok = false;
        const QByteArray data = zip.fileData(entry, &ok);
        return ok ? data : QByteArray();
    }
    return {};
}
} // namespace


namespace scraper {

RetroAchievements::RetroAchievements(QObject* parent)
    : QObject(parent)
{}

int RetroAchievements::console_for(const QString& esde_system)
{
    return consoles().value(esde_system.toLower(), 0);
}

QString RetroAchievements::dir()
{
    return paths::writableConfigDir() % QStringLiteral("/metadata/ra");
}

QString RetroAchievements::hash_file(int console, const QString& path)
{
    if (console <= 0 || path.isEmpty())
        return {};
    char hash[33] = {};
    const QFileInfo info(path);
    // arcade and MS-DOS hash the zip itself (its name, its contents); a
    // cartridge in a .zip is the ROM inside
    if (info.suffix().compare(QLatin1String("zip"), Qt::CaseInsensitive) == 0 && console != 27 && console != 26) {
        QString inner;
        const QByteArray rom = rom_in_zip(path, inner);
        if (rom.isEmpty())
            return {};
        if (!rc_hash_generate_from_buffer(hash, static_cast<uint32_t>(console),
                                          reinterpret_cast<const uint8_t*>(rom.constData()), static_cast<size_t>(rom.size())))
            return {};
        return QString::fromLatin1(hash);
    }
    const QByteArray utf8 = QDir::toNativeSeparators(path).toUtf8();
    if (!rc_hash_generate_from_file(hash, static_cast<uint32_t>(console), utf8.constData()))
        return {};
    return QString::fromLatin1(hash);
}

QVariantMap RetroAchievements::console_list(int console)
{
    QFile file(dir() % QStringLiteral("/console-") % QString::number(console) % QStringLiteral(".json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
}

QVariantMap RetroAchievements::state() const
{
    return QVariantMap {
        { QStringLiteral("running"), m_running },
        { QStringLiteral("stage"), m_stage },
        { QStringLiteral("done"), m_done },
        { QStringLiteral("total"), m_total },
    };
}

void RetroAchievements::report()
{
    emit progress(state());
}

void RetroAchievements::get(const QUrl& url, std::function<void(int, const QByteArray&)> done)
{
    if (!m_net)
        m_net = new QNetworkAccessManager(this);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "PegasusX/1.0 (Pegasus frontend)");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* const reply = m_net->get(request);
    const int run = m_run;
    connect(reply, &QNetworkReply::finished, this, [this, reply, done, run]{
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        if (run != m_run)
            return;
        done(status, body);
    });
}

QUrl RetroAchievements::apiUrl(const QString& endpoint, const QList<QPair<QString, QString>>& params) const
{
    QUrl url(QLatin1String(API_BASE) % endpoint);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("z"), m_user);
    query.addQueryItem(QStringLiteral("y"), m_key);
    for (const auto& p : params)
        query.addQueryItem(p.first, p.second);
    url.setQuery(query);
    return url;
}

void RetroAchievements::checkLogin(const QString& user, const QString& key)
{
    m_user = user.trimmed();
    m_key = key.trimmed();
    if (m_user.isEmpty() || m_key.isEmpty()) {
        emit loginChecked(QVariantMap { { QStringLiteral("ok"), false }, { QStringLiteral("text"), tr("No username and web API key yet") } });
        return;
    }
    get(apiUrl(QStringLiteral("API_GetUserProfile.php"), { { QStringLiteral("u"), m_user } }), [this](int status, const QByteArray& body){
        const QJsonObject profile = QJsonDocument::fromJson(body).object();
        QVariantMap out;
        if (status == 200 && profile.contains(QStringLiteral("User"))) {
            out.insert(QStringLiteral("ok"), true);
            out.insert(QStringLiteral("text"), tr("Login works · %1 points").arg(profile.value(QStringLiteral("TotalPoints")).toVariant().toString()));
        }
        else {
            out.insert(QStringLiteral("ok"), false);
            out.insert(QStringLiteral("text"), status == 401 ? tr("The username or key isn't accepted")
                       : status == 0 ? tr("No connection") : tr("RetroAchievements answered %1").arg(status));
        }
        emit loginChecked(out);
    });
}

void RetroAchievements::start(std::vector<Job> jobs, const QString& user, const QString& key, bool icons,
                              const QString& media_dir, bool progress_only)
{
    if (m_running)
        return;
    m_run++;
    m_running = true;
    m_stop = false;
    m_user = user.trimmed();
    m_key = key.trimmed();
    m_icons = icons;
    m_media_dir = media_dir;
    m_jobs = std::move(jobs);
    m_matches.clear();
    m_result = Result();
    m_done = 0;
    m_total = static_cast<int>(m_jobs.size());
    Log::info(LOG_TAG, LOGMSG("%1 games%2").arg(m_jobs.size()).arg(progress_only ? QStringLiteral(" (progress only)") : QString()));

    if (progress_only) {
        for (const Job& job : m_jobs) {
            Match m;
            m.job = job;
            m.ra_id = job.ra_id;
            m_matches.push_back(std::move(m));
        }
        m_stage = tr("your progress");
        report();
        fetchProgress([this]{ writeAll(); finish(); });
        return;
    }

    QList<int> needed;
    for (const Job& job : m_jobs) {
        const int console = console_for(job.system);
        if (console > 0 && !needed.contains(console))
            needed.append(console);
    }
    m_stage = tr("the game lists");
    report();
    fetchLists(needed, [this]{ hashAndMatch(); });
}

void RetroAchievements::stop()
{
    if (!m_running)
        return;
    m_stop = true;
}

void RetroAchievements::finish()
{
    m_running = false;
    m_stage.clear();
    Log::info(LOG_TAG, LOGMSG("Done: %1 matched (%2 with achievements), %3 not on RA, %4 unsupported, %5 with progress%6")
              .arg(m_result.matched).arg(m_result.with_achievements).arg(m_result.not_found).arg(m_result.unsupported)
              .arg(m_result.progress).arg(m_result.error.isEmpty() ? QString() : QStringLiteral(" · ") % m_result.error));
    report();
    emit finished(QVariantMap {
        { QStringLiteral("matched"), m_result.matched },
        { QStringLiteral("withAchievements"), m_result.with_achievements },
        { QStringLiteral("notFound"), m_result.not_found },
        { QStringLiteral("unsupported"), m_result.unsupported },
        { QStringLiteral("progress"), m_result.progress },
        { QStringLiteral("stopped"), m_stop.load() },
        { QStringLiteral("error"), m_result.error },
    });
}

// each console's game list with hashes, one call each; the copy kept for a
// week is used when it's there
void RetroAchievements::fetchLists(QList<int> consoles, std::function<void()> then)
{
    while (!consoles.isEmpty()) {
        const QVariantMap cached = console_list(consoles.first());
        const QDateTime fetched = QDateTime::fromString(cached.value(QStringLiteral("fetched")).toString(), Qt::ISODate);
        if (fetched.isValid() && fetched.daysTo(QDateTime::currentDateTimeUtc()) < KEEP_DAYS)
            consoles.removeFirst();
        else
            break;
    }
    if (consoles.isEmpty() || m_stop) {
        then();
        return;
    }
    const int console = consoles.takeFirst();
    m_stage = tr("the game list of console %1").arg(console);
    report();
    get(apiUrl(QStringLiteral("API_GetGameList.php"), { { QStringLiteral("i"), QString::number(console) }, { QStringLiteral("h"), QStringLiteral("1") } }),
        [this, console, consoles, then](int status, const QByteArray& body){
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (status != 200 || !doc.isArray()) {
            Log::warning(LOG_TAG, LOGMSG("Console %1: the game list couldn't be read (%2)").arg(console).arg(status));
            if (m_result.error.isEmpty())
                m_result.error = status == 401 ? tr("The username or key isn't accepted")
                               : status == 0 ? tr("No connection") : tr("RetroAchievements answered %1").arg(status);
            if (status == 401 || status == 0) {
                finish();
                return;
            }
            fetchLists(consoles, then);
            return;
        }
        QJsonArray games;
        for (const QJsonValue& v : doc.array()) {
            const QJsonObject g = v.toObject();
            games.append(QJsonObject {
                { QStringLiteral("id"), g.value(QStringLiteral("ID")).toInt() },
                { QStringLiteral("title"), g.value(QStringLiteral("Title")).toString() },
                { QStringLiteral("icon"), g.value(QStringLiteral("ImageIcon")).toString() },
                { QStringLiteral("achievements"), g.value(QStringLiteral("NumAchievements")).toInt() },
                { QStringLiteral("points"), g.value(QStringLiteral("Points")).toInt() },
                { QStringLiteral("hashes"), g.value(QStringLiteral("Hashes")).toArray() },
            });
        }
        QDir().mkpath(dir());
        QSaveFile file(dir() % QStringLiteral("/console-") % QString::number(console) % QStringLiteral(".json"));
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(QJsonObject {
                { QStringLiteral("fetched"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
                { QStringLiteral("console"), console },
                { QStringLiteral("games"), games },
            }).toJson(QJsonDocument::Compact));
            file.commit();
        }
        Log::info(LOG_TAG, LOGMSG("Console %1: %2 games listed").arg(console).arg(games.size()));
        fetchLists(consoles, then);
    });
}

// off the main thread: every game's hash, looked up in its console's list
void RetroAchievements::hashAndMatch()
{
    m_stage = tr("hashing");
    m_done = 0;
    report();
    const std::vector<Job> jobs = m_jobs;
    const int run = m_run;
    std::atomic<bool>* const stop = &m_stop;
    auto* const progress_count = new std::atomic<int>(0);
    m_hash_watcher = new QFutureWatcher<std::vector<Match>>(this);
    auto* const timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, progress_count]{ m_done = progress_count->load(); report(); });
    timer->start(500);
    connect(m_hash_watcher, &QFutureWatcher<std::vector<Match>>::finished, this, [this, run, timer, progress_count]{
        timer->stop();
        timer->deleteLater();
        const std::vector<Match> matches = m_hash_watcher->result();
        m_hash_watcher->deleteLater();
        m_hash_watcher = nullptr;
        delete progress_count;
        if (run != m_run)
            return;
        m_matches = matches;
        for (const Match& m : m_matches) {
            if (m.unsupported) m_result.unsupported++;
            else if (m.ra_id > 0) { m_result.matched++; if (m.achievements > 0) m_result.with_achievements++; }
            else m_result.not_found++;
        }
        m_done = m_total;
        if (m_stop) {
            writeAll();
            finish();
            return;
        }
        m_stage = tr("the icons");
        report();
        downloadIcons(0, [this]{
            m_stage = tr("your progress");
            report();
            fetchProgress([this]{ writeAll(); finish(); });
        });
    });
    m_hash_watcher->setFuture(QtConcurrent::run([jobs, stop, progress_count]{
        // the lists, read once per console: hash -> game, plain title -> game
        struct Entry { int id; QString title; QString icon; int achievements; int points; };
        QHash<int, QHash<QString, Entry>> by_hash;
        QHash<int, QHash<QString, Entry>> by_title;
        std::vector<Match> out;
        out.reserve(jobs.size());
        for (const Job& job : jobs) {
            if (stop->load())
                break;
            Match m;
            m.job = job;
            const int console = console_for(job.system);
            if (console <= 0) {
                m.unsupported = true;
                out.push_back(std::move(m));
                (*progress_count)++;
                continue;
            }
            if (!by_hash.contains(console)) {
                QHash<QString, Entry> hashes;
                QHash<QString, Entry> titles;
                const QVariantList games = console_list(console).value(QStringLiteral("games")).toList();
                for (const QVariant& v : games) {
                    const QVariantMap g = v.toMap();
                    const Entry e { g.value(QStringLiteral("id")).toInt(), g.value(QStringLiteral("title")).toString(),
                                    g.value(QStringLiteral("icon")).toString(), g.value(QStringLiteral("achievements")).toInt(),
                                    g.value(QStringLiteral("points")).toInt() };
                    for (const QVariant& h : g.value(QStringLiteral("hashes")).toList())
                        hashes.insert(h.toString().toLower(), e);
                    const QString plain = plain_title(e.title);
                    if (!plain.isEmpty() && !titles.contains(plain))
                        titles.insert(plain, e);
                }
                by_hash.insert(console, hashes);
                by_title.insert(console, titles);
            }
            m.hash = hash_file(console, job.path);
            const Entry* found = nullptr;
            if (!m.hash.isEmpty()) {
                const auto it = by_hash[console].constFind(m.hash.toLower());
                if (it != by_hash[console].cend())
                    found = &it.value();
            }
            else {
                // the file can't be hashed (a format rcheevos doesn't read, eg. .chd): by its name
                const auto it = by_title[console].constFind(plain_title(job.title));
                if (it != by_title[console].cend()) {
                    found = &it.value();
                    m.by_title = true;
                }
                else {
                    m.unsupported = true;
                }
            }
            if (found) {
                m.ra_id = found->id;
                m.ra_title = found->title;
                m.icon = found->icon;
                m.achievements = found->achievements;
                m.points = found->points;
            }
            out.push_back(std::move(m));
            (*progress_count)++;
        }
        return out;
    }));
}

// RA's icon of each matched game, into <media>/<system>/ra/<game>.png (once)
void RetroAchievements::downloadIcons(size_t index, std::function<void()> then)
{
    if (!m_icons || m_stop) {
        then();
        return;
    }
    while (index < m_matches.size()) {
        Match& m = m_matches[index];
        if (m.ra_id > 0 && !m.icon.isEmpty()) {
            const QString target = m_media_dir % QLatin1Char('/') % m.job.system % QStringLiteral("/ra/")
                % QFileInfo(m.job.path).completeBaseName() % QStringLiteral(".png");
            if (QFileInfo::exists(target)) {
                m.icon_file = target;
                index++;
                continue;
            }
            m_done = static_cast<int>(index);
            get(QUrl(QLatin1String(MEDIA_BASE) % m.icon), [this, index, target, then](int status, const QByteArray& body){
                if (status == 200 && !body.isEmpty()) {
                    QDir().mkpath(QFileInfo(target).absolutePath());
                    QSaveFile file(target);
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(body);
                        if (file.commit())
                            m_matches[index].icon_file = target;
                    }
                }
                downloadIcons(index + 1, then);
            });
            return;
        }
        index++;
    }
    then();
}

// your progress on the matched games, in batches
void RetroAchievements::fetchProgress(std::function<void()> then)
{
    QStringList ids;
    for (const Match& m : m_matches) {
        if (m.ra_id > 0 && !m.progress_known)
            ids.append(QString::number(m.ra_id));
    }
    if (ids.isEmpty() || m_stop) {
        then();
        return;
    }
    const QStringList batch = ids.mid(0, PROGRESS_BATCH);
    m_done = m_total - ids.size();
    report();
    get(apiUrl(QStringLiteral("API_GetUserProgress.php"), { { QStringLiteral("u"), m_user }, { QStringLiteral("i"), batch.join(QLatin1Char(',')) } }),
        [this, batch, then](int status, const QByteArray& body){
        const QJsonObject all = QJsonDocument::fromJson(body).object();
        for (Match& m : m_matches) {
            if (m.ra_id <= 0 || !batch.contains(QString::number(m.ra_id)))
                continue;
            m.progress_known = true;  // asked, answered or not
            const QJsonObject p = all.value(QString::number(m.ra_id)).toObject();
            if (status == 200 && !p.isEmpty()) {
                m.earned = p.value(QStringLiteral("NumAchieved")).toInt();
                m.earned_hardcore = p.value(QStringLiteral("NumAchievedHardcore")).toInt();
                if (p.value(QStringLiteral("NumPossibleAchievements")).toInt() > 0)
                    m.achievements = p.value(QStringLiteral("NumPossibleAchievements")).toInt();
                m.has_progress = true;
                m_result.progress++;
            }
        }
        if (status != 200)
            Log::warning(LOG_TAG, LOGMSG("Progress: answered %1").arg(status));
        fetchProgress(then);
    });
}

// what was found, into each game's entry (x-ra-* fields), per collection
void RetroAchievements::writeAll()
{
    m_stage = tr("writing");
    report();
    QHash<QString, std::vector<providers::esde::GameEdit>> per_collection;
    for (const Match& m : m_matches) {
        if (m.unsupported && m.ra_id <= 0)
            continue;
        QVariantMap changes;
        if (m.ra_id > 0) {
            changes.insert(QStringLiteral("x-ra-id"), QString::number(m.ra_id));
            changes.insert(QStringLiteral("x-ra-title"), m.ra_title);
            changes.insert(QStringLiteral("x-ra-achievements"), QString::number(m.achievements));
            changes.insert(QStringLiteral("x-ra-points"), QString::number(m.points));
            changes.insert(QStringLiteral("x-ra-match"), m.by_title ? QStringLiteral("title") : m.hash.isEmpty() ? QString() : QStringLiteral("hash"));
            if (!m.icon_file.isEmpty())
                changes.insert(QStringLiteral("x-ra-icon"), QUrl::fromLocalFile(m.icon_file).toString());
            if (m.has_progress) {
                changes.insert(QStringLiteral("x-ra-earned"), QString::number(m.earned));
                changes.insert(QStringLiteral("x-ra-earned-hardcore"), QString::number(m.earned_hardcore));
            }
        }
        else {
            // checked, not on RA: said, so it isn't checked again until you ask
            changes.insert(QStringLiteral("x-ra-id"), QStringLiteral("0"));
            changes.insert(QStringLiteral("x-ra-match"), QStringLiteral("none"));
        }
        if (!m.hash.isEmpty())
            changes.insert(QStringLiteral("x-ra-hash"), m.hash);
        providers::esde::GameEdit edit;
        edit.path = m.job.path;
        edit.title = m.job.title;
        edit.changes = changes;
        per_collection[m.job.collection].push_back(std::move(edit));
    }
    for (auto it = per_collection.begin(); it != per_collection.end(); ++it) {
        const QString error = providers::esde::save_games_metadata(it.key(), it.value(), true);
        if (!error.isEmpty() && m_result.error.isEmpty())
            m_result.error = error;
    }
}

} // namespace scraper
