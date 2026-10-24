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

// built on every platform
#include "providers/LibraryRules.h"
#include "ScraperKeys.h"
#include "Scraper.h"
#include "ThemeStore.h"
#include "model/gaming/Assets.h"
// Android only (ES-DE import, emulators, metadata editing)
#ifdef WITH_COMPAT_ESDE
#include "providers/esde/EsdeProvider.h"
#endif

#include <QCursor>
#include <algorithm>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QDateTime>
#include <QSettings>
#include <QFile>
#include <QRegularExpression>
#include <QDir>
#include <QDirIterator>
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

QObject* Settings::themeStore()
{
    if (!m_theme_store) {
        m_theme_store = new ThemeStore(this);
        m_theme_store->setCurrentDirGetter([this]{ return m_themes.currentQmlDir(); });
        // a theme added, updated or removed: Pegasus' list read again; the
        // one in use updated: built again
        connect(m_theme_store, &ThemeStore::themesChanged, this, [this](const QString& dir){
            const bool current = QDir(dir).absolutePath() == QDir(m_themes.currentQmlDir()).absolutePath();
            m_themes.rescan();
            if (current)
                emit currentThemeUpdated();
        });
    }
    return m_theme_store;
}

void Settings::postInit()
{
#ifdef WITH_COMPAT_ESDE
    // apps may have been installed or removed while Pegasus X was away
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [](Qt::ApplicationState state){
        if (state == Qt::ApplicationActive)
            providers::esde::invalidate_emulator_setup();
    });
#endif
#ifdef WITH_COMPAT_ESDE
    // before the library is read: no game in a collection that isn't its own
    providers::esde::split_legacy_edits_file();
#endif
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
        // Android entries open their own apps: they're set in Settings > Library
        if (coll.is_android)
            continue;
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

