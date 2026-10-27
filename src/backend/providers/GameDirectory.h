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
#include <QtGlobal>
#include <utility>
#include <vector>

namespace model { class Collection; class Game; }


// Pegasus X: the game directory. One metadata file per system, owned by
// Pegasus X, is the only metadata Pegasus reads:
//
//     <Pegasus X folder>/gamedirectory/<system>/metadata.pegasus.txt
//
// It's made by importing your ROM folders (their games, their own metadata
// files and media folders) and ES-DE's gamelists, and written to by your
// edits in Settings > Library and by the scraper. Your own files are read
// once at import and never written to. Scraped art goes to
// downloaded_media/<system>/<kind>/. Backups (before a hand edit, before a
// scrape that overwrites) go to backup/<system>/, and so does what Clean up
// sets aside for games whose files are gone.
namespace providers {
namespace gamedir {

/// <Pegasus X folder>/gamedirectory
QString dir();
/// the folder name of a system: its short name, else its name made safe
QString key_for(const QString& collection_name, const QString& short_name);
/// gamedirectory/<key>/metadata.pegasus.txt
QString system_file(const QString& key);
/// every system's metadata file, sorted
QStringList metafiles();
/// is this file one of the game directory's?
bool is_inside(const QString& path);
/// the system (folder name) of one of its files, or empty
QString system_of(const QString& path);

/// A copy of a system file in backup/<system>/ before it's changed
/// (`reason`: "edit", "scrape", "import"); the last 3 of each reason are kept
void backup(const QString& system_file_path, const QString& reason);
/// backup/<system>/ of a system, or backup/ with an empty system
QString backup_dir(const QString& system = QString());
/// how much the backups take: { files, bytes }
std::pair<int, qint64> backup_size();
/// removes every backup (and what Clean up set aside)
int clear_backups();

/// Games whose files are gone: their entries and their scraped art
/// (downloaded_media/<system>/<kind>/<name>.*) set aside in
/// backup/<system>/cleanup/. { games, files } found (`dry`: only counted)
struct Cleanup { int games = 0; int files = 0; QStringList titles; QString error; };
Cleanup clean_up(const QString& media_dir, bool dry);

/// Was the library made the old way (imports, overrides, ROM folders as
/// game directories) and the game directory not made yet?
bool migration_needed();
/// Has the game directory been made (the old layers aren't read any more)?
bool migrated();
/// Writes the library as read now (every collection and game, with your
/// edits and scraped data applied) into the game directory, makes it the
/// only game directory, and sets the old metadata folders aside in
/// backup/. Returns how many games were written, -1 when nothing was done.
int migrate(const std::vector<model::Collection*>& collections, const std::vector<model::Game*>& games);
/// a migration just happened: the library has to be read again from the
/// game directory (answered once)
bool take_reload_wanted();

} // namespace gamedir
} // namespace providers
