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

QStringList Settings::libraryMediaDirs() const
{
    return pegasusx::libraryMediaDirs();
}

QStringList Settings::esdeImports() const
{
    return pegasusx::esdeImports();
}

QStringList Settings::ignoredDirs() const
{
    return pegasusx::ignoredDirs();
}



namespace {
/// The list without the entries at the given indices
QStringList remove_indices(QStringList list, const QVariantList& indices)
{
    std::vector<int> to_remove;
    for (const QVariant& index : indices) {
        bool ok = false;
        const int idx = index.toInt(&ok);
        if (ok && 0 <= idx && idx < list.size())
            to_remove.push_back(idx);
    }
    // remove from the back, so the other indices stay valid
    std::sort(to_remove.rbegin(), to_remove.rend());
    to_remove.erase(std::unique(to_remove.begin(), to_remove.end()), to_remove.end());
    for (const int idx : to_remove)
        list.removeAt(idx);
    return list;
}
} // namespace

void Settings::addLibraryRomDir(const QString& picked)
{
    // a picked metadata file adds its folder
    const QString path = QFileInfo(picked).isFile() ? QFileInfo(picked).absolutePath() : picked;
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
    const QStringList before = pegasusx::libraryRomDirs();
    const QStringList after = remove_indices(before, indices);
    if (after.size() == before.size())
        return;
    pegasusx::setLibraryRomDirs(after);
    emit libraryChanged();
}

void Settings::addLibraryMediaDir(const QString& path)
{
    if (!QFileInfo(path).isDir()) {
        Log::warning(LOGMSG("Media folder `%1` not found, ignored").arg(path));
        return;
    }
    // kept in the order added (not sorted): a later folder wins over an earlier one
    QStringList dirs = pegasusx::libraryMediaDirs();
    const QString clean = ::pretty_path(path);
    dirs.removeAll(clean);
    dirs.append(clean);
    pegasusx::setLibraryMediaDirs(dirs);
    emit libraryChanged();
}

void Settings::removeLibraryMediaDirs(const QVariantList& indices)
{
    const QStringList before = pegasusx::libraryMediaDirs();
    const QStringList after = remove_indices(before, indices);
    if (after.size() == before.size())
        return;
    pegasusx::setLibraryMediaDirs(after);
    emit libraryChanged();
}

void Settings::addEsdeImport(const QString& path)
{
    if (!QFileInfo::exists(path)) {
        Log::warning(LOGMSG("ES-DE folder `%1` not found, ignored").arg(path));
        return;
    }
    QStringList paths = pegasusx::esdeImports();
    const QString clean = ::pretty_path(path);
    if (paths.contains(clean))
        return;
    paths.append(clean);
    pegasusx::setEsdeImports(paths);
    emit libraryChanged();
}

void Settings::removeEsdeImports(const QVariantList& indices)
{
    const QStringList before = pegasusx::esdeImports();
    const QStringList after = remove_indices(before, indices);
    if (after.size() == before.size())
        return;
    pegasusx::setEsdeImports(after);
    emit libraryChanged();
}

void Settings::addIgnoredDir(const QString& picked)
{
    // a picked file ignores its folder
    const QString path = QFileInfo(picked).isFile() ? QFileInfo(picked).absolutePath() : picked;
    if (!QFileInfo(path).isDir()) {
        Log::warning(LOGMSG("Folder `%1` not found, ignored").arg(path));
        return;
    }
    QStringList dirs = pegasusx::ignoredDirs();
    const QString clean = ::pretty_path(path);
    if (dirs.contains(clean))
        return;
    dirs.append(clean);
    dirs.sort();
    pegasusx::setIgnoredDirs(dirs);
    emit libraryChanged();
}

void Settings::removeIgnoredDirs(const QVariantList& indices)
{
    const QStringList before = pegasusx::ignoredDirs();
    const QStringList after = remove_indices(before, indices);
    if (after.size() == before.size())
        return;
    pegasusx::setIgnoredDirs(after);
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
                { QStringLiteral("retroarch"), option.retroarch },
                { QStringLiteral("custom"), option.custom },
                { QStringLiteral("command"), option.command },
            });
        }
        out.append(QVariantMap {
            { QStringLiteral("name"), system.name },
            { QStringLiteral("fullname"), system.fullname },
            { QStringLiteral("current"), system.current },
            { QStringLiteral("defaultLabel"), system.default_label },
            { QStringLiteral("options"), options },
            { QStringLiteral("collectionName"), system.collection_name },
            { QStringLiteral("metadataName"), system.metadata_name },
            { QStringLiteral("nameSource"), system.name_source },
            { QStringLiteral("shortName"), system.short_name },
            { QStringLiteral("status"), system.status },
            { QStringLiteral("folders"), system.folders },
            { QStringLiteral("artSources"), system.art_sources },
            { QStringLiteral("ignored"), system.ignored },
        });
    }
#endif
    return out;
}

void Settings::setSystemEmulator(const QString& system_name, const QString& label)
{
    pegasusx::setSystemEmulator(system_name, label);
}

