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


#pragma once

#include "AppSettings.h"
#include "KeyEditor.h"
#include "Locales.h"
#include "Themes.h"
#include "Providers.h"
#include "utils/QmlHelpers.h"

#include <QObject>
#include <QPointer>
#include <vector>


class QNetworkAccessManager;

namespace model { class Game; }
class ThemeStore;
class Updater;
namespace scraper { class Scraper; struct Job; class LaunchBoxDb; }

namespace model {

/// Provides a settings interface for the frontend layer
class Settings : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool fullscreen
               READ fullscreen WRITE setFullscreen
               NOTIFY fullscreenChanged)
    Q_PROPERTY(bool mouseSupport
               READ mouseSupport WRITE setMouseSupport
               NOTIFY mouseSupportChanged)
    Q_PROPERTY(bool verifyFiles
               READ verifyFiles WRITE setVerifyFiles
               NOTIFY verifyFilesChanged)
    Q_PROPERTY(bool scanOnLaunch
               READ scanOnLaunch WRITE setScanOnLaunch
               NOTIFY scanOnLaunchChanged)
    Q_PROPERTY(bool showMissingGames
               READ showMissingGames WRITE setShowMissingGames
               NOTIFY showMissingGamesChanged)
    Q_PROPERTY(QStringList gameDirs READ gameDirs NOTIFY gameDirsChanged)
    Q_PROPERTY(bool esdeImportSupported READ esdeImportSupported CONSTANT)
    Q_PROPERTY(QString esdeImportDir READ esdeImportDir WRITE setEsdeImportDir NOTIFY esdeImportDirChanged)
    Q_PROPERTY(bool esdeAppsAsAndroid READ esdeAppsAsAndroid WRITE setEsdeAppsAsAndroid NOTIFY esdeImportDirChanged)
    Q_PROPERTY(bool esdeGamesAsAndroid READ esdeGamesAsAndroid WRITE setEsdeGamesAsAndroid NOTIFY esdeImportDirChanged)
    /// Pegasus X: for games your own metadata defines, how much of ES-DE's data is used:
    /// "media", "fill" (details you don't have), "replace"
    Q_PROPERTY(QString esdeImportUse READ esdeImportUse WRITE setEsdeImportUse NOTIFY esdeImportDirChanged)
    Q_PROPERTY(bool esdeImportRunning READ esdeImportRunning NOTIFY esdeImportRunningChanged)
    Q_PROPERTY(bool mergeAndroid READ mergeAndroid WRITE setMergeAndroid NOTIFY libraryRulesChanged)
    /// Pegasus X: a newer build looked for when Pegasus X starts (off unless turned on)
    Q_PROPERTY(bool checkUpdatesOnStart READ checkUpdatesOnStart WRITE setCheckUpdatesOnStart NOTIFY pegasusxSettingsChanged)
    /// Pegasus X: the Pegasus X folder (settings, themes, metadata, logs, art)
    Q_PROPERTY(QString rootDir READ rootDir NOTIFY pegasusxSettingsChanged)
    Q_PROPERTY(QString defaultRootDir READ defaultRootDir CONSTANT)
    Q_PROPERTY(bool rootChosen READ rootChosen NOTIFY pegasusxSettingsChanged)
    /// no folder chosen and never asked: the setup shows at the first start
    Q_PROPERTY(bool rootSetupNeeded READ rootSetupNeeded NOTIFY pegasusxSettingsChanged)
    /// the Pegasus X folder that couldn't be used at this start (SD card
    /// missing?), the app's own folder being used instead; else empty
    Q_PROPERTY(QString rootMissing READ rootMissing CONSTANT)
    Q_PROPERTY(QStringList androidGrantedDirs READ androidGrantedDirs NOTIFY androidDirsChanged)

    QML_CONST_PROPERTY(model::KeyEditor, keyEditor)
    QML_CONST_PROPERTY(model::Locales, locales)
    QML_CONST_PROPERTY(model::Themes, themes)
    QML_CONST_PROPERTY(model::Providers, providers)
    Q_PROPERTY(QObject* themeStore READ themeStore CONSTANT)
    Q_PROPERTY(QObject* updater READ updater CONSTANT)