QVariantMap Settings::gameLaunchInfo(QObject* game_obj) const
{
    const auto* const game = qobject_cast<model::Game*>(game_obj);
    if (!game)
        return {};
    const QString command = game->launchCmd();
    // the app it opens: "-n <package>/<activity>", else the entry is the app
    static const QRegularExpression rx_component(QStringLiteral("-n\\s+([\\w.]+)/"));
    QString package;
    const QRegularExpressionMatch match = rx_component.match(command);
    if (match.hasMatch())
        package = match.captured(1);
    else {
        const QString key = providers::library::game_key(*game);
        if (key.startsWith(QLatin1String("android:")))
            package = key.mid(8);
    }
    return QVariantMap {
        { QStringLiteral("command"), command },
        { QStringLiteral("package"), package },
    };
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
#ifdef WITH_COMPAT_ESDE
            providers::esde::invalidate_emulator_setup();
#endif
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

bool Settings::applyAndroidKind(QObject* collections, QObject* game_obj, const QString& kind)
{
    auto* const game = qobject_cast<model::Game*>(game_obj);
    const auto* const list = qobject_cast<model::ObjectListModel*>(collections);
    if (!game || !list || kind.isEmpty())
        return false;

    model::Collection* android = nullptr;
    model::Collection* android_games = nullptr;
    for (int i = 0; i < list->count(); i++) {
        auto* const coll = qobject_cast<model::Collection*>(list->get(i));
        if (!coll)
            continue;
        const QString name = coll->name().toLower();
        const QString short_name = coll->shortName().toLower();
        if (name == QLatin1String("android games") || short_name == QLatin1String("androidgames"))
            android_games = coll;
        else if (name == QLatin1String("android") || short_name == QLatin1String("android"))
            android = coll;
    }
    const bool is_game = kind == QLatin1String("Game");
    model::Collection* const target = (mergeAndroid() || !is_game) ? android : android_games;
    if (!target)
        return false;

    // the section tags, as the library rules set them
    QStringList& tags = game->tagList();
    QStringList& genres = game->genreList();
    for (const QString& other : { QStringLiteral("Game"), QStringLiteral("Emulator"), QStringLiteral("Application") }) {
        tags.removeAll(other);
        genres.removeAll(other);
    }
    tags.append(kind);
    if (!is_game)
        genres.append(kind);

    // into the target collection, A to Z; out of the other Android one
    model::Collection* const other = (target == android) ? android_games : android;
    if (other && other->gameList()->indexOf(game) >= 0) {
        std::vector<model::Game*> rest;
        for (model::Game* const g : other->gameList()->entries()) {
            if (g != game)
                rest.push_back(g);
        }
        other->gameList()->update(std::move(rest));
    }
    std::vector<model::Game*> games = target->gameList()->entries();
    if (std::find(games.cbegin(), games.cend(), game) == games.cend())
        games.push_back(game);
    std::stable_sort(games.begin(), games.end(), [](model::Game* a, model::Game* b) {
        return a->title().compare(b->title(), Qt::CaseInsensitive) < 0;
    });
    // a new list even when only the tags changed: themes read them again
    target->gameList()->update(std::move(games));

    std::vector<model::Collection*> colls;
    for (model::Collection* const coll : game->collectionsModel()->entries()) {
        if (coll != other && coll != target)
            colls.push_back(coll);
    }
    colls.push_back(target);
    std::sort(colls.begin(), colls.end(), model::sort_collections);
    game->collectionsModel()->update(std::move(colls));
    return true;
}

QVariantMap Settings::metadataLocation(const QString& collection_name, const QString& game_path) const
{
#ifdef WITH_COMPAT_ESDE
    const providers::esde::MetadataLocation loc = providers::esde::metadata_location(collection_name, game_path);
    return QVariantMap {
        { QStringLiteral("file"), loc.file },
        { QStringLiteral("isImport"), loc.is_import },
        { QStringLiteral("isNew"), loc.is_new },
    };
#else
    Q_UNUSED(collection_name);
    Q_UNUSED(game_path);
    return {};
#endif
}

QString Settings::saveGameMetadata(const QString& collection_name, const QString& game_path,
                                   const QString& game_title, const QVariantMap& changes)
{
#ifdef WITH_COMPAT_ESDE
    return providers::esde::save_game_metadata(collection_name, game_path, game_title, changes);
#else
    Q_UNUSED(collection_name);
    Q_UNUSED(game_path);
    Q_UNUSED(game_title);
    Q_UNUSED(changes);
    return QStringLiteral("Not supported on this platform");
#endif
}

QString Settings::saveCollectionMetadata(const QString& collection_name, const QVariantMap& changes)
{
#ifdef WITH_COMPAT_ESDE
    return providers::esde::save_collection_metadata(collection_name, changes);
#else
    Q_UNUSED(collection_name);
    Q_UNUSED(changes);
    return QStringLiteral("Not supported on this platform");
#endif
}

QVariantMap Settings::collectionDetails(const QString& collection_name) const
{
#ifdef WITH_COMPAT_ESDE
    const providers::esde::CollectionDetails d = providers::esde::collection_details(collection_name);
    return QVariantMap {
        { QStringLiteral("shortname"), d.short_name },
        { QStringLiteral("extensions"), d.extensions },
        { QStringLiteral("launch"), d.launch },
        { QStringLiteral("defaultLabel"), d.default_label },
        { QStringLiteral("defaultCommand"), d.default_command },
        { QStringLiteral("romRoot"), d.rom_root },
    };
#else
    Q_UNUSED(collection_name);
    return {};
#endif
}

QVariantList Settings::emulatorChoices(const QString& collection_name, const QString& short_name,
                                       const QString& rom_root) const
{
    QVariantList out;
#ifdef WITH_COMPAT_ESDE
    for (const auto& c : providers::esde::collection_choices(collection_name, short_name, rom_root)) {
        out.append(QVariantMap {
            { QStringLiteral("label"), c.label },
            { QStringLiteral("command"), c.command },
            { QStringLiteral("installed"), c.installed },
            { QStringLiteral("recommended"), c.recommended },
            { QStringLiteral("retroarch"), c.retroarch },
            { QStringLiteral("custom"), c.custom },
        });
    }
#else
    Q_UNUSED(collection_name);
    Q_UNUSED(short_name);
    Q_UNUSED(rom_root);
#endif
    return out;
}

QString Settings::saveCustomEmulator(const QString& collection_name, const QString& short_name,
                                     const QString& label, const QString& command, bool remove)
{
#ifdef WITH_COMPAT_ESDE
    return providers::esde::save_custom_emulator(collection_name, short_name, label.simplified(), command.simplified(), remove);
#else
    Q_UNUSED(collection_name); Q_UNUSED(short_name); Q_UNUSED(label); Q_UNUSED(command); Q_UNUSED(remove);
    return {};
#endif
}

namespace {
const QString SCRAPER_GROUP = QStringLiteral("scraper/");

QVariantMap scraper_defaults()
{
    return QVariantMap {
        { QStringLiteral("source"), QStringLiteral("screenscraper") },
        { QStringLiteral("games"), QStringLiteral("all") },
        { QStringLiteral("systems"), QStringList() },
        { QStringLiteral("ssUser"), QString() },
        { QStringLiteral("ssPassword"), QString() },
        { QStringLiteral("ssUse"), true },
        { QStringLiteral("sgdbKey"), QString() },
        // IGDB: your own Twitch app (dev.twitch.tv), and whether it fills in
        // what the first source didn't have
        { QStringLiteral("igdbClientId"), QString() },
        { QStringLiteral("igdbClientSecret"), QString() },
        { QStringLiteral("igdbFill"), true },
        // what to get
        { QStringLiteral("getNames"), true },
        { QStringLiteral("getRatings"), true },
        { QStringLiteral("getDetails"), true },
        { QStringLiteral("getVideos"), true },
        { QStringLiteral("getScreenshots"), true },
        { QStringLiteral("getTitlescreens"), true },
        { QStringLiteral("getCovers"), true },
        { QStringLiteral("getBackcovers"), true },
        { QStringLiteral("getBox3d"), true },
        { QStringLiteral("getPhysical"), true },
        { QStringLiteral("getFanart"), true },
        { QStringLiteral("getLogos"), true },
        { QStringLiteral("getMiximages"), true },
        { QStringLiteral("getManuals"), true },
        // miximages: made here as ES-DE makes them ("generate"), or
        // ScreenScraper's ready-made ones (mixrbv1 / mixrbv2); ES-DE's options
        { QStringLiteral("miximageStyle"), QStringLiteral("generate") },
        { QStringLiteral("mixOverwrite"), true },
        { QStringLiteral("mixResolution"), QStringLiteral("1280x960") },
        { QStringLiteral("mixFormat"), QStringLiteral("png") },
        { QStringLiteral("mixHorizontalFit"), QStringLiteral("crop") },
        { QStringLiteral("mixVerticalFit"), QStringLiteral("contain") },
        { QStringLiteral("mixThreshold"), QStringLiteral("high") },
        { QStringLiteral("mixBlankColor"), QStringLiteral("black") },
        { QStringLiteral("mixScaling"), QStringLiteral("sharp") },
        { QStringLiteral("mixBoxSize"), QStringLiteral("medium") },
        { QStringLiteral("mixPhysicalSize"), QStringLiteral("medium") },
        { QStringLiteral("mixRemoveLetterboxes"), true },
        { QStringLiteral("mixRemovePillarboxes"), true },
        { QStringLiteral("mixRotateBoxes"), true },
        { QStringLiteral("mixIncludeMarquee"), true },
        { QStringLiteral("mixIncludeBox"), true },
        { QStringLiteral("mixCoverFallback"), true },
        { QStringLiteral("mixIncludePhysical"), true },
        // other
        { QStringLiteral("region"), QStringLiteral("us") },
        { QStringLiteral("language"), QStringLiteral("en") },
        { QStringLiteral("retries"), 3 },
        { QStringLiteral("retryDelay"), 3 },
        { QStringLiteral("hashMaxMiB"), 800 },
        { QStringLiteral("overwrite"), false },
        { QStringLiteral("useHashes"), true },
        { QStringLiteral("useMetadataNames"), true },
        { QStringLiteral("interactive"), false },
        { QStringLiteral("autoAcceptSingle"), true },
        // pictures: biggest size kept (ES-DE: 2560x1440, logos 1000x600), and
        // empty borders trimmed (off, logos, all)
        { QStringLiteral("imageMaxSize"), QStringLiteral("2560") },
        { QStringLiteral("trimBorders"), QStringLiteral("off") },
    };
}

bool scraper_has_igdb(const QVariantMap& settings)
{
    return !settings.value(QStringLiteral("igdbClientId")).toString().trimmed().isEmpty()
        && !settings.value(QStringLiteral("igdbClientSecret")).toString().trimmed().isEmpty();
}

QString unmask(const unsigned char* data, int length)
{
    QByteArray out;
    out.reserve(length);
    for (int i = 0; i < length; i++)
        out.append(static_cast<char>(data[i] ^ PEGASUSX_SS_MASK));
    return QString::fromUtf8(out);
}
} // namespace

QVariantMap Settings::scraperSettings() const
{
    const QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    QVariantMap out = scraper_defaults();
    for (auto it = out.begin(); it != out.end(); ++it) {
        const QVariant saved = file.value(SCRAPER_GROUP + it.key());
        if (!saved.isValid())
            continue;
        // the ini keeps everything as text: back to the default's type
        if (it.value().type() == QVariant::Bool)
            it.value() = saved.toString() == QLatin1String("true") || saved.toString() == QLatin1String("1");
        else if (it.value().type() == QVariant::Int)
            it.value() = saved.toInt();
        else if (it.value().type() == QVariant::StringList)
            it.value() = saved.toStringList();
        else
            it.value() = saved.toString();
    }
    out.insert(QStringLiteral("hasScreenScraper"), scraperHasScreenScraper());
    return out;
}

void Settings::setScraperSetting(const QString& key, const QVariant& value)
{
    QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    if (value.type() == QVariant::List)
        file.setValue(SCRAPER_GROUP + key, value.toStringList());
    else
        file.setValue(SCRAPER_GROUP + key, value);
}

bool Settings::scraperHasScreenScraper() const
{
    return PEGASUSX_SS_DEVID_LEN > 0 && PEGASUSX_SS_DEVPASSWORD_LEN > 0;
}

void Settings::checkScraperAccounts()
{
    if (!m_netman)
        m_netman = new QNetworkAccessManager(this);
    const QVariantMap settings = scraperSettings();
    auto* const result = new QVariantMap();
    auto* const pending = new int(3);
    const auto finish = [this, result, pending]{
        if (--(*pending) > 0)
            return;
        emit scraperAccountsChecked(*result);
        delete result;
        delete pending;
    };

    // ScreenScraper: what your account allows today
    const QString user = settings.value(QStringLiteral("ssUser")).toString();
    if (!scraperHasScreenScraper()) {
        result->insert(QStringLiteral("ss"), tr("Not in this build yet (the app's developer login is missing)"));
        result->insert(QStringLiteral("ssOk"), false);
        finish();
    }
    else if (user.isEmpty()) {
        result->insert(QStringLiteral("ss"), tr("Without an account: slower, fewer games a day"));
        result->insert(QStringLiteral("ssOk"), true);
        finish();
    }
    else {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("devid"), unmask(PEGASUSX_SS_DEVID, PEGASUSX_SS_DEVID_LEN));
        query.addQueryItem(QStringLiteral("devpassword"), unmask(PEGASUSX_SS_DEVPASSWORD, PEGASUSX_SS_DEVPASSWORD_LEN));
        query.addQueryItem(QStringLiteral("softname"), QStringLiteral("PegasusX"));
        query.addQueryItem(QStringLiteral("output"), QStringLiteral("json"));
        query.addQueryItem(QStringLiteral("ssid"), user);
        query.addQueryItem(QStringLiteral("sspassword"), settings.value(QStringLiteral("ssPassword")).toString());
        QUrl url(QStringLiteral("https://api.screenscraper.fr/api2/ssuserInfos.php"));
        url.setQuery(query);
        QNetworkReply* const reply = m_netman->get(QNetworkRequest(url));
        connect(reply, &QNetworkReply::finished, this, [reply, result, finish]{
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll();
            reply->deleteLater();
            const QJsonObject info = QJsonDocument::fromJson(body).object()
                .value(QStringLiteral("response")).toObject().value(QStringLiteral("ssuser")).toObject();
            if (!info.isEmpty()) {
                const int max = info.value(QStringLiteral("maxrequestsperday")).toString().toInt();
                const int used = info.value(QStringLiteral("requeststoday")).toString().toInt();
                result->insert(QStringLiteral("ss"), max > 0
                    ? tr("%1 · %2 games left today").arg(info.value(QStringLiteral("id")).toString()).arg(qMax(0, max - used))
                    : info.value(QStringLiteral("id")).toString());
                result->insert(QStringLiteral("ssOk"), true);
            }
            else {
                result->insert(QStringLiteral("ss"), (status == 403 || body.contains("Erreur de login"))
                    ? tr("Wrong username or password")
                    : status == 0 ? tr("No connection") : tr("ScreenScraper is busy or offline (%1)").arg(status));
                result->insert(QStringLiteral("ssOk"), false);
            }
            finish();
        });
    }

    // IGDB: a token from Twitch with your app's Client ID and secret
    if (!scraper_has_igdb(settings)) {
        result->insert(QStringLiteral("igdb"), tr("No Client ID and secret yet"));
        result->insert(QStringLiteral("igdbOk"), false);
        finish();
    }
    else {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("client_id"), settings.value(QStringLiteral("igdbClientId")).toString().trimmed());
        query.addQueryItem(QStringLiteral("client_secret"), settings.value(QStringLiteral("igdbClientSecret")).toString().trimmed());
        query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("client_credentials"));
        QUrl url(QStringLiteral("https://id.twitch.tv/oauth2/token"));
        url.setQuery(query);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
        QNetworkReply* const reply = m_netman->post(request, QByteArray());
        connect(reply, &QNetworkReply::finished, this, [reply, result, finish]{
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool ok = status == 200 && QJsonDocument::fromJson(reply->readAll()).object().contains(QStringLiteral("access_token"));
            reply->deleteLater();
            result->insert(QStringLiteral("igdb"), ok ? tr("Login works")
                : (status == 400 || status == 401 || status == 403) ? tr("The Client ID or secret isn't accepted")
                : status == 0 ? tr("No connection") : tr("Twitch answered %1").arg(status));
            result->insert(QStringLiteral("igdbOk"), ok);
            finish();
        });
    }

    // SteamGridDB: one search with your key
    const QString key = settings.value(QStringLiteral("sgdbKey")).toString().trimmed();
    if (key.isEmpty()) {
        result->insert(QStringLiteral("sgdb"), tr("No API key yet"));
        result->insert(QStringLiteral("sgdbOk"), false);
        finish();
    }
    else {
        QNetworkRequest request(QUrl(QStringLiteral("https://www.steamgriddb.com/api/v2/search/autocomplete/zelda")));
        request.setRawHeader("Authorization", "Bearer " + key.toUtf8());
        QNetworkReply* const reply = m_netman->get(request);
        connect(reply, &QNetworkReply::finished, this, [reply, result, finish]{
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            reply->deleteLater();
            const bool ok = status == 200;
            result->insert(QStringLiteral("sgdb"), ok ? tr("Key works")
                : status == 401 ? tr("The key isn't accepted")
                : status == 0 ? tr("No connection") : tr("SteamGridDB answered %1").arg(status));
            result->insert(QStringLiteral("sgdbOk"), ok);
            finish();
        });
    }
}