QVariantList Settings::installedApps() const
{
    QVariantList out;
#ifdef Q_OS_ANDROID
    for (const QString& entry : android::launchable_apps()) {
        const int tab = entry.lastIndexOf(QLatin1Char('\t'));
        if (tab <= 0)
            continue;
        out.append(QVariantMap {
            { QStringLiteral("label"), entry.left(tab) },
            { QStringLiteral("package"), entry.mid(tab + 1) },
        });
    }
#endif
    return out;
}

QStringList Settings::hiddenGames() const
{
    return pegasusx::hiddenGames();
}

void Settings::hideGame(const QString& path)
{
    QStringList paths = pegasusx::hiddenGames();
    const QString clean = ::pretty_path(path);
    if (clean.isEmpty() || paths.contains(clean))
        return;
    paths.append(clean);
    pegasusx::setHiddenGames(paths);
    emit libraryChanged();
}

void Settings::unhideGame(const QString& path)
{
    QStringList paths = pegasusx::hiddenGames();
    if (paths.removeAll(path) == 0)
        return;
    pegasusx::setHiddenGames(paths);
    emit libraryChanged();
}

void Settings::removeIgnoredDir(const QString& path)
{
    QStringList dirs = pegasusx::ignoredDirs();
    if (dirs.removeAll(path) == 0)
        return;
    pegasusx::setIgnoredDirs(dirs);
    emit libraryChanged();
}

void Settings::saveCustomEmulator(const QString& system_name, const QString& old_name,
                                  const QString& name, const QString& command)
{
    const QString clean_name = name.trimmed();
    if (system_name.isEmpty() || clean_name.isEmpty() || command.trimmed().isEmpty())
        return;

    QVariantList list = pegasusx::customEmulators(system_name);
    QVariantList out;
    for (const QVariant& entry : list) {
        const QString entry_name = entry.toMap().value(QStringLiteral("name")).toString();
        if (entry_name != old_name && entry_name != clean_name)
            out.append(entry);
    }
    out.append(QVariantMap {
        { QStringLiteral("name"), clean_name },
        { QStringLiteral("command"), command.trimmed() },
    });
    pegasusx::setCustomEmulators(system_name, out);

    // a renamed emulator that was picked stays picked
    if (!old_name.isEmpty() && pegasusx::systemEmulator(system_name) == QStringLiteral("@custom:") + old_name)
        pegasusx::setSystemEmulator(system_name, QStringLiteral("@custom:") + clean_name);
    emit libraryChanged();
}

void Settings::removeCustomEmulator(const QString& system_name, const QString& name)
{
    QVariantList out;
    for (const QVariant& entry : pegasusx::customEmulators(system_name)) {
        if (entry.toMap().value(QStringLiteral("name")).toString() != name)
            out.append(entry);
    }
    pegasusx::setCustomEmulators(system_name, out);
    if (pegasusx::systemEmulator(system_name) == QStringLiteral("@custom:") + name)
        pegasusx::setSystemEmulator(system_name, QString());
    emit libraryChanged();
}

void Settings::setCollectionNameSource(const QString& system_name, const QString& source)
{
    pegasusx::setCollectionNameSource(system_name, source);
    emit libraryChanged();
}

QVariantMap Settings::testEmulatorCommand(const QString& system_name, const QString& command) const
{
#ifdef WITH_COMPAT_ESDE
    const auto result = providers::esde::test_emulator(system_name, command);
    return QVariantMap {
        { QStringLiteral("game"), result.game },
        { QStringLiteral("error"), result.error },
    };
#else
    Q_UNUSED(system_name);
    Q_UNUSED(command);
    return QVariantMap { { QStringLiteral("error"), QStringLiteral("Not supported on this platform") } };
#endif
}

QString Settings::openSystemEmulator(const QString& system_name) const
{
#ifdef WITH_COMPAT_ESDE
    return providers::esde::open_emulator(system_name);
#else
    Q_UNUSED(system_name);
    return QStringLiteral("Not supported on this platform");
#endif
}

QString Settings::appGameActivity(const QString& package_name) const
{
#ifdef Q_OS_ANDROID
    return ::android::find_game_activity(package_name);
#else
    Q_UNUSED(package_name);
    return QString();
#endif
}

QString Settings::screenAspect() const
{
    return pegasusx::screenAspect();
}

void Settings::setScreenAspect(const QString& aspect)
{
    if (aspect == pegasusx::screenAspect())
        return;
    pegasusx::setScreenAspect(aspect);
    emit screenAspectChanged();
}

QVariantMap Settings::testSystemEmulator(const QString& system_name) const
{
#ifdef WITH_COMPAT_ESDE
    const auto result = providers::esde::test_emulator(system_name);
    return QVariantMap {
        { QStringLiteral("game"), result.game },
        { QStringLiteral("error"), result.error },
    };
#else
    Q_UNUSED(system_name);
    return QVariantMap { { QStringLiteral("error"), QStringLiteral("Not supported on this platform") } };
#endif
}

} // namespace model
