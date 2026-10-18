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

#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QStringBuilder>


namespace {
const QString KEY_ROM_DIRS = QStringLiteral("library/romDirs");
const QString KEY_MEDIA_DIR_OLD = QStringLiteral("library/mediaDir");  // single folder, before multiple
const QString KEY_MEDIA_DIRS = QStringLiteral("library/mediaDirs");
const QString KEY_ESDE_IMPORTS = QStringLiteral("library/esdeImports");
const QString KEY_IGNORED_DIRS = QStringLiteral("library/ignoredDirs");
const QString KEY_HIDDEN_GAMES = QStringLiteral("library/hiddenGames");
const QString KEY_CUSTOM_PREFIX = QStringLiteral("customEmulators/");
const QString KEY_NAME_PREFIX = QStringLiteral("collectionNames/");

void write_list(const QString& key, const QStringList& list)
{
    QSettings file(pegasusx::settingsFilePath(), QSettings::IniFormat);
    if (list.isEmpty())
        file.remove(key);
    else
        file.setValue(key, list);
}
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

QStringList libraryMediaDirs()
{
    const QSettings file(settingsFilePath(), QSettings::IniFormat);
    QStringList dirs = file.value(KEY_MEDIA_DIRS).toStringList();
    const QString old_dir = file.value(KEY_MEDIA_DIR_OLD).toString();
    if (!old_dir.isEmpty() && !dirs.contains(old_dir))
        dirs.prepend(old_dir);
    return dirs;
}

void setLibraryMediaDirs(const QStringList& dirs)
{
    write_list(KEY_MEDIA_DIRS, dirs);
    QSettings file(settingsFilePath(), QSettings::IniFormat);
    file.remove(KEY_MEDIA_DIR_OLD);
}

QStringList esdeImports()
{
    const QSettings file(settingsFilePath(), QSettings::IniFormat);
    return file.value(KEY_ESDE_IMPORTS).toStringList();
}

void setEsdeImports(const QStringList& paths)
{
    write_list(KEY_ESDE_IMPORTS, paths);
}

QStringList ignoredDirs()
{
    const QSettings file(settingsFilePath(), QSettings::IniFormat);
    return file.value(KEY_IGNORED_DIRS).toStringList();
}

void setIgnoredDirs(const QStringList& dirs)
{
    write_list(KEY_IGNORED_DIRS, dirs);
}

QVariantList customEmulators(const QString& system_name)
{
    const QSettings file(settingsFilePath(), QSettings::IniFormat);
    const QByteArray json = file.value(KEY_CUSTOM_PREFIX + system_name).toString().toUtf8();
    return QJsonDocument::fromJson(json).array().toVariantList();
}

void setCustomEmulators(const QString& system_name, const QVariantList& list)
{
    QSettings file(settingsFilePath(), QSettings::IniFormat);
    if (list.isEmpty())
        file.remove(KEY_CUSTOM_PREFIX + system_name);
    else
        file.setValue(KEY_CUSTOM_PREFIX + system_name,
            QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(list)).toJson(QJsonDocument::Compact)));
}

QString collectionNameSource(const QString& system_name)
{
    const QSettings file(settingsFilePath(), QSettings::IniFormat);
    return file.value(KEY_NAME_PREFIX + system_name, QStringLiteral("metadata")).toString();
}

void setCollectionNameSource(const QString& system_name, const QString& source)
{
    QSettings file(settingsFilePath(), QSettings::IniFormat);
    if (source.isEmpty() || source == QLatin1String("metadata"))
        file.remove(KEY_NAME_PREFIX + system_name);
    else
        file.setValue(KEY_NAME_PREFIX + system_name, source);
}

QStringList hiddenGames()
{
    const QSettings file(settingsFilePath(), QSettings::IniFormat);
    return file.value(KEY_HIDDEN_GAMES).toStringList();
}

void setHiddenGames(const QStringList& paths)
{
    write_list(KEY_HIDDEN_GAMES, paths);
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
