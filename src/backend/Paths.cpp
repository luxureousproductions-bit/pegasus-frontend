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


#include "Paths.h"

#include "AppSettings.h"

#ifdef Q_OS_ANDROID
#include "platform/AndroidHelpers.h"
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>

#include <functional>


namespace {
using QSP = QStandardPaths;

void remove_orgname(QString& str)
{
    const QRegularExpression replace_regex(QStringLiteral("(/pegasus-frontend){2}$"));
    str.replace(replace_regex, QStringLiteral("/pegasus-frontend"));
}

void create_dir_if_not_exists(const QString& dir_path)
{
    Q_ASSERT(!dir_path.isEmpty());
    QDir(dir_path).mkpath(QLatin1String(".")); // does nothing if already exists
}

QString get_private_config_dir()
{
#ifdef Q_OS_ANDROID
    const QString dir_path = QSP::writableLocation(QSP::GenericDataLocation)
                           + QStringLiteral("/pegasus-frontend");
#else
    QString dir_path = AppSettings::general.portable
        ? paths::app_dir_path() + QStringLiteral("/config")
        : QSP::writableLocation(QSP::AppConfigLocation);
    remove_orgname(dir_path);
#endif
    create_dir_if_not_exists(dir_path);
    return dir_path;
}

const QString ROOT_FILE = QStringLiteral("pegasusx-root.txt");

// the Pegasus X folder, when pegasusx-root.txt names one that can be used
QString get_appconfig_dir()
{
    const QString own = get_private_config_dir();
    QFile file(own + QLatin1Char('/') + ROOT_FILE);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString root = QDir::cleanPath(QString::fromUtf8(file.readAll()).trimmed());
        if (!root.isEmpty() && (QFileInfo(root).isDir() || QDir().mkpath(root)) && QFileInfo(root).isWritable())
            return root;
    }
    return own;
}

QString get_cache_dir()
{
    QString dir_path = QSP::writableLocation(QSP::CacheLocation);
    remove_orgname(dir_path);
    create_dir_if_not_exists(dir_path);
    return dir_path;
}

} // namespace


namespace paths {

QString homePath()
{
    static const QString home_path = [](){
        const auto env = QProcessEnvironment::systemEnvironment();

#ifdef Q_OS_WIN32
        // allow overriding the home directory on Windows:
        // QDir::homePath() checks the env vars last on this platform,
        // but we want it to be the first
        return env.value("PEGASUS_HOME", env.value("HOME", QDir::homePath()));
#else
        // on other platforms, QDir::homePath() returns $HOME first
        return env.value(QStringLiteral("PEGASUS_HOME"), QDir::homePath());
#endif
    }();
    return home_path;
}

QString app_dir_path()
{
#if defined(Q_OS_ANDROID)
    // On Android it is on a system partition
    return QString();

#elif defined(Q_OS_MACOS)
    // On Mac the executable is in an app bundle, under `Pegasus.app/Contents/MacOS/`
    return QCoreApplication::applicationDirPath() + QStringLiteral("/../../../");

#else
    return QCoreApplication::applicationDirPath();
#endif
}

const QStringList& configDirs()
{
    static const QStringList config_dir_paths = [](){
        QStringList paths(QLatin1String(":"));
        paths << app_dir_path();

        const QString local_config_dir = app_dir_path() + QStringLiteral("/config");
        if (QFileInfo::exists(local_config_dir))
            paths << local_config_dir;

        if (!AppSettings::general.portable) {
            paths << writableConfigDir();
            paths << QSP::standardLocations(QSP::AppConfigLocation);
            paths << QSP::standardLocations(QSP::AppDataLocation);

            // do not add the organization name to the search path
            const QRegularExpression regex(QStringLiteral("(/pegasus-frontend){2}$"));
            paths.replaceInStrings(regex, QStringLiteral("/pegasus-frontend"));
        }

#ifdef Q_OS_ANDROID
        const QStringList all_roots = android::storage_paths();
        for (const QString& storage_root : all_roots) {
            QString path = storage_root + QStringLiteral("/pegasus-frontend");
            if (QFileInfo::exists(path))
                paths << std::move(path);
        }
#endif

        paths.removeDuplicates();
        return paths;
    }();

    return config_dir_paths;
}

QString writableConfigDir()
{
    static const QString config_dir = get_appconfig_dir();
    return config_dir;
}

QString privateConfigDir()
{
    static const QString dir = get_private_config_dir();
    return dir;
}

QString pegasusxRootSetting()
{
    QFile file(privateConfigDir() + QLatin1Char('/') + ROOT_FILE);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QDir::cleanPath(QString::fromUtf8(file.readAll()).trimmed());
}

bool setPegasusxRootSetting(const QString& dir)
{
    const QString path = privateConfigDir() + QLatin1Char('/') + ROOT_FILE;
    if (dir.isEmpty())
        return !QFileInfo::exists(path) || QFile::remove(path);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    file.write(QDir::cleanPath(dir).toUtf8() + '\n');
    return true;
}

QString writableCacheDir()
{
    static const QString cache_dir = get_cache_dir();
    return cache_dir;
}

} // namespace paths
