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


// Pegasus X: is the first-start setup done? One marker says so:
// `pegasusx-setup-done` in the app's own storage (AppDataLocation: on Android
// the app's files folder, which Android's Clear data and an uninstall empty;
// the Pegasus X folders are on shared storage and stay). While setup isn't
// done, the step it's at is `pegasusx-setup-step` beside it (1-4), so a
// restart goes on from there. Both are written with SafeFile.
//
// Decided once at the start (decide()), one "Setup:" log line:
// 1. the marker is there: done.
// 2. a step file (or an older build's `pegasusx-wizard` in the private
//    folder, moved here): setup goes on at that step.
// 3. an older build finished setup (`pegasusx-root-asked` or a folder setting
//    in the private folder, no step) and the app's cache folder isn't empty
//    (the app's data wasn't cleared): done, marker written.
// 4. otherwise setup runs from step 1; if the Pegasus X folder already holds
//    Pegasus data (after Clear data, or another Pegasus's), step 1 asks:
//    use it, or start fresh.
namespace setupstate {

struct Dirs {
    QString app_data;   ///< the marker and the step (AppDataLocation)
    QString priv;       ///< paths::privateConfigDir (older builds' markers)
    QString data;       ///< the Pegasus X folder in use
    QString cache;      ///< paths::writableCacheDir
};

struct State {
    bool done = false;
    int step = 1;          ///< where setup goes on (when not done)
    QString found;         ///< the Pegasus X folder, when it holds data setup asks about
    QString log;           ///< the "Setup: ..." line
};

QString done_path(const Dirs&);
QString step_path(const Dirs&);

/// Done or not, and where (writes the marker for an older build's finished setup)
State decide(const Dirs&);

/// Does `dir` hold Pegasus data (settings, game folders, the game directory...)?
bool holds_data(const QString& dir);

/// The step setup is at (0: none)
int step(const Dirs&);
void set_step(const Dirs&, int step);
/// Setup finished or skipped: the marker, no step
bool finish(const Dirs&);
/// Setup to do again from `step` (Run setup again, Start over): no marker
void reopen(const Dirs&, int step);

/// This run's state (main.cpp decides it before the backend starts; without
/// that, eg. in tests: done)
State& session();
/// This run's folders
Dirs dirs();

} // namespace setupstate
