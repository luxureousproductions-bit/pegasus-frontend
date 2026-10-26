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


#include "Overrides.h"

#include "AppSettings.h"
#include "Log.h"
#include "Paths.h"
#include "model/gaming/Assets.h"
#include "model/gaming/Collection.h"
#include "model/gaming/Game.h"
#include "model/gaming/GameFile.h"
#include "model/gaming/GameFileListModel.h"
#include "model/gaming/GameListModel.h"
#include "providers/SearchContext.h"
#include "providers/pegasus_metadata/PegasusFilter.h"
#include "providers/pegasus_metadata/PegasusMetadata.h"
#include "utils/HashMap.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSet>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QSettings>
#include <QUrl>
#include <QStringBuilder>
#include <functional>
#include <unordered_set>


namespace {
const QString LOG_TAG = QStringLiteral("Overrides");

const std::vector<AssetType>& all_asset_types()
{
    static const std::vector<AssetType> types {
        AssetType::BOX_FRONT, AssetType::BOX_BACK, AssetType::BOX_SPINE, AssetType::BOX_FULL,
        AssetType::CARTRIDGE, AssetType::LOGO, AssetType::POSTER,
        AssetType::ARCADE_MARQUEE, AssetType::ARCADE_BEZEL, AssetType::ARCADE_PANEL,
        AssetType::ARCADE_CABINET_L, AssetType::ARCADE_CABINET_R,
        AssetType::UI_TILE, AssetType::UI_BANNER, AssetType::UI_STEAMGRID, AssetType::BACKGROUND,
        AssetType::MUSIC, AssetType::SCREENSHOT, AssetType::TITLESCREEN, AssetType::VIDEO,
    };
    return types;
}

QStringList metafiles_in(const QString& dir)
{
    QStringList out;
    for (const QFileInfo& finfo : QDir(dir).entryInfoList(
            { QStringLiteral("*.metadata.pegasus.txt"), QStringLiteral("*.metadata.txt") }, QDir::Files, QDir::Name))
        out.append(finfo.absoluteFilePath());
    return out;
}

// how a layer's values go onto a game
enum class Mode { MediaOnly, Fill, Replace };

// where scraped and picked art is kept now (as Settings::dataDir)
QString art_dir()
{
    const QSettings ini(paths::writableConfigDir() + QStringLiteral("/pegasusx.ini"), QSettings::IniFormat);
    QString base = QDir::cleanPath(ini.value(QStringLiteral("data/dir")).toString());
    if (base.isEmpty() || base == QLatin1String("."))
        base = paths::writableConfigDir();
    return QFileInfo(base).fileName().compare(QLatin1String("downloaded_media"), Qt::CaseInsensitive) == 0
        ? base : base + QStringLiteral("/downloaded_media");
}

// A file a layer names that isn't there (moved with the art folder, or
// deleted): found again by its last parts (<system>/<kind>/<file>) in the art
// folder, else left out, so it never hides the art the game has elsewhere
QString s_art;  // art_dir(), read once per library read (apply())

QStringList existing_files(const QStringList& files, const QString& game_title, int& fixed, int& dropped)
{
    const QString& art = s_art;
    QStringList out;
    for (const QString& f : files) {
        QString local = f;
        if (local.startsWith(QLatin1String("file:")))
            local = QUrl(f).toLocalFile();
        if (local.isEmpty() || local.contains(QLatin1String("://")) || !local.startsWith(QLatin1Char('/'))
                || QFileInfo::exists(local)) {
            out.append(f);
            continue;
        }
        const QStringList parts = local.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QString found;
        for (int keep = 3; keep >= 2 && found.isEmpty(); keep--) {
            if (parts.size() < keep)
                continue;
            const QString candidate = art + QLatin1Char('/') + parts.mid(parts.size() - keep).join(QLatin1Char('/'));
            if (QFileInfo::exists(candidate))
                found = candidate;
        }
        if (!found.isEmpty()) {
            out.append(found);
            fixed++;
            continue;
        }
        dropped++;
        Log::warning(LOG_TAG, LOGMSG("`%1`: `%2` isn't there, left out").arg(game_title, local));
    }
    return out;
}

int s_fixed = 0;    // missing files found again in the art folder
int s_dropped = 0;  // missing files left out

void apply_game(model::Game& live, const model::Game& over, Mode mode)
{
    const bool replace = mode == Mode::Replace;
    const bool details = mode != Mode::MediaOnly;
    const auto take_text = [&](const QString& value, const QString& current, std::function<void(QString)> set) {
        if (!details || value.isEmpty())
            return;
        if (replace || current.isEmpty())
            set(value);
    };
    const auto take_list = [&](const QStringList& value, QStringList& current) {
        if (!details || value.isEmpty())
            return;
        if (replace || current.isEmpty())
            current = value;
    };

    // who wrote what: you (in the editor) or the scraper
    const auto marker_keys = [&over](const char* marker) {
        QSet<QString> out;
        const QStringList values = over.extraMap().value(QLatin1String(marker)).toStringList()
            .join(QLatin1Char(',')).split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString& key : values)
            out.insert(key.trimmed().toLower());
        return out;
    };
    const QSet<QString> edited = marker_keys("pegasusx-edited");
    const QSet<QString> scraped = marker_keys("pegasusx-scraped");

