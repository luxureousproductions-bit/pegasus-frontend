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
#include "PegasusAssets.h"
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
#include "providers/Overrides.h"
#include "ScraperKeys.h"
#include "LaunchBoxDb.h"
#include "RetroAchievements.h"
#include "providers/GameDirectory.h"
#include "Scraper.h"
#include "ThemeStore.h"
#include "Updater.h"
#include "model/gaming/Assets.h"
// Android only (ES-DE import, emulators, metadata editing)
#ifdef WITH_COMPAT_ESDE
#include "providers/esde/EsdeProvider.h"
#endif

#include <QCoreApplication>
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
#include <QPointer>
#include <QSettings>
#include <QSaveFile>
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
// Pegasus X: settings of its own features, kept apart from Pegasus' own
QString pegasusx_settings_path()
{
    return paths::writableConfigDir() + QStringLiteral("/pegasusx.ini");
}

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

// Pegasus X: older builds listed <Pegasus X folder>/metadata/imports as a game
// folder; they're read by themselves now (always, and after your own files)
void drop_imports_game_dirs()
{
    QStringList kept;
    bool dropped = false;
    AppSettings::parse_gamedirs([&](const QString& line){
        if (QDir::cleanPath(line).endsWith(QLatin1String("/metadata/imports")))
            dropped = true;
        else
            kept.append(line);
    });
    if (!dropped)
        return;
    rewrite_gamedircfg([&kept](QTextStream& stream){
        for (const QString& line : kept)
            stream << line << '\n';
    });
    Log::info(LOGMSG("The ES-DE imports are no longer listed as a game folder (they're read by themselves)"));
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
namespace {
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

Settings::Settings(QObject* parent)
    : QObject(parent)
{}

QString Settings::startupScreen() const
{
    return qApp ? qApp->property("pegasusxOpen").toString() : QString();
}

void Settings::testDump(const QString& text) const
{
    const QString path = qEnvironmentVariable("PEGASUSX_TEST_DUMP");
    if (path.isEmpty())
        return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        file.write(text.toUtf8());
}

QString Settings::testScrapeTitle() const
{
    return qEnvironmentVariable("PEGASUSX_TEST_SCRAPE");
}

QString Settings::rootDir() const
{
    return paths::writableConfigDir();
}

QString Settings::defaultRootDir() const
{
    return paths::privateConfigDir();
}

bool Settings::rootChosen() const
{
    return !paths::pegasusxRootSetting().isEmpty();
}

QString Settings::rootMissing() const
{
    return paths::pegasusxRootMissing();
}

bool Settings::rootSetupNeeded() const
{
    return !rootChosen() && !QFileInfo::exists(paths::privateConfigDir() + QStringLiteral("/pegasusx-root-asked"));
}

void Settings::setRootSetupAsked()
{
    QFile file(paths::privateConfigDir() + QStringLiteral("/pegasusx-root-asked"));
    if (file.open(QIODevice::WriteOnly))
        file.write("asked\n");
    emit pegasusxSettingsChanged();
}

int Settings::wizardStep() const
{
    QFile file(paths::privateConfigDir() + QStringLiteral("/pegasusx-wizard"));
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    return qBound(0, QString::fromLatin1(file.readAll()).trimmed().toInt(), 4);
}

void Settings::setWizardStep(int step)
{
    const QString path = paths::privateConfigDir() + QStringLiteral("/pegasusx-wizard");
    if (step <= 0) {
        QFile::remove(path);
        return;
    }
    QDir().mkpath(paths::privateConfigDir());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QByteArray::number(step) + '\n');
}

namespace {
// the files and folders of one folder into another (what's already there is
// kept when the copy fails); returns how many files were copied, -1 on error
int copy_tree(const QString& from, const QString& to, const QStringList& skip, QString& error)
{
    int copied = 0;
    QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString rel = path.mid(from.length() + 1);
        const QString top = rel.section(QLatin1Char('/'), 0, 0);
        if (skip.contains(top))
            continue;
        const QFileInfo info = it.fileInfo();
        const QString target = to + QLatin1Char('/') + rel;
        if (info.isDir()) {
            QDir().mkpath(target);
            continue;
        }
        QDir().mkpath(QFileInfo(target).absolutePath());
        if (QFileInfo::exists(target))
            QFile::remove(target);
        if (!QFile::copy(path, target)) {
            error = QStringLiteral("Couldn't copy %1").arg(rel);
            return -1;
        }
        copied++;
    }
    return copied;
}

// a text file with the old folder's paths in it: the new folder's
void rewrite_paths_in(const QString& file_path, const QString& from, const QString& to)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QString text = QString::fromUtf8(file.readAll());
    file.close();
    const QString before = text;
    text.replace(from + QLatin1Char('/'), to + QLatin1Char('/'));
    // the folder itself on a line of its own (game_dirs.txt)
    const QRegularExpression rx_line_end(QStringLiteral("(^|\\n)") + QRegularExpression::escape(from) + QStringLiteral("(\\r?\\n|$)"));
    text.replace(rx_line_end, QStringLiteral("\\1") + to + QStringLiteral("\\2"));
    if (text == before)
        return;
    QSaveFile out(file_path);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        out.write(text.toUtf8());
        out.commit();
    }
}

// after the copy: what in the new folder still points at the old one
void fix_moved_root(const QString& now, const QString& dir, const QString& old_media)
{
    // the game directories (the imports folder is one)
    rewrite_paths_in(dir + QStringLiteral("/game_dirs.txt"), now, dir);
    // the art folder setting
    {
        QSettings ini(dir + QStringLiteral("/pegasusx.ini"), QSettings::IniFormat);
        const QString media = QDir::cleanPath(ini.value(QStringLiteral("data/dir")).toString());
        if (media == now + QStringLiteral("/media") || media == now + QStringLiteral("/downloaded_media")
                || old_media == now + QStringLiteral("/media") || old_media == now + QStringLiteral("/downloaded_media"))
            ini.remove(QStringLiteral("data/dir"));
        else if (media.startsWith(now + QLatin1Char('/')))
            ini.setValue(QStringLiteral("data/dir"), dir + media.mid(now.length()));
    }
#ifdef WITH_COMPAT_ESDE
    // the metadata files' relative paths were written for the old place
    for (const QString& sub : { QStringLiteral("/metadata/overrides"), QStringLiteral("/metadata/imports"), QStringLiteral("/metadata/imports-fill") }) {
        const QDir folder(dir + sub);
        for (const QFileInfo& finfo : folder.entryInfoList({ QStringLiteral("*.metadata.pegasus.txt") }, QDir::Files))
            providers::esde::rebase_override_file(finfo.absoluteFilePath(), now + sub);
    }
#endif
}

// the art folder is no place for the gallery app
void hide_from_gallery(const QString& dir)
{
    QDir().mkpath(dir);
    QFile marker(dir + QStringLiteral("/.nomedia"));
    if (!marker.exists() && marker.open(QIODevice::WriteOnly))
        marker.close();
}
} // namespace

