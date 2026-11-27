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
#include <QJsonObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>


// Pegasus X: the scraper's ESRB descriptors step (Scraper > Content, off
// unless turned on). The ESRB content descriptors of games that already have
// an x-esrb letter, from IGDB through the Pegasus X server
// (server/esrb-worker: no key, no login), written as x-esrb-descriptors
// ("Blood and Gore, Strong Language") and, only when the server gives any,
// x-esrb-interactive. Never the letter or anything else; never a guess.
// What's asked and how each answer is read; the requests themselves are the
// scraper's (Scraper::esrbBegin).
namespace scraper {
namespace esrb {

/// the server's address by default (Content settings > Server address)
QString default_server();
/// the endpoint of a Server address setting: https (http only to this
/// machine, for tests); a bare address gets /v1/esrb. Invalid: not usable.
QUrl endpoint(const QString& setting);

enum : int {
    BATCH = 10,              ///< games a request (the server's most)
    PER_MINUTE = 30,         ///< requests a minute from one address (the server's most)
    SPACING_MS = 2200,       ///< between two requests' starts: under 30 a minute
    GAME_TRIES = 6,          ///< "later" answers for one game before it waits for the next run
    FAILURES = 6,            ///< requests in a row with no usable answer before the step stops
    MAX_WAIT_S = 300,        ///< the longest wait before asking again
};

/// IGDB's platform id for an ES-DE system name (0: none). The same table as
/// server/esrb-worker/worker.js's PLATFORMS (a test compares them).
int igdb_platform(const QString& system);
const QHash<QString, int>& platforms();

/// One game as it's asked
struct Game {
    QString title;            ///< for the log
    QString name;             ///< the name sent (no IGDB id)
    QString letter;           ///< its x-esrb, as a letter the server takes
    int igdb_id = 0;
    int platform = 0;         ///< IGDB's platform id
    int year = 0;
    bool has_descriptors = false;  ///< an x-esrb-descriptors line already (one typed by hand too)
    bool has_interactive = false;
};
/// why it can't be asked ("" when it can)
QString why_not(const Game& game);
/// its entry in a request: { igdbId, esrb } or { name, platform, year, esrb }
QJsonObject request_entry(const Game& game);

/// What one result means for its game
struct Outcome {
    enum Kind : int { Write = 0, Nothing = 1, Later = 2 };
    int kind = Nothing;
    QVariantMap changes;      ///< Write: x-esrb-descriptors / x-esrb-interactive
    int retry_after = 0;      ///< Later: seconds
    QString log;              ///< the log line (without the game's name)
    QString result;           ///< for the Last games list
};
/// a result of the server's ({ status, descriptors, interactive, ... }) for a
/// game: only "ok" with descriptors (or interactive elements) writes, a line
/// the game has is kept unless `overwrite`
Outcome read_result(const QJsonObject& result, const Game& game, bool overwrite);

/// the wait the server asked for: its Retry-After header, else the body's
/// retryAfter, else `fallback` (seconds, 1 to MAX_WAIT_S)
int retry_after(const QByteArray& header, const QJsonObject& body, int fallback);

} // namespace esrb
} // namespace scraper
