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

#include "Log.h"
#include "Paths.h"
#include "model/gaming/Assets.h"
#include "model/gaming/Collection.h"
#include "model/gaming/Game.h"
#include "model/gaming/GameFile.h"
#include "model/gaming/GameFileListModel.h"
#include "providers/SearchContext.h"
#include "providers/pegasus_metadata/PegasusMetadata.h"
#include "utils/HashMap.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSettings>
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

    if (replace && !over.title().isEmpty() && over.title() != live.title())
        live.setTitle(over.title());
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
    for (const AssetType type : all_asset_types()) {
        const QStringList& files = over.assets().get(type);
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

    // extras (manuals, removed art, ...)
    if (details) {
        for (auto it = over.extraMap().cbegin(); it != over.extraMap().cend(); ++it) {
            if (it.key() == QLatin1String("pegasusx-source"))
                continue;
            if (replace || !live.extraMap().contains(it.key()))
                live.extraMapMut().insert(it.key(), it.value());
        }
    }
}

void apply_collection(model::Collection& live, const model::Collection& over, Mode mode)
{
    const bool replace = mode == Mode::Replace;
    if (mode != Mode::MediaOnly) {
        if (!over.commonLaunchCmd().isEmpty() && (replace || live.commonLaunchCmd().isEmpty()))
            live.setCommonLaunchCmd(over.commonLaunchCmd());
        if (!over.commonLaunchWorkdir().isEmpty() && (replace || live.commonLaunchWorkdir().isEmpty()))
            live.setCommonLaunchWorkdir(over.commonLaunchWorkdir());
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

// one folder of files, onto the live games (by their files) and collections (by name)
int apply_layer(const QString& dir, Mode mode, const QString& what,
                const HashMap<QString, model::Game*>& live_by_file,
                const HashMap<QString, model::Collection*>& live_colls,
                std::function<bool(const model::Game&)> wanted)
{
    const QStringList files = metafiles_in(dir);
    if (files.isEmpty())
        return 0;
    providers::SearchContext sctx(QStringList {});
    const providers::pegasus::Metadata reader(LOG_TAG);
    for (const QString& file : files)
        reader.apply_metafile(file, sctx);

    int applied = 0;
    std::unordered_set<model::Game*> done;
    for (const auto& pair : sctx.current_filepath_to_entry_map()) {
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
    for (const auto& pair : sctx.current_collection_games()) {
        const auto live_it = live_colls.find(pair.first->name());
        if (live_it != live_colls.cend())
            apply_collection(*live_it->second, *pair.first, mode);
    }
    sctx.discard_all();
    Log::info(LOG_TAG, LOGMSG("%1: %2 files, %3 games").arg(what).arg(files.size()).arg(applied));
    return applied;
}
} // namespace


namespace providers {
namespace overrides {

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
    if (metafiles_in(fill_dir).isEmpty() && metafiles_in(overrides_dir).isEmpty())
        return;

    QElapsedTimer timer;
    timer.start();
    HashMap<QString, model::Game*> by_file;
    for (model::Game* const game : games) {
        for (const model::GameFile* const file : game->filesModel()->entries())
            by_file.emplace(file->path(), game);
    }
    HashMap<QString, model::Collection*> colls;
    for (model::Collection* const coll : collections)
        colls.emplace(coll->name(), coll);

    // 1. ES-DE's data for games your own files define
    const QString mode_name = import_fill_mode();
    const Mode mode = mode_name == QLatin1String("media") ? Mode::MediaOnly
                    : mode_name == QLatin1String("replace") ? Mode::Replace : Mode::Fill;
    apply_layer(fill_dir, mode, QStringLiteral("Imports (") + mode_name + QLatin1Char(')'), by_file, colls,
        [](const model::Game& live) {
            // only games defined by your own files (the imports' own games have it all already)
            const QStringList source = live.extraMap().value(QStringLiteral("pegasusx-source")).toStringList();
            return !source.contains(QStringLiteral("esde"));
        });
    // 2. your edits and scraped data
    apply_layer(overrides_dir, Mode::Replace, QStringLiteral("Overrides"), by_file, colls,
        [](const model::Game&) { return true; });
    Log::info(LOG_TAG, LOGMSG("Applied in %1 ms").arg(timer.elapsed()));
}

} // namespace overrides
} // namespace providers