namespace {
// what a scrape saved, onto the game as it's loaded now
void apply_scraped(model::Game& game, const QVariantMap& changes)
{
    static const QHash<QString, AssetType> kinds {
        { QStringLiteral("assets.box_front"), AssetType::BOX_FRONT },
        { QStringLiteral("assets.box_back"), AssetType::BOX_BACK },
        { QStringLiteral("assets.cartridge"), AssetType::CARTRIDGE },
        { QStringLiteral("assets.screenshot"), AssetType::SCREENSHOT },
        { QStringLiteral("assets.titlescreen"), AssetType::TITLESCREEN },
        { QStringLiteral("assets.background"), AssetType::BACKGROUND },
        { QStringLiteral("assets.logo"), AssetType::LOGO },
        { QStringLiteral("assets.video"), AssetType::VIDEO },
        { QStringLiteral("assets.steam"), AssetType::UI_STEAMGRID },
    };
    for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
        const QString& key = it.key();
        if (kinds.contains(key))
            game.assetsMut().set_files(kinds.value(key), it.value().toStringList());
        else if (key == QLatin1String("x-manual"))
            game.extraMapMut().insert(QStringLiteral("manual"), it.value().toStringList());
        else if (key == QLatin1String("x-pegasusx-hidden-art"))
            game.extraMapMut().insert(QStringLiteral("pegasusx-hidden-art"), it.value().toStringList());
        else if (key == QLatin1String("title"))
            game.setTitle(it.value().toString());
        else if (key == QLatin1String("description"))
            game.setDescription(it.value().toString());
        else if (key == QLatin1String("summary"))
            game.setSummary(it.value().toString());
        else if (key == QLatin1String("developer"))
            game.developerList() = it.value().toStringList();
        else if (key == QLatin1String("publisher"))
            game.publisherList() = it.value().toStringList();
        else if (key == QLatin1String("genre"))
            game.genreList() = it.value().toStringList();
        else if (key == QLatin1String("release"))
            game.setReleaseDate(QDate::fromString(it.value().toString().left(10), Qt::ISODate));
        else if (key == QLatin1String("players"))
            game.setPlayerCount(it.value().toInt());
        else if (key == QLatin1String("rating"))
            game.setRating(it.value().toString().remove(QLatin1Char('%')).toFloat() / 100.f);
    }
}

