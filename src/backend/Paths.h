// Pegasus Frontend
// Copyright (C) 2018  Mátyás Mustoha
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

class QString;
class QStringList;


namespace paths {

/// Returns $PEGASUS_HOME if defined, or $HOME if defined,
/// otherwise QDir::homePath().
QString homePath();

/// Returns the directory that contains the Pegasus executable/app bundle.
QString app_dir_path();

/// Returns the directory paths where config files may be located
const QStringList& configDirs();

/// Returns a directory path where persistent data could be stored.
/// Pegasus X: the Pegasus X folder, when one was chosen (see privateConfigDir)
QString writableConfigDir();

/// Pegasus X: the app's own config folder (on Android, in its app storage).
/// A file `pegasusx-root.txt` there names the Pegasus X folder: everything
/// (settings, themes, metadata, logs, art) lives in that folder instead.
QString privateConfigDir();
/// The Pegasus X folder named in pegasusx-root.txt, or empty
QString pegasusxRootSetting();
/// The Pegasus X folder named in pegasusx-root.txt when it couldn't be used
/// at this start (SD card missing?), so the app's own folder is in use; else empty
QString pegasusxRootMissing();
/// Writes (or with an empty path, removes) the Pegasus X folder setting.
/// Takes effect when Pegasus X starts again.
bool setPegasusxRootSetting(const QString& dir);

/// Returns a directory path where cache data could be stored
QString writableCacheDir();

} // namespace paths
