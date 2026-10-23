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


// Pegasus X: file names matched without regard to upper / lower case, so
// "Manuals", "manuals" or "Covers" are found on Linux (case-sensitive) like
// on Android and Windows. On those, and on macOS, the file system already
// ignores case and these return the path as given.
namespace pathcase {
/// The path as it's spelled on disk, or empty if nothing matches. A plain
/// stat first; folder listings (kept for the session) only when that misses.
QString resolve(const QString& path);
/// Whether the path exists as a file, case aside
bool file_exists(const QString& path);
/// Forgets the folder listings (eg. after a rescan)
void clear_cache();
} // namespace pathcase
