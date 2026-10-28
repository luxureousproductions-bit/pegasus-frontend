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

#include <QSet>
#include <QString>
#include <QVariantMap>
#include <QStringList>
#include <functional>
#include <utility>
#include <vector>


namespace providers {
namespace esde {



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
    QStringList imported_files;  ///< the esde-import files already written there
};

/// Removes the import files made from these folders (your own files stay).
/// Returns how many were removed.
int esde_delete_import(const QStringList& folders);
/// The folder the import files are written to (a game directory):
/// <Pegasus X folder>/metadata/imports
QString imports_game_dir();
/// Older edits files in metafiles/ become override files (once, at startup)
void move_edits_to_overrides();
/// Pegasus X: makes the relative paths in an override file absolute, after
/// it moved from `old_dir` (where they were written) to where it is now
bool rebase_override_file(const QString& file_path, const QString& old_dir);

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
/// `android_systems`: of "androidapps" and "androidgames", the ones that go
/// into one "Android" collection. `progress` is called before each system
/// (done, total, name) and once more at the end; it may be called from a
/// worker thread.
EsdeImportResult esde_import(const QString& esde_dir, const QStringList& system_names, const QStringList& android_systems,
                             const std::function<void(int, int, const QString&)>& progress);


// -- ROM folders (Settings > Game directories) ---------------------------------------
// A ROM folder without metadata gets the same kind of file as the ES-DE
// import, for each system in it (the folder itself, eg. ".../snes", or the
// system folders in it): its games, with a gamelist.xml there (and ES-DE's
// gamelists and art, if ES-DE is on the device). Remembered, and made again
// when games are added or removed.

/// The systems with games in a folder (by its name or its folders' names)
std::vector<EsdeImportSystem> rom_folder_systems(const QString& folder);
/// Every system known (ES-DE's list and your custom ones): for picking
/// systems before a ROM folder
struct KnownSystem {
    QString name;        ///< eg. "snes"
    QString fullname;    ///< eg. "Nintendo SNES (Super Nintendo)"
    QString short_name;  ///< the theme name, eg. "snes"
    QString folder;      ///< the folder name expected, eg. "snes"
    QString extensions;  ///< ".sfc .smc .zip"
};
std::vector<KnownSystem> known_systems();
/// Imports one folder as one system (when its name doesn't say which)
EsdeImportResult import_folder_as(const QString& folder, const QString& system);
/// Imports only these systems of a ROM folder (empty: every one found)
EsdeImportResult rom_folder_import_only(const QString& folder, const QStringList& systems);
/// Reads the folders a system was imported from again (merging)
EsdeImportResult reimport_system(const QString& system_file);
/// Pegasus X: one game as its ROM folder has it now (the folder's own
/// metadata file and media folders, ES-DE's gamelist and media), for
/// Library's "Restore the original art" and "Import ES-DE's details"
struct OriginalGame {
    bool found = false;
    QString folder;           ///< the ROM folder it was imported from
    bool own_metadata = false;  ///< the folder has a metadata file of its own with this game
    bool gamelist = false;    ///< ES-DE has a gamelist for the folder
    QVariantMap details;      ///< title, developer/publisher/genre (lists), players, release, rating, description
    QVariantMap assets;       ///< "assets.<kind>" and "x-manual" -> the files (QStringList)
    QString error;
};
OriginalGame original_game(const QString& collection_name, const QString& game_path);
/// Reads every remembered ROM folder again, changed or not
int rescan_rom_folders();
/// the remembered ROM folders: (folder, system)
std::vector<std::pair<QString, QString>> watched_rom_folders();
void forget_rom_folders(const QStringList& folders);
/// Writes the game lists of a ROM folder's systems
EsdeImportResult rom_folder_import(const QString& folder, const std::function<void(int, int, const QString&)>& progress);
/// The remembered folders whose games changed since their list was made: made
/// again. Returns how many. Quick when nothing changed.
int refresh_rom_folders(bool force = false);
/// Remembers system folders with a list made by Pegasus X (for refresh_rom_folders)
void remember_rom_folders(const QStringList& folders, const QString& system);


// -- Emulators (Settings > Emulators) ---------------------------------------------

/// One emulator that can launch a system's games
struct EmulatorChoice {
    QString label;    ///< ES-DE's name for it, eg. "NetherSX2 (Standalone)"
    QString command;  ///< the launch command for a Pegasus metadata file
    bool installed = false;
    bool retroarch = false;  ///< a RetroArch core, which also has to be downloaded in RetroArch
    bool recommended = false;  ///< ES-DE's default for the system
    bool custom = false;  ///< one you saved yourself (Settings > Library)
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
    bool is_android = false;  ///< Android apps and games: each game opens its own app
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

/// The launch command the user's metadata had before Pegasus X first changed
/// it, read from the <file>.bak copy kept then. `known` is false if nothing
/// was changed yet. With an empty `game_path`, the collection's command.
struct OriginalLaunch {
    bool known = false;
    QString command;
};
OriginalLaunch original_launch(const QString& collection_name, const QString& game_path);

/// All of a collection's original commands at once (see original_launch):
/// the collection's, and those of the games that had their own
struct OriginalLaunches {
    bool collection_known = false;
    QString collection_command;
    std::vector<GameEmulator> games;
};
OriginalLaunches original_launches(const QString& collection_name);

// Pegasus X: Settings > Edit metadata

/// Where a game's (or, with an empty `game_path`, a collection's) details are
/// saved: your own file first, then an ES-DE import; a game Pegasus found by
/// itself goes into Pegasus X's own file (`is_new`)
struct MetadataLocation {
    QString file;
    bool is_import = false;
    bool is_new = false;
};
MetadataLocation metadata_location(const QString& collection_name, const QString& game_path);

/// Writes the changed fields of a game. Keys: title, sort-by, developer,
/// publisher, genre (lists), release, players, rating, summary, description,
/// assets.<kind>, x-manual; an empty value removes the field. In an ES-DE
/// import the change is marked, so importing again keeps it.
/// (scraped: written by the scraper, so marked x-pegasusx-scraped instead of
/// x-pegasusx-edited)
QString save_game_metadata(const QString& collection_name, const QString& game_path,
                           const QString& game_title, const QVariantMap& changes, bool scraped = false);
/// The same for a collection: name, shortname, summary, description, assets.<kind>
QString save_collection_metadata(const QString& collection_name, const QVariantMap& changes);
/// Pegasus X's old edits file (all collections in one, which put games in
/// collections declared before theirs) split into one file per collection
void split_legacy_edits_file();
/// The emulator setup (ES-DE's lists and the installed apps) is read again
/// next time it's needed (see shared_emulator_setup)
void invalidate_emulator_setup();

/// The emulator a collection uses when its metadata has no launch command:
/// ES-DE's default for its system if installed, else the first installed one.
/// `collections`: (name, short name); `rom_roots`: their ROM folders.
struct DefaultLaunch {
    QString label;
    QString command;  ///< empty: nothing installed for it
};
std::vector<DefaultLaunch> default_launches(const std::vector<std::pair<QString, QString>>& collections,
                                            const QStringList& rom_roots);

/// The emulators of a collection's system (as in Settings > Emulators)
std::vector<EmulatorChoice> collection_choices(const QString& collection_name, const QString& short_name, const QString& rom_root);
/// Saves (or with `remove`, forgets) an emulator of yours: a name and a launch
/// command, offered for the collection's system (or the collection itself if
/// its system isn't known). Returns an error, or empty.
QString save_custom_emulator(const QString& collection_name, const QString& short_name,
                             const QString& label, const QString& command, bool remove);

/// A collection's own lines, for Settings > Edit metadata
struct CollectionDetails {
    QString short_name;
    QString extensions;
    QString launch;           ///< from its metadata; empty if none
    QString default_label;    ///< used when `launch` is empty
    QString default_command;
    QString rom_root;         ///< the folder above its metadata file's
};
CollectionDetails collection_details(const QString& collection_name);

/// Starts `command` with a game file. Returns an error, or empty.
QString test_emulator_command(const QString& command, const QString& game_path);


// -- Android collections: each game opens its own app ------------------------------

struct AndroidApp {
    QString label;
    QString package;
    QString component;  ///< its start screen
    bool game = false;  ///< Android knows it's a game
};

/// The installed apps that can be opened, games first, then by name
std::vector<AndroidApp> android_apps();

/// The packages of every emulator ES-DE knows (its find rules, and the custom
/// ones of the ES-DE folder), to tell emulators apart from other apps
QSet<QString> emulator_packages();

/// The command that opens an app
QString android_app_command(const QString& component);

/// For each game of an Android collection: the package of the app it opens
/// (from its own launch line, an android:<package> file or the package
/// name inside an ES-DE .app file), keyed by the game's file
std::vector<std::pair<QString, QString>> android_game_apps(const QString& collection_name);

/// Gives every game of an Android collection that opens no app yet the
/// launch line of its app: found by package (android:<package> files, ES-DE
/// .app files) or by name. Returns how many games got one.
int match_android_games(const QString& collection_name);

/// Adds the installed apps Android knows are games, and aren't in the
/// collection yet, as games of it (file android:<package>, with their
/// launch line). Returns how many were added.
int add_android_games(const QString& collection_name);

} // namespace esde
} // namespace providers
