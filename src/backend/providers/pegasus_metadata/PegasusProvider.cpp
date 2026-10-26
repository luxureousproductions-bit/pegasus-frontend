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


#include "PegasusProvider.h"

#include "Log.h"
#include "Paths.h"
#include "providers/Overrides.h"
#include "providers/SearchContext.h"
#include "providers/pegasus_metadata/PegasusMetadata.h"
#include "providers/pegasus_metadata/PegasusFilter.h"
#include "utils/PathTools.h"
#include "utils/StdHelpers.h"

#include <QDirIterator>
#include <QSet>

#include <algorithm>


namespace {
bool is_metadata_file(const QString& filename)
{
    return filename == QLatin1String("metadata.pegasus.txt")
        || filename == QLatin1String("metadata.txt")
        || filename.endsWith(QLatin1String(".metadata.pegasus.txt"))
        || filename.endsWith(QLatin1String(".metadata.txt"));
}

std::vector<QString> find_metafiles_in(const QString& dir_path)
{
    constexpr auto dir_filters = QDir::Files | QDir::NoDotAndDotDot;
    constexpr auto dir_flags = QDirIterator::FollowSymlinks;

    std::vector<QString> result;

    QDirIterator dir_it(dir_path, dir_filters, dir_flags);
    while (dir_it.hasNext()) {
        dir_it.next();
        if (is_metadata_file(dir_it.fileName())) {
            QString path = ::clean_abs_path(dir_it.fileInfo());
            result.emplace_back(std::move(path));
        }
    }

    return result;
}

// Pegasus X: an ES-DE import (made by Pegasus X)
bool is_import_metafile(const QString& path)
{
    return path.contains(QLatin1String("/metadata/imports/"))
        || path.contains(QLatin1String("/metadata/imports-fill/"))
        || path.endsWith(QLatin1String("/esde-import.metadata.pegasus.txt"));
}

// Pegasus X: the files in a stable order, as the first file that lists a
// game file owns it: the global metafiles, then each game folder's (in
// game_dirs.txt order, each folder's sorted by name), then the ES-DE imports
// last, so your own files always win over an import. (This used to be one
// alphabetical list, which put `.../PegasusX/metadata/imports` first.)
std::vector<QString> find_all_metafiles(const QStringList& gamedirs)
{
    std::vector<QString> own;
    std::vector<QString> imports;
    QSet<QString> seen;
    const auto add_dir = [&](const QString& dir_path) {
        std::vector<QString> found = find_metafiles_in(dir_path);
        std::sort(found.begin(), found.end());
        for (QString& path : found) {
            if (seen.contains(path))
                continue;
            seen.insert(path);
            (is_import_metafile(path) ? imports : own).push_back(std::move(path));
        }
    };

    add_dir(paths::writableConfigDir() + QLatin1String("/metafiles"));
    for (const QString& dir_path : gamedirs)
        add_dir(dir_path);
    // the ES-DE imports are always read (they used to be a game folder of their own)
    add_dir(paths::writableConfigDir() + QLatin1String("/metadata/imports"));

    own.insert(own.end(), std::make_move_iterator(imports.begin()), std::make_move_iterator(imports.end()));
    return own;
}
} // namespace


namespace providers {
namespace pegasus {

PegasusProvider::PegasusProvider(QObject* parent)
    : Provider(QLatin1String("pegasus_metafiles"), QStringLiteral("Pegasus Metafiles"), PROVIDER_FLAG_INTERNAL | PROVIDER_FLAG_CACHEABLE, parent)
{}

Provider& PegasusProvider::run(SearchContext& sctx)
{
    providers::overrides::begin_import_scan();
    const std::vector<QString> metafile_paths = find_all_metafiles(sctx.root_game_dirs());
    if (metafile_paths.empty()) {
        Log::info(display_name(), LOGMSG("No metadata files found"));
        return *this;
    }

    const Metadata metahelper(display_name());
    std::vector<FileFilter> all_filters;

    const float progress_step = 1.f / metafile_paths.size();
    float progress = 0.f;

    for (const QString& path : metafile_paths) {
        Log::info(display_name(), LOGMSG("Found `%1`").arg(::pretty_path(path)));

        std::vector<FileFilter> filters = metahelper.apply_metafile(path, sctx);
        all_filters.insert(all_filters.end(),
            std::make_move_iterator(filters.begin()),
            std::make_move_iterator(filters.end()));

        progress += progress_step;
        emit progressChanged(progress);
    }

    for (FileFilter& filter : all_filters) {
        apply_filter(filter, sctx);

        for (QString& dir_path : filter.directories)
            sctx.pegasus_add_game_dir(dir_path);
    }

    return *this;
}

} // namespace pegasus
} // namespace providers