    // the title: an override block is headed by the title it was made under,
    // so only a block that says it changed the title replaces it
    const bool title_changed = edited.contains(QStringLiteral("title")) || scraped.contains(QStringLiteral("title"));
    if (replace && title_changed && !over.title().isEmpty() && over.title() != live.title())
        live.setTitle(over.title());

    // a field you cleared in the editor (listed as edited, with no value):
    // cleared here too, instead of the metadata file's value showing again
    if (replace && details) {
        const auto cleared = [&edited](const char* key) { return edited.contains(QLatin1String(key)); };
        if (cleared("summary") && over.summary().isEmpty())
            live.setSummary(QString());
        if (cleared("description") && over.description().isEmpty())
            live.setDescription(QString());
        if (cleared("developer") && over.developerListConst().isEmpty())
            live.developerList().clear();
        if (cleared("publisher") && over.publisherListConst().isEmpty())
            live.publisherList().clear();
        if (cleared("genre") && over.genreListConst().isEmpty())
            live.genreList().clear();
        if (cleared("tag") && over.tagListConst().isEmpty())
            live.tagList().clear();
        if (cleared("players") && over.playerCount() <= 1)
            live.setPlayerCount(1);
        if (cleared("release") && !over.releaseDate().isValid())
            live.setReleaseDate(QDate());
        if (cleared("rating") && over.rating() <= 0.f)
            live.setRating(0.f);
    }
    if (details && !over.sortBy().isEmpty() && over.sortBy() != over.title() && (replace || live.sortBy().isEmpty() || live.sortBy() == live.title()))
        live.setSortBy(over.sortBy());
    take_text(over.summary(), live.summary(), [&live](QString v){ live.setSummary(std::move(v)); });
    take_text(over.description(), live.description(), [&live](QString v){ live.setDescription(std::move(v)); });
    take_list(over.developerListConst(), live.developerList());
    take_list(over.publisherListConst(), live.publisherList());
    take_list(over.genreListConst(), live.genreList());
    take_list(over.tagListConst(), live.tagList());
    if (details && over.playerCount() > 1 && (replace || live.playerCount() <= 1))
        live.setPlayerCount(over.playerCount());
    if (details && over.releaseDate().isValid() && (replace || !live.releaseDate().isValid()))
        live.setReleaseDate(over.releaseDate());
    if (details && over.rating() > 0.f && (replace || live.rating() <= 0.f))
        live.setRating(over.rating());
    if (details && !over.launchCmd().isEmpty() && (replace || live.launchCmd().isEmpty()))
        live.setLaunchCmd(over.launchCmd());

    // art: replace: the layer's files instead of the live ones; else after them
    // (only files that are there: a missing one keeps the game's own art)
    for (const AssetType type : all_asset_types()) {
        const QStringList files = existing_files(over.assets().get(type), live.title(), s_fixed, s_dropped);
        if (files.isEmpty())
            continue;
        if (replace) {
            live.assetsMut().set_files(type, files);
            continue;
        }
        QStringList merged = live.assets().get(type);
        for (const QString& f : files) {
            if (!merged.contains(f))
                merged.append(f);
        }
        live.assetsMut().set_files(type, merged);
    }

    // extras (manuals, removed art, ...): the manual is media, the rest details
    for (auto it = over.extraMap().cbegin(); it != over.extraMap().cend(); ++it) {
        if (it.key() == QLatin1String("pegasusx-source") || it.key() == QLatin1String("pegasusx-edited")
                || it.key() == QLatin1String("pegasusx-scraped"))
            continue;
        if (!details && it.key() != QLatin1String("manual"))
            continue;
        if (replace || !live.extraMap().contains(it.key()))
            live.extraMapMut().insert(it.key(), it.value());
    }
}

