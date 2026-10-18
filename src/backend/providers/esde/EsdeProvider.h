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
#include <vector>


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

/// Pegasus X: games from the ROM folders picked in Settings > Pegasus X library.
/// System folders are recognized by name (ES-DE's names and common variants)
/// and get ES-DE's launch commands, so no metadata files are needed.
/// Details: ES-DE gamelists picked in "ES-DE import", a gamelist.xml in the
/// system folder, then Pegasus metadata files (each wins over the previous).
/// Art, each source winning over the previous: metadata files, the ROM
/// folders' own media folders, media picked in "ES-DE import", then the
/// picked media folders (a later pick wins over an earlier one).
class LibraryProvider : public Provider {
    Q_OBJECT

public:
    explicit LibraryProvider(QObject* parent = nullptr);

    Provider& run(SearchContext&) final;
    QStringList cacheInputFiles() const final;
};


/// One emulator that can be picked for a system
struct EmulatorOption {
    QString label;
    bool installed;
    bool retroarch;  ///< a RetroArch core, which also has to be downloaded in RetroArch
    bool custom;          ///< added by the user ("@custom:<name>")
    QString command;      ///< for custom ones, the launch command
};

/// A system that has games, with the emulators that can launch it
struct SystemEmulators {
    QString name;
    QString fullname;
    QString current;        ///< chosen in Pegasus X, empty for the default
    QString default_label;  ///< what the default resolves to right now
    std::vector<EmulatorOption> options;

    // for the library hub
    QString collection_name;  ///< the name it's shown with
    QString metadata_name;    ///< the collection name in the user's metadata files, if any
    QString name_source;      ///< "metadata" or "esde"
    QString short_name;       ///< the theme's short name (logos, RetroAchievements)
    QString status;           ///< "ready", "missing" (emulator not installed), "none"
    QStringList folders;      ///< its game folders
    QStringList art_sources;  ///< its art folders, the one that wins last
    QStringList ignored;      ///< ignored folders inside its game folders
};

/// For the Emulators settings screen: the systems found in the Pegasus X
/// library and (if that data source is on) in the ES-DE ROMs folders
std::vector<SystemEmulators> emulator_choices();

/// The result of launching a system's first game with its chosen emulator
struct EmulatorTest {
    QString game;   ///< file name of the game used
    QString error;  ///< empty if Android started the emulator
};

/// For the "Test with a game" button: the system's chosen emulator, or
/// `command` if given (an emulator being added)
EmulatorTest test_emulator(const QString& system_name, const QString& command = QString());

/// Opens the app of the system's chosen emulator (its start screen);
/// returns an error, or empty
QString open_emulator(const QString& system_name);

} // namespace esde
} // namespace providers