namespace {
// a picked data folder -> where the art goes: its downloaded_media (ES-DE's
// layout), or the folder itself when it is a downloaded_media folder
QString art_dir_of(const QString& picked)
{
    const QString clean = QDir::cleanPath(picked);
    if (QFileInfo(clean).fileName().compare(QLatin1String("downloaded_media"), Qt::CaseInsensitive) == 0)
        return clean;
    return clean + QStringLiteral("/downloaded_media");
}

// a folder of art for one system: it has kind folders in it
bool is_system_art_dir(const QString& dir)
{
    static const QStringList kinds {
        QStringLiteral("covers"), QStringLiteral("3dboxes"), QStringLiteral("backcovers"), QStringLiteral("fanart"),
        QStringLiteral("screenshots"), QStringLiteral("titlescreens"), QStringLiteral("marquees"),
        QStringLiteral("miximages"), QStringLiteral("physicalmedia"), QStringLiteral("videos"), QStringLiteral("manuals"),
    };
    for (const QString& sub : QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (kinds.contains(sub.toLower()))
            return true;
    }
    return false;
}

// everything of `from` into `to` (what's there already is kept)
bool move_tree(const QString& from, const QString& to)
{
    if (!QFileInfo::exists(to))
        return QDir().mkpath(QFileInfo(to).absolutePath()) && QDir().rename(from, to);
    QDirIterator it(from, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString target = to + path.mid(from.length());
        QDir().mkpath(QFileInfo(target).absolutePath());
        if (!QFileInfo::exists(target))
            QFile::rename(path, target);
    }
    QDir(from).removeRecursively();
    return true;
}
} // namespace

void Settings::move_media_folder()
{
    // scraped art kept by older builds outside downloaded_media: the Pegasus
    // X folder's media/, or <data folder>/<system>/<kind>
    const QString root = QDir::cleanPath(paths::writableConfigDir());
    QSettings ini(pegasusx_settings_path(), QSettings::IniFormat);
    const QString saved = QDir::cleanPath(ini.value(QStringLiteral("data/dir")).toString());
    const bool picked = !saved.isEmpty() && saved != QLatin1String(".") && saved != root + QStringLiteral("/media");
    if (!picked && !saved.isEmpty())
        ini.remove(QStringLiteral("data/dir"));
    const QString base = picked ? saved : root;
    const QString art = art_dir_of(base);
    if (art == base)
        return;  // a downloaded_media folder was picked: nothing to move

    std::vector<std::pair<QString, QString>> moves;  // from, to
    if (!picked && QFileInfo(root + QStringLiteral("/media")).isDir())
        moves.emplace_back(root + QStringLiteral("/media"), art);
    for (const QFileInfo& finfo : QDir(base).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString name = finfo.fileName().toLower();
        if (name == QLatin1String("downloaded_media") || name == QLatin1String("media") || name == QLatin1String("themes")
                || name == QLatin1String("metadata") || name == QLatin1String("metafiles"))
            continue;
        if (is_system_art_dir(finfo.absoluteFilePath()))
            moves.emplace_back(finfo.absoluteFilePath(), art + QLatin1Char('/') + finfo.fileName());
    }
    if (moves.empty())
        return;
    for (const auto& move : moves) {
        if (!move_tree(move.first, move.second)) {
            Log::warning(LOGMSG("Couldn't move `%1` to `%2`").arg(move.first, move.second));
            continue;
        }
        // the metadata files Pegasus X wrote point at the new place
        for (const QString& sub : { QStringLiteral("/metadata/overrides"), QStringLiteral("/metadata/imports"),
                                    QStringLiteral("/metadata/imports-fill"), QStringLiteral("/metafiles") }) {
            for (const QFileInfo& finfo : QDir(root + sub).entryInfoList({ QStringLiteral("*.metadata.pegasus.txt") }, QDir::Files))
                rewrite_paths_in(finfo.absoluteFilePath(), move.first, move.second);
        }
        Log::info(LOGMSG("Scraped art moved from `%1` to `%2`").arg(move.first, move.second));
    }
    hide_from_gallery(art);
}

QVariantMap Settings::rootDirContents() const
{
    qint64 files = 0, bytes = 0;
    QDirIterator it(paths::writableConfigDir(), QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        files++;
        bytes += it.fileInfo().size();
    }
    return QVariantMap {
        { QStringLiteral("files"), files },
        { QStringLiteral("mb"), static_cast<double>(bytes) / 1048576.0 },
        { QStringLiteral("dir"), paths::writableConfigDir() },
    };
}

QString Settings::setRootDir(const QString& dir_in, bool move_data)
{
    const QString dir = QDir::cleanPath(dir_in);
    const QString now = QDir::cleanPath(paths::writableConfigDir());
    if (dir.isEmpty())
        return tr("Pick a folder first.");
    if (!QFileInfo(dir).isDir() && !QDir().mkpath(dir))
        return tr("Couldn't make the folder %1.").arg(dir);
    if (dir == now) {
        setRootSetupAsked();
        return {};
    }
    if (dir.startsWith(now + QLatin1Char('/')))
        return tr("The folder can't be inside the one used now.");
    {
        QFile probe(dir + QStringLiteral("/.pegasusx-write-test"));
        if (!probe.open(QIODevice::WriteOnly))
            return tr("Pegasus X can't write in %1. Pick a folder on this device's storage.").arg(dir);
        probe.close();
        probe.remove();
    }
    if (move_data) {
        // never over another Pegasus setup's files (upstream Pegasus's folder, say)
        const QDir target(dir);
        if (target.exists(QStringLiteral("settings.txt")) || target.exists(QStringLiteral("game_dirs.txt"))
                || target.exists(QStringLiteral("metadata")) || target.exists(QStringLiteral("pegasusx.ini")))
            return tr("%1 already holds Pegasus data. \"Use it, start fresh\" uses what's there; or pick another folder.").arg(dir);
    }
    const QString old_media = QDir::cleanPath(dataDir());
    // the copy can take a while (the art): off the UI thread
    QtConcurrent::run([this, now, dir, move_data, old_media]{
        QString error;
        if (move_data) {
            // the Pegasus X folder setting itself and the caches stay out
            const int copied = copy_tree(now, dir, { QStringLiteral("pegasusx-root.txt"), QStringLiteral("pegasusx-root-asked"),
                                                      QStringLiteral(".pegasusx-write-test") }, error);
            if (copied >= 0) {
                Log::info(LOGMSG("Pegasus X folder: %1 files copied from `%2` to `%3`").arg(copied).arg(now, dir));
                fix_moved_root(now, dir, old_media);
            }
        }
        if (error.isEmpty()) {
            hide_from_gallery(dir + QStringLiteral("/downloaded_media"));
            if (!paths::setPegasusxRootSetting(dir))
                error = tr("Couldn't save the folder setting.");
        }
        QMetaObject::invokeMethod(this, [this, error]{
            if (error.isEmpty()) {
                setRootSetupAsked();
                emit pegasusxSettingsChanged();
            }
            emit rootDirChanged(error);
        }, Qt::QueuedConnection);
    });
    return {};
}

void Settings::quitApp()
{
    QCoreApplication::quit();
}

bool Settings::checkUpdatesOnStart() const
{
    const QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    return file.value(QStringLiteral("updater/checkOnStart"), false).toBool();
}

void Settings::setCheckUpdatesOnStart(bool value)
{
    QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    file.setValue(QStringLiteral("updater/checkOnStart"), value);
    emit pegasusxSettingsChanged();
}

QObject* Settings::updater()
{
    if (!m_updater)
        m_updater = new Updater(this);
    return m_updater;
}