void apply_collection(model::Collection& live, const model::Collection& over, Mode mode)
{
    const bool replace = mode == Mode::Replace;
    if (mode != Mode::MediaOnly) {
        // the games took the collection's command when they joined it, so
        // a new one goes to them too (those with the old one, or none)
        if (!over.commonLaunchCmd().isEmpty() && (replace || live.commonLaunchCmd().isEmpty())) {
            const QString before = live.commonLaunchCmd();
            live.setCommonLaunchCmd(over.commonLaunchCmd());
            for (model::Game* const game : live.gameList()->entries()) {
                if (game->launchCmd().isEmpty() || game->launchCmd() == before)
                    game->setLaunchCmd(over.commonLaunchCmd());
            }
        }
        if (!over.commonLaunchWorkdir().isEmpty() && (replace || live.commonLaunchWorkdir().isEmpty())) {
            const QString before = live.commonLaunchWorkdir();
            live.setCommonLaunchWorkdir(over.commonLaunchWorkdir());
            for (model::Game* const game : live.gameList()->entries()) {
                if (game->launchWorkdir().isEmpty() || game->launchWorkdir() == before)
                    game->setLaunchWorkdir(over.commonLaunchWorkdir());
            }
        }
        if (!over.summary().isEmpty() && (replace || live.summary().isEmpty()))
            live.setSummary(over.summary());
        if (!over.description().isEmpty() && (replace || live.description().isEmpty()))
            live.setDescription(over.description());
        if (!over.sortBy().isEmpty() && (replace || live.sortBy().isEmpty()))
            live.setSortBy(over.sortBy());
    }
    for (const AssetType type : all_asset_types()) {
        const QStringList& files = over.assets().get(type);
        if (!files.isEmpty())
            live.assetsMut().set_files(type, files);
    }
}

// ES-DE import files that had games your own files have (see note_import_conflict)
QMutex s_conflicts_mutex;
QSet<QString> s_import_conflicts;
bool s_import_scanned = false;  // made again by this read (else: from the file below)

QString conflicts_file() { return paths::writableCacheDir() + QStringLiteral("/import-conflicts.txt"); }

// files, onto the live games (by their files) and collections (by name)
int apply_files(const QStringList& files, Mode mode, const QString& what,
                const HashMap<QString, model::Game*>& live_by_file,
                const HashMap<QString, model::Collection*>& live_colls,
                std::function<bool(const model::Game&)> wanted, bool with_collections = true)
{
    if (files.isEmpty())
        return 0;
    providers::SearchContext sctx(QStringList {});
    const providers::pegasus::Metadata reader(LOG_TAG);
    {
        // read as written: a picture that isn't where the file says (the art
        // folder moved, the SD card is slow) is looked for again by
        // existing_files() instead of being dropped by the reader first
        const bool verify = AppSettings::general.verify_files;
        AppSettings::general.verify_files = false;
        for (const QString& file : files)
            reader.apply_metafile(file, sctx);
        AppSettings::general.verify_files = verify;
    }

    // collections first (their launch command reaches their games), so a
    // game's own override still wins
    for (const auto& pair : sctx.current_collection_games()) {
        if (!with_collections)
            break;
        const auto live_it = live_colls.find(pair.first->name());
        if (live_it != live_colls.cend())
            apply_collection(*live_it->second, *pair.first, mode);
    }
    int applied = 0;
    std::unordered_set<model::Game*> done;
    // by file path, and by URI (Android apps: `android:<package>`, which the
    // reader also files under a made-up absolute path that matches nothing)
    const auto visit = [&](const HashMap<QString, model::GameFile*>& map) {
        for (const auto& pair : map) {
            model::Game* const over = pair.second ? pair.second->parentGame() : nullptr;
            if (!over || done.count(over))
                continue;
            const auto live_it = live_by_file.find(pair.first);
            if (live_it == live_by_file.cend())
                continue;
            done.insert(over);
            if (!wanted(*live_it->second))
                continue;
            apply_game(*live_it->second, *over, mode);
            applied++;
        }
    };
    visit(sctx.current_uri_to_entry_map());
    visit(sctx.current_filepath_to_entry_map());
    sctx.discard_all();
    Log::info(LOG_TAG, LOGMSG("%1: %2 files, %3 games").arg(what).arg(files.size()).arg(applied));
    return applied;
}

// one folder of files
int apply_layer(const QString& dir, Mode mode, const QString& what,
                const HashMap<QString, model::Game*>& live_by_file,
                const HashMap<QString, model::Collection*>& live_colls,
                std::function<bool(const model::Game&)> wanted)
{
    return apply_files(metafiles_in(dir), mode, what, live_by_file, live_colls, std::move(wanted));
}
} // namespace


