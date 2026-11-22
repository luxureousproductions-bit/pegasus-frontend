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

#include <QObject>

class QQmlApplicationEngine;


/// Manages the dynamic reload of the frontend layer
///
/// When we launch a game, the frontend stack will be teared down to save
/// resources. However, this happens asyncronously (see QObject destructor).
/// When it's done, the relevant signal will be triggered. After the actual
/// execution is finished, the frontend layer can be rebuilt again.
///
/// Some funtions require a pointer to the API object, to connect and make
/// it accessible to the frontend.
class FrontendLayer : public QObject {
    Q_OBJECT

public:
    explicit FrontendLayer(QObject* const api_public, QObject* const api_private, QObject* parent = nullptr);
    ~FrontendLayer() override;

    void rebuild();
    void teardown();

    void clearCache();
    bool isLoaded() const { return m_engine != nullptr; }

    /// Pegasus X: while Pegasus X is out of sight (a game runs) or Android is
    /// short of memory, the loaded theme stays as it is, but what the window
    /// and the QML engine only keep as a cache is let go: the scene graph's
    /// unused graphics (QQuickWindow::releaseResources) and the components
    /// nothing uses (trimComponentCache). `why` goes in the log.
    void releaseMemory(const QString& why);
    /// Pegasus X: Android's onTrimMemory warnings (Android 13 sends them well
    /// before the last-ditch onLowMemory). Never unloads the theme, so the
    /// return stays instant:
    ///  - in front, memory low (RUNNING_LOW): the QML engine's garbage is
    ///    collected and its unused components let go; critical
    ///    (RUNNING_CRITICAL): releaseMemory too (only unused graphics go)
    ///  - out of sight (UI_HIDDEN): nothing more (going to the background
    ///    already released, applicationStateChanged)
    ///  - in the background list (BACKGROUND, MODERATE, COMPLETE): garbage
    ///    collected and releaseMemory, once per level while away; not if
    ///    Pegasus X is already back in front when Qt gets to it (Qt doesn't
    ///    run while Android has it in the background)
    void trimMemory(int level);

signals:
    void rebuildComplete();
    void teardownComplete();

private:
    QObject* const m_api_public;
    QObject* const m_api_private;
    QQmlApplicationEngine* m_engine;
    int m_trimmed_level = 0;  ///< the highest trim level handled since Pegasus X was last in front
};
