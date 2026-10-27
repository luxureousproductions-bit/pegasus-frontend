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


#include "GameDirectory.h"

#include "AppSettings.h"
#include "Log.h"
#include "Paths.h"
#include "model/gaming/Assets.h"
#include "model/gaming/Collection.h"
#include "model/gaming/Game.h"
#include "model/gaming/GameFile.h"
#include "model/gaming/GameFileListModel.h"
#include "model/gaming/GameListModel.h"
#include "types/AssetType.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringBuilder>
#include <QTextStream>
#include <QUrl>
#include <algorithm>
#include <atomic>


namespace {
constexpr auto LOG_TAG = "Game directory";
const QString FILE_NAME = QStringLiteral("metadata.pegasus.txt");
std::atomic<bool> s_reload_wanted { false };

QString safe_name(const QString& name)
{
    QString out;
    for (const QChar c : name) {
        if (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_'))
            out += c.toLower();
        else if (c == QLatin1Char(' ') || c == QLatin1Char('.'))
            out += QLatin1Char('_');
    }
    return out.isEmpty() ? QStringLiteral("other") : out;
}

// a value with line breaks, as the format writes it (continuation lines, "." for an empty one)
QString metafile_value(const QString& value)
{
    QStringList lines = value.trimmed().split(QLatin1Char('\n'));
    for (int i = 1; i < lines.size(); i++) {
        const QString line = lines.at(i).trimmed();
        lines[i] = QStringLiteral("  ") % (line.isEmpty() ? QStringLiteral(".") : line);
    }
    return lines.join(QLatin1Char('\n'));
}

QString asset_key(AssetType type)
{
    switch (type) {
        case AssetType::BOX_FRONT: return QStringLiteral("box_front");
        case AssetType::BOX_BACK: return QStringLiteral("box_back");
        case AssetType::BOX_SPINE: return QStringLiteral("box_spine");
        case AssetType::BOX_FULL: return QStringLiteral("box_full");
        case AssetType::CARTRIDGE: return QStringLiteral("cartridge");
        case AssetType::LOGO: return QStringLiteral("logo");
        case AssetType::POSTER: return QStringLiteral("poster");
        case AssetType::ARCADE_MARQUEE: return QStringLiteral("marquee");
        case AssetType::ARCADE_BEZEL: return QStringLiteral("bezel");
        case AssetType::ARCADE_PANEL: return QStringLiteral("panel");
        case AssetType::ARCADE_CABINET_L: return QStringLiteral("cabinet_left");
        case AssetType::ARCADE_CABINET_R: return QStringLiteral("cabinet_right");
        case AssetType::UI_TILE: return QStringLiteral("tile");
        case AssetType::UI_BANNER: return QStringLiteral("banner");
        case AssetType::UI_STEAMGRID: return QStringLiteral("steam");
        case AssetType::BACKGROUND: return QStringLiteral("background");
        case AssetType::MUSIC: return QStringLiteral("music");
        case AssetType::SCREENSHOT: return QStringLiteral("screenshot");
        case AssetType::TITLESCREEN: return QStringLiteral("titlescreen");
        case AssetType::VIDEO: return QStringLiteral("video");
        default: return {};
    }
}

QString local_path(const QString& value)
{
    const QUrl url(value);
    return url.isLocalFile() ? url.toLocalFile() : value;
}

QString joined(const QStringList& list)
{
    QStringList out;
    for (const QString& value : list) {
        if (!value.trimmed().isEmpty())
            out.append(value.trimmed());
    }
    return out.join(QStringLiteral(", "));
}

// one game, as the game directory writes it: everything it has, absolute paths
QString game_entry(const model::Game& game, const QString& collection_launch)
{
    QString out;
    QTextStream stream(&out);
    stream << "game: " << metafile_value(game.title()) << '\n';
    QStringList files;
    for (const model::GameFile* const file : game.filesModel()->entries())
        files.append(file->hasUri() ? file->uri() : file->path());
    if (files.size() == 1) {
        stream << "file: " << files.first() << '\n';
    }
    else {
        stream << "files:\n";
        for (const QString& file : files)
            stream << "  " << file << '\n';
    }
    if (!game.sortBy().isEmpty() && game.sortBy() != game.title())
        stream << "sort-by: " << game.sortBy() << '\n';
    const QString developers = joined(game.developerListConst());
    const QString publishers = joined(game.publisherListConst());
    const QString genres = joined(game.genreListConst());
    const QString tags = joined(game.tagListConst());
    if (!developers.isEmpty()) stream << "developer: " << developers << '\n';
    if (!publishers.isEmpty()) stream << "publisher: " << publishers << '\n';
    if (!genres.isEmpty()) stream << "genre: " << genres << '\n';
    if (!tags.isEmpty()) stream << "tag: " << tags << '\n';
    if (game.playerCount() > 1) stream << "players: " << game.playerCount() << '\n';
    if (game.releaseDate().isValid()) stream << "release: " << game.releaseDate().toString(Qt::ISODate) << '\n';
    if (!game.summary().isEmpty()) stream << "summary: " << metafile_value(game.summary()) << '\n';
    if (!game.description().isEmpty()) stream << "description: " << metafile_value(game.description()) << '\n';
    // the game's own emulator (not the collection's, which every game gets)
    if (!game.launchCmd().isEmpty() && game.launchCmd() != collection_launch)
        stream << "launch: " << game.launchCmd() << '\n';
    if (game.rating() > 0.f) stream << "rating: " << qRound(game.rating() * 100.f) << "%\n";

    const model::Assets& a = game.assets();
    const std::vector<std::pair<AssetType, const QStringList*>> lists {
        { AssetType::BOX_FRONT, &a.boxFrontList() }, { AssetType::BOX_BACK, &a.boxBackList() },
        { AssetType::BOX_SPINE, &a.boxSpineList() }, { AssetType::BOX_FULL, &a.boxFullList() },
        { AssetType::CARTRIDGE, &a.cartridgeList() }, { AssetType::LOGO, &a.logoList() },
        { AssetType::POSTER, &a.posterList() }, { AssetType::ARCADE_MARQUEE, &a.marqueeList() },
        { AssetType::ARCADE_BEZEL, &a.bezelList() }, { AssetType::ARCADE_PANEL, &a.panelList() },
        { AssetType::ARCADE_CABINET_L, &a.cabinetLeftList() }, { AssetType::ARCADE_CABINET_R, &a.cabinetRightList() },
        { AssetType::UI_TILE, &a.tileList() }, { AssetType::UI_BANNER, &a.bannerList() },
        { AssetType::UI_STEAMGRID, &a.steamList() }, { AssetType::BACKGROUND, &a.backgroundList() },
        { AssetType::MUSIC, &a.musicList() }, { AssetType::SCREENSHOT, &a.screenshotList() },
        { AssetType::TITLESCREEN, &a.titlescreenList() }, { AssetType::VIDEO, &a.videoList() },
    };
    for (const auto& entry : lists) {
        const QString key = asset_key(entry.first);
        QStringList paths;
        for (const QString& f : *entry.second) {
            const QString path = local_path(f);
            if (!path.isEmpty() && !path.startsWith(QLatin1String("image://")) && !paths.contains(path))
                paths.append(path);
        }
        if (key.isEmpty() || paths.isEmpty())
            continue;
        stream << "assets." << key << ": " << paths.first() << '\n';
        for (int i = 1; i < paths.size(); i++)
            stream << "  " << paths.at(i) << '\n';
    }
    // x- fields that are the game's (not Pegasus X's own bookkeeping)
    for (auto it = game.extraMap().cbegin(); it != game.extraMap().cend(); ++it) {
        const QString& key = it.key();
        if (key == QLatin1String("pegasusx-source") || key == QLatin1String("pegasusx-edited")
                || key == QLatin1String("pegasusx-scraped") || key == QLatin1String("box3d"))
            continue;
        QStringList values = it.value().toStringList();
        if (values.isEmpty())
            values.append(it.value().toString());
        for (QString& value : values)
            value = local_path(value);
        if (values.isEmpty() || values.first().isEmpty())
            continue;
        stream << "x-" << key << ": " << values.first() << '\n';
        for (int i = 1; i < values.size(); i++)
            stream << "  " << values.at(i) << '\n';
    }
    stream.flush();
    return out;
}

bool write_all(const QString& path, const QString& content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Text) && file.write(content.toUtf8()) >= 0 && file.commit();
}

void rewrite_game_dirs(const QStringList& dirs)
{
    const QString path = paths::writableConfigDir() % QStringLiteral("/game_dirs.txt");
    QString text;
    for (const QString& dir : dirs)
        text += dir % QLatin1Char('\n');
    if (!write_all(path, text))
        Log::warning(LOG_TAG, LOGMSG("Could not write `%1`").arg(path));
}
} // namespace