public:
    explicit Settings(QObject* parent = nullptr);
    void postInit();
    /// older builds' <Pegasus X folder>/media -> downloaded_media
    void move_media_folder();

    QObject* themeStore();
    QObject* updater();
    bool checkUpdatesOnStart() const;
    void setCheckUpdatesOnStart(bool);
    QString rootDir() const;
    QString defaultRootDir() const;
    bool rootChosen() const;
    bool rootSetupNeeded() const;
    QString rootMissing() const;
    Q_INVOKABLE void setRootSetupAsked();
    /// The first-start setup's step (1-4), or 0 when it's done (kept outside
    /// the Pegasus X folder, so picking one in step 1 doesn't lose it)
    Q_INVOKABLE int wizardStep() const;
    Q_INVOKABLE void setWizardStep(int step);
    /// Makes `dir` the Pegasus X folder (from the next start); with `move_data`,
    /// what's in the folder used now is copied there first. Returns what's
    /// wrong right away, or "" when the work started: rootDirChanged tells how
    /// it went (the copy runs in the background).
    Q_INVOKABLE QString setRootDir(const QString& dir, bool move_data);
    /// how much is in the folder used now (files, MB), for the setup
    Q_INVOKABLE QVariantMap rootDirContents() const;
    Q_INVOKABLE void quitApp();
    Q_INVOKABLE void closeMenu() { emit closeMenuRequested(); }
    /// the screen asked for with --pegasusx-open (testing), or empty
    Q_INVOKABLE QString startupScreen() const;
    /// the game the test environment scrapes at startup (PEGASUSX_TEST_SCRAPE), or empty
    Q_INVOKABLE QString testScrapeTitle() const;
    /// the test environment: text written to PEGASUSX_TEST_DUMP (the art each game has)
    Q_INVOKABLE void testDump(const QString& text) const;

    bool fullscreen() const { return AppSettings::general.fullscreen; }
    void setFullscreen(bool);

    bool mouseSupport() const { return AppSettings::general.mouse_support; }
    void setMouseSupport(bool);

    bool verifyFiles() const { return AppSettings::general.verify_files; }
    void setVerifyFiles(bool);

    bool scanOnLaunch() const { return AppSettings::general.scan_on_launch; }
    void setScanOnLaunch(bool);

    bool showMissingGames() const { return AppSettings::general.show_missing_games; }
    void setShowMissingGames(bool);

    QStringList gameDirs() const;
    Q_INVOKABLE void addGameDir(const QString&);
    Q_INVOKABLE void removeGameDirs(const QVariantList&);

    // Pegasus X: ES-DE import
    bool esdeImportSupported() const;
    QString esdeImportDir() const;
    void setEsdeImportDir(const QString&);
    Q_INVOKABLE QVariantList esdeImportSystems() const;
    QString esdeImportUse() const;
    void setEsdeImportUse(const QString&);
    bool esdeAppsAsAndroid() const;
    void setEsdeAppsAsAndroid(bool);
    bool esdeGamesAsAndroid() const;
    void setEsdeGamesAsAndroid(bool);
    bool esdeImportRunning() const { return m_esde_import_running; }
    /// Runs in the background: esdeImportProgress while it works, then esdeImportFinished
    Q_INVOKABLE void startEsdeImport(const QStringList& system_names);
    /// Pegasus X: a ROM folder's systems with games ([{ name, fullname, games, folders, hasGamelist, listed }])
    Q_INVOKABLE QVariantList romFolderSystems(const QString& folder);
    /// whether a folder has a Pegasus metadata file of its own
    Q_INVOKABLE bool folderHasMetadata(const QString& folder);
    /// Makes a ROM folder's game lists (background: romFolderImportFinished)
    Q_INVOKABLE void importRomFolder(const QString& folder);

    // Pegasus X: Settings > Game directory
    /// the systems in the game directory: [{ key, name, shortName, games, folders, source, file }]
    Q_INVOKABLE QVariantList gameDirectorySystems() const;
    /// every system known: [{ name, fullName, shortName, folder, extensions, inDirectory }]
    Q_INVOKABLE QVariantList knownSystems() const;
    /// a ROM folder's systems among those picked: [{ name, fullName, folders, games,
    /// ownMetadata, mediaFolder, gamelist }] plus the picked ones without a folder (games 0)
    Q_INVOKABLE QVariantList matchSystems(const QString& folder, const QStringList& picked);
    /// imports these systems of a ROM folder (background: romFolderImportFinished)
    Q_INVOKABLE void importSystems(const QString& folder, const QStringList& systems);
    /// imports one folder as one system (background: romFolderImportFinished)
    Q_INVOKABLE void importFolderAs(const QString& folder, const QString& system);
    /// reads a system's folders again (background: romFolderImportFinished)
    Q_INVOKABLE void reimportSystem(const QString& key);
    /// sets a system's file aside in backup/<system>/ and forgets its folders
    Q_INVOKABLE QString removeSystem(const QString& key);
    /// the ROM folders watched for new games: [{ folder, system, systems }]
    Q_INVOKABLE QVariantList watchedRomFolders() const;
    /// reads every watched folder again (background: romFolderImportFinished)
    Q_INVOKABLE void rescanRomFolders();
    /// a ROM folder (and the system folders in it) no longer watched; with
    /// `remove_games`, its games leave the game directory (set aside in
    /// backup/). Returns how many games left
    Q_INVOKABLE int removeRomFolder(const QString& folder, bool remove_games);

    // Pegasus X: emulators
    Q_INVOKABLE QVariantList emulatorCollections() const;
    /// Same as emulatorCollections(), in the background: emulatorCollectionsReady
    Q_INVOKABLE void requestEmulatorCollections();
    /// Same as esdeImportSystems(), in the background: esdeImportSystemsReady
    Q_INVOKABLE void requestEsdeImportSystems();
    /// Changes the launch command of a loaded game right away (the metadata
    /// file is changed too, so this lasts until the next scan reads it)
    Q_INVOKABLE void setLiveLaunchCommand(QObject* game, const QString& command);
    Q_INVOKABLE QVariantMap gameEmulators(const QString& collection_name) const;
    /// The command the metadata had before Pegasus X changed it:
    /// { known: bool, command: string }; game_path empty for the collection
    Q_INVOKABLE QVariantMap originalLaunch(const QString& collection_name, const QString& game_path) const;
    Q_INVOKABLE QString setCollectionEmulator(const QString& collection_name, const QString& command);
    Q_INVOKABLE QString setGameEmulator(const QString& collection_name, const QString& game_path,
                                        const QString& game_title, const QString& command);
    Q_INVOKABLE QString testEmulatorCommand(const QString& command, const QString& game_path) const;

    /// Downloads ES-DE's latest Android emulator lists (emulatorListUpdated
    /// when done); they're used instead of the copy built into the app
    Q_INVOKABLE void updateEmulatorList();

    // Pegasus X: Android collections
    Q_INVOKABLE QVariantList androidApps() const;
    Q_INVOKABLE QVariantMap androidGameApps(const QString& collection_name) const;
    Q_INVOKABLE QString androidAppCommand(const QString& component) const;
    Q_INVOKABLE int matchAndroidGames(const QString& collection_name);
    Q_INVOKABLE int addAndroidGames(const QString& collection_name);
    // Emulators screen, kept responsive: reads and writes happen in the
    // background, one at a time and in order, and report back with signals
    /// The last list of emulatorCollectionsReady, to show at once
    Q_INVOKABLE QVariantList cachedEmulatorCollections() const { return m_emu_cache; }
    /// The games of a collection as { title, path, obj }, read in C++
    Q_INVOKABLE QVariantList collectionGames(QObject* collections, const QString& collection_name) const;
    /// collectionDetailsReady: { gameCommands, gameApps, apps, original, gameOriginals }
    Q_INVOKABLE void requestCollectionDetails(const QString& collection_name, bool android);
    /// Writes a launch command (game_path empty: the collection's); emulatorSaved
    Q_INVOKABLE void saveEmulator(const QString& collection_name, const QString& game_path,
                                  const QString& game_title, const QString& command);
    /// action "match" or "add"; androidGamesDone
    Q_INVOKABLE void requestAndroidGames(const QString& action, const QString& collection_name);
    /// The same command for many loaded games at once
    Q_INVOKABLE void setLiveLaunchCommands(const QVariantList& games, const QString& command);
    // Library: hidden games and collections, one Android collection
    bool mergeAndroid() const;
    void setMergeAndroid(bool);
    /// { collections: [names], games: [{ path, title, collections }] }
    Q_INVOKABLE QVariantMap hiddenItems() const;
    /// What a game is hidden by: its first file's path, or its URI
    Q_INVOKABLE QString gameKey(QObject* game) const;
    Q_INVOKABLE void setCollectionHidden(const QString& name, bool hidden);
    Q_INVOKABLE void setGameHidden(const QString& path, const QString& title,
                                   const QStringList& collections, bool hidden);
    /// Android entries moved to "Game" / "Application" / "Emulator", by gameKey
    Q_INVOKABLE QVariantMap androidKinds() const;
    /// Moves an Android entry to a section; an empty kind: back to automatic
    Q_INVOKABLE void setAndroidKind(const QString& key, const QString& kind);
    /// Puts a moved Android entry in its section right away, without reading
    /// the library again: its tags, and with Merge off, Android or Android
    /// Games. False when that can't be done live (back to automatic, or no
    /// Android Games collection yet): the library is read again instead.
    Q_INVOKABLE bool applyAndroidKind(QObject* collections, QObject* game, const QString& kind);
    /// { command, package } a game launches with now (for Android entries, the app)
    Q_INVOKABLE QVariantMap gameLaunchInfo(QObject* game) const;
    /// Removes the ES-DE import files of these folders; returns how many
    Q_INVOKABLE int deleteEsdeImport(const QStringList& folders);
    // Edit metadata
    /// { file, isImport, isNew } of where a game (or with an empty path, the collection) is saved
    Q_INVOKABLE QVariantMap metadataLocation(const QString& collection_name, const QString& game_path) const;
    /// Writes changed fields (see EsdeProvider.h); returns an error, or empty
    Q_INVOKABLE QString saveGameMetadata(const QString& collection_name, const QString& game_path,
                                         const QString& game_title, const QVariantMap& changes);
    Q_INVOKABLE QString saveCollectionMetadata(const QString& collection_name, const QVariantMap& changes);
    /// { shortname, extensions, launch, defaultLabel, defaultCommand } of a collection
    Q_INVOKABLE QVariantMap collectionDetails(const QString& collection_name) const;
    /// The emulators of a collection's system: [{ label, command, installed, recommended, retroarch }]
    Q_INVOKABLE QVariantList emulatorChoices(const QString& collection_name, const QString& short_name,
                                             const QString& rom_root) const;
    /// Saves an emulator of yours (a name and a command) for a collection's
    /// system; with `remove`, forgets it. Returns an error, or empty.
    Q_INVOKABLE QString saveCustomEmulator(const QString& collection_name, const QString& short_name,
                                           const QString& label, const QString& command, bool remove);
    /// Days since ES-DE's emulator list was downloaded, -1 if never
    Q_INVOKABLE int emulatorListAgeDays() const;

    // Scraper (Settings > Library > Scraper)
    /// Every scraper setting, with its default when it was never set
    Q_INVOKABLE QVariantMap scraperSettings() const;
    Q_INVOKABLE void setScraperSetting(const QString& key, const QVariant& value);
    /// Whether this build has the app's ScreenScraper developer login
    Q_INVOKABLE bool scraperHasScreenScraper() const;
    /// Asks ScreenScraper and SteamGridDB whether the logins work
    /// (scraperAccountsChecked)
    Q_INVOKABLE void checkScraperAccounts();
    /// Scrapes the games the Scraper settings pick (systems and filter);
    /// returns an error, or empty when it started (scraperProgress, scraperFinished)
    Q_INVOKABLE QString startScraper(QObject* collections);
    Q_INVOKABLE QString startMiximageGenerator(QObject* collections);
    Q_INVOKABLE void stopScraper();
    /// Interactive mode: the match picked (index in the choice's candidates),
    /// a new search name, or this game skipped
    Q_INVOKABLE void scraperChoose(int index);
    Q_INVOKABLE void scraperRefine(const QString& search_name);
    Q_INVOKABLE void scraperSkip();
    /// the picker highlighted a match: its picture is fetched next
    Q_INVOKABLE void scraperThumbFirst(int index);
    /// The choice waiting for a pick (see scraperNeedsChoice), or empty
    Q_INVOKABLE QVariantMap scraperPendingChoice() const;
    /// Scrapes one game (Library's game page) with the Scraper settings;
    /// `overwrite`: replace what it has, else only fill what's empty
    Q_INVOKABLE QString scrapeGame(QObject* game, const QString& collection_name, bool overwrite);
    /// { running, done, total, current, title, result, ok, saved, notFound, failed, stopReason }
    Q_INVOKABLE QVariantMap scraperState() const;

    // Pegasus X: the LaunchBox database (a source without a login)
    /// { present, date, size, platforms, games, working, stage, progress, error }
    Q_INVOKABLE QVariantMap launchBoxStatus();
    /// downloads it (when missing or old, or `force`) and reads it in for the
    /// library's systems (launchBoxProgress, launchBoxFinished)
    Q_INVOKABLE void launchBoxUpdate(QObject* collections, bool force);
    Q_INVOKABLE void launchBoxCancel();
    /// the test environment: read the LaunchBox database in before the test scrape
    Q_INVOKABLE bool testLaunchBox() const { return qEnvironmentVariable("PEGASUSX_TEST_LAUNCHBOX") == QLatin1String("1"); }

    // Pegasus X: Settings > Library's status rows
    /// { games, box, shots, details, fanart, videos, any (games missing what
    /// Complete my library fills by default), review, collections: { name: missing art },
    /// igdb, sgdb, ss (logins there) }
    Q_INVOKABLE QVariantMap libraryStatus(QObject* collections) const;
    /// Pegasus X: backups (backup/<system>/) and games whose files are gone
    /// { backupFiles, backupMB, goneGames, goneFiles, goneTitles }
    Q_INVOKABLE QVariantMap libraryHousekeeping() const;
    Q_INVOKABLE int clearBackups();
    /// sets aside the entries and scraped art of games whose files are gone
    /// (into backup/<system>/cleanup/); the library is read again
    Q_INVOKABLE QVariantMap cleanUpGoneGames();
    /// Complete my library: only what's missing, the kinds ticked in `what`
    /// { boxes, shots, details, fanart, videos }; games it isn't sure about go
    /// to Needs your check. Returns an error, or empty when it started
    Q_INVOKABLE QString completeLibrary(QObject* collections, const QVariantMap& what);
    /// Needs your check: [{ path, title, collection, system, closest, info, score, igdb }]
    Q_INVOKABLE QVariantList reviewList() const;
    /// a game there: "use" its closest match (IGDB id), or "choose" from the matches
    Q_INVOKABLE QString reviewScrape(QObject* collections, const QString& path, const QString& mode, int igdb_id);
    /// never scraped again (Complete my library, the Scraper's runs)
    Q_INVOKABLE void setNeverScrape(const QString& path, const QString& title, bool never);
    /// Pegasus X: what the scraper did lately (Library > Scraper > Log)
    Q_INVOKABLE QStringList scraperLog() const;
    /// Settings > Library is open: it reads the library again when you leave;
    /// otherwise a finished scrape does it
    Q_PROPERTY(bool libraryOpen MEMBER m_library_open)

    // Pegasus X's data folder: where art you pick (and later, scraped art) is kept
    Q_PROPERTY(QString dataDir READ dataDir WRITE setDataDir NOTIFY dataDirChanged)
    /// The folder a file browser was last used in (its "Last folder" jump)
    Q_PROPERTY(QString lastBrowseFolder READ lastBrowseFolder WRITE setLastBrowseFolder NOTIFY lastBrowseFolderChanged)
    QString lastBrowseFolder() const;
    void setLastBrowseFolder(const QString&);
    /// The last 5 folders file browsers were used in, newest first
    Q_PROPERTY(QStringList recentBrowseFolders READ recentBrowseFolders NOTIFY lastBrowseFolderChanged)
    QStringList recentBrowseFolders() const;
    /// Asked once whether to set up the data folder
    Q_PROPERTY(bool dataDirAsked READ dataDirAsked WRITE setDataDirAsked NOTIFY dataDirChanged)
    bool dataDirAsked() const;
    void setDataDirAsked(bool);
    QString dataDir() const;
    void setDataDir(const QString&);
    /// Makes a folder (and the ones above it); true if it exists now
    Q_INVOKABLE bool makeDir(const QString& path) const;
    /// A copy of `source` in the data folder (<data>/<collection>/<kind>/<name>.<ext>),
    /// or `source` itself if there's no data folder, it's already in it, or copying fails
    Q_INVOKABLE QString importArt(const QString& source, const QString& collection_name,
                                  const QString& kind_folder, const QString& name, const QString& short_name) const;
    /// The first of these folders that exists, or empty
    Q_INVOKABLE QString firstExistingDir(const QStringList& paths) const;
    /// Every genre used in the library, A to Z
    Q_INVOKABLE QStringList libraryGenres(QObject* all_games) const;
    /// Reads the games again from the cache (quick), applying the changes
    Q_INVOKABLE void reloadLibrary();
    /// Settings' "Reload all games": scans again and lands on the theme
    /// (reloadAllRequested, then the scan), the menu closed
    Q_INVOKABLE void reloadAllGames();
    /// The current theme's own art for a system, found by the collection's
    /// short name (or name) in the theme's folder: { logo, background } paths
    /// (empty when the theme has none, or isn't a folder on disk)
    Q_INVOKABLE QVariantMap themeSystemArt(const QString& collection_name, const QString& short_name) const;
    /// Where the emulator list comes from, eg. "Downloaded Oct 20, 2026"
    Q_INVOKABLE QString emulatorListInfo() const;

    QStringList androidGrantedDirs() const;
    Q_INVOKABLE void requestAndroidDir();

    Q_INVOKABLE void reloadProviders();