QObject* Settings::themeStore()
{
    if (!m_theme_store) {
        m_theme_store = new ThemeStore(this);
        m_theme_store->setCurrentDirGetter([this]{ return m_themes.currentQmlDir(); });
        // a theme added, updated or removed: Pegasus' list read again; the
        // one in use updated: built again
        connect(m_theme_store, &ThemeStore::themesChanged, this, [this](const QString& dir, const QString& old_dir){
            const QString now = QDir(m_themes.currentQmlDir()).absolutePath();
            const bool current = QDir(dir).absolutePath() == now;
            // the theme in use was updated into a folder of a new name: it follows
            const bool moved = !old_dir.isEmpty() && QDir(old_dir).absolutePath() == now;
            m_themes.rescan();
            if (moved) {
                const int index = m_themes.indexOfDir(dir);
                if (index >= 0)
                    m_themes.setCurrentIndex(index);
            }
            if (current || moved)
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
    // scraped art kept in <Pegasus X folder>/media by older builds: in
    // downloaded_media now (ES-DE's name), the paths to it with it
    move_media_folder();
    // the ES-DE imports are read by themselves now: no longer a game folder
    drop_imports_game_dirs();
#ifdef WITH_COMPAT_ESDE
    // before the library is read: no game in a collection that isn't its own
    providers::esde::split_legacy_edits_file();
    providers::esde::move_edits_to_overrides();
#endif
    change_mouse_support(AppSettings::general.mouse_support);
#ifdef WITH_COMPAT_ESDE
    // games added to a ROM folder while Pegasus X was away: in the list
    // before the library is read (quick when nothing changed)
    providers::esde::refresh_rom_folders();
#endif
#if defined(PEGASUSX_TESTENV) && defined(WITH_COMPAT_ESDE)
    // the test environment: the imports it asks for, before the library is
    // read (PEGASUSX_TEST_ESDE_IMPORT=1, PEGASUSX_TEST_ROM_FOLDERS=a;b)
    {
        QStringList written;
        if (qEnvironmentVariable("PEGASUSX_TEST_ESDE_IMPORT") == QLatin1String("1")) {
            const auto result = providers::esde::esde_import(providers::esde::esde_default_dir(), {},
                                                             { QStringLiteral("androidapps") }, {});
            Log::info(LOGMSG("Test: ES-DE import: %1 systems, %2 games, error `%3`")
                .arg(result.systems).arg(result.games).arg(result.error));
            written += result.folders;
        }
        for (const QString& folder : qEnvironmentVariable("PEGASUSX_TEST_ROM_FOLDERS").split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            const auto found = providers::esde::rom_folder_systems(folder);
            QStringList names;
            for (const auto& sys : found)
                names.append(sys.name + QLatin1Char('=') + QString::number(sys.games));
            Log::info(LOGMSG("Test: ROM folder `%1`: systems %2").arg(folder, names.join(QStringLiteral(", "))));
            const auto result = providers::esde::rom_folder_import(folder, {});
            Log::info(LOGMSG("Test: ROM folder import: %1 systems, %2 games, error `%3`")
                .arg(result.systems).arg(result.games).arg(result.error));
            written += result.folders;
        }
    }
#endif

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

QString Settings::esdeImportUse() const
{
    return providers::overrides::import_fill_mode();
}

void Settings::setEsdeImportUse(const QString& mode)
{
    providers::overrides::set_import_fill_mode(mode);
    emit esdeImportDirChanged();
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
#ifdef WITH_COMPAT_ESDE
            // the added games are in Pegasus X's imports folder, a game directory
#endif
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

QVariantList Settings::romFolderSystems(const QString& folder)
{
    QVariantList out;
#ifdef WITH_COMPAT_ESDE
    for (const auto& sys : providers::esde::rom_folder_systems(folder)) {
        out.append(QVariantMap {
            { QStringLiteral("name"), sys.name },
            { QStringLiteral("fullname"), sys.fullname },
            { QStringLiteral("games"), sys.games },
            { QStringLiteral("folders"), sys.folders },
            { QStringLiteral("hasGamelist"), sys.has_gamelist },
            { QStringLiteral("listed"), !sys.imported_files.isEmpty() },
        });
    }
#else
    Q_UNUSED(folder);
#endif
    return out;
}

bool Settings::folderHasMetadata(const QString& folder)
{
    for (const QFileInfo& finfo : QDir(folder).entryInfoList(QDir::Files | QDir::Readable)) {
        const QString name = finfo.fileName();
        if (name == QLatin1String("metadata.pegasus.txt") || name == QLatin1String("metadata.txt")
                || name.endsWith(QLatin1String(".metadata.pegasus.txt")) || name.endsWith(QLatin1String(".metadata.txt")))
            return true;
    }
    return false;
}

void Settings::importRomFolder(const QString& folder)
{
#ifdef WITH_COMPAT_ESDE
    QtConcurrent::run([this, folder]{
        const auto result = providers::esde::rom_folder_import(folder, {});
        // back on the main thread: each system folder is a game directory, so
        // Pegasus reads its list
        QMetaObject::invokeMethod(this, [this, result]{
            emit romFolderImportFinished(QVariantMap {
                { QStringLiteral("systems"), result.systems },
                { QStringLiteral("games"), result.games },
                { QStringLiteral("error"), result.error },
            });
        }, Qt::QueuedConnection);
    });
#else
    emit romFolderImportFinished(QVariantMap { { QStringLiteral("error"), QStringLiteral("Not supported on this platform") }, { QStringLiteral("folder"), folder } });
#endif
}

// -- Settings > Game directory ---------------------------------------------------

QVariantList Settings::gameDirectorySystems() const
{
    QVariantList out;
    for (const QString& path : providers::gamedir::metafiles()) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QVariantMap item;
        item.insert(QStringLiteral("key"), providers::gamedir::system_of(path));
        item.insert(QStringLiteral("file"), path);
        QStringList folders;
        QString source;
        int games = 0;
        QString name, short_name;
        bool own = false;
        while (!file.atEnd()) {
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (line.startsWith(QLatin1String("# folder: ")))
                folders.append(line.mid(10).trimmed());
            else if (line.startsWith(QLatin1String("# Made by Pegasus X from ES-DE")))
                source = QStringLiteral("gamelist");
            else if (line.startsWith(QLatin1String("# own metadata:")))
                own = true;
            else if (line.startsWith(QLatin1String("game:")))
                games++;
            else if (name.isEmpty() && line.startsWith(QLatin1String("collection:")))
                name = line.mid(11).trimmed();
            else if (short_name.isEmpty() && line.startsWith(QLatin1String("shortname:")))
                short_name = line.mid(10).trimmed();
        }
        item.insert(QStringLiteral("name"), name.isEmpty() ? item.value(QStringLiteral("key")).toString() : name);
        item.insert(QStringLiteral("shortName"), short_name);
        item.insert(QStringLiteral("games"), games);
        item.insert(QStringLiteral("folders"), folders);
        item.insert(QStringLiteral("source"), own ? QStringLiteral("own metadata") : source);
        out.append(item);
    }
    return out;
}

QVariantList Settings::knownSystems() const
{
    QVariantList out;
#ifdef WITH_COMPAT_ESDE
    QSet<QString> in_dir;
    for (const QString& path : providers::gamedir::metafiles())
        in_dir.insert(providers::gamedir::system_of(path));
    for (const auto& sys : providers::esde::known_systems()) {
        out.append(QVariantMap {
            { QStringLiteral("name"), sys.name },
            { QStringLiteral("fullName"), sys.fullname },
            { QStringLiteral("shortName"), sys.short_name },
            { QStringLiteral("folder"), sys.folder },
            { QStringLiteral("extensions"), sys.extensions },
            { QStringLiteral("inDirectory"), in_dir.contains(providers::gamedir::key_for(sys.fullname, sys.short_name)) },
        });
    }
#endif
    return out;
}

QVariantList Settings::matchSystems(const QString& folder, const QStringList& picked)
{
    QVariantList out;
#ifdef WITH_COMPAT_ESDE
    QSet<QString> found;
    for (const auto& sys : providers::esde::rom_folder_systems(folder)) {
        if (!picked.isEmpty() && !picked.contains(sys.name))
            continue;
        found.insert(sys.name);
        bool own = false, media = false;
        for (const QString& dir : sys.folders) {
            own = own || folderHasMetadata(dir);
            for (const QString& sub : { QStringLiteral("/media"), QStringLiteral("/.media"), QStringLiteral("/skraper") })
                media = media || QFileInfo(dir + sub).isDir();
        }
        out.append(QVariantMap {
            { QStringLiteral("name"), sys.name },
            { QStringLiteral("fullName"), sys.fullname },
            { QStringLiteral("folders"), sys.folders },
            { QStringLiteral("games"), sys.games },
            { QStringLiteral("ownMetadata"), own },
            { QStringLiteral("mediaFolder"), media },
            { QStringLiteral("gamelist"), sys.has_gamelist },
        });
    }
    for (const auto& sys : providers::esde::known_systems()) {
        if (!picked.contains(sys.name) || found.contains(sys.name))
            continue;
        out.append(QVariantMap {
            { QStringLiteral("name"), sys.name },
            { QStringLiteral("fullName"), sys.fullname },
            { QStringLiteral("folders"), QStringList() },
            { QStringLiteral("games"), 0 },
            { QStringLiteral("folderName"), sys.folder },
        });
    }
#else
    Q_UNUSED(folder); Q_UNUSED(picked);
#endif
    return out;
}

#ifdef WITH_COMPAT_ESDE
namespace {
QVariantMap import_result_map(const providers::esde::EsdeImportResult& result)
{
    return QVariantMap {
        { QStringLiteral("systems"), result.systems },
        { QStringLiteral("games"), result.games },
        { QStringLiteral("error"), result.error },
    };
}
} // namespace
#endif

void Settings::importSystems(const QString& folder, const QStringList& systems)
{
#ifdef WITH_COMPAT_ESDE
    QtConcurrent::run([this, folder, systems]{
        const auto result = providers::esde::rom_folder_import_only(folder, systems);
        QMetaObject::invokeMethod(this, [this, result]{ emit romFolderImportFinished(import_result_map(result)); }, Qt::QueuedConnection);
    });
#else
    Q_UNUSED(folder); Q_UNUSED(systems);
#endif
}

void Settings::importFolderAs(const QString& folder, const QString& system)
{
#ifdef WITH_COMPAT_ESDE
    QtConcurrent::run([this, folder, system]{
        const auto result = providers::esde::import_folder_as(folder, system);
        QMetaObject::invokeMethod(this, [this, result]{ emit romFolderImportFinished(import_result_map(result)); }, Qt::QueuedConnection);
    });
#else
    Q_UNUSED(folder); Q_UNUSED(system);
#endif
}

void Settings::requestOriginalGame(const QString& collection_name, const QString& game_path)
{
#ifdef WITH_COMPAT_ESDE
    QtConcurrent::run([this, collection_name, game_path]{
        const providers::esde::OriginalGame og = providers::esde::original_game(collection_name, game_path);
        QVariantMap out {
            { QStringLiteral("found"), og.found },
            { QStringLiteral("folder"), og.folder },
            { QStringLiteral("ownMetadata"), og.own_metadata },
            { QStringLiteral("gamelist"), og.gamelist },
            { QStringLiteral("details"), og.details },
            { QStringLiteral("assets"), og.assets },
            { QStringLiteral("error"), og.error },
        };
        QMetaObject::invokeMethod(this, [this, game_path, out]{ emit originalGameReady(game_path, out); }, Qt::QueuedConnection);
    });
#else
    QVariantMap out { { QStringLiteral("found"), false }, { QStringLiteral("error"), tr("Not in this build") } };
    emit originalGameReady(game_path, out);
#endif
}

void Settings::reimportSystem(const QString& key)
{
#ifdef WITH_COMPAT_ESDE
    const QString file = providers::gamedir::system_file(key);
    QtConcurrent::run([this, file]{
        const auto result = providers::esde::reimport_system(file);
        QMetaObject::invokeMethod(this, [this, result]{ emit romFolderImportFinished(import_result_map(result)); }, Qt::QueuedConnection);
    });
#else
    Q_UNUSED(key);
#endif
}

QString Settings::removeSystem(const QString& key)
{
    const QString file = providers::gamedir::system_file(key);
    if (!QFileInfo::exists(file))
        return tr("No such system in the game directory.");
    // its folders are forgotten (no longer watched), the file set aside
    QStringList folders;
    {
        QFile f(file);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!f.atEnd()) {
                const QString line = QString::fromUtf8(f.readLine()).trimmed();
                if (!line.startsWith(QLatin1Char('#')) && !line.isEmpty())
                    break;
                if (line.startsWith(QLatin1String("# folder: ")))
                    folders.append(line.mid(10).trimmed());
            }
        }
    }
#ifdef WITH_COMPAT_ESDE
    providers::esde::forget_rom_folders(folders);
#endif
    const QString aside = providers::gamedir::backup_dir(key) + QStringLiteral("/removed-")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QDir().mkpath(aside);
    if (!QFile::rename(file, aside + QStringLiteral("/metadata.pegasus.txt")))
        return tr("Couldn't move %1").arg(file);
    QDir(QFileInfo(file).absolutePath()).rmdir(QStringLiteral("."));
    Log::info(LOGMSG("Game directory: `%1` set aside in `%2`").arg(file, aside));
    // the library is read again when Game directory is left (more systems
    // may be removed first), not here
    return {};
}

QVariantList Settings::watchedRomFolders() const
{
    QVariantList out;
#ifdef WITH_COMPAT_ESDE
    for (const auto& entry : providers::esde::watched_rom_folders())
        out.append(QVariantMap { { QStringLiteral("folder"), entry.first }, { QStringLiteral("system"), entry.second } });
#endif
    return out;
}

int Settings::removeRomFolder(const QString& folder, bool remove_games)
{
    const QString prefix = QDir::cleanPath(folder) + QLatin1Char('/');
#ifdef WITH_COMPAT_ESDE
    QStringList under;
    for (const auto& entry : providers::esde::watched_rom_folders()) {
        const QString clean = QDir::cleanPath(entry.first) + QLatin1Char('/');
        if (clean == prefix || clean.startsWith(prefix))
            under.append(entry.first);
    }
    providers::esde::forget_rom_folders(under);
#endif
    int removed = 0;
    if (remove_games)
        removed = providers::gamedir::remove_folder_games(folder);
    // (the library is read again when Game directory is left)
    return removed;
}

void Settings::rescanRomFolders()
{
#ifdef WITH_COMPAT_ESDE
    QtConcurrent::run([this]{
        const int n = providers::esde::rescan_rom_folders();
        QMetaObject::invokeMethod(this, [this, n]{
            emit romFolderImportFinished(QVariantMap { { QStringLiteral("systems"), n }, { QStringLiteral("games"), 0 }, { QStringLiteral("error"), QString() } });
        }, Qt::QueuedConnection);
    });
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

namespace {
void apply_scraped(model::Game& game, const QVariantMap& changes);
} // namespace

QString Settings::saveGameMetadata(const QString& collection_name, const QString& game_path,
                                   const QString& game_title, const QVariantMap& changes, QObject* game_obj)
{
#ifdef WITH_COMPAT_ESDE
    const QString error = providers::esde::save_game_metadata(collection_name, game_path, game_title, changes);
    // the loaded game shows it right away, in every theme, without a read
    auto* const game = qobject_cast<model::Game*>(game_obj);
    if (error.isEmpty() && game) {
        apply_scraped(*game, changes);
        // art you removed: off the loaded game too
        const QStringList hidden = changes.value(QStringLiteral("x-pegasusx-hidden-art")).toStringList();
        if (!hidden.isEmpty())
            game->assetsMut().remove_listed(hidden);
    }
    return error;
#else
    Q_UNUSED(game_obj);
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
        // miximages, made on the device: their options
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
        // keyless sources: who the game is by its checksum (Hasheous), free
        // boxes/screenshots/title screens (libretro), and how sure a match
        // found by name has to be (0-100) to be used without asking
        { QStringLiteral("useHasheous"), true },
        { QStringLiteral("useLibretro"), true },
        { QStringLiteral("useLaunchBox"), true },
        { QStringLiteral("launchboxFill"), true },
        { QStringLiteral("getBanners"), true },
        { QStringLiteral("getSpines"), true },
        { QStringLiteral("minMatch"), 60 },
        { QStringLiteral("useMetadataNames"), true },
        { QStringLiteral("interactive"), false },
        { QStringLiteral("autoAcceptSingle"), true },
        // pictures: biggest size kept (ES-DE: 2560x1440, logos 1000x600), and
        // empty borders trimmed (off, logos, all)
        { QStringLiteral("imageMaxSize"), QStringLiteral("2560") },
        { QStringLiteral("trimBorders"), QStringLiteral("off") },
        // fan art kept 16:9 (as ScreenScraper's): banners onto a blurred background
        { QStringLiteral("fanartFit"), QStringLiteral("original") },
    };
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
    // the sources on offer: ScreenScraper and LaunchBox (a source from an
    // older build, IGDB or SteamGridDB, becomes LaunchBox); a build without
    // ScreenScraper starts on LaunchBox
    const QString source = out.value(QStringLiteral("source")).toString();
    const bool gone = source != QLatin1String("screenscraper") && source != QLatin1String("launchbox");
    if (gone || (source == QLatin1String("screenscraper") && !scraperHasScreenScraper() && !file.contains(SCRAPER_GROUP + QStringLiteral("source"))))
        out.insert(QStringLiteral("source"), QStringLiteral("launchbox"));
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
    auto* const pending = new int(1);
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
    bool assets = false, details = false, extra = false;
    for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
        const QString& key = it.key();
        if (kinds.contains(key)) {
            game.assetsMut().set_files(kinds.value(key), it.value().toStringList());
            assets = true;
            continue;
        }
        if (key.startsWith(QLatin1String("x-")))
            extra = true;
        else
            details = true;
        if (key == QLatin1String("x-manual"))
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
        else if (key.startsWith(QLatin1String("x-")))
            game.extraMapMut().insert(key.mid(2), it.value().toStringList().isEmpty() ? QVariant(QStringList { it.value().toString() }) : it.value());
    }
    // themes bound to the game read again only what changed (a details
    // change can re-sort a theme's lists: not for art or extra fields alone)
    if (assets)
        game.assetsMut().notifyChanged();
    if (details)
        game.notifyDataChanged();
    if (extra)
        game.notifyExtraChanged();
}

// one game for the scraper, with what it has now
scraper::Job make_scraper_job(const model::Collection& coll, model::Game* game, const QString& system, const QString& path)
{
    const auto local = [](const QString& url) {
        return url.startsWith(QLatin1String("file:")) ? QUrl(url).toLocalFile() : url;
    };
    const model::Assets& assets = game->assets();
    const model::Collection* const coll_ptr = &coll;
    scraper::Job job;
    job.collection = coll_ptr->name();
    job.media_folder = coll_ptr->shortName().isEmpty() ? coll_ptr->name() : coll_ptr->shortName();
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
        { "assets.steam", &assets.steamList() }, { "assets.banner", &assets.bannerList() },
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
                if (pegasus_assets::is_3d_box(f)) job.has_box3d = true;
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

QString Settings::scrapeCollection(QObject* collections, const QString& collection_name, const QString& filter)
{
    QVariantMap overrides;
    overrides.insert(QStringLiteral("systems"), QStringList { collection_name });
    overrides.insert(QStringLiteral("games"), filter == QLatin1String("missing") ? QStringLiteral("missing") : QStringLiteral("all"));
    overrides.insert(QStringLiteral("overwrite"), filter == QLatin1String("replace"));
    // one collection: in the background, no questions (unsure games go to Needs your check)
    overrides.insert(QStringLiteral("interactive"), false);
    overrides.insert(QStringLiteral("reviewQueue"), true);
    return startScraperWith(collections, false, overrides);
}

QString Settings::startScraperWith(QObject* collections, bool offline, const QVariantMap& overrides)
{
    if (m_scraper && m_scraper->running())
        return tr("The scraper is already running.");
    const auto* const list = qobject_cast<model::ObjectListModel*>(collections);
    if (!list)
        return tr("No library to scrape.");
    QVariantMap settings = scraperSettings();
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it)
        settings.insert(it.key(), it.value());
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
    const bool has_lb = launchBox().ready();
    if (!offline && source == QLatin1String("launchbox") && !has_lb)
        return tr("Download LaunchboxDB in Account settings first.");
    if (!offline && source == QLatin1String("screenscraper") && !has_ss && !has_lb
            && !settings.value(QStringLiteral("useLibretro")).toBool())
        return tr("ScreenScraper isn't in this build yet. Download LaunchboxDB in Account settings.");

    std::vector<scraper::Job> jobs;
    QSet<QString> seen;
    const QSet<QString> never = scraper::Scraper::neverScrape();
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
            if (seen.contains(path) || never.contains(path))
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
            m_scrape_games.insert(path, game);
        }
    }
    if (jobs.empty())
        return tr("No games to scrape with these settings.");
    m_single_game.clear();
    return runScraper(std::move(jobs), settings);
}

