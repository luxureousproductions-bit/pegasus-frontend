// Pegasus X
// Copyright (C) 2026  Esteban (MisterE88)
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

#include <QHash>
#include <QString>
#include <QStringList>
#include <vector>

namespace model { class Game; }


// Pegasus X: Library > Metadata. Spellings of one genre, publisher or
// developer shown as one name, without touching a game file: an alias
// ("Nintendo of America" -> "Nintendo") in metadata/aliases.json in the
// Pegasus X folder (SafeFile; damaged-file recovery; in backups, deleted by
// Start over). Applied when games are loaded, after the game list cache (so
// the cache, and the game, keep the spellings their files have: an alias
// removed shows them again at once, and a rescrape's old spelling folds in
// again). Never a chain: A->B then B->C keeps A->C and B->C; a loop is
// refused.
namespace aliases {

enum Field : int { Genre = 0, Publisher = 1, Developer = 2, FieldCount = 3 };
/// "genre", "publisher", "developer" (-1: none)
int field_of(const QString& name);
QString field_name(int field);

struct Table {
    QHash<QString, QString> map[FieldCount];   ///< spelling -> shown name
    bool empty() const;
};

QString file_path();
/// The file (missing, or not readable: no aliases)
Table load(const QString& path);
bool save(const QString& path, const Table& table);

/// This run's aliases (read from the file the first time; thread-safe copy)
Table current();
void set_current(const Table&);
/// read again from the file (after a restore, Start over)
void reload();

/// A spelling as shown (itself when it has no alias)
QString shown(const Table&, int field, const QString& spelling);
/// A game's list as shown: each spelling's alias, each name once
QStringList shown_list(const Table&, int field, const QStringList& raw);

/// `from` (spellings) shown as `to`: chains folded (an alias to one of them
/// follows it to `to`; `to` with an alias of its own: its end). Returns an
/// error ("" when added): a loop, or nothing to do.
QString add(Table&, int field, const QStringList& from, const QString& to);
/// Undo: these spellings shown as they are again
int remove(Table&, int field, const QStringList& from);
/// Every spelling shown as `to`
QStringList spellings_of(const Table&, int field, const QString& to);

/// Loaded games (the scan, after the cache): their lists kept as the files
/// have them, the shown ones made from them
void apply(const std::vector<model::Game*>& games, const Table&);
/// One game's list changed in place (Library, Change this genre, a scrape):
/// `raw` as its file has it now, the shown one made from it
void set_raw(model::Game& game, int field, const QStringList& raw);
/// After an alias change: the shown lists made again; true if it changed
bool reapply(model::Game& game, const Table&);
/// A game's list as its file has it
QStringList raw_list(const model::Game& game, int field);


// -- Suggestions: spellings that look like one name ------------------------------------------

/// Invisible differences only: letter case, apostrophe kinds, runs of
/// spaces, the spacing around "/" (as Library's genres always grouped)
QString invisible_key(const QString&);
/// Same company: no accents or punctuation, "&" as "and", no leading "The",
/// no trailing Inc / Ltd / Co / Corp / LLC / GmbH / S.A. / K.K.
QString company_key(const QString&);
/// ...and no trailing region: NA, USA, of America, Europe, EU, UK, Japan, JP
QString region_key(const QString&);
/// Close, maybe one name: the same without spaces, one inside the other, or
/// 1-2 letters off (by length). Never a division and its parent (one's
/// words all in the other's: "Sega AM2" and "Sega").
bool maybe_same(const QString& region_key_a, const QString& region_key_b);

struct Value {
    QString name;   ///< as shown (the most used spelling)
    int count = 0;  ///< games
};
struct Suggestion {
    int level = 0;            ///< 0 near-certain, 1 regional, 2 maybe
    std::vector<int> values;  ///< indexes in the values given, most games first
    int games = 0;
};
/// Groups of values that look like one name, never automatic: near-certain
/// (the same company_key), regional (the same region_key), maybe
/// (maybe_same); most games first within a level
std::vector<Suggestion> suggest(const std::vector<Value>& values);

} // namespace aliases