signals:
    void pegasusxSettingsChanged();
    /// setRootDir finished: `error` is empty when the folder is set
    void rootDirChanged(QString error);
    /// Pegasus X: the theme in use was updated in the theme downloader: built again
    void currentThemeUpdated();
    /// Pegasus X: the menu closes, back to the theme (eg. "Use it now")
    void closeMenuRequested();
    /// { ss: text, ssOk, sgdb: text, sgdbOk }
    void scraperAccountsChecked(QVariantMap result);
    void scraperProgress(QVariantMap state);
    /// { title, collection, searchName, source, candidates: [...] }
    void scraperNeedsChoice(QVariantMap choice);
    /// a match's picture arrived (url: a local file) or didn't (error)
    void scraperThumb(int index, QString url, int error);
    void reloadAllRequested();
    void scraperFinished(QVariantMap state);
    void launchBoxProgress(QVariantMap status);
    void launchBoxFinished(QVariantMap status);
    void fullscreenChanged();
    void mouseSupportChanged();
    void verifyFilesChanged();
    void scanOnLaunchChanged();
    void showMissingGamesChanged();
    void gameDirsChanged();
    void esdeImportDirChanged();
    void esdeImportRunningChanged();
    void esdeImportProgress(int done, int total, QString system);
    void esdeImportFinished(QVariantMap result);
    void romFolderImportFinished(QVariantMap result);
    void emulatorCollectionsReady(QVariantList collections);
    void collectionDetailsReady(QString collection, QVariantMap details);
    void emulatorSaved(QString collection, QString gamePath, QString error);
    void androidGamesDone(QString action, QString collection, int count);
    void emulatorListUpdated(bool ok, QString message);
    void esdeImportSystemsReady(QVariantList systems);
    void androidDirsChanged();
    void providerReloadingRequested();
    void libraryReloadRequested();
    void libraryRulesChanged();
    void dataDirChanged();
    void lastBrowseFolderChanged();

private:
    bool m_esde_import_running = false;
    QNetworkAccessManager* m_netman = nullptr;
    scraper::Scraper* m_scraper = nullptr;
    mutable scraper::LaunchBoxDb* m_launchbox = nullptr;
    scraper::LaunchBoxDb& launchBox() const;
    QPointer<model::Game> m_single_game;
    ThemeStore* m_theme_store = nullptr;
    Updater* m_updater = nullptr;
    QString startScraperWith(QObject* collections, bool offline);  ///< scraped alone: updated when done
    QString runScraper(std::vector<scraper::Job>&& jobs, const QVariantMap& settings);
    bool m_library_open = false;
    QVariantList m_emu_cache;
    QVariantList m_android_apps_cache;
};

} // namespace model