QStringList Settings::scraperLog() const
{
    return m_scraper ? m_scraper->log() : QStringList();
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
    // the test environment scrapes one game by itself: the first match
    if (!qEnvironmentVariable("PEGASUSX_TEST_SCRAPE").isEmpty()) {
        settings.insert(QStringLiteral("interactive"), false);
        settings.insert(QStringLiteral("autoAcceptSingle"), true);
    }
    const QString source = settings.value(QStringLiteral("source")).toString();
    // (with none: libretro's free boxes and screenshots only)
    if (!scraperHasScreenScraper() && !launchBox().ready()
            && !settings.value(QStringLiteral("useLibretro")).toBool())
        return tr("Download LaunchboxDB in Library > Scraper > Account settings first.");
    if (source == QLatin1String("launchbox") && !launchBox().ready())
        return tr("Download LaunchboxDB in Library > Scraper > Account settings first.");
    std::vector<scraper::Job> jobs;
    jobs.push_back(make_scraper_job(*coll, game, scraper::Scraper::system_of(coll->name(), coll->shortName()),
                                    game->filesModel()->entries().front()->path()));
    m_scrape_games.insert(game->filesModel()->entries().front()->path(), game);
    m_single_game = game;  // what it saves shows at once (apply_scraped)
    const QString error = runScraper(std::move(jobs), settings);
    if (!error.isEmpty())
        m_single_game.clear();
    return error;
}