namespace providers {
namespace overrides {

bool is_import_file(const QString& metafile_path)
{
    const QString dir = QDir::cleanPath(paths::writableConfigDir() + QStringLiteral("/metadata/imports")) + QLatin1Char('/');
    return QDir::cleanPath(metafile_path).startsWith(dir);
}

void begin_import_scan()
{
    QMutexLocker lock(&s_conflicts_mutex);
    s_import_conflicts.clear();
    s_import_scanned = true;
}

void note_import_conflict(const QString& metafile_path)
{
    QMutexLocker lock(&s_conflicts_mutex);
    s_import_conflicts.insert(metafile_path);
}

QString import_fill_mode()
{
    const QSettings file(paths::writableConfigDir() + QStringLiteral("/pegasusx.ini"), QSettings::IniFormat);
    const QString mode = file.value(QStringLiteral("esdeImport/use"), QStringLiteral("fill")).toString();
    return (mode == QLatin1String("media") || mode == QLatin1String("replace")) ? mode : QStringLiteral("fill");
}

void set_import_fill_mode(const QString& mode)
{
    QSettings file(paths::writableConfigDir() + QStringLiteral("/pegasusx.ini"), QSettings::IniFormat);
    file.setValue(QStringLiteral("esdeImport/use"), mode);
}

void apply(const std::vector<model::Collection*>& collections, const std::vector<model::Game*>& games)
{
    const QString root = paths::writableConfigDir() + QStringLiteral("/metadata");
    const QString fill_dir = root + QStringLiteral("/imports-fill");
    const QString overrides_dir = root + QStringLiteral("/overrides");
    QStringList conflicts;
    {
        QMutexLocker lock(&s_conflicts_mutex);
        if (s_import_scanned) {
            // kept for the starts that use the cached library
            s_import_scanned = false;
            conflicts = s_import_conflicts.values();
            conflicts.sort();
            QSaveFile file(conflicts_file());
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(conflicts.join(QLatin1Char('\n')).toUtf8());
                file.commit();
            }
        }
        else {
            QFile file(conflicts_file());
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
                conflicts = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        }
    }
    for (int i = conflicts.size() - 1; i >= 0; i--) {
        if (!QFileInfo::exists(conflicts.at(i)))
            conflicts.removeAt(i);
    }
    if (conflicts.isEmpty() && metafiles_in(fill_dir).isEmpty() && metafiles_in(overrides_dir).isEmpty())
        return;

    QElapsedTimer timer;
    timer.start();
    s_art = art_dir();
    HashMap<QString, model::Game*> by_file;
    for (model::Game* const game : games) {
        for (const model::GameFile* const file : game->filesModel()->entries()) {
            by_file.emplace(file->path(), game);
            if (file->hasUri())
                by_file.emplace(file->uri(), game);
        }
    }
    HashMap<QString, model::Collection*> colls;
    for (model::Collection* const coll : collections)
        colls.emplace(coll->name(), coll);

    // 1. ES-DE's data for games your own files define
    const QString mode_name = import_fill_mode();
    const Mode mode = mode_name == QLatin1String("media") ? Mode::MediaOnly
                    : mode_name == QLatin1String("replace") ? Mode::Replace : Mode::Fill;
    const auto yours = [](const model::Game& live) {
        // only games defined by your own files (the imports' own games have it all already)
        const QStringList source = live.extraMap().value(QStringLiteral("pegasusx-source")).toStringList();
        return !source.contains(QStringLiteral("esde"));
    };
    apply_layer(fill_dir, mode, QStringLiteral("Imports (") + mode_name + QLatin1Char(')'), by_file, colls, yours);
    // the imports' games your own files have too (your file won them): their
    // data the same way, without the imports' collections
    apply_files(conflicts, mode, QStringLiteral("Imports onto your games (") + mode_name + QLatin1Char(')'),
                by_file, colls, yours, false);
    // 2. your edits and scraped data
    apply_layer(overrides_dir, Mode::Replace, QStringLiteral("Overrides"), by_file, colls,
        [](const model::Game&) { return true; });
    if (s_fixed > 0 || s_dropped > 0)
        Log::info(LOG_TAG, LOGMSG("Art files that moved: %1 found again, %2 not there (left out)").arg(s_fixed).arg(s_dropped));
    s_fixed = s_dropped = 0;
    Log::info(LOG_TAG, LOGMSG("Applied in %1 ms").arg(timer.elapsed()));
}

} // namespace overrides
} // namespace providers