// one game for the scraper, with what it has now
scraper::Job make_scraper_job(const model::Collection& coll, model::Game* game, const QString& system, const QString& path)
{
    const auto local = [](const QString& url) {
        return url.startsWith(QLatin1String("file:")) ? QUrl(url).toLocalFile() : url;
    };
    static const QRegularExpression rx_3d(QStringLiteral("box3d|box_3d|3dbox"), QRegularExpression::CaseInsensitiveOption);
    const model::Assets& assets = game->assets();
    const model::Collection* const coll_ptr = &coll;
    scraper::Job job;
    job.collection = coll_ptr->name();
    job.system = system;
    job.title = game->title();
    job.path = path;
    job.has_description = !game->description().isEmpty() || !game->summary().isEmpty();
    job.has_developer = !game->developerListConst().isEmpty();
    job.has_publisher = !game->publisherListConst().isEmpty();
    job.has_genre = !game->genreListConst().isEmpty();
    job.has_release = game->releaseDate().isValid();
    job.has_players = game->playerCount() > 1;
    job.has_rating = game->rating() > 0.0f;
    job.title_is_file_name = !path.startsWith(QLatin1String("android:"))
        && QFileInfo(path).completeBaseName().compare(game->title(), Qt::CaseInsensitive) == 0;
    const std::vector<std::pair<const char*, const QStringList*>> slot_lists {
        { "assets.box_front", &assets.boxFrontList() }, { "assets.box_back", &assets.boxBackList() },
        { "assets.cartridge", &assets.cartridgeList() }, { "assets.screenshot", &assets.screenshotList() },
        { "assets.titlescreen", &assets.titlescreenList() }, { "assets.background", &assets.backgroundList() },
        { "assets.logo", &assets.logoList() }, { "assets.video", &assets.videoList() },
        { "assets.steam", &assets.steamList() },
    };
    for (const auto& entry : slot_lists) {
        QStringList files_now;
        for (const QString& f : *entry.second) {
            if (!f.startsWith(QLatin1String("image://")))
                files_now.append(local(f));
        }
        job.slot_files.insert(QLatin1String(entry.first), files_now);
        if (QLatin1String(entry.first) == QLatin1String("assets.box_front")) {
            for (const QString& f : files_now) {
                if (rx_3d.match(f).hasMatch()) job.has_box3d = true;
                else job.has_box2d = true;
            }
        }
    }
    job.hidden_art = game->extraMap().value(QStringLiteral("pegasusx-hidden-art")).toStringList();
    const QVariant manual = game->extraMap().value(QStringLiteral("manual"));
    job.slot_files.insert(QStringLiteral("x-manual"), manual.toStringList());
    return job;
}
} // namespace