// -- Settings > Library: status, Complete my library, Needs your check ---------

namespace {
bool has_real(const QStringList& files)
{
    for (const QString& f : files) {
        if (!f.startsWith(QLatin1String("image://")))
            return true;
    }
    return false;
}

// what a game misses, of the kinds Complete my library fills
struct Missing {
    bool box = false, shots = false, details = false, fanart = false, videos = false;
};
Missing missing_of(const model::Game& game)
{
    const model::Assets& a = game.assets();
    Missing m;
    m.box = !has_real(a.boxFrontList());
    m.shots = a.screenshotList().isEmpty();
    m.details = game.description().isEmpty() && game.summary().isEmpty();
    m.fanart = a.backgroundList().isEmpty();
    m.videos = a.videoList().isEmpty();
    return m;
}
} // namespace

QVariantMap Settings::libraryStatus(QObject* collections) const
{
    QVariantMap out;
    const auto* const list = qobject_cast<model::ObjectListModel*>(collections);
    int games = 0, box = 0, shots = 0, details = 0, fanart = 0, videos = 0, any = 0;
    QVariantMap per_coll;
    QSet<QString> seen;
    const QSet<QString> never = scraper::Scraper::neverScrape();
    for (int i = 0; list && i < list->count(); i++) {
        auto* const coll = qobject_cast<model::Collection*>(list->get(i));
        if (!coll)
            continue;
        int coll_missing = 0;
        for (model::Game* const game : coll->gameList()->entries()) {
            const std::vector<model::GameFile*>& files = game->filesModel()->entries();
            const QString path = files.empty() ? QString() : files.front()->path();
            const Missing m = missing_of(*game);
            const bool art_missing = m.box || m.shots;
            if (art_missing && !never.contains(path))
                coll_missing++;
            if (path.isEmpty() || seen.contains(path))
                continue;
            seen.insert(path);
            games++;
            if (never.contains(path))
                continue;
            box += m.box;
            shots += m.shots;
            details += m.details;
            fanart += m.fanart;
            videos += m.videos;
            any += (m.box || m.shots || m.details);
        }
        per_coll.insert(coll->name(), coll_missing);
    }
    const QVariantMap settings = scraperSettings();
    out.insert(QStringLiteral("games"), games);
    out.insert(QStringLiteral("box"), box);
    out.insert(QStringLiteral("shots"), shots);
    out.insert(QStringLiteral("details"), details);
    out.insert(QStringLiteral("fanart"), fanart);
    out.insert(QStringLiteral("videos"), videos);
    out.insert(QStringLiteral("any"), any);
    out.insert(QStringLiteral("collections"), per_coll);
    out.insert(QStringLiteral("review"), scraper::Scraper::reviewList().size());
    out.insert(QStringLiteral("ss"), scraperHasScreenScraper());
    out.insert(QStringLiteral("launchbox"), launchBox().ready());
    return out;
}

