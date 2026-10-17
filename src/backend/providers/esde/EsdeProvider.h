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
/// and get ES-DE's launch commands, so no metadata files are needed. Media is
/// read from the picked media folder (<system>/<type>/) and <system>/media/.
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
};

/// A system that has games, with the emulators that can launch it
struct SystemEmulators {
    QString name;
    QString fullname;
    QString current;        ///< chosen in Pegasus X, empty for the default
    QString default_label;  ///< what the default resolves to right now
    std::vector<EmulatorOption> options;
};

/// For the Emulators settings screen: the systems found in the Pegasus X
/// library and (if that data source is on) in the ES-DE ROMs folders
std::vector<SystemEmulators> emulator_choices();

} // namespace esde
} // namespace providers
