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

#include <QString>
#include <QStringList>
#include <vector>

namespace model { class Collection; }
namespace model { class Game; }


// Pegasus X: changes to the found library before any theme sees it. They're
// applied after the game list cache is written, so the cache always has
// everything and a change here only needs a quick reload, not a full scan.
//   - Android: every Android collection (Pegasus' own "Android", and those
//     with the shortname android / androidapps / androidgames or named
//     "Android Apps" / "Android Games") becomes one "Android" collection,
//     games first. Each entry is tagged Game, Emulator or Application (tags),
//     and apps and emulators also get that genre (as XboxOSv2 expects).
//   - Hidden games and collections are left out.
namespace providers {
namespace library {

struct HiddenGame {
    QString path;         ///< its first file's path (or URI)
    QString title;
    QStringList collections;
};
struct HiddenList {
    QStringList collections;
    std::vector<HiddenGame> games;
};

/// config/pegasusx-hidden.json
HiddenList load_hidden();
bool save_hidden(const HiddenList&);

/// The key a game is hidden by: its first file's path, or its URI
QString game_key(const model::Game&);

bool merge_android_enabled();
void set_merge_android_enabled(bool);

void apply_library_rules(std::vector<model::Collection*>& collections, std::vector<model::Game*>& games);

} // namespace library
} // namespace providers