QVariantMap Settings::libraryHousekeeping() const
{
    const auto size = providers::gamedir::backup_size();
    QString data = dataDir();
    if (data.isEmpty())
        data = paths::writableConfigDir() + QStringLiteral("/downloaded_media");
    const providers::gamedir::Cleanup gone = providers::gamedir::clean_up(data, true);
    return QVariantMap {
        { QStringLiteral("backupFiles"), size.first },
        { QStringLiteral("backupMB"), static_cast<double>(size.second) / 1048576.0 },
        { QStringLiteral("goneGames"), gone.games },
        { QStringLiteral("goneFiles"), gone.files },
        { QStringLiteral("goneTitles"), QVariant(gone.titles.mid(0, 20)) },
    };
}

void Settings::requestLibraryHousekeeping()
{
    QtConcurrent::run([this]{
        const QVariantMap out = libraryHousekeeping();
        QMetaObject::invokeMethod(this, [this, out]{ emit libraryHousekeepingReady(out); }, Qt::QueuedConnection);
    });
}

int Settings::clearBackups()
{
    return providers::gamedir::clear_backups();
}

QVariantMap Settings::cleanUpGoneGames()
{
    QString data = dataDir();
    if (data.isEmpty())
        data = paths::writableConfigDir() + QStringLiteral("/downloaded_media");
    const providers::gamedir::Cleanup done = providers::gamedir::clean_up(data, false);
    if (done.games > 0 && done.error.isEmpty())
        reloadLibrary();
    return QVariantMap {
        { QStringLiteral("games"), done.games },
        { QStringLiteral("files"), done.files },
        { QStringLiteral("error"), done.error },
    };
}

QString Settings::completeLibrary(QObject* collections, const QVariantMap& what)
{
    if (m_scraper && m_scraper->running())
        return tr("The scraper is already running.");
    const auto* const list = qobject_cast<model::ObjectListModel*>(collections);
    if (!list)
        return tr("No library to scrape.");
    const bool want_box = what.value(QStringLiteral("boxes")).toBool();
    const bool want_shots = what.value(QStringLiteral("shots")).toBool();
    const bool want_details = what.value(QStringLiteral("details")).toBool();
    const bool want_fanart = what.value(QStringLiteral("fanart")).toBool();
    const bool want_videos = what.value(QStringLiteral("videos")).toBool();
    if (!want_box && !want_shots && !want_details && !want_fanart && !want_videos)
        return tr("Tick what to fill first.");

    QVariantMap settings = scraperSettings();
    const bool has_ss = scraperHasScreenScraper();
    // only what's empty, nothing asked: what it isn't sure about waits in Needs your check
    settings.insert(QStringLiteral("overwrite"), false);
    settings.insert(QStringLiteral("interactive"), false);
    settings.insert(QStringLiteral("reviewQueue"), true);
    settings.insert(QStringLiteral("getNames"), false);
    settings.insert(QStringLiteral("getCovers"), want_box);
    settings.insert(QStringLiteral("getScreenshots"), want_shots);
    settings.insert(QStringLiteral("getTitlescreens"), want_shots);
    settings.insert(QStringLiteral("getDetails"), want_details);
    settings.insert(QStringLiteral("getRatings"), want_details);
    settings.insert(QStringLiteral("getFanart"), want_fanart);
    settings.insert(QStringLiteral("getLogos"), want_fanart);
    settings.insert(QStringLiteral("getVideos"), want_videos);
    settings.insert(QStringLiteral("getManuals"), want_videos);
    settings.insert(QStringLiteral("getBackcovers"), false);
    settings.insert(QStringLiteral("getPhysical"), false);
    const bool has_lb = launchBox().ready();
    if (!has_ss && !has_lb && !settings.value(QStringLiteral("useLibretro")).toBool())
        return tr("Download LaunchboxDB in Scraper > Account settings first.");

    std::vector<scraper::Job> jobs;
    QSet<QString> seen;
    const QSet<QString> never = scraper::Scraper::neverScrape();
    for (int i = 0; i < list->count(); i++) {
        auto* const coll = qobject_cast<model::Collection*>(list->get(i));
        if (!coll)
            continue;
        const QString system = scraper::Scraper::system_of(coll->name(), coll->shortName());
        // no source for this system: nothing could be found
        if (!has_ss && !scraper::Scraper::hasFreeArt(system)
                && !(has_lb && launchBox().hasSystem(system)))
            continue;
        for (model::Game* const game : coll->gameList()->entries()) {
            const std::vector<model::GameFile*>& files = game->filesModel()->entries();
            if (files.empty())
                continue;
            const QString path = files.front()->path();
            if (seen.contains(path) || never.contains(path))
                continue;
            const Missing m = missing_of(*game);
            const bool take = (want_box && m.box) || (want_shots && m.shots) || (want_details && m.details)
                           || (want_fanart && m.fanart) || (want_videos && m.videos);
            if (!take)
                continue;
            seen.insert(path);
            jobs.push_back(make_scraper_job(*coll, game, system, path));
            m_scrape_games.insert(path, game);
        }
    }
    if (jobs.empty())
        return tr("Nothing is missing.");
    m_single_game.clear();
    return runScraper(std::move(jobs), settings);
}

QVariantList Settings::reviewList() const
{
    return scraper::Scraper::reviewList();
}

