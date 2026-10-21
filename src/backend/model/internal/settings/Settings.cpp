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
#include "model/gaming/Collection.h"
#include "model/gaming/CollectionListModel.h"
#include "model/gaming/Game.h"
#include "model/gaming/GameFile.h"
#include "model/gaming/GameFileListModel.h"
#include "model/gaming/GameListModel.h"
#include "utils/PathTools.h"

#ifdef Q_OS_ANDROID
#include "platform/AndroidHelpers.h"
#endif

#ifdef WITH_COMPAT_ESDE
#include "providers/esde/EsdeProvider.h"
#include "providers/LibraryRules.h"
#endif

#include <QCursor>
#include <algorithm>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QThreadPool>
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

// The Emulators screen's background work: one thread, so changes are written
// in the order they were made and a read sees the changes before it
QThreadPool* emulator_pool()
{
    static QThreadPool* pool = [] {
        QThreadPool* p = new QThreadPool;
        p->setMaxThreadCount(1);
        p->setExpiryTimeout(-1);
        return p;
    }();
    return pool;
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
            { QStringLiteral("importedFiles"), sys.imported_files },
        });
    }
#endif
    return out;
}

bool Settings::esdeAppsAsAndroid() const
{
    const QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    return file.value(QStringLiteral("esdeImport/appsAsAndroid"), true).toBool();
}

void Settings::setEsdeAppsAsAndroid(bool value)
{
    QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    file.setValue(QStringLiteral("esdeImport/appsAsAndroid"), value);
    emit esdeImportDirChanged();
}

bool Settings::esdeGamesAsAndroid() const
{
    const QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    return file.value(QStringLiteral("esdeImport/gamesAsAndroid"), true).toBool();
}

void Settings::setEsdeGamesAsAndroid(bool value)
{
    QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    file.setValue(QStringLiteral("esdeImport/gamesAsAndroid"), value);
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
            { QStringLiteral("isAndroid"), coll.is_android },
            { QStringLiteral("options"), options },
        });
    }
#endif
    return out;
}

void Settings::requestEmulatorCollections()
{
    QtConcurrent::run(emulator_pool(), [this]{
        const QVariantList list = emulatorCollections();
        QMetaObject::invokeMethod(this, [this, list]{
            m_emu_cache = list;
            emit emulatorCollectionsReady(list);
        }, Qt::QueuedConnection);
    });
}

namespace {
// the Game / Application / Emulator tag Settings > Library gave an Android entry
QString android_kind_tag(const model::Game& game)
{
    for (const QString& tag : game.tagListConst()) {
        if (tag == QLatin1String("Game") || tag == QLatin1String("Application") || tag == QLatin1String("Emulator"))
            return tag;
    }
    return {};
}
} // namespace

QVariantList Settings::collectionGames(QObject* collections, const QString& collection_name) const
{
    QVariantList out;
    // CollectionListModel has no Q_OBJECT of its own: use the generic list
    const auto* const list = qobject_cast<model::ObjectListModel*>(collections);
    if (!list)
        return out;
    for (int i = 0; i < list->count(); i++) {
        auto* const coll = qobject_cast<model::Collection*>(list->get(i));
        if (!coll || coll->name() != collection_name)
            continue;
        const std::vector<model::Game*>& games = coll->gameList()->entries();
        out.reserve(out.size() + static_cast<int>(games.size()));
        for (model::Game* const game : games) {
            const std::vector<model::GameFile*>& files = game->filesModel()->entries();
            if (files.empty())
                continue;
            out.append(QVariantMap {
                { QStringLiteral("title"), game->title() },
                { QStringLiteral("path"), files.front()->path() },
                { QStringLiteral("key"), providers::library::game_key(*game) },
                { QStringLiteral("source"), providers::library::source_of(*game) },
                { QStringLiteral("kind"), android_kind_tag(*game) },
                { QStringLiteral("package"), providers::library::game_key(*game).startsWith(QLatin1String("android:"))
                    ? providers::library::game_key(*game).mid(8) : QString() },
                { QStringLiteral("obj"), QVariant::fromValue(static_cast<QObject*>(game)) },
            });
        }
    }
    return out;
}