namespace providers {
namespace gamedir {

QString dir()
{
    return paths::writableConfigDir() % QStringLiteral("/gamedirectory");
}

QString key_for(const QString& collection_name, const QString& short_name)
{
    return safe_name(short_name.isEmpty() ? collection_name : short_name);
}

QString system_file(const QString& key)
{
    return dir() % QLatin1Char('/') % key % QLatin1Char('/') % FILE_NAME;
}

QStringList metafiles()
{
    QStringList out;
    for (const QFileInfo& sub : QDir(dir()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const QFileInfo file(sub.absoluteFilePath() % QLatin1Char('/') % FILE_NAME);
        if (file.isFile())
            out.append(file.absoluteFilePath());
    }
    return out;
}

bool is_inside(const QString& path)
{
    return QDir::cleanPath(path).startsWith(QDir::cleanPath(dir()) % QLatin1Char('/'));
}

QString system_of(const QString& path)
{
    if (!is_inside(path))
        return {};
    return QFileInfo(path).dir().dirName();
}

QString backup_dir(const QString& system)
{
    const QString root = paths::writableConfigDir() % QStringLiteral("/backup");
    return system.isEmpty() ? root : root % QLatin1Char('/') % system;
}

void backup(const QString& system_file_path, const QString& reason)
{
    const QString system = system_of(system_file_path);
    if (system.isEmpty() || !QFileInfo::exists(system_file_path))
        return;
    const QString folder = backup_dir(system);
    QDir().mkpath(folder);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString target = folder % QLatin1Char('/') % FILE_NAME % QLatin1Char('.') % reason % QLatin1Char('.') % stamp;
    if (QFileInfo::exists(target))
        return;  // the same second: the one there is as good
    if (!QFile::copy(system_file_path, target)) {
        Log::warning(LOG_TAG, LOGMSG("Could not back up `%1`").arg(system_file_path));
        return;
    }
    // the last 3 of this reason
    QFileInfoList olds = QDir(folder).entryInfoList({ FILE_NAME % QLatin1Char('.') % reason % QStringLiteral(".*") },
                                                    QDir::Files, QDir::Name);
    while (olds.size() > 3) {
        QFile::remove(olds.first().absoluteFilePath());
        olds.removeFirst();
    }
}

std::pair<int, qint64> backup_size()
{
    int files = 0;
    qint64 bytes = 0;
    QDirIterator it(backup_dir(), QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        files++;
        bytes += it.fileInfo().size();
    }
    return { files, bytes };
}

int clear_backups()
{
    const auto before = backup_size();
    QDir(backup_dir()).removeRecursively();
    Log::info(LOG_TAG, LOGMSG("Backups cleared: %1 files").arg(before.first));
    return before.first;
}

bool migrated()
{
    return QFileInfo::exists(dir() % QStringLiteral("/.migrated"));
}

bool migration_needed()
{
    if (migrated() || !metafiles().isEmpty())
        return false;
    const QString metadata = paths::writableConfigDir() % QStringLiteral("/metadata");
    bool old_layers = false;
    for (const char* sub : { "/imports", "/imports-fill", "/overrides" })
        old_layers = old_layers || QDir(metadata % QLatin1String(sub)).exists();
    bool other_dirs = false;
    AppSettings::parse_gamedirs([&other_dirs](const QString& line){
        if (QDir::cleanPath(line) != QDir::cleanPath(dir()))
            other_dirs = true;
    });
    return old_layers || other_dirs;
}

int migrate(const std::vector<model::Collection*>& collections, const std::vector<model::Game*>& games)
{
    Q_UNUSED(games);
    if (!migration_needed())
        return -1;
    Log::info(LOG_TAG, LOGMSG("Making the game directory from the library as it is (%1 collections)").arg(collections.size()));
    int written = 0;
    QStringList done_keys;
    for (const model::Collection* const coll : collections) {
        if (!coll || coll->gameList()->entries().empty())
            continue;
        QString key = key_for(coll->name(), coll->shortName());
        // two collections with one short name: the second by its name
        if (done_keys.contains(key))
            key = safe_name(coll->name());
        if (done_keys.contains(key))
            continue;
        done_keys.append(key);

        QString text;
        QTextStream stream(&text);
        stream << "# Pegasus X game directory: " << coll->name() << "\n"
               << "# Made from your library on " << QDateTime::currentDateTime().toString(Qt::ISODate) << ".\n"
               << "# Pegasus X writes here (imports, Settings > Library, the scraper).\n\n"
               << "collection: " << coll->name() << '\n';
        if (!coll->shortName().isEmpty())
            stream << "shortname: " << coll->shortName() << '\n';
        if (!coll->sortBy().isEmpty() && coll->sortBy() != coll->name())
            stream << "sort-by: " << coll->sortBy() << '\n';
        if (!coll->summary().isEmpty())
            stream << "summary: " << metafile_value(coll->summary()) << '\n';
        if (!coll->description().isEmpty())
            stream << "description: " << metafile_value(coll->description()) << '\n';
        if (!coll->commonLaunchCmd().isEmpty())
            stream << "launch: " << coll->commonLaunchCmd() << '\n';
        if (!coll->commonLaunchWorkdir().isEmpty())
            stream << "workdir: " << coll->commonLaunchWorkdir() << '\n';
        stream << '\n';
        int count = 0;
        for (const model::Game* const game : coll->gameList()->entries()) {
            if (!game || game->filesModel()->entries().empty())
                continue;
            stream << game_entry(*game, coll->commonLaunchCmd()) << '\n';
            count++;
        }
        stream.flush();
        const QString path = system_file(key);
        if (!write_all(path, text)) {
            Log::warning(LOG_TAG, LOGMSG("Could not write `%1`: the library stays as it was").arg(path));
            return -1;
        }
        Log::info(LOG_TAG, LOGMSG("Wrote %1 games to `%2`").arg(count).arg(path));
        written += count;
    }

    // the old folders set aside (nothing deleted), the game directory the only one
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString metadata = paths::writableConfigDir() % QStringLiteral("/metadata");
    const QString aside = backup_dir() % QStringLiteral("/old-metadata-") % stamp;
    for (const char* sub : { "/imports", "/imports-fill", "/overrides" }) {
        const QString from = metadata % QLatin1String(sub);
        if (!QDir(from).exists())
            continue;
        QDir().mkpath(aside);
        if (QDir().rename(from, aside % QLatin1String(sub)))
            Log::info(LOG_TAG, LOGMSG("Set aside `%1` in `%2`").arg(from, aside));
        else
            Log::warning(LOG_TAG, LOGMSG("Could not move `%1` to `%2`").arg(from, aside));
    }
    QStringList old_dirs;
    AppSettings::parse_gamedirs([&old_dirs](const QString& line){ old_dirs.append(line); });
    if (!old_dirs.isEmpty())
        write_all(aside % QStringLiteral("/game_dirs.txt"), old_dirs.join(QLatin1Char('\n')) % QLatin1Char('\n'));
    QDir().mkpath(dir());
    rewrite_game_dirs({ dir() });
    write_all(dir() % QStringLiteral("/.migrated"), stamp % QLatin1Char('\n'));
    s_reload_wanted = true;
    Log::info(LOG_TAG, LOGMSG("The game directory is the only game directory now (%1 games); the library is read again").arg(written));
    return written;
}

bool take_reload_wanted()
{
    return s_reload_wanted.exchange(false);
}

} // namespace gamedir
} // namespace providers
