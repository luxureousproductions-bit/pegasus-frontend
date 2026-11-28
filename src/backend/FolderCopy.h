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
#include <QtGlobal>
#include <functional>


// Pegasus X: one folder's files copied into another, with its progress
// (Settings > Pegasus X folder > Use it and move my data there)
namespace folder_copy {

/// how much a copy() copies
struct Size {
    int files = 0;
    qint64 bytes = 0;
};
/// the files under `from`, but the top-level names in `skip`
Size size_of(const QString& from, const QStringList& skip);

/// one file, 1 MiB at a time (`step`: each part's bytes); a part-written
/// copy is removed
bool copy_file(const QString& from, const QString& to, const std::function<void(qint64)>& step = {});

/// (bytes copied, files copied, the file now: relative to the folder)
using Progress = std::function<void(qint64, int, const QString&)>;
/// the files and folders of `from` into `to`, but the top-level names in
/// `skip` (what's already there is replaced; what the copy didn't reach is
/// kept when it fails). Returns the files copied, -1 on an error (`error`).
int copy(const QString& from, const QString& to, const QStringList& skip, QString& error,
         const Progress& progress = {});

} // namespace folder_copy
