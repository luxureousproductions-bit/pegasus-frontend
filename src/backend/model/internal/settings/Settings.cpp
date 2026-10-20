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
#include "model/gaming/Game.h"
#include "utils/PathTools.h"

#ifdef Q_OS_ANDROID
#include "platform/AndroidHelpers.h"
#endif

#ifdef WITH_COMPAT_ESDE
#include "providers/esde/EsdeProvider.h"
#endif

#include <QCursor>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QtConcurrent/QtConcurrent>
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

namespace {
// Pegasus X: settings of its own features, kept apart from Pegasus' own
QString pegasusx_settings_path()
{
    return paths::writableConfigDir() + QStringLiteral("/pegasusx.ini");
}
} // namespace

bool Settings::esdeImportSupported() const
{
#ifdef WITH_COMPAT_ESDE
    return true;
#else
    return false;
#endif
}

QString Settings::esdeImportDir() const
{
    const QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    const QString saved = file.value(QStringLiteral("esdeImport/dir")).toString();
#ifdef WITH_COMPAT_ESDE
    if (saved.isEmpty() || !QFileInfo(saved).isDir())
        return providers::esde::esde_default_dir();
#endif
    return saved;
}

void Settings::setEsdeImportDir(const QString& dir)
{
    QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    file.setValue(QStringLiteral("esdeImport/dir"), dir.isEmpty() ? QString() : ::pretty_path(dir));
    emit esdeImportDirChanged();
}

QVariantList Settings::esdeImportSystems() const
{
    QVariantList out;
#ifdef WITH_COMPAT_ESDE
    for (const auto& sys : providers::esde::esde_import_systems(esdeImportDir())) {
        out.append(QVariantMap {
            { QStringLiteral("name"), sys.name },
            { QStringLiteral("fullname"), sys.fullname },
            { QStringLiteral("shortName"), sys.short_name },
            { QStringLiteral("games"), sys.games },
            { QStringLiteral("hasGamelist"), sys.has_gamelist },
            { QStringLiteral("hasMedia"), sys.has_media },
            { QStringLiteral("folders"), sys.folders },
        });
    }
#endif
    return out;
}

bool Settings::esdeCombineAndroid() const
{
    const QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    return file.value(QStringLiteral("esdeImport/combineAndroid"), true).toBool();
}

void Settings::setEsdeCombineAndroid(bool value)
{
    QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    file.setValue(QStringLiteral("esdeImport/combineAndroid"), value);
    emit esdeImportDirChanged();
}

QVariantList Settings::emulatorCollections() const
{
    QVariantList out;
#ifdef WITH_COMPAT_ESDE
    for (const auto& coll : providers::esde::emulator_collections()) {
        QVariantList options;
        for (const auto& option : coll.options) {
            options.append(QVariantMap {
                { QStringLiteral("label"), option.label },
                { QStringLiteral("command"), option.command },
                { QStringLiteral("installed"), option.installed },
                { QStringLiteral("retroarch"), option.retroarch },
                { QStringLiteral("recommended"), option.recommended },
            });
        }
        out.append(QVariantMap {
            { QStringLiteral("name"), coll.name },
            { QStringLiteral("shortName"), coll.short_name },
            { QStringLiteral("system"), coll.system },
            { QStringLiteral("systemName"), coll.system_fullname },
            { QStringLiteral("command"), coll.command },
            { QStringLiteral("currentLabel"), coll.current_label },
            { QStringLiteral("currentInstalled"), coll.current_installed },
            { QStringLiteral("missingApp"), coll.missing_app },
            { QStringLiteral("files"), coll.files },
            { QStringLiteral("gameOverrides"), coll.game_overrides },
            { QStringLiteral("options"), options },
        });
    }
#endif
    return out;
}

void Settings::requestEmulatorCollections()
{
    QtConcurrent::run([this]{
        const QVariantList list = emulatorCollections();
        emit emulatorCollectionsReady(list);  // queued to the screen
    });
}

void Settings::requestEsdeImportSystems()
{
    QtConcurrent::run([this]{
        const QVariantList list = esdeImportSystems();
        emit esdeImportSystemsReady(list);
    });
}

void Settings::setLiveLaunchCommand(QObject* game, const QString& command)
{
    auto* const game_ptr = qobject_cast<model::Game*>(game);
    if (game_ptr)
        game_ptr->setLaunchCmd(command);
}

namespace {
// ES-DE's Android configuration, from its repository
const QString ESDE_LISTS_URL = QStringLiteral(
    "https://gitlab.com/es-de/emulationstation-de/-/raw/master/resources/systems/android/");

QString esde_lists_dir()
{
    return paths::writableConfigDir() + QStringLiteral("/esde-lists");
}
} // namespace

QString Settings::emulatorListInfo() const
{
    const QFileInfo finfo(esde_lists_dir() + QStringLiteral("/es_systems.xml"));
    if (!finfo.exists())
        return tr("Built into this version of Pegasus X");
    return tr("Downloaded %1").arg(QLocale().toString(finfo.lastModified().date(), QLocale::ShortFormat));
}