QString Settings::startScraper(QObject* collections)
{
    return startScraperWith(collections, false);
}

// ES-DE's offline generator: miximages from the art the games have, for the
// systems picked, nothing downloaded
QString Settings::startMiximageGenerator(QObject* collections)
{
    return startScraperWith(collections, true);
}

QString Settings::startScraperWith(QObject* collections, bool offline)
{
    if (m_scraper && m_scraper->running())
        return tr("The scraper is already running.");
    const auto* const list = qobject_cast<model::ObjectListModel*>(collections);
    if (!list)
        return tr("No library to scrape.");
    QVariantMap settings = scraperSettings();
    const QStringList systems = settings.value(QStringLiteral("systems")).toStringList();
    const QString filter = offline ? QStringLiteral("all") : settings.value(QStringLiteral("games")).toString();
    if (systems.isEmpty())
        return tr("Pick the systems first.");
    if (offline) {
        settings.insert(QStringLiteral("offlineMix"), true);
        settings.insert(QStringLiteral("interactive"), false);
    }
    const QString source = settings.value(QStringLiteral("source")).toString();
    const bool has_ss = scraperHasScreenScraper();
    const bool has_sgdb = !settings.value(QStringLiteral("sgdbKey")).toString().trimmed().isEmpty();
    const bool has_igdb = scraper_has_igdb(settings);
    if (!offline && source == QLatin1String("steamgriddb") && !has_sgdb)
        return tr("Enter your SteamGridDB API key in Account settings first.");
    if (!offline && source == QLatin1String("igdb") && !has_igdb)
        return tr("Enter your IGDB Client ID and secret in Account settings first.");
    if (!offline && source == QLatin1String("screenscraper") && !has_ss && !has_sgdb && !has_igdb)
        return tr("ScreenScraper isn't in this build yet. Pick IGDB or SteamGridDB, or add their login.");

    std::vector<scraper::Job> jobs;
    QSet<QString> seen;
    for (int i = 0; i < list->count(); i++) {
        auto* const coll = qobject_cast<model::Collection*>(list->get(i));
        if (!coll || !systems.contains(coll->name()))
            continue;
        const QString system = scraper::Scraper::system_of(coll->name(), coll->shortName());
        for (model::Game* const game : coll->gameList()->entries()) {
            const std::vector<model::GameFile*>& files = game->filesModel()->entries();
            if (files.empty())
                continue;
            const QString path = files.front()->path();
            if (seen.contains(path))
                continue;
            const model::Assets& assets = game->assets();
            const bool no_details = game->description().isEmpty() && game->summary().isEmpty() && game->developerListConst().isEmpty();
            const auto real = [](const QStringList& l) {
                for (const QString& f : l) { if (!f.startsWith(QLatin1String("image://"))) return true; }
                return false;
            };
            const bool no_image = !real(assets.boxFrontList()) && assets.screenshotList().isEmpty();
            const bool no_video = assets.videoList().isEmpty();
            bool take = true;
            if (filter == QLatin1String("favorites")) take = game->isFavorite();
            else if (filter == QLatin1String("nometadata")) take = no_details;
            else if (filter == QLatin1String("noimage")) take = no_image;
            else if (filter == QLatin1String("novideo")) take = no_video;
            else if (filter == QLatin1String("missing")) take = no_details || no_image || no_video || assets.logoList().isEmpty();
            if (!take)
                continue;
            seen.insert(path);

            jobs.push_back(make_scraper_job(*coll, game, system, path));
        }
    }
    if (jobs.empty())
        return tr("No games to scrape with these settings.");
    m_single_game.clear();
    return runScraper(std::move(jobs), settings);
}

