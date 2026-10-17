// Pegasus Frontend
// Copyright (C) 2017  Mátyás Mustoha
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


#include "Settings.h"

#include "AppSettings.h"
#include "Log.h"
#include "Paths.h"
#include "PegasusXSettings.h"
#include "utils/PathTools.h"

#ifdef Q_OS_ANDROID
#include "platform/AndroidHelpers.h"
#endif
#ifdef WITH_COMPAT_ESDE
#include "providers/esde/EsdeProvider.h"
#endif

#include <QCursor>
#include <QVariantMap>
#include <algorithm>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSet>
#include <QTextStream>

#ifdef Q_OS_ANDROID
#include <QtAndroid>
#include <QtAndroidExtras/QAndroidIntent>
#endif


namespace {

void rewrite_gamedircfg(const std::function<void(QTextStream&)>& callback)
{
    const QString config_file_path = paths::writableConfigDir() + QStringLiteral("/game_dirs.txt");

    QFile config_file(config_file_path);
    if (!config_file.open(QFile::WriteOnly | QFile::Text)) {
        Log::warning(LOGMSG("Failed to save game directory settings to `%1`").arg(config_file_path));
        return;
    }

    QTextStream stream(&config_file);
    stream.setCodec("UTF-8");
    callback(stream);
    Log::info(LOGMSG("Game directory list saved"));
}

void change_mouse_support(bool enabled)
{
    if (enabled)
        QGuiApplication::restoreOverrideCursor();
    else
        QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
}

} // namespace


