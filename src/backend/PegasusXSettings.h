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

#include <QString>
#include <QStringList>


/// Pegasus X: settings of the Pegasus X library (ROM and media folders) and
/// the emulator choices, kept in their own file (pegasusx.ini in the config
/// directory) so they survive rescans and updates
namespace pegasusx {

QString settingsFilePath();

QStringList libraryRomDirs();
void setLibraryRomDirs(const QStringList&);

/// Media folders, in the order they were added; a folder added later wins
/// over an earlier one when both have art for the same game
QStringList libraryMediaDirs();
void setLibraryMediaDirs(const QStringList&);

/// Folders and files picked from an ES-DE folder (the whole ES-DE folder,
/// downloaded_media, gamelists, or a single system's folder in them); only
/// these are used by the library, nothing from ES-DE is loaded on its own
QStringList esdeImports();
void setEsdeImports(const QStringList&);

/// Folders inside the ROM and media folders that are skipped
QStringList ignoredDirs();
void setIgnoredDirs(const QStringList&);

/// The emulator (an ES-DE command label) chosen for a system, empty for the default
QString systemEmulator(const QString& system_name);
void setSystemEmulator(const QString& system_name, const QString& label);

} // namespace pegasusx