QString Settings::scrapeGame(QObject* game_obj, const QString& collection_name, bool overwrite)
{
    if (m_scraper && m_scraper->running())
        return tr("The scraper is already running.");
    auto* const game = qobject_cast<model::Game*>(game_obj);
    if (!game || game->filesModel()->entries().empty())
        return tr("No game to scrape.");
    const model::Collection* coll = nullptr;
    for (model::Collection* const c : game->collectionsModel()->entries()) {
        if (c->name() == collection_name || !coll)
            coll = c;
    }
    if (!coll)
        return tr("No game to scrape.");
    QVariantMap settings = scraperSettings();
    settings.insert(QStringLiteral("overwrite"), overwrite);
    // one game: always shows what it found, to pick and accept
    settings.insert(QStringLiteral("interactive"), true);
    const QString source = settings.value(QStringLiteral("source")).toString();
    const bool has_sgdb = !settings.value(QStringLiteral("sgdbKey")).toString().trimmed().isEmpty();
    const bool has_igdb = scraper_has_igdb(settings);
    if (!scraperHasScreenScraper() && !has_sgdb && !has_igdb)
        return tr("Enter your IGDB or SteamGridDB login in Library > Scraper > Account settings first.");
    if (source == QLatin1String("igdb") && !has_igdb)
        return tr("Enter your IGDB Client ID and secret in Library > Scraper > Account settings first.");
    if (source == QLatin1String("steamgriddb") && !has_sgdb)
        return tr("Enter your SteamGridDB key in Library > Scraper > Account settings first.");
    std::vector<scraper::Job> jobs;
    jobs.push_back(make_scraper_job(*coll, game, scraper::Scraper::system_of(coll->name(), coll->shortName()),
                                    game->filesModel()->entries().front()->path()));
    m_single_game = game;  // what it saves shows at once (apply_scraped)
    const QString error = runScraper(std::move(jobs), settings);
    if (!error.isEmpty())
        m_single_game.clear();
    return error;
}