QString Settings::reviewScrape(QObject* collections, const QString& path, const QString& mode, int launchbox_id)
{
    if (m_scraper && m_scraper->running())
        return tr("The scraper is already running.");
    const auto* const list = qobject_cast<model::ObjectListModel*>(collections);
    model::Game* found = nullptr;
    const model::Collection* found_coll = nullptr;
    for (int i = 0; list && i < list->count() && !found; i++) {
        auto* const coll = qobject_cast<model::Collection*>(list->get(i));
        if (!coll)
            continue;
        for (model::Game* const game : coll->gameList()->entries()) {
            const std::vector<model::GameFile*>& files = game->filesModel()->entries();
            if (!files.empty() && files.front()->path() == path) {
                found = game;
                found_coll = coll;
                break;
            }
        }
    }
    if (!found)
        return tr("This game isn't in the library any more.");
    QVariantMap settings = scraperSettings();
    settings.insert(QStringLiteral("overwrite"), false);
    settings.insert(QStringLiteral("reviewQueue"), true);
    settings.insert(QStringLiteral("interactive"), mode == QLatin1String("choose"));
    // "use": the LaunchBox match it found, no questions asked
    if (mode == QLatin1String("use") && launchbox_id > 0 && launchBox().ready())
        settings.insert(QStringLiteral("source"), QStringLiteral("launchbox"));
    scraper::Job job = make_scraper_job(*found_coll, found,
        scraper::Scraper::system_of(found_coll->name(), found_coll->shortName()), path);
    m_scrape_games.insert(path, found);
    if (mode == QLatin1String("use"))
        job.launchbox_id = launchbox_id;
    std::vector<scraper::Job> jobs;
    jobs.push_back(std::move(job));
    m_single_game = found;
    const QString error = runScraper(std::move(jobs), settings);
    if (!error.isEmpty())
        m_single_game.clear();
    return error;
}

void Settings::setNeverScrape(const QString& path, const QString& title, bool never)
{
    scraper::Scraper::setNeverScrape(path, title, never);
}

// -- Pegasus X: RetroAchievements ------------------------------------------------

namespace {
const QString RA_GROUP = QStringLiteral("retroachievements/");
// a game's x-ra-* fields as loaded (extra["ra-id"]...): "" when not there
QString ra_extra(const model::Game& game, const char* key)
{
    const QVariant v = game.extraMap().value(QLatin1String(key));
    const QStringList list = v.toStringList();
    return list.isEmpty() ? v.toString() : list.first();
}
} // namespace

scraper::RetroAchievements& Settings::ra()
{
    if (!m_ra) {
        m_ra = new scraper::RetroAchievements(this);
        connect(m_ra, &scraper::RetroAchievements::progress, this, &Settings::raProgress);
        connect(m_ra, &scraper::RetroAchievements::loginChecked, this, &Settings::raLoginChecked);
        // every game written: the loaded game takes its x-ra-* fields in
        // place (themes see game.extra["ra-id"]... at once, no library read)
        connect(m_ra, &scraper::RetroAchievements::gameWritten, this, [this](const QString& path, const QVariantMap& changes){
            const QPointer<model::Game> game = m_ra_games.value(path);
            if (game)
                apply_scraped(*game, changes);
        });
        connect(m_ra, &scraper::RetroAchievements::finished, this, [this](QVariantMap result){
            m_ra_quiet = false;
            m_ra_games.clear();
            emit raFinished(result);
        });
    }
    return *m_ra;
}

QVariantMap Settings::raSettings() const
{
    const QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    return QVariantMap {
        { QStringLiteral("user"), file.value(RA_GROUP + QStringLiteral("user")).toString() },
        { QStringLiteral("key"), file.value(RA_GROUP + QStringLiteral("key")).toString() },
        { QStringLiteral("icons"), file.value(RA_GROUP + QStringLiteral("icons"), true).toBool() },
        { QStringLiteral("autoRefresh"), file.value(RA_GROUP + QStringLiteral("autoRefresh"), true).toBool() },
    };
}

void Settings::setRaSetting(const QString& key, const QVariant& value)
{
    QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    file.setValue(RA_GROUP + key, value);
}

void Settings::raCheckLogin()
{
    const QVariantMap s = raSettings();
    ra().checkLogin(s.value(QStringLiteral("user")).toString(), s.value(QStringLiteral("key")).toString());
}

QVariantMap Settings::raState() const
{
    return m_ra ? m_ra->state() : QVariantMap { { QStringLiteral("running"), false } };
}

void Settings::raStop()
{
    if (m_ra)
        m_ra->stop();
}

QVariantMap Settings::gameExtraFields(const QString& collection_name, const QString& game_path, const QString& prefix) const
{
#ifdef WITH_COMPAT_ESDE
    return providers::esde::game_extra_fields(collection_name, game_path, prefix);
#else
    Q_UNUSED(collection_name); Q_UNUSED(game_path); Q_UNUSED(prefix);
    return {};
#endif
}

void Settings::raForgetLists()
{
    QDir dir(scraper::RetroAchievements::dir());
    for (const QString& name : dir.entryList({ QStringLiteral("console-*.json") }, QDir::Files))
        dir.remove(name);
}

QString Settings::raMatch(QObject* collections, bool only_new)
{
    return raStart(collections, only_new, false, nullptr, QString());
}

QString Settings::raRefreshProgress(QObject* collections)
{
    return raStart(collections, false, true, nullptr, QString());
}

QString Settings::raCheckGame(QObject* game_obj, const QString& collection_name)
{
    auto* const game = qobject_cast<model::Game*>(game_obj);
    if (!game)
        return tr("No game to check.");
    return raStart(nullptr, false, false, game, collection_name);
}

QString Settings::raStart(QObject* collections, bool only_new, bool progress_only, model::Game* one_game, const QString& one_collection)
{
    if (m_ra && m_ra->running())
        return tr("RetroAchievements is still working.");
    const QVariantMap s = raSettings();
    const QString user = s.value(QStringLiteral("user")).toString().trimmed();
    const QString key = s.value(QStringLiteral("key")).toString().trimmed();
    if (user.isEmpty() || key.isEmpty())
        return tr("Enter your RetroAchievements username and web API key first.");
    std::vector<scraper::RetroAchievements::Job> jobs;
    QSet<QString> seen;
    const auto add = [&](const model::Collection& coll, model::Game* game) {
        const std::vector<model::GameFile*>& files = game->filesModel()->entries();
        if (files.empty())
            return;
        const QString path = files.front()->path();
        if (path.startsWith(QLatin1String("android:")) || seen.contains(path))
            return;
        const int ra_id = ra_extra(*game, "ra-id").toInt();
        const bool checked = !ra_extra(*game, "ra-id").isEmpty();
        if (progress_only && ra_id <= 0)
            return;
        if (only_new && checked)
            return;
        seen.insert(path);
        m_ra_games.insert(path, game);
        scraper::RetroAchievements::Job job;
        job.path = path;
        job.title = game->title();
        job.collection = coll.name();
        job.system = scraper::Scraper::system_of(coll.name(), coll.shortName());
        job.ra_id = progress_only ? ra_id : 0;
        jobs.push_back(std::move(job));
    };
    if (one_game) {
        const model::Collection* coll = nullptr;
        for (model::Collection* const c : one_game->collectionsModel()->entries()) {
            if (c->name() == one_collection || !coll)
                coll = c;
        }
        if (!coll)
            return tr("No game to check.");
        add(*coll, one_game);
    }
    else {
        const auto* const list = qobject_cast<model::ObjectListModel*>(collections);
        if (!list)
            return tr("No library.");
        for (int i = 0; i < list->count(); i++) {
            auto* const coll = qobject_cast<model::Collection*>(list->get(i));
            if (!coll)
                continue;
            for (model::Game* const game : coll->gameList()->entries())
                add(*coll, game);
        }
    }
    if (jobs.empty())
        return progress_only ? tr("No matched games yet: Match my games first.")
             : only_new ? tr("Every game has been checked already.") : tr("No games to check.");
    QString media = dataDir();
    if (media.isEmpty())
        media = paths::writableConfigDir() + QStringLiteral("/downloaded_media");
    ra().start(std::move(jobs), user, key, s.value(QStringLiteral("icons")).toBool(), media, progress_only);
    return {};
}

