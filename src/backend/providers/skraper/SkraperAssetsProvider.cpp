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


#include "SkraperAssetsProvider.h"

#include "Log.h"
#include "model/gaming/Assets.h"
#include "model/gaming/Game.h"
#include "model/gaming/GameFile.h"
#include "providers/SearchContext.h"
#include "utils/PathTools.h"

#include <QDir>
#include <QDirIterator>
#include <QStringBuilder>
#include <array>


namespace {
HashMap<QString, model::Game*> build_gamepath_db(const HashMap<QString, model::GameFile*>& filepath_to_entry_map)
{
    HashMap<QString, model::Game*> map;

    // TODO: C++17
    for (const auto& entry : filepath_to_entry_map) {
        const QFileInfo finfo(entry.first);
        QString path = ::clean_abs_dir(finfo) % '/' % finfo.completeBaseName();
        map.emplace(std::move(path), entry.second->parentGame());
    }

    return map;
}
} // namespace


namespace providers {
namespace skraper {

SkraperAssetsProvider::SkraperAssetsProvider(QObject* parent)
    : Provider(QLatin1String("skraper"), QStringLiteral("Skraper Assets"), PROVIDER_FLAG_CACHEABLE, parent)
{}

Provider& SkraperAssetsProvider::run(SearchContext& sctx)
{
    // NOTE: The entries are ordered by priority. Folder names are matched
    //       case-insensitively. Besides Skraper's own folder names, the ES-DE
    //       ones and common singular/plural variants are accepted too.
    const HashMap<AssetType, QStringList, EnumHash> ASSET_DIRS {
        { AssetType::ARCADE_MARQUEE, {
            QStringLiteral("screenmarquee"),
            QStringLiteral("screenmarqueesmall"),
        }},
        { AssetType::BACKGROUND, {
            QStringLiteral("fanart"),
            QStringLiteral("fanarts"),
            QStringLiteral("background"),
            QStringLiteral("backgrounds"),
        }},
        { AssetType::BOX_BACK, {
            QStringLiteral("box2dback"),
            QStringLiteral("backcover"),
            QStringLiteral("backcovers"),
            QStringLiteral("boxback"),
        }},
        { AssetType::BOX_FRONT, {
            QStringLiteral("box2dfront"),
            QStringLiteral("cover"),
            QStringLiteral("covers"),
            QStringLiteral("boxfront"),
            QStringLiteral("boxart"),
            QStringLiteral("boxarts"),
            QStringLiteral("supporttexture"),
            QStringLiteral("box3d"),
            QStringLiteral("3dbox"),
            QStringLiteral("3dboxes"),
        }},
        { AssetType::BOX_FULL, {
            QStringLiteral("boxtexture"),
        }},
        { AssetType::BOX_SPINE, {
            QStringLiteral("box2dside"),
            QStringLiteral("spine"),
            QStringLiteral("spines"),
        }},
        { AssetType::CARTRIDGE, {
            QStringLiteral("support"),
            QStringLiteral("physicalmedia"),
            QStringLiteral("cartridge"),
            QStringLiteral("cartridges"),
            QStringLiteral("disc"),
            QStringLiteral("discs"),
        }},
        { AssetType::LOGO, {
            QStringLiteral("wheel"),
            QStringLiteral("wheels"),
            QStringLiteral("wheelcarbon"),
            QStringLiteral("wheelsteel"),
            QStringLiteral("marquee"),
            QStringLiteral("marquees"),
            QStringLiteral("logo"),
            QStringLiteral("logos"),
        }},
        { AssetType::POSTER, {
            QStringLiteral("poster"),
            QStringLiteral("posters"),
        }},
        { AssetType::SCREENSHOT, {
            QStringLiteral("screenshot"),
            QStringLiteral("screenshots"),
            QStringLiteral("snap"),
            QStringLiteral("snaps"),
        }},
        { AssetType::TITLESCREEN, {
            QStringLiteral("screenshottitle"),
            QStringLiteral("titlescreen"),
            QStringLiteral("titlescreens"),
        }},
        { AssetType::UI_STEAMGRID, {
            QStringLiteral("steamgrid"),
            QStringLiteral("miximage"),
            QStringLiteral("miximages"),
            QStringLiteral("mixrbv1"),
            QStringLiteral("mixrbv2"),
        }},
        { AssetType::VIDEO, {
            QStringLiteral("videos"),
            QStringLiteral("video"),
        }},
    };

    const std::array<QString, 3> MEDIA_DIRS {
        QStringLiteral("/skraper/"),
        QStringLiteral("/media/"),
        QStringLiteral("/.media/"),
    };

    constexpr auto DIR_FILTERS = QDir::Files | QDir::Readable | QDir::NoDotAndDotDot;
    constexpr auto DIR_FLAGS = QDirIterator::Subdirectories | QDirIterator::FollowSymlinks;


    const HashMap<QString, model::Game*> extless_path_to_game = build_gamepath_db(sctx.current_filepath_to_entry_map());

    size_t found_assets_cnt = 0;
    for (const QString& root_dir : sctx.pegasus_game_dirs()) {
        for (const QString& media_dir_subpath : MEDIA_DIRS) {
            const QString game_media_dir = root_dir % media_dir_subpath;
            if (!QFileInfo::exists(game_media_dir))
                continue;

            // Map the lowercase name of each folder to its actual name, so eg.
            // both "Covers" and "covers" are found on case-sensitive storage
            HashMap<QString, QString> existing_dirs;
            const QStringList subdir_names = QDir(game_media_dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& subdir_name : subdir_names)
                existing_dirs.emplace(subdir_name.toLower(), subdir_name);

            // TODO: C++17
            for (const auto& asset_dir_entry : ASSET_DIRS) {
                const AssetType asset_type = asset_dir_entry.first;
                const QStringList& dir_names = asset_dir_entry.second;
                for (const QString& dir_name_lower : dir_names) {
                    const auto existing_it = existing_dirs.find(dir_name_lower);
                    if (existing_it == existing_dirs.cend())
                        continue;

                    const QString& dir_name = existing_it->second;
                    const QString search_dir = game_media_dir % dir_name;
                    const int subpath_len = media_dir_subpath.length() + dir_name.length();

                    QDirIterator dir_it(search_dir, DIR_FILTERS, DIR_FLAGS);
                    while (dir_it.hasNext()) {
                        dir_it.next();
                        const QFileInfo finfo = dir_it.fileInfo();

                        const QString game_path = ::clean_abs_dir(finfo).remove(root_dir.length(), subpath_len)
                                                % '/' % finfo.completeBaseName();
                        const auto it = extless_path_to_game.find(game_path);
                        if (it == extless_path_to_game.cend())
                            continue;

                        model::Game& game = *(it->second);
                        game.assetsMut().add_file(asset_type, dir_it.filePath());
                        found_assets_cnt++;
                    }
                }
            }
        }
    }

    Log::info(display_name(), LOGMSG("%1 assets found").arg(QString::number(found_assets_cnt)));
    return *this;
}

} // namespace skraper
} // namespace providers