QString Settings::runScraper(std::vector<scraper::Job>&& jobs, const QVariantMap& settings)
{
    const bool has_ss = scraperHasScreenScraper();
    QString data = dataDir();
    if (data.isEmpty())
        data = paths::writableConfigDir() + QStringLiteral("/media");

    if (!m_scraper) {
        m_scraper = new scraper::Scraper(this);
        connect(m_scraper, &scraper::Scraper::progress, this, &Settings::scraperProgress);
        connect(m_scraper, &scraper::Scraper::choiceNeeded, this, &Settings::scraperNeedsChoice);
        connect(m_scraper, &scraper::Scraper::finished, this, [this](QVariantMap state){
            // one game: the game shows what was saved right away (Library's
            // page reads it again), without reading the library again
            if (m_single_game && state.value(QStringLiteral("saved")).toInt() > 0)
                apply_scraped(*m_single_game, state.value(QStringLiteral("changes")).toMap());
            m_single_game.clear();
            emit scraperFinished(state);
            // the library shows it: right away, unless Library is open (it
            // reads the library again when you leave it)
            if (!m_library_open && state.value(QStringLiteral("saved")).toInt() > 0)
                reloadLibrary();
        });
    }
    m_scraper->start(std::move(jobs), settings,
                     has_ss ? unmask(PEGASUSX_SS_DEVID, PEGASUSX_SS_DEVID_LEN) : QString(),
                     has_ss ? unmask(PEGASUSX_SS_DEVPASSWORD, PEGASUSX_SS_DEVPASSWORD_LEN) : QString(),
                     data);
    return {};
}

void Settings::stopScraper()
{
    if (m_scraper)
        m_scraper->stop();
}

void Settings::scraperChoose(int index)
{
    if (m_scraper)
        m_scraper->choose(index);
}

void Settings::scraperRefine(const QString& search_name)
{
    if (m_scraper)
        m_scraper->refine(search_name);
}

void Settings::scraperSkip()
{
    if (m_scraper)
        m_scraper->skip();
}

QVariantMap Settings::scraperPendingChoice() const
{
    return m_scraper ? m_scraper->pendingChoice() : QVariantMap();
}

QVariantMap Settings::scraperState() const
{
    return m_scraper ? m_scraper->state() : QVariantMap { { QStringLiteral("running"), false } };
}

int Settings::emulatorListAgeDays() const
{
    const QFileInfo finfo(esde_lists_dir() + QStringLiteral("/es_systems.xml"));
    if (!finfo.exists())
        return -1;
    return static_cast<int>(finfo.lastModified().daysTo(QDateTime::currentDateTime()));
}

QString Settings::dataDir() const
{
    const QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    return file.value(QStringLiteral("data/dir")).toString();
}

void Settings::setDataDir(const QString& dir)
{
    QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    file.setValue(QStringLiteral("data/dir"), dir);
    emit dataDirChanged();
}

QString Settings::lastBrowseFolder() const
{
    const QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    return file.value(QStringLiteral("browse/last")).toString();
}

void Settings::setLastBrowseFolder(const QString& dir)
{
    QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    file.setValue(QStringLiteral("browse/last"), dir);
    QStringList recent = file.value(QStringLiteral("browse/recent")).toStringList();
    recent.removeAll(dir);
    recent.prepend(dir);
    while (recent.size() > 5)
        recent.removeLast();
    file.setValue(QStringLiteral("browse/recent"), recent);
    emit lastBrowseFolderChanged();
}

QStringList Settings::recentBrowseFolders() const
{
    const QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    QStringList recent = file.value(QStringLiteral("browse/recent")).toStringList();
    const QString last = file.value(QStringLiteral("browse/last")).toString();
    if (!last.isEmpty() && !recent.contains(last))
        recent.prepend(last);  // from before the list was kept
    return recent;
}

bool Settings::dataDirAsked() const
{
    const QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    return file.value(QStringLiteral("data/asked"), false).toBool();
}

