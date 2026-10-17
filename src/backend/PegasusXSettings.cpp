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


#include "PegasusXSettings.h"

#include "Paths.h"

#include <QSettings>
#include <QStringBuilder>


namespace {
const QString KEY_ROM_DIRS = QStringLiteral("library/romDirs");
const QString KEY_MEDIA_DIR = QStringLiteral("library/mediaDir");
const QString KEY_EMULATOR_PREFIX = QStringLiteral("emulators/");
} // namespace


namespace pegasusx {

QString settingsFilePath()
{
    return paths::writableConfigDir() % QStringLiteral("/pegasusx.ini");
}

QStringList libraryRomDirs()
{
    const QSettings file(settingsFilePath(), QSettings::IniFormat);
    return file.value(KEY_ROM_DIRS).toStringList();
}

void setLibraryRomDirs(const QStringList& dirs)
{
    QSettings file(settingsFilePath(), QSettings::IniFormat);
    if (dirs.isEmpty())
        file.remove(KEY_ROM_DIRS);
    else
        file.setValue(KEY_ROM_DIRS, dirs);
}

QString libraryMediaDir()
{
    const QSettings file(settingsFilePath(), QSettings::IniFormat);
    return file.value(KEY_MEDIA_DIR).toString();
}

void setLibraryMediaDir(const QString& dir)
{
    QSettings file(settingsFilePath(), QSettings::IniFormat);
    if (dir.isEmpty())
        file.remove(KEY_MEDIA_DIR);
    else
        file.setValue(KEY_MEDIA_DIR, dir);
}

QString systemEmulator(const QString& system_name)
{
    const QSettings file(settingsFilePath(), QSettings::IniFormat);
    return file.value(KEY_EMULATOR_PREFIX + system_name).toString();
}

void setSystemEmulator(const QString& system_name, const QString& label)
{
    QSettings file(settingsFilePath(), QSettings::IniFormat);
    if (label.isEmpty())
        file.remove(KEY_EMULATOR_PREFIX + system_name);
    else
        file.setValue(KEY_EMULATOR_PREFIX + system_name, label);
}

} // namespace pegasusx
