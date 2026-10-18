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
#include <QVariantList>
#include <QVariantMap>


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
    Q_PROPERTY(QStringList androidGrantedDirs READ androidGrantedDirs NOTIFY androidDirsChanged)

    // Pegasus X library
    Q_PROPERTY(bool pegasusxLibrarySupported READ pegasusxLibrarySupported CONSTANT)
    Q_PROPERTY(QStringList libraryRomDirs READ libraryRomDirs NOTIFY libraryChanged)
    Q_PROPERTY(QStringList libraryMediaDirs READ libraryMediaDirs NOTIFY libraryChanged)
    Q_PROPERTY(QStringList esdeImports READ esdeImports NOTIFY libraryChanged)
    Q_PROPERTY(QStringList ignoredDirs READ ignoredDirs NOTIFY libraryChanged)
    Q_PROPERTY(QStringList hiddenGames READ hiddenGames NOTIFY libraryChanged)
    Q_PROPERTY(QString screenAspect READ screenAspect WRITE setScreenAspect NOTIFY screenAspectChanged)

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

    QStringList androidGrantedDirs() const;
    Q_INVOKABLE void requestAndroidDir();

    Q_INVOKABLE void reloadProviders();

    // Pegasus X library
    bool pegasusxLibrarySupported() const;
    QStringList libraryRomDirs() const;
    QStringList libraryMediaDirs() const;
    QStringList esdeImports() const;
    QStringList ignoredDirs() const;
    Q_INVOKABLE void addLibraryRomDir(const QString&);
    Q_INVOKABLE void removeLibraryRomDirs(const QVariantList&);
    Q_INVOKABLE void addLibraryMediaDir(const QString&);
    Q_INVOKABLE void removeLibraryMediaDirs(const QVariantList&);
    Q_INVOKABLE void addEsdeImport(const QString&);
    Q_INVOKABLE void removeEsdeImports(const QVariantList&);
    Q_INVOKABLE void addIgnoredDir(const QString&);
    Q_INVOKABLE void removeIgnoredDirs(const QVariantList&);
    /// [{ name, fullname, current, defaultLabel, options: [{ label, installed }] }]
    Q_INVOKABLE QVariantList emulatorSystems() const;
    Q_INVOKABLE void setSystemEmulator(const QString& system_name, const QString& label);
    /// installed apps, for "Other installed app": [{ label, package }]
    Q_INVOKABLE QVariantList installedApps() const;
    /// launches the system's first game with its chosen emulator: { game, error }
    Q_INVOKABLE QVariantMap testSystemEmulator(const QString& system_name) const;

    // Pegasus X library hub
    QStringList hiddenGames() const;
    Q_INVOKABLE void hideGame(const QString& path);
    Q_INVOKABLE void unhideGame(const QString& path);
    Q_INVOKABLE void removeIgnoredDir(const QString& path);
    Q_INVOKABLE void saveCustomEmulator(const QString& system_name, const QString& old_name,
                                        const QString& name, const QString& command);
    Q_INVOKABLE void removeCustomEmulator(const QString& system_name, const QString& name);
    Q_INVOKABLE void setCollectionNameSource(const QString& system_name, const QString& source);
    Q_INVOKABLE QVariantMap testEmulatorCommand(const QString& system_name, const QString& command) const;
    Q_INVOKABLE QString openSystemEmulator(const QString& system_name) const;
    /// "package/activity" of an installed app's screen that opens game files
    Q_INVOKABLE QString appGameActivity(const QString& package_name) const;

    QString screenAspect() const;
    void setScreenAspect(const QString&);

signals:
    void fullscreenChanged();
    void mouseSupportChanged();
    void verifyFilesChanged();
    void scanOnLaunchChanged();
    void showMissingGamesChanged();
    void gameDirsChanged();
    void androidDirsChanged();
    void providerReloadingRequested();
    void libraryChanged();
    void screenAspectChanged();
};

} // namespace model
