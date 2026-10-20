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


#pragma once

#include "providers/Provider.h"

#include <QString>
#include <QStringList>
#include <functional>


namespace providers {
namespace esde {

/// Pegasus X: reads an existing ES-DE (Android) setup directly. Systems and
/// emulators come from ES-DE's own bundled configuration (plus the user's
/// custom_systems overrides), metadata from ES-DE/gamelists and media from
/// ES-DE/downloaded_media. Launch commands are translated to 'am start'.
/// Disabled by default; turn it on in Settings > Data sources.
class EsdeProvider : public Provider {
    Q_OBJECT

public:
    explicit EsdeProvider(QObject* parent = nullptr);

    Provider& run(SearchContext&) final;
    QStringList cacheInputFiles() const final;
};


// -- ES-DE import (Settings > ES-DE import) --------------------------------------

/// A system of an ES-DE setup that has games in its ROMs folders
struct EsdeImportSystem {
    QString name;        ///< ES-DE's name, eg. "snesna"
    QString fullname;
    QString short_name;  ///< the name themes use, eg. "snes"
    int games = 0;
    bool has_gamelist = false;
    bool has_media = false;
    QStringList folders;  ///< its folders in the ROMs folders
};

struct EsdeImportResult {
    int systems = 0;
    int games = 0;
    QStringList written_files;
    QStringList folders;  ///< the system folders a file was written to
    QString error;
};

/// The ES-DE folder found on this device, or empty
QString esde_default_dir();

/// The systems of the ES-DE setup in `esde_dir` that have games
std::vector<EsdeImportSystem> esde_import_systems(const QString& esde_dir);

/// For each system (all if empty), writes esde-import.metadata.pegasus.txt
/// into each of its ROM folders: every game there, with the details from
/// ES-DE's gamelist and the paths of its art and manuals in ES-DE's media
/// folder. Multi-disc games are one entry with all their discs. No launch
/// commands are written; the folder's other metadata files or the emulator
/// settings provide those. Running it again replaces the file.
/// `combine_android`: Android Apps and Android Games go into one "Android"
/// collection. `progress` is called before each system (done, total, name)
/// and once more at the end; it may be called from a worker thread.
EsdeImportResult esde_import(const QString& esde_dir, const QStringList& system_names, bool combine_android,
                             const std::function<void(int, int, const QString&)>& progress);

} // namespace esde
} // namespace providers
