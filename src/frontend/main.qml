// Pegasus Frontend
// Copyright (C) 2017-2018  Mátyás Mustoha
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


import "qrc:/qmlutils" as PegasusUtils
import QtQuick 2.8
import QtQuick.Window 2.2


Window {
    id: appWindow
    visible: true
    width: 1280
    height: 720
    title: "Pegasus"
    color: "#000"

    visibility: Internal.settings.fullscreen
                ? Window.FullScreen : Window.AutomaticVisibility

    onClosing: {
        theme.source = "";
        Internal.system.quit();
    }

    FontLoader { id: sansFont; source: "/fonts/Roboto-Regular.ttf" }
    FontLoader { id: sansBoldFont; source: "/fonts/Roboto-Bold.ttf" }
    FontLoader { id: monoFont; source: "/fonts/RobotoMono-Regular.ttf" }
    FontLoader { id: condensedFont; source: "/fonts/RobotoCondensed-Regular.ttf" }
    FontLoader { id: condensedBoldFont; source: "/fonts/RobotoCondensed-Bold.ttf" }


    // a globally avalable utility object
    QtObject {
        id: global

        readonly property real winScale: Math.min(width / 1280.0, height / 720.0)

        property QtObject fonts: QtObject {
            readonly property string sans: sansFont.name
            readonly property string sansBold: sansBoldFont.name
            readonly property string condensed: condensedFont.name
            readonly property string condensedBold: condensedBoldFont.name
            readonly property string mono: monoFont.name
        }
    }


    // legacy global objects
    QtObject {
        id: globalFonts
        readonly property string sans: global.fonts.sans
        readonly property string condensed: global.fonts.condensed
    }
    function vpx(value) {
        return global.winScale * value;
    }


    // the main content
    FocusScope {
        id: content
        anchors.fill: parent
        enabled: focus

        Loader {
            id: theme
            anchors.fill: parent

            focus: true
            enabled: focus

            readonly property url apiThemePath: Internal.settings.themes.currentQmlPath

            function getThemeFile() {
                if (Internal.scanner.running)
                    return "";
                if (api.collections.count === 0)
                    return "messages/NoGamesError.qml";

                return apiThemePath;
            }
            // Pegasus X: changing themes used to build the new one right away,
            // under the open menu, while the old one was still being torn
            // down (its videos and pictures still held). Now the old theme
            // goes first, its memory is collected, and the new one is built
            // once you leave the menu.
            property bool swapPending: false
            onApiThemePathChanged: {
                source = "";
                swapPending = true;
                themeSwap.restart();
            }
            function finishSwap() {
                if (!swapPending || mainMenu.focus)
                    return;
                swapPending = false;
                gc();
                splashScreen.restoreMenu = false;
                splashScreen.focus = true;  // until the new theme is ready
                source = Qt.binding(getThemeFile);
            }
            Timer {
                id: themeSwap
                interval: 120
                onTriggered: {
                    gc();
                    theme.finishSwap();
                }
            }

            Keys.onPressed: {
                if (api.keys.isCancel(event) || api.keys.isMenu(event)) {
                    event.accepted = true;
                    mainMenu.focus = true;
                }

                if (event.key === Qt.Key_F5) {
                    event.accepted = true;

                    theme.source = "";
                    Internal.meta.clearQMLCache();
                    theme.source = Qt.binding(getThemeFile);
                }
            }

            source: getThemeFile()
            asynchronous: true
            onStatusChanged: {
                if (status == Loader.Error)
                    source = "messages/ThemeError.qml";
            }
            onLoaded: item.focus = focus
            onFocusChanged: if (item) item.focus = focus
        }

        Loader {
            id: mainMenu
            anchors.fill: parent

            source: "MenuLayer.qml"
            asynchronous: true

            onLoaded: item.focus = focus
            onFocusChanged: if (item) item.focus = focus
            enabled: focus
        }
        Connections {
            target: mainMenu.item

            function onClose() {
                theme.focus = true;
                theme.finishSwap();  // a theme picked in the menu loads now
            }

            function onRequestShutdown() {
                powerDialog.source = "dialogs/ShutdownDialog.qml"
                powerDialog.focus = true;
            }
            function onRequestSuspend() {
                Internal.system.suspend()
            }                        
            function onRequestReboot() {
                powerDialog.source = "dialogs/RebootDialog.qml"
                powerDialog.focus = true;
            }
            function onRequestQuit() {
                theme.source = "";
                Internal.system.quit();
            }
        }
        PegasusUtils.HorizontalSwipeArea {
            id: menuSwipe

            width: vpx(40)
            height: parent.height
            anchors.right: parent.right

            onSwipeLeft: {
                if (!mainMenu.focus)
                    mainMenu.focus = true;
            }
        }
    }


    Loader {
        id: powerDialog
        anchors.fill: parent
    }
    Connections {
        target: powerDialog.item
        function onCancel() { content.focus = true; }
    }


    Loader {
        id: multifileSelector
        anchors.fill: parent
    }
    Connections {
        target: multifileSelector.item
        function onCancel() { content.focus = true; }
    }


    Loader {
        id: genericMessage
        anchors.fill: parent
    }
    Connections {
        target: genericMessage.item
        function onClose() { content.focus = true; }
    }


    Connections {
        target: api
        function onEventSelectGameFile(game) {
            multifileSelector.setSource("dialogs/MultifileSelector.qml", {"game": game})
            multifileSelector.focus = true;
        }
        function onEventLaunchError(msg) {
            genericMessage.setSource("dialogs/GenericOkDialog.qml",
                { "title": qsTr("Error"), "message": msg });
            genericMessage.focus = true;
        }
    }
    // Pegasus X: "Reload all games" lands on the theme (its home screen), the
    // menu closed; other reloads from the settings give the menu back
    Connections {
        target: Internal.settings
        function onReloadAllRequested() { splashScreen.toTheme = true; }
    }
    Connections {
        target: Internal.scanner
        function onRunningChanged() {
            if (Internal.scanner.running) {
                // Pegasus X: remember if the menu was open (eg. a reload from
                // the settings), to give it back the focus afterwards
                if (!splashScreen.focus)
                    splashScreen.restoreMenu = mainMenu.focus && !splashScreen.toTheme;
                splashScreen.focus = true;
            }
        }
    }


    SplashLayer {
        id: splashScreen
        focus: true
        enabled: false
        visible: focus

        readonly property bool dataLoading: Internal.scanner.running
        readonly property bool skinLoading: theme.status === Loader.Null || theme.status === Loader.Loading

        showDataProgressText: dataLoading
        progress: Internal.scanner.progress
        stage: Internal.scanner.stage

        property bool restoreMenu: false
        property bool toTheme: false

        function hideMaybe() {
            if (focus && !dataLoading && !skinLoading) {
                content.focus = true;
                // The theme reloads under the open menu and may take the focus
                // while doing so, leaving the menu visible but deaf to the keys
                if (restoreMenu)
                    mainMenu.forceActiveFocus();
                restoreMenu = false;
                toTheme = false;
                Internal.scanner.reset();
            }
        }

        onSkinLoadingChanged: hideMaybe()
        onDataLoadingChanged: hideMaybe()
    }
}
