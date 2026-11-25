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
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <vector>


// Pegasus X: damaged-file recovery at startup (build 2 of it; safe saves,
// SafeFile.h, are build 1). Before anything reads them, each of Pegasus X's
// own files is checked:
//
// 1. Missing after an interrupted save (SafeFile's fallback left
//    `<name>.pxsave` and/or `<name>.pxold` beside it, the file itself gone):
//    the newest whole copy that passes the check is put back (`.pxsave` when
//    `.pxold` is there too: it was written in full before the old file was
//    moved; else whichever of them is there), else its last good copy, else
//    it starts at its defaults.
// 2. Damaged (the check below fails): moved, never deleted or written over,
//    to a `damaged` folder (the Pegasus X folder's; the private folder's for
//    the private files), its date and time in the name. Its last good copy
//    takes its place if that passes the check, else it starts at its
//    defaults (no file: what a first start has). A last good copy that
//    doesn't pass is moved to `damaged` too.
// 3. Fine: `.pxsave` / `.pxold` left beside it by an interrupted save are
//    removed, and its last good copy (`<name>.pxgood`, beside it, written
//    with SafeFile) is made again from it when they differ.
// Nothing else is touched. Each action is a "Recovery:" line in the log, and
// what was fixed is kept for the notice after the start (last()).
namespace recovery {

/// What a file is, for its check
enum class Kind {
    Settings,     ///< settings.txt: "key: value" lines
    Lines,        ///< game_dirs.txt, favorites.txt: one path a line (may be empty)
    RomFolders,   ///< pegasusx-rom-folders.txt: "folder<TAB>system" lines
    Root,         ///< pegasusx-root.txt: one folder path
    JsonObject,   ///< a JSON object
    JsonArray,    ///< a JSON array
    Sqlite,       ///< stats.db
    Ini,          ///< pegasusx.ini, pegasusx-private.ini
    SdlMappings,  ///< sdl_controllers.txt: SDL gamepad mappings
};

struct Spec {
    QString dir;          ///< the folder it's in
    QString name;         ///< its file name
    Kind kind;
    QString label;        ///< what it is, for the notice ("Favorites")
    QString damaged_dir;  ///< where a damaged copy goes
};

struct Fixed {
    QString name;         ///< the file name
    QString label;
    QString path;         ///< where the file is
    QString how;          ///< "good copy", "defaults", "put back"
    QString reason;       ///< what was wrong ("not JSON", "missing after an interrupted save")
    QStringList moved;    ///< what went to a damaged folder (full paths there)
};

struct Report {
    std::vector<Fixed> fixed;
    QStringList log;      ///< the "Recovery:" lines (for the log, once it's open)
};

/// The check: empty if `bytes` are a good `kind` file, else why not.
/// `good`: the last good copy's bytes (an empty list when it had folders is damage)
QString check(Kind kind, const QByteArray& bytes, const QString& path, const QByteArray& good = QByteArray());

/// One file: put back, checked, moved, replaced or left (see above)
void recover(const Spec& spec, Report& report, const QDateTime& now = QDateTime::currentDateTime());

/// The good copy's name beside a file
QString good_path(const QString& path);

/// Pegasus X's own files: pegasusx-root.txt (the private folder: run before
/// the Pegasus X folder is decided), then the rest
Report run_root();
Report run_all();

/// What the start's runs fixed (for the notice), and their log lines
const Report& last();

/// A list emptied on purpose (the ROM folders): its good copy emptied too,
/// so the next start doesn't take it for damage
void saved_empty(const QString& path);

} // namespace recovery