void Settings::requestCollectionDetails(const QString& collection_name, bool android)
{
    const QVariantList cached_apps = m_android_apps_cache;
    QtConcurrent::run(emulator_pool(), [this, collection_name, android, cached_apps]{
        QVariantMap details;
        details.insert(QStringLiteral("gameCommands"), gameEmulators(collection_name));
        if (android) {
            details.insert(QStringLiteral("gameApps"), androidGameApps(collection_name));
            details.insert(QStringLiteral("apps"), cached_apps.isEmpty() ? androidApps() : cached_apps);
        }
        QVariantMap original { { QStringLiteral("known"), false }, { QStringLiteral("command"), QString() } };
        QVariantMap game_originals;
#ifdef WITH_COMPAT_ESDE
        const providers::esde::OriginalLaunches originals = providers::esde::original_launches(collection_name);
        original.insert(QStringLiteral("known"), originals.collection_known);
        original.insert(QStringLiteral("command"), originals.collection_command);
        for (const auto& game : originals.games)
            game_originals.insert(game.path, game.command);
#endif
        details.insert(QStringLiteral("original"), original);
        details.insert(QStringLiteral("gameOriginals"), game_originals);

        QMetaObject::invokeMethod(this, [this, collection_name, details]{
            if (details.contains(QStringLiteral("apps")))
                m_android_apps_cache = details.value(QStringLiteral("apps")).toList();
            emit collectionDetailsReady(collection_name, details);
        }, Qt::QueuedConnection);
    });
}

void Settings::saveEmulator(const QString& collection_name, const QString& game_path,
                            const QString& game_title, const QString& command)
{
    QtConcurrent::run(emulator_pool(), [this, collection_name, game_path, game_title, command]{
        const QString error = game_path.isEmpty()
            ? setCollectionEmulator(collection_name, command)
            : setGameEmulator(collection_name, game_path, game_title, command);
        QMetaObject::invokeMethod(this, [this, collection_name, game_path, error]{
            emit emulatorSaved(collection_name, game_path, error);
        }, Qt::QueuedConnection);
    });
}

void Settings::requestAndroidGames(const QString& action, const QString& collection_name)
{
    QtConcurrent::run(emulator_pool(), [this, action, collection_name]{
        const int count = action == QLatin1String("add")
            ? addAndroidGames(collection_name)
            : matchAndroidGames(collection_name);
        QMetaObject::invokeMethod(this, [this, action, collection_name, count]{
            emit androidGamesDone(action, collection_name, count);
        }, Qt::QueuedConnection);
    });
}

