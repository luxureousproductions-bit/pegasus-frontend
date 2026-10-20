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
    Q_PROPERTY(bool esdeCombineAndroid READ esdeCombineAndroid WRITE setEsdeCombineAndroid NOTIFY esdeImportDirChanged)
    Q_PROPERTY(bool esdeImportRunning READ esdeImportRunning NOTIFY esdeImportRunningChanged)
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
    bool esdeCombineAndroid() const;
    void setEsdeCombineAndroid(bool);
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
    Q_INVOKABLE QString setCollectionEmulator(const QString& collection_name, const QString& command);
    Q_INVOKABLE QString setGameEmulator(const QString& collection_name, const QString& game_path,
                                        const QString& game_title, const QString& command);
    Q_INVOKABLE QString testEmulatorCommand(const QString& command, const QString& game_path) const;

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
    void esdeImportSystemsReady(QVariantList systems);
    void androidDirsChanged();
    void providerReloadingRequested();

private:
    bool m_esde_import_running = false;
};

} // namespace model
