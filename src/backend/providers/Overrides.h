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


#pragma once

#include <QString>
#include <vector>

namespace model { class Collection; class Game; }


// Pegasus X: what's laid on top of the metadata files after the library is
// read, in this order (the later wins):
//
//  1. metadata/imports-fill/*.metadata.pegasus.txt: ES-DE's data (and ROM
//     folder lists) for games your own metadata files define. How much of
//     it is used is the ES-DE import's "Use" setting: media only, media and
//     details you don't have (the default), or everything (replacing yours)
//  2. metadata/overrides/*.metadata.pegasus.txt: your edits in Settings >
//     Library, scraped data and emulator choices. These always win
//
// Neither folder is a game directory, so Pegasus itself never reads them as
// metadata, and the files that define your games are never written to.
namespace providers {
namespace overrides {

/// "media", "fill" or "replace" (Settings > ES-DE import > Use)
QString import_fill_mode();
void set_import_fill_mode(const QString& mode);

void apply(const std::vector<model::Collection*>& collections, const std::vector<model::Game*>& games);

/// An ES-DE import file listed a game file your own metadata file already
/// has (your file wins): its data for that game is used on top of yours as
/// the "Use" setting says, when the library is done
void note_import_conflict(const QString& metafile_path);
/// A full read of the metadata files starts (the list above is made again;
/// a library read from the cache uses the one kept from the last full read)
void begin_import_scan();
/// Is this one of the ES-DE import files (metadata/imports)?
bool is_import_file(const QString& metafile_path);

} // namespace overrides
} // namespace providers
