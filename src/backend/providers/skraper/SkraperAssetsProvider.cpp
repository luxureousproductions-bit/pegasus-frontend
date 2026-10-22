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
#include <QFile>
#include <QStringBuilder>
#include <array>
#include <functional>

#ifdef Q_OS_UNIX
#include <dirent.h>
#endif


namespace {
/// Calls `on_file(dir, name)` for every (non-hidden) file in `dir` and its
/// subfolders. Pegasus X: on Android and Linux this reads the folders
/// directly instead of asking for each file's details one by one, which on
/// Android's shared storage is slow (thousands of art files at startup)
void for_each_file(const QString& dir, const std::function<void(const QString&, const QString&)>& on_file, int depth = 0)
{
    if (depth > 16)
        return;
#ifdef Q_OS_UNIX
    const QByteArray encoded = QFile::encodeName(dir);
    DIR* const handle = opendir(encoded.constData());
    if (!handle)
        return;
    QStringList subdirs;
    while (const dirent* const entry = readdir(handle)) {
        if (entry->d_name[0] == '.')  // ".", ".." and hidden files
            continue;
        const QString name = QFile::decodeName(entry->d_name);
        bool is_dir = entry->d_type == DT_DIR;
        bool is_file = entry->d_type == DT_REG;
        if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK) {
            const QFileInfo finfo(dir % QLatin1Char('/') % name);
            is_dir = finfo.isDir();
            is_file = finfo.isFile();
        }
        if (is_dir)
            subdirs.append(dir % QLatin1Char('/') % name);
        else if (is_file)
            on_file(dir, name);
    }
    closedir(handle);
    for (const QString& subdir : subdirs)
        for_each_file(subdir, on_file, depth + 1);
#else
    QDirIterator dir_it(dir, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (dir_it.hasNext()) {
        dir_it.next();
        on_file(dir_it.fileInfo().absolutePath(), dir_it.fileName());
    }
#endif
}

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

const HashMap<AssetType, QStringList, EnumHash>& asset_dirs()
{
    // NOTE: The entries are ordered by priority. Folder names are matched
    //       case-insensitively. Besides Skraper's own folder names, the ES-DE
    //       ones and common singular/plural variants are accepted too.
    static const HashMap<AssetType, QStringList, EnumHash> ASSET_DIRS {
        { AssetType::ARCADE_MARQUEE, {
            QStringLiteral("screenmarquee"),
            QStringLiteral("screenmarqueesmall"),
            QStringLiteral("screenmarquees"),
        }},
        // less common ones, so every kind of art has a home
        { AssetType::ARCADE_BEZEL, {
            QStringLiteral("bezel"),
            QStringLiteral("bezels"),
            QStringLiteral("border"),
            QStringLiteral("borders"),
        }},
        { AssetType::ARCADE_PANEL, {
            QStringLiteral("panel"),
            QStringLiteral("panels"),
            QStringLiteral("controlpanel"),
            QStringLiteral("controlpanels"),
        }},
        { AssetType::ARCADE_CABINET_L, {
            QStringLiteral("cabinetleft"),
            QStringLiteral("cabinet_left"),
        }},
        { AssetType::ARCADE_CABINET_R, {
            QStringLiteral("cabinetright"),
            QStringLiteral("cabinet_right"),
        }},
        { AssetType::UI_BANNER, {
            QStringLiteral("banner"),
            QStringLiteral("banners"),
        }},
        { AssetType::UI_TILE, {
            QStringLiteral("tile"),
            QStringLiteral("tiles"),
            QStringLiteral("icon"),
            QStringLiteral("icons"),
        }},
        { AssetType::MUSIC, {
            QStringLiteral("music"),
            QStringLiteral("musics"),
            QStringLiteral("soundtrack"),
            QStringLiteral("soundtracks"),
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
            QStringLiteral("boxfull"),
            QStringLiteral("fullbox"),
            QStringLiteral("fullboxes"),
        }},
        { AssetType::BOX_SPINE, {
            QStringLiteral("box2dside"),
            QStringLiteral("spine"),
            QStringLiteral("spines"),
            QStringLiteral("boxspine"),
            QStringLiteral("boxside"),
            QStringLiteral("sides"),
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
            QStringLiteral("flyer"),
            QStringLiteral("flyers"),
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
            QStringLiteral("title"),
            QStringLiteral("titles"),
        }},
        { AssetType::UI_STEAMGRID, {
            QStringLiteral("steamgrid"),
            QStringLiteral("steamgrids"),
            QStringLiteral("grid"),
            QStringLiteral("grids"),
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
    return ASSET_DIRS;
}

SkraperAssetsProvider::SkraperAssetsProvider(QObject* parent)
    : Provider(QLatin1String("skraper"), QStringLiteral("Skraper Assets"), PROVIDER_FLAG_CACHEABLE, parent)
{}

Provider& SkraperAssetsProvider::run(SearchContext& sctx)
{
    const HashMap<AssetType, QStringList, EnumHash>& ASSET_DIRS = asset_dirs();

    const std::array<QString, 3> MEDIA_DIRS {
        QStringLiteral("/skraper/"),
        QStringLiteral("/media/"),
        QStringLiteral("/.media/"),
    };


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

                    for_each_file(search_dir, [&](const QString& dir, const QString& name) {
                        // the media tree mirrors the game folder: <game dir>/media/covers/Sub/Game.png
                        // is for <game dir>/Sub/Game.*
                        const int dot = name.lastIndexOf(QLatin1Char('.'));
                        const QString basename = dot > 0 ? name.left(dot) : name;
                        const QString game_path = QDir::cleanPath(dir).remove(root_dir.length(), subpath_len)
                                                % '/' % basename;
                        const auto it = extless_path_to_game.find(game_path);
                        if (it == extless_path_to_game.cend())
                            return;

                        model::Game& game = *(it->second);
                        game.assetsMut().add_file(asset_type, dir % QLatin1Char('/') % name);
                        found_assets_cnt++;
                    });
                }
            }
        }
    }

    Log::info(display_name(), LOGMSG("%1 assets found").arg(QString::number(found_assets_cnt)));
    return *this;
}

} // namespace skraper
} // namespace providers
