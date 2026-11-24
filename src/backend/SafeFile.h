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

#include <QByteArray>
#include <QString>
#include <functional>


// Pegasus X: saving a file so a dead battery (or a full or odd storage) never
// leaves it half-written or empty.
//
// 1. The usual way (QSaveFile, as the other Pegasus X files are saved): the
//    whole new text goes into a temporary file next to it, is flushed to the
//    storage, and then takes the old file's place in one step (a rename).
// 2. If that can't be done (the storage refuses the temporary file or the
//    rename over the old file), the fallback, which never writes into the
//    old file either:
//    a. the whole new text into `<name>.pxsave` next to it, flushed to the
//       storage, and its size checked;
//    b. the old file renamed to `<name>.pxold` (a free name);
//    c. `<name>.pxsave` renamed to the file's own name (now free);
//    d. `<name>.pxold` deleted.
//    If (a) fails, nothing else is done. If (b) fails, `.pxsave` is deleted.
//    If (c) fails, `.pxold` is renamed back. The old file is kept in each case.
// A save that can't be done keeps the old file and logs a warning; it never
// writes into the old file directly.
namespace safefile {

/// Saves `data` as `path` (see above). `text`: as QIODevice::Text (line
/// ends as the system writes them: the same as the file had). True when
/// saved; false: the old file is as it was, and a warning is in the log.
bool write(const QString& path, const QByteArray& data, bool text = false);

/// The two names the fallback uses next to `path`
QString fallback_new_path(const QString& path);
QString fallback_old_path(const QString& path);

/// For the tests: called at each step ("usual-written", "usual-commit",
/// "fallback-half", "fallback-written", "fallback-old-moved",
/// "fallback-new-moved"); returning `Die` stops right there as a power cut
/// would (nothing cleaned up), `Fail` makes that step fail ("usual-commit":
/// the storage refusing the rename, so the fallback runs).
enum class Step { Go, Fail, Die };
extern std::function<Step(const char*)> test_hook;

} // namespace safefile