void Settings::setLiveLaunchCommands(const QVariantList& games, const QString& command)
{
    for (const QVariant& game : games)
        setLiveLaunchCommand(game.value<QObject*>(), command);
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

QVariantList Settings::androidApps() const
{
    QVariantList out;
#ifdef WITH_COMPAT_ESDE
    for (const auto& app : providers::esde::android_apps()) {
        out.append(QVariantMap {
            { QStringLiteral("label"), app.label },
            { QStringLiteral("package"), app.package },
            { QStringLiteral("component"), app.component },
            { QStringLiteral("game"), app.game },
        });
    }
#endif
    return out;
}

QVariantMap Settings::androidGameApps(const QString& collection_name) const
{
    QVariantMap out;
#ifdef WITH_COMPAT_ESDE
    for (const auto& entry : providers::esde::android_game_apps(collection_name))
        out.insert(entry.first, entry.second);
#else
    Q_UNUSED(collection_name);
#endif
    return out;
}

QString Settings::androidAppCommand(const QString& component) const
{
#ifdef WITH_COMPAT_ESDE
    return providers::esde::android_app_command(component);
#else
    Q_UNUSED(component);
    return {};
#endif
}

int Settings::matchAndroidGames(const QString& collection_name)
{
#ifdef WITH_COMPAT_ESDE
    return providers::esde::match_android_games(collection_name);
#else
    Q_UNUSED(collection_name);
    return 0;
#endif
}

int Settings::addAndroidGames(const QString& collection_name)
{
#ifdef WITH_COMPAT_ESDE
    return providers::esde::add_android_games(collection_name);
#else
    Q_UNUSED(collection_name);
    return 0;
#endif
}

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

QVariantMap Settings::originalLaunch(const QString& collection_name, const QString& game_path) const
{
#ifdef WITH_COMPAT_ESDE
    const providers::esde::OriginalLaunch original = providers::esde::original_launch(collection_name, game_path);
    return QVariantMap {
        { QStringLiteral("known"), original.known },
        { QStringLiteral("command"), original.command },
    };
#else
    Q_UNUSED(collection_name);
    Q_UNUSED(game_path);
    return QVariantMap { { QStringLiteral("known"), false }, { QStringLiteral("command"), QString() } };
#endif
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
    // Android apps always go into "Android"; Android Games keeps its own
    // collection, which Settings > Library merges into Android (or not)
    const QStringList android_systems { QStringLiteral("androidapps") };

    // the work runs in the background, so the screen can show its progress
    QtConcurrent::run([this, esde_dir, system_names, android_systems]{
        const auto result = providers::esde::esde_import(esde_dir, system_names, android_systems,
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

bool Settings::mergeAndroid() const
{
    return providers::library::merge_android_enabled();
}

void Settings::setMergeAndroid(bool value)
{
    providers::library::set_merge_android_enabled(value);
    emit libraryRulesChanged();
}

QVariantMap Settings::hiddenItems() const
{
    const providers::library::HiddenList list = providers::library::load_hidden();
    QVariantList games;
    for (const auto& entry : list.games) {
        games.append(QVariantMap {
            { QStringLiteral("path"), entry.path },
            { QStringLiteral("title"), entry.title },
            { QStringLiteral("collections"), entry.collections },
        });
    }
    return QVariantMap {
        { QStringLiteral("collections"), list.collections },
        { QStringLiteral("games"), games },
    };
}

QString Settings::gameKey(QObject* game) const
{
    const auto* const game_ptr = qobject_cast<model::Game*>(game);
    return game_ptr ? providers::library::game_key(*game_ptr) : QString();
}

void Settings::setCollectionHidden(const QString& name, bool hidden)
{
    providers::library::HiddenList list = providers::library::load_hidden();
    list.collections.removeAll(name);
    if (hidden)
        list.collections.append(name);
    providers::library::save_hidden(list);
    emit libraryRulesChanged();
}

void Settings::setGameHidden(const QString& path, const QString& title,
                             const QStringList& collections, bool hidden)
{
    if (path.isEmpty())
        return;
    providers::library::HiddenList list = providers::library::load_hidden();
    list.games.erase(std::remove_if(list.games.begin(), list.games.end(),
        [&path](const providers::library::HiddenGame& entry) { return entry.path == path; }), list.games.end());
    if (hidden) {
        providers::library::HiddenGame entry;
        entry.path = path;
        entry.title = title;
        entry.collections = collections;
        list.games.push_back(std::move(entry));
    }
    providers::library::save_hidden(list);
    emit libraryRulesChanged();
}

int Settings::deleteEsdeImport(const QStringList& folders)
{
#ifdef WITH_COMPAT_ESDE
    return providers::esde::esde_delete_import(folders);
#else
    Q_UNUSED(folders);
    return 0;
#endif
}

QVariantMap Settings::androidKinds() const
{
    return providers::library::load_kinds();
}

void Settings::setAndroidKind(const QString& key, const QString& kind)
{
    providers::library::set_kind(key, kind);
    emit libraryRulesChanged();
}

void Settings::reloadLibrary()
{
    Log::info(LOGMSG("Reloading the library..."));
    emit libraryReloadRequested();
}

void Settings::reloadProviders()
{
    Log::info(LOGMSG("Reloading..."));
    emit providerReloadingRequested();
}

} // namespace model
