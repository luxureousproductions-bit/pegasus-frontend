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


// -- Emulators (Settings > Emulators) ---------------------------------------------

/// One emulator that can launch a system's games
struct EmulatorChoice {
    QString label;    ///< ES-DE's name for it, eg. "NetherSX2 (Standalone)"
    QString command;  ///< the launch command for a Pegasus metadata file
    bool installed = false;
    bool retroarch = false;  ///< a RetroArch core, which also has to be downloaded in RetroArch
    bool recommended = false;  ///< ES-DE's default for the system
};

/// A collection defined in the user's metadata files
struct EmulatorCollection {
    QString name;
    QString short_name;
    QString system;           ///< the ES-DE system it was matched to, empty if none
    QString system_fullname;
    QString command;          ///< its current launch command, empty if none
    QString current_label;    ///< the emulator that command is, empty if not one of `options`
    bool current_installed = false;
    QString missing_app;      ///< for a hand-written command whose app isn't installed: its package
    QStringList files;        ///< the metadata files that define it
    int game_overrides = 0;   ///< games in those files with their own launch command
    std::vector<EmulatorChoice> options;
};

/// A game's own launch command (in a metadata file)
struct GameEmulator {
    QString path;     ///< the game's (first) file
    QString command;
};

/// Every collection in the metadata files of the game directories, with
/// the emulators that can launch it
std::vector<EmulatorCollection> emulator_collections();

/// The games of a collection that have their own launch command
std::vector<GameEmulator> game_emulators(const QString& collection_name);

/// Writes `command` as the collection's launch command in every metadata
/// file that defines it. Returns an error, or empty.
QString set_collection_emulator(const QString& collection_name, const QString& command);

/// Writes `command` as the game's own launch command (after its description,
/// before its rating), or removes it if `command` is empty, in the metadata
/// file that has the game; a game without an entry gets one. Returns an
/// error, or empty.
QString set_game_emulator(const QString& collection_name, const QString& game_path,
                          const QString& game_title, const QString& command);

/// Starts `command` with a game file. Returns an error, or empty.
QString test_emulator_command(const QString& command, const QString& game_path);

} // namespace esde
} // namespace providers