QVariantMap Settings::raStatus(QObject* collections) const
{
    QVariantMap out;
    int games = 0, checked = 0, matched = 0, with = 0, not_found = 0, earned = 0, total = 0;
    const auto* const list = qobject_cast<model::ObjectListModel*>(collections);
    QSet<QString> seen;
    for (int i = 0; list && i < list->count(); i++) {
        auto* const coll = qobject_cast<model::Collection*>(list->get(i));
        if (!coll)
            continue;
        for (model::Game* const game : coll->gameList()->entries()) {
            const std::vector<model::GameFile*>& files = game->filesModel()->entries();
            if (files.empty() || files.front()->path().startsWith(QLatin1String("android:")) || seen.contains(files.front()->path()))
                continue;
            seen.insert(files.front()->path());
            games++;
            const QString id = ra_extra(*game, "ra-id");
            if (id.isEmpty())
                continue;
            checked++;
            if (id.toInt() <= 0) {
                not_found++;
                continue;
            }
            matched++;
            const int n = ra_extra(*game, "ra-achievements").toInt();
            if (n > 0) {
                with++;
                total += n;
                earned += ra_extra(*game, "ra-earned").toInt();
            }
        }
    }
    out.insert(QStringLiteral("games"), games);
    out.insert(QStringLiteral("checked"), checked);
    out.insert(QStringLiteral("matched"), matched);
    out.insert(QStringLiteral("withAchievements"), with);
    out.insert(QStringLiteral("notFound"), not_found);
    out.insert(QStringLiteral("unsupported"), checked - matched - not_found);
    out.insert(QStringLiteral("earned"), earned);
    out.insert(QStringLiteral("total"), total);
    // the cached lists
    int lists = 0;
    QDateTime oldest;
    for (const QFileInfo& f : QDir(scraper::RetroAchievements::dir()).entryInfoList({ QStringLiteral("console-*.json") }, QDir::Files)) {
        lists++;
        if (!oldest.isValid() || f.lastModified() < oldest)
            oldest = f.lastModified();
    }
    out.insert(QStringLiteral("lists"), lists);
    out.insert(QStringLiteral("listsDate"), oldest.isValid() ? oldest.toString(QStringLiteral("yyyy-MM-dd")) : QString());
    return out;
}

void Settings::raAfterGame(model::Game* game)
{
    if (!game || !raSettings().value(QStringLiteral("autoRefresh")).toBool())
        return;
    if (ra_extra(*game, "ra-id").toInt() <= 0)
        return;
    if (m_ra && m_ra->running())
        return;
    const model::Collection* coll = game->collectionsModel()->entries().empty() ? nullptr : game->collectionsModel()->entries().front();
    if (!coll)
        return;
    // the game's loaded fields are updated in place when it's written (gameWritten)
    m_ra_quiet = true;
    if (!raStart(nullptr, false, true, game, coll->name()).isEmpty())
        m_ra_quiet = false;
}

scraper::LaunchBoxDb& Settings::launchBox() const
{
    if (!m_launchbox) {
        m_launchbox = new scraper::LaunchBoxDb(const_cast<Settings*>(this));
        connect(m_launchbox, &scraper::LaunchBoxDb::progress, this, &Settings::launchBoxProgress);
        connect(m_launchbox, &scraper::LaunchBoxDb::finished, this, &Settings::launchBoxFinished);
    }
    return *m_launchbox;
}

QVariantMap Settings::launchBoxStatus()
{
    return launchBox().status();
}

void Settings::launchBoxUpdate(QObject* collections, bool force)
{
    QStringList systems;
    const auto* const list = qobject_cast<model::ObjectListModel*>(collections);
    for (int i = 0; list && i < list->count(); i++) {
        auto* const coll = qobject_cast<model::Collection*>(list->get(i));
        if (!coll)
            continue;
        const QString system = scraper::Scraper::system_of(coll->name(), coll->shortName());
        if (!system.isEmpty() && !systems.contains(system))
            systems.append(system);
    }
    launchBox().update(systems, force);
}

void Settings::launchBoxCancel()
{
    launchBox().cancel();
}

QString Settings::runScraper(std::vector<scraper::Job>&& jobs, const QVariantMap& settings)
{
    const bool has_ss = scraperHasScreenScraper();
    QString data = dataDir();
    if (data.isEmpty())
        data = paths::writableConfigDir() + QStringLiteral("/downloaded_media");

    if (!m_scraper) {
        m_scraper = new scraper::Scraper(this);
        m_scraper->setLaunchBox(&launchBox());
        connect(m_scraper, &scraper::Scraper::progress, this, &Settings::scraperProgress);
        // every game saved: the loaded game takes the changes in place, so
        // themes show them at once and the library isn't read again
        connect(m_scraper, &scraper::Scraper::gameSaved, this, [this](const QString& path, const QVariantMap& changes){
            const QPointer<model::Game> game = m_scrape_games.value(path);
            if (game)
                apply_scraped(*game, changes);
            else
                m_scrape_unapplied++;
        });
        connect(m_scraper, &scraper::Scraper::choiceNeeded, this, &Settings::scraperNeedsChoice);
        connect(m_scraper, &scraper::Scraper::thumbReady, this, &Settings::scraperThumb);
        connect(m_scraper, &scraper::Scraper::finished, this, [this](QVariantMap state){
            m_single_game.clear();
            emit scraperFinished(state);
            // the loaded games took what was saved as it went (gameSaved);
            // only a game that isn't loaded any more needs a read
            if (m_scrape_unapplied > 0 && !m_library_open)
                reloadLibrary();
            m_scrape_unapplied = 0;
            m_scrape_games.clear();
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

void Settings::scraperThumbFirst(int index)
{
    if (m_scraper)
        m_scraper->thumbFirst(index);
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
    const QString saved = file.value(QStringLiteral("data/dir")).toString();
    // always <folder>/downloaded_media/<system>/<kind>, laid out like ES-DE's:
    // in the Pegasus X folder, or in the folder picked
    return art_dir_of(saved.isEmpty() ? paths::writableConfigDir() : saved);
}

void Settings::setDataDir(const QString& dir)
{
    QSettings file(pegasusx_settings_path(), QSettings::IniFormat);
    file.setValue(QStringLiteral("data/dir"), dir);
    emit dataDirChanged();
    // art of the old layout in it: into its downloaded_media
    move_media_folder();
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
                            const QString& kind_folder, const QString& name, const QString& short_name) const
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
        return text.trimmed();  // as the scraper names its files
    };
    // media/<system>/<kind>: the collection's short name (as the scraper)
    const QString dir = clean_data + QLatin1Char('/') + safe(short_name.isEmpty() ? collection_name : short_name)
                      + QLatin1Char('/') + safe(kind_folder);
    hide_from_gallery(clean_data);
    if (!QDir().mkpath(dir))
        return source;
    // one file per game and kind, named like the scraper names them (the
    // game's file), written over; older time-stamped copies go. Library's
    // picture views don't cache, so the new picture shows at once.
    const QString base = safe(name);
    const QString ext = QFileInfo(source).suffix();
    const QString target = dir + QLatin1Char('/') + base + (ext.isEmpty() ? QString() : QLatin1Char('.') + ext);
    static const QRegularExpression rx_stamp(QStringLiteral("^~\\d{14}$"));
    for (const QFileInfo& other : QDir(dir).entryInfoList(QDir::Files)) {
        const QString stem = other.completeBaseName();
        const bool same = stem == base || (stem.startsWith(base) && rx_stamp.match(stem.mid(base.length())).hasMatch());
        if (same)
            QFile::remove(other.absoluteFilePath());
    }
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
#ifdef WITH_COMPAT_ESDE
    // games added to a ROM folder since its list was made: in the list
    providers::esde::refresh_rom_folders();
#endif
    emit reloadAllRequested();
    reloadProviders();
}

void Settings::reloadProviders()
{
    Log::info(LOGMSG("Reloading..."));
    emit providerReloadingRequested();
}

} // namespace model