void Settings::setDataDirAsked(bool value)
{
    QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    file.setValue(QStringLiteral("data/asked"), value);
    emit dataDirChanged();
}

bool Settings::makeDir(const QString& path) const
{
    return !path.isEmpty() && QDir().mkpath(path);
}

QString Settings::importArt(const QString& source, const QString& collection_name,
                            const QString& kind_folder, const QString& name) const
{
    const QString data = dataDir();
    if (data.isEmpty() || source.isEmpty() || !QFileInfo(source).isFile())
        return source;
    const QString clean_data = QDir::cleanPath(data);
    if (QDir::cleanPath(source).startsWith(clean_data + QLatin1Char('/')))
        return source;  // already there

    // names that work on every file system
    const auto safe = [](QString text) {
        static const QRegularExpression rx_bad(QStringLiteral("[\\\\/:*?\"<>|]"));
        text.replace(rx_bad, QStringLiteral("_"));
        return text.simplified();
    };
    const QString dir = clean_data + QLatin1Char('/') + safe(collection_name) + QLatin1Char('/') + safe(kind_folder);
    if (!QDir().mkpath(dir))
        return source;
    // A new name each time (the time it was picked): picture views keep a
    // file they've shown by its path, so reusing the old name kept showing
    // the old picture. (Earlier copies stay: the metadata may still point at
    // one until the change is saved.)
    const QString base = safe(name);
    const QString ext = QFileInfo(source).suffix();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmss"));
    const QString target = dir + QLatin1Char('/') + base + QLatin1Char('~') + stamp
        + (ext.isEmpty() ? QString() : QLatin1Char('.') + ext);
    if (!QFile::copy(source, target)) {
        Log::warning(LOGMSG("Could not copy `%1` to `%2`").arg(source, target));
        return source;
    }
    Log::info(LOGMSG("Art copied to `%1`").arg(target));
    return target;
}

QString Settings::firstExistingDir(const QStringList& paths) const
{
    for (const QString& path : paths) {
        if (!path.isEmpty() && QFileInfo(path).isDir())
            return path;
    }
    return {};
}

QStringList Settings::libraryGenres(QObject* all_games) const
{
    QStringList out;
    const auto* const list = qobject_cast<model::ObjectListModel*>(all_games);
    if (!list)
        return out;
    QSet<QString> seen;
    for (int i = 0; i < list->count(); i++) {
        const auto* const game = qobject_cast<model::Game*>(list->get(i));
        if (!game)
            continue;
        for (const QString& genre : game->genreListConst()) {
            const QString key = genre.trimmed().toLower();
            if (key.isEmpty() || seen.contains(key) || key == QLatin1String("application") || key == QLatin1String("emulator"))
                continue;
            seen.insert(key);
            out.append(genre.trimmed());
        }
    }
    std::sort(out.begin(), out.end(), [](const QString& a, const QString& b) { return a.compare(b, Qt::CaseInsensitive) < 0; });
    return out;
}

void Settings::reloadLibrary()
{
    Log::info(LOGMSG("Reloading the library..."));
    emit libraryReloadRequested();
}

QVariantMap Settings::themeSystemArt(const QString& collection_name, const QString& short_name) const
{
    QString dir = m_themes.currentQmlDir();
    if (dir.isEmpty() || dir.startsWith(QLatin1Char(':')) || dir.startsWith(QLatin1String("qrc")))
        return {};

    // every picture of the theme, by lower-case name (read once per theme)
    static QString cached_dir;
    static QHash<QString, QStringList> pictures;
    if (cached_dir != dir) {
        cached_dir = dir;
        pictures.clear();
        static const QStringList exts { QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                        QStringLiteral("webp"), QStringLiteral("svg"), QStringLiteral("gif") };
        QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo fi = it.fileInfo();
            if (exts.contains(fi.suffix().toLower()))
                pictures[fi.completeBaseName().toLower()].append(fi.absoluteFilePath());
        }
    }

    QString normalized;
    for (const QChar c : collection_name.toLower()) {
        if (c.isLetterOrNumber())
            normalized += c;
    }
    QString logo;
    QString background;
    for (const QString& key : { short_name.toLower(), normalized }) {
        if (key.isEmpty())
            continue;
        for (const QString& path : pictures.value(key)) {
            const QString in = path.mid(dir.length()).toLower();
            if (logo.isEmpty() && in.contains(QLatin1String("logo")))
                logo = path;
            else if (background.isEmpty() && (in.contains(QLatin1String("background")) || in.contains(QLatin1String("fanart"))))
                background = path;
        }
        if (!logo.isEmpty() && !background.isEmpty())
            break;
    }
    return QVariantMap {
        { QStringLiteral("logo"), logo },
        { QStringLiteral("background"), background },
    };
}

void Settings::reloadAllGames()
{
    emit reloadAllRequested();
    reloadProviders();
}

void Settings::reloadProviders()
{
    Log::info(LOGMSG("Reloading..."));
    emit providerReloadingRequested();
}

} // namespace model