void Settings::updateEmulatorList()
{
    if (!m_netman)
        m_netman = new QNetworkAccessManager(this);

    // both files are checked before either is kept, so a failed or partial
    // download never replaces a working list
    auto* const results = new QHash<QString, QByteArray>();
    auto* const pending = new int(2);
    const QStringList names { QStringLiteral("es_systems.xml"), QStringLiteral("es_find_rules.xml") };
    for (const QString& name : names) {
        QNetworkRequest request(QUrl(ESDE_LISTS_URL + name));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* const reply = m_netman->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, name, results, pending]{
            if (reply->error() == QNetworkReply::NoError)
                results->insert(name, reply->readAll());
            else
                Log::warning(LOGMSG("Emulator list: could not download %1: %2").arg(name, reply->errorString()));
            reply->deleteLater();

            if (--(*pending) > 0)
                return;

            const QByteArray systems = results->value(QStringLiteral("es_systems.xml"));
            const QByteArray rules = results->value(QStringLiteral("es_find_rules.xml"));
            delete results;
            delete pending;

            if (systems.isEmpty() || rules.isEmpty()) {
                emit emulatorListUpdated(false, tr("Couldn't download the list. Check the internet connection."));
                return;
            }
            if (!systems.contains("<systemList") || systems.count("<system>") < 100 || !rules.contains("<ruleList")) {
                emit emulatorListUpdated(false, tr("The downloaded list doesn't look right, kept the current one."));
                return;
            }

            QDir().mkpath(esde_lists_dir());
            for (const auto& entry : { qMakePair(QStringLiteral("es_systems.xml"), systems),
                                       qMakePair(QStringLiteral("es_find_rules.xml"), rules) }) {
                QFile file(esde_lists_dir() + QLatin1Char('/') + entry.first);
                if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    emit emulatorListUpdated(false, tr("Couldn't save the list."));
                    return;
                }
                file.write(entry.second);
            }
            Log::info(LOGMSG("Emulator list updated from ES-DE (%1 systems)").arg(systems.count("<system>")));
            emit emulatorListUpdated(true, tr("Updated: %1 systems.").arg(systems.count("<system>")));
        });
    }
}

QVariantMap Settings::gameEmulators(const QString& collection_name) const
{
    QVariantMap out;
#ifdef WITH_COMPAT_ESDE
    for (const auto& entry : providers::esde::game_emulators(collection_name))
        out.insert(entry.path, entry.command);
#else
    Q_UNUSED(collection_name);
#endif
    return out;
}

QString Settings::setCollectionEmulator(const QString& collection_name, const QString& command)
{
#ifdef WITH_COMPAT_ESDE
    return providers::esde::set_collection_emulator(collection_name, command);
#else
    Q_UNUSED(collection_name);
    Q_UNUSED(command);
    return QStringLiteral("Not supported on this platform");
#endif
}

QString Settings::setGameEmulator(const QString& collection_name, const QString& game_path,
                                  const QString& game_title, const QString& command)
{
#ifdef WITH_COMPAT_ESDE
    return providers::esde::set_game_emulator(collection_name, game_path, game_title, command);
#else
    Q_UNUSED(collection_name);
    Q_UNUSED(game_path);
    Q_UNUSED(game_title);
    Q_UNUSED(command);
    return QStringLiteral("Not supported on this platform");
#endif
}

QString Settings::testEmulatorCommand(const QString& command, const QString& game_path) const
{
#ifdef WITH_COMPAT_ESDE
    return providers::esde::test_emulator_command(command, game_path);
#else
    Q_UNUSED(command);
    Q_UNUSED(game_path);
    return QStringLiteral("Not supported on this platform");
#endif
}

void Settings::startEsdeImport(const QStringList& system_names)
{
#ifdef WITH_COMPAT_ESDE
    if (m_esde_import_running)
        return;
    m_esde_import_running = true;
    emit esdeImportRunningChanged();

    const QString esde_dir = esdeImportDir();
    const bool combine_android = esdeCombineAndroid();

    // the work runs in the background, so the screen can show its progress
    QtConcurrent::run([this, esde_dir, system_names, combine_android]{
        const auto result = providers::esde::esde_import(esde_dir, system_names, combine_android,
            [this](int done, int total, const QString& system) {
                emit esdeImportProgress(done, total, system);  // queued to the screen
            });

        // back on the main thread: the folders written to have to be game
        // directories, or Pegasus won't read the new metadata files
        QMetaObject::invokeMethod(this, [this, result]{
            QSet<QString> known;
            AppSettings::parse_gamedirs([&known](const QString& line){
                known.insert(::pretty_path(line));
            });
            for (const QString& folder : result.folders) {
                if (!known.contains(::pretty_path(QFileInfo(folder))))
                    addGameDir(folder);
            }

            m_esde_import_running = false;
            emit esdeImportRunningChanged();
            emit esdeImportFinished(QVariantMap {
                { QStringLiteral("systems"), result.systems },
                { QStringLiteral("games"), result.games },
                { QStringLiteral("files"), result.written_files },
                { QStringLiteral("error"), result.error },
            });
        }, Qt::QueuedConnection);
    });
#else
    Q_UNUSED(system_names);
    emit esdeImportFinished(QVariantMap { { QStringLiteral("error"), QStringLiteral("Not supported on this platform") } });
#endif
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

} // namespace model
