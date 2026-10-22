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


class QNetworkAccessManager;

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
    Q_PROPERTY(bool esdeImportRunning READ esdeImportRunning NOTIFY esdeImportRunningChanged)
    Q_PROPERTY(bool mergeAndroid READ mergeAndroid WRITE setMergeAndroid NOTIFY libraryRulesChanged)
    Q_PROPERTY(QStringList androidGrantedDirs READ androidGrantedDirs NOTIFY androidDirsChanged)

    QML_CONST_PROPERTY(model::KeyEditor, keyEditor)
    QML_CONST_PROPERTY(model::Locales, locales)
    QML_CONST_PROPERTY(model::Themes, themes)
    QML_CONST_PROPERTY(model::Providers, providers)

public:
    explicit Settings(QObject* parent = nullptr);
    void postInit();

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
    bool esdeAppsAsAndroid() const;
    void setEsdeAppsAsAndroid(bool);
    bool esdeGamesAsAndroid() const;
    void setEsdeGamesAsAndroid(bool);
    bool esdeImportRunning() const { return m_esde_import_running; }
    /// Runs in the background: esdeImportProgress while it works, then esdeImportFinished
    Q_INVOKABLE void startEsdeImport(const QStringList& system_names);

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

    // Pegasus X's data folder: where art you pick (and later, scraped art) is kept
    Q_PROPERTY(QString dataDir READ dataDir WRITE setDataDir NOTIFY dataDirChanged)
    QString dataDir() const;
    void setDataDir(const QString&);
    /// Makes a folder (and the ones above it); true if it exists now
    Q_INVOKABLE bool makeDir(const QString& path) const;
    /// A copy of `source` in the data folder (<data>/<collection>/<kind>/<name>.<ext>),
    /// or `source` itself if there's no data folder, it's already in it, or copying fails
    Q_INVOKABLE QString importArt(const QString& source, const QString& collection_name,
                                  const QString& kind_folder, const QString& name) const;
    /// The first of these folders that exists, or empty
    Q_INVOKABLE QString firstExistingDir(const QStringList& paths) const;
    /// Every genre used in the library, A to Z
    Q_INVOKABLE QStringList libraryGenres(QObject* all_games) const;
    /// Reads the games again from the cache (quick), applying the changes
    Q_INVOKABLE void reloadLibrary();
    /// Where the emulator list comes from, eg. "Downloaded Oct 20, 2026"
    Q_INVOKABLE QString emulatorListInfo() const;

    QStringList androidGrantedDirs() const;
    Q_INVOKABLE void requestAndroidDir();

    Q_INVOKABLE void reloadProviders();

signals:
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

private:
    bool m_esde_import_running = false;
    QNetworkAccessManager* m_netman = nullptr;
    QVariantList m_emu_cache;
    QVariantList m_android_apps_cache;
};

} // namespace model
