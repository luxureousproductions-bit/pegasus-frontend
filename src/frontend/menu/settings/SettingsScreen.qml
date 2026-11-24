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


FocusScope {
    id: root

    signal close

    // Pegasus X: how long a screen takes to open (in the log, for the test
    // environment and for finding slow ones)
    property real openedAt: 0
    // Pegasus X: Gamepad layout is a full screen (it slid in over the main list
    // before); opened from Advanced it goes on top, so Advanced is hidden under it
    property bool subCovered: false
    function openScreen(url) {
        openedAt = Date.now();
        subCovered = false;
        subscreen.source = "";  // a fresh screen every time (see openModal)
        subscreen.source = url;
        subscreen.focus = true;
        root.state = "sub";
    }
    // NOTE: The source is cleared first, so every opening gets a fresh screen;
    //       a Loader keeps the old item when given the same source again, which
    //       would show stale data and keep its old focus
    function openModal(url) {
        openedAt = Date.now();
        modal.source = "";
        modal.source = url;
        modal.focus = true;
        root.state = "modal";
    }

    anchors.fill: parent
    enabled: focus
    visible: 0 < (x + width) && x < Window.window.width


    SettingsMain {
        id: main
        focus: true
        anchors.right: parent.right

        onClose: root.close()
        onOpenKeySettings: root.openScreen("KeyEditor.qml")
        onOpenGamepadSettings: root.openScreen("GamepadEditor.qml")
        onOpenGameDirSettings: root.openScreen("GameDirectory.qml")
        onOpenEsdeImport: root.openModal("EsdeImport.qml")
        onOpenThemeDownloader: root.openScreen("ThemeDownloader.qml")
        onOpenUpdater: root.openModal("UpdateDialog.qml")
        onOpenRootFolder: root.openModal("RootFolderSetup.qml")
        onOpenBackupReset: root.openScreen("BackupResetScreen.qml")
        onOpenMetadataEditor: root.openScreen("MetadataEditor.qml")
        onOpenScraper: root.openScreen("ScraperSection.qml")
        onOpenRetroAchievements: root.openScreen("RetroAchievementsScreen.qml")
        onOpenAndroidSafSettings: root.openModal("AndroidSafEditor.qml")
        onOpenProviderSettings: root.openModal("ProviderEditor.qml")
        onReloadRequested: Internal.settings.reloadAllGames()
        onOpenAdvanced: root.openScreen("SettingsAdvanced.qml")
    }


    Loader {
        id: modal
        asynchronous: true

        anchors.fill: parent
        z: 2  // above a sub screen it was opened from

        enabled: focus
        onLoaded: {
            item.focus = focus;
            console.log("[timing] " + source.toString().split("/").pop() + " opened in " + (Date.now() - root.openedAt) + " ms");
        }
        onFocusChanged: if (item) item.focus = focus
    }
    Connections {
        target: modal.item
        function onClose() {
            if (root.state === "sub")
                return;  // opened from a sub screen (below)
            main.focus = true;
            root.state = "";
        }
    }


    Loader {
        id: subscreen
        asynchronous: true

        width: parent.width
        height: parent.height
        anchors.left: main.right
        visible: !root.subCovered

        enabled: focus
        onLoaded: {
            item.focus = focus;
            console.log("[timing] " + source.toString().split("/").pop() + " opened in " + (Date.now() - root.openedAt) + " ms");
        }
        onFocusChanged: if (item) item.focus = focus
    }
    Connections {
        target: subscreen.item
        ignoreUnknownSignals: true
        function onClose() {
            main.focus = true;
            root.state = "";
        }
        // the Game directory screen: Import gamelist opens the ES-DE import on top
        function onOpenEsdeImport() {
            modal.source = "";
            modal.source = "EsdeImport.qml";
            modal.focus = true;
        }
        // Pegasus X: Settings > Advanced: its screens open on top of it (B
        // comes back to it), as they did from the main list
        function onOpenRootFolder() { root.openedAt = Date.now(); modal.source = ""; modal.source = "RootFolderSetup.qml"; modal.focus = true; }
        function onOpenAndroidSafSettings() { root.openedAt = Date.now(); modal.source = ""; modal.source = "AndroidSafEditor.qml"; modal.focus = true; }
        function onOpenProviderSettings() { root.openedAt = Date.now(); modal.source = ""; modal.source = "ProviderEditor.qml"; modal.focus = true; }
        function onOpenGamepadSettings() { root.openedAt = Date.now(); root.subCovered = true; modal.source = ""; modal.source = "GamepadEditor.qml"; modal.focus = true; }
        // Library: a game's "Scraper options..." opens the Scraper on top
        // (Back returns to the game)
        function onOpenScraper() {
            root.openedAt = Date.now();
            modal.source = "";
            modal.source = "ScraperSection.qml";
            modal.focus = true;
        }
    }
    Connections {
        target: modal.item
        ignoreUnknownSignals: true
        function onClose() {
            if (root.state === "sub" && subscreen.item) {
                // back to the screen under it
                root.subCovered = false;
                subscreen.focus = true;
                if (subscreen.item.refresh)
                    subscreen.item.refresh();
                // Pegasus X: the Scraper opened from a game in Library fills the
                // screen: it goes, or it stays drawn over the game while the keys
                // go to the game under it (unloaded after this signal: not from
                // inside its own close)
                if (String(modal.source).indexOf("ScraperSection.qml") >= 0
                        || String(subscreen.source).indexOf("SettingsAdvanced.qml") >= 0)  // (and a screen of Advanced's)
                    Qt.callLater(function() { modal.source = ""; });
            }
        }
    }


    states: [
        State {
            name: "sub"
            AnchorChanges {
                target: main
                anchors.right: subscreen.left
            }
            AnchorChanges {
                target: subscreen
                anchors.left: undefined
                anchors.right: parent.right
            }
        }
    ]

    // fancy easing curves, a la material design
    readonly property var bezierDecelerate: [ 0,0, 0.2,1, 1,1 ]
    readonly property var bezierSharp: [ 0.4,0, 0.6,1, 1,1 ]
    readonly property var bezierStandard: [ 0.4,0, 0.2,1, 1,1 ]

    transitions: [
        Transition {
            from: ""; to: "sub"
            AnchorAnimation {
                duration: 425
                easing { type: Easing.Bezier; bezierCurve: bezierStandard }
            }
        },
        Transition {
            from: "sub"; to: ""
            AnchorAnimation {
                duration: 400
                easing { type: Easing.Bezier; bezierCurve: bezierSharp }
            }
            onRunningChanged: if (!running) subscreen.source = ""
        }
    ]
}