namespace model {

Settings::Settings(QObject* parent)
    : QObject(parent)
{}

void Settings::postInit()
{
    change_mouse_support(AppSettings::general.mouse_support);

    m_themes.postInit();
}

void Settings::setFullscreen(bool new_val)
{
    if (new_val == AppSettings::general.fullscreen)
        return;

    AppSettings::general.fullscreen = new_val;
    AppSettings::save_config();

    emit fullscreenChanged();
}

void Settings::setMouseSupport(bool new_val)
{
    if (new_val == AppSettings::general.mouse_support)
        return;

    AppSettings::general.mouse_support = new_val;
    AppSettings::save_config();

    change_mouse_support(AppSettings::general.mouse_support);

    emit mouseSupportChanged();
}

void Settings::setVerifyFiles(bool new_val)
{
    if (new_val == AppSettings::general.verify_files)
        return;

    AppSettings::general.verify_files = new_val;
    AppSettings::save_config();

    emit verifyFilesChanged();
}

void Settings::setScanOnLaunch(bool new_val)
{
    if (new_val == AppSettings::general.scan_on_launch)
        return;

    AppSettings::general.scan_on_launch = new_val;
    AppSettings::save_config();

    emit scanOnLaunchChanged();
}

void Settings::setShowMissingGames(bool new_val)
{
    if (new_val == AppSettings::general.show_missing_games)
        return;

    AppSettings::general.show_missing_games = new_val;
    AppSettings::save_config();

    emit showMissingGamesChanged();
}

QStringList Settings::gameDirs() const
{
    QSet<QString> dirset;
    AppSettings::parse_gamedirs([&dirset](const QString& line){
        dirset.insert(::pretty_path(line));
    });

    QStringList dirlist;
    for (const QString& dir : qAsConst(dirset))
        dirlist.append(dir);

    dirlist.sort();
    return dirlist;
}

void Settings::addGameDir(const QString& path)
{
    const QFileInfo finfo(path);
    if (!finfo.exists() || !finfo.isDir()) {
        Log::warning(LOGMSG("Game directory `%1` not found, ignored").arg(path));
        return;
    }

    QSet<QString> dirset;
    AppSettings::parse_gamedirs([&dirset](const QString& line){
        dirset.insert(::pretty_path(line));
    });


    const auto count_before = dirset.count();
    dirset << ::pretty_path(finfo);
    const auto count_after = dirset.count();

    if (count_before == count_after) {
        Log::warning(LOGMSG("Game directory `%1` already known, ignored").arg(path));
        return;
    }


    rewrite_gamedircfg([&dirset](QTextStream& stream){
        for (const QString& dir : qAsConst(dirset)) {
            stream << dir << '\n';
        }
    });

    emit gameDirsChanged();
}

void Settings::removeGameDirs(const QVariantList& idx_var_list)
{
    bool changed = false;

    std::vector<int> idx_list;
    for (const QVariant& idx_var : idx_var_list) {
        bool valid_int = false;
        const int idx = idx_var.toInt(&valid_int);
        if (valid_int)
            idx_list.push_back(idx);
    }

    idx_list.erase(std::unique(idx_list.begin(), idx_list.end()), idx_list.end());
    std::sort(idx_list.rbegin(), idx_list.rend()); // to remove the largest index first


    auto dirlist = gameDirs();
    for (const int idx : idx_list) {
        if (idx < 0 || dirlist.count() <= idx)
            continue;

        dirlist.removeAt(idx);
        changed = true;
    }
    if (!changed)
        return;


    rewrite_gamedircfg([&dirlist](QTextStream& stream){
        for (const QString& dir : qAsConst(dirlist)) {
            stream << dir << '\n';
        }
    });

    emit gameDirsChanged();
}

QStringList Settings::androidGrantedDirs() const
{
#ifdef Q_OS_ANDROID
    return android::granted_paths();
#else
    return {};
#endif
}

void Settings::requestAndroidDir()
{
#ifdef Q_OS_ANDROID
    android::request_saf_permission([this](){
        emit androidDirsChanged();
    });
#endif
}

void Settings::reloadProviders()
{
    Log::info(LOGMSG("Reloading..."));
    emit providerReloadingRequested();
}


bool Settings::pegasusxLibrarySupported() const
{
#ifdef WITH_COMPAT_ESDE
    return true;
#else
    return false;
#endif
}

QStringList Settings::libraryRomDirs() const
{
    return pegasusx::libraryRomDirs();
}

QString Settings::libraryMediaDir() const
{
    return pegasusx::libraryMediaDir();
}

void Settings::addLibraryRomDir(const QString& path)
{
    const QFileInfo finfo(path);
    if (!finfo.exists() || !finfo.isDir()) {
        Log::warning(LOGMSG("ROM folder `%1` not found, ignored").arg(path));
        return;
    }

    QStringList dirs = pegasusx::libraryRomDirs();
    const QString clean = ::pretty_path(path);
    if (dirs.contains(clean))
        return;

    dirs.append(clean);
    dirs.sort();
    pegasusx::setLibraryRomDirs(dirs);
    emit libraryChanged();
}

void Settings::removeLibraryRomDirs(const QVariantList& indices)
{
    QStringList dirs = pegasusx::libraryRomDirs();
    std::vector<int> to_remove;
    for (const QVariant& index : indices) {
        bool ok = false;
        const int idx = index.toInt(&ok);
        if (ok && 0 <= idx && idx < dirs.size())
            to_remove.push_back(idx);
    }
    if (to_remove.empty())
        return;

    // remove from the back, so the other indices stay valid
    std::sort(to_remove.rbegin(), to_remove.rend());
    to_remove.erase(std::unique(to_remove.begin(), to_remove.end()), to_remove.end());
    for (const int idx : to_remove)
        dirs.removeAt(idx);

    pegasusx::setLibraryRomDirs(dirs);
    emit libraryChanged();
}

void Settings::setLibraryMediaDir(const QString& path)
{
    if (!path.isEmpty() && !QFileInfo(path).isDir()) {
        Log::warning(LOGMSG("Media folder `%1` not found, ignored").arg(path));
        return;
    }
    pegasusx::setLibraryMediaDir(path.isEmpty() ? QString() : ::pretty_path(path));
    emit libraryChanged();
}

QVariantList Settings::emulatorSystems() const
{
    QVariantList out;
#ifdef WITH_COMPAT_ESDE
    for (const auto& system : providers::esde::emulator_choices()) {
        QVariantList options;
        for (const auto& option : system.options) {
            options.append(QVariantMap {
                { QStringLiteral("label"), option.label },
                { QStringLiteral("installed"), option.installed },
            });
        }
        out.append(QVariantMap {
            { QStringLiteral("name"), system.name },
            { QStringLiteral("fullname"), system.fullname },
            { QStringLiteral("current"), system.current },
            { QStringLiteral("defaultLabel"), system.default_label },
            { QStringLiteral("options"), options },
        });
    }
#endif
    return out;
}

void Settings::setSystemEmulator(const QString& system_name, const QString& label)
{
    pegasusx::setSystemEmulator(system_name, label);
}

} // namespace model
