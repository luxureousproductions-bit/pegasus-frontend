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


import "common"
import "gamedireditor"
import QtQuick 2.6


// Pegasus X: the Pegasus X folder, where everything lives (settings, themes,
// metadata, logs, art as downloaded_media/<system>/<kind>). Shown at the first start,
// and from Settings > Pegasus X folder. Changing it means starting Pegasus X
// again: once the data is copied it starts again by itself (a moment to read
// the note first; Restart now does it at once). In setup, setup goes on at
// step 2 after the restart.
FocusScope {
    id: root

    signal close

    property bool firstRun: false
    property string step: "intro"      // intro, picked, moving, done
    property string chosen: ""
    property string error: ""
    property var contents: ({})
    // the copy as it goes (Settings.rootDirProgress): { bytes, totalBytes, files, totalFiles, file, stage }
    property var progress: ({})
    readonly property bool copying: step === "moving" && progress.totalBytes !== undefined
    readonly property real fraction: {
        var p = progress;
        if (p.stage === "paths")
            return 1;
        if ((p.totalBytes || 0) > 0)
            return Math.min(1, (p.bytes || 0) / p.totalBytes);
        if ((p.totalFiles || 0) > 0)
            return Math.min(1, (p.files || 0) / p.totalFiles);
        return 0;
    }
    function mb(bytes) {
        return ((bytes || 0) / 1048576).toFixed(1);
    }

    anchors.fill: parent
    enabled: focus
    visible: focus

    Component.onCompleted: contents = Internal.settings.rootDirContents()

    Connections {
        target: Internal.settings
        function onRootDirProgress(p) {
            if (root.step === "moving")
                root.progress = p;
        }
        function onRootDirChanged(error) {
            if (root.step !== "moving")
                return;
            root.error = error;
            root.step = error === "" ? "done" : "picked";
            dialog.choice = 0;
            if (root.step === "done")
                restartTimer.start();
        }
    }

    function pickFolder() {
        filePicker.pickFolders = true;
        filePicker.fileExtensions = [];
        filePicker.openAt(Internal.settings.rootChosen ? Internal.settings.rootDir : "");
        filePicker.focus = true;
    }
    function apply(move) {
        progress = {};
        step = "moving";
        error = Internal.settings.setRootDir(chosen, move);
        if (error !== "")
            step = "picked";
        // else: the copy runs in the background, onRootDirChanged ends it
    }
    function leave() {
        if (firstRun)
            Internal.settings.setRootSetupAsked();
        root.close();
    }

    PegasusDialog {
        id: dialog
        focus: true
        z: 1
        title: qsTr("Pegasus X folder") + api.tr
        busy: root.step === "moving"
        dialogWidth: Math.min(parent.width * 0.75, vpx(820))
        buttons: {
            switch (root.step) {
                case "intro": return [ qsTr("Pick a folder") + api.tr, root.firstRun ? qsTr("Keep it inside the app") + api.tr : qsTr("Keep as is") + api.tr ];
                case "picked": return [ qsTr("Use it and move my data there") + api.tr, qsTr("Use it, start fresh") + api.tr, qsTr("Pick another") + api.tr ];
                case "done": return [ qsTr("Restart now") + api.tr ];
            }
            return [];
        }
        text: {
            var now = Internal.settings.rootDir;
            var c = root.contents || {};
            var what = qsTr("%1 files, %2 MB").arg(c.files || 0).arg((c.mb || 0).toFixed(1)) + api.tr;
            switch (root.step) {
                case "intro":
                    return (root.firstRun ? qsTr("Welcome to Pegasus X.") + api.tr + "\n\n" : "")
                        + (Internal.settings.rootChosen
                           ? qsTr("Everything Pegasus X keeps (settings, themes, game metadata, scraped art, logs) lives in one folder. Right now that's:\n%1\n(%2). You can pick another folder and move your data there: scraped art goes to downloaded_media/<system>/<kind> in it, themes to themes/.")
                           : qsTr("Everything Pegasus X keeps (settings, themes, game metadata, scraped art, logs) lives in one folder. Right now that's the app's own storage:\n%1\n(%2), which goes when the app is uninstalled and is awkward to reach. You can pick a folder of your own instead, like PegasusX on the internal storage or an SD card: scraped art goes to downloaded_media/<system>/<kind> in it, themes to themes/."))
                            .arg(now).arg(what) + api.tr;
                case "picked":
                    return (root.error !== "" ? root.error + "\n\n" : "")
                        + qsTr("Pegasus X folder: %1\n\n\"Move my data\" copies what's in %2 (%3) there: your settings, themes, metadata and art, so nothing changes but the place. \"Start fresh\" leaves it behind.")
                            .arg(root.chosen).arg(now).arg(what) + api.tr;
                case "moving":
                    if (!root.copying)
                        return qsTr("Setting up %1...").arg(root.chosen) + api.tr;
                    return qsTr("Copying your settings, themes, metadata and art\nfrom %1\nto %2\n\nThis can take a while; Pegasus X keeps running.").arg(now).arg(root.chosen) + api.tr;
                case "done":
                    return (root.firstRun
                        ? qsTr("Done. Pegasus X starts again now with %1, and setup goes on from step 2.")
                        : qsTr("Done. Pegasus X starts again now with %1.")).arg(root.chosen) + api.tr;
            }
            return "";
        }
        onPicked: {
            switch (root.step) {
                case "intro":
                    if (index === 0) root.pickFolder();
                    else root.leave();
                    break;
                case "picked":
                    if (index === 0) root.apply(true);
                    else if (index === 1) root.apply(false);
                    else root.pickFolder();
                    break;
                case "done":
                    Internal.settings.restartApp();
                    break;
            }
        }
        onCancelled: {
            if (root.step === "done")
                Internal.settings.restartApp();
            else if (root.step !== "moving")
                root.leave();
        }

        // Pegasus X: the copy's progress bar (moving the data)
        Column {
            width: parent.width
            spacing: dialog.textSize * 0.4
            visible: root.copying

            Rectangle {
                width: parent.width
                height: dialog.textSize * 0.7
                radius: height / 2
                color: "#333"
                Rectangle {
                    width: parent.width * root.fraction
                    height: parent.height
                    radius: height / 2
                    color: "#3aa"
                }
            }
            Text {
                width: parent.width
                text: root.progress.stage === "paths"
                    ? qsTr("Copied: %1 files, %2 MB. Updating the folder paths in them...").arg(root.progress.files || 0).arg(root.mb(root.progress.totalBytes)) + api.tr
                    : qsTr("%1% · %2 of %3 MB · %4 of %5 files").arg(Math.floor(root.fraction * 100))
                        .arg(root.mb(root.progress.bytes)).arg(root.mb(root.progress.totalBytes))
                        .arg(root.progress.files || 0).arg(root.progress.totalFiles || 0) + api.tr
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: dialog.textSize * 0.85
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            Text {
                width: parent.width
                visible: text !== ""
                text: root.progress.stage === "copying" ? (root.progress.file || "") : ""
                color: "#999"
                font.family: globalFonts.sans
                font.pixelSize: dialog.textSize * 0.7
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideMiddle
            }
        }
    }

    // the folder is set: Pegasus X starts again by itself
    Timer {
        id: restartTimer
        interval: 2500
        onTriggered: {
            console.log("Pegasus X folder: set to " + root.chosen + ", starting again");
            Internal.settings.restartApp();
        }
    }

    FilePicker {
        id: filePicker
        z: 2
        enabled: focus
        visible: focus
        onCancel: dialog.focus = true
        onPickFolder: {
            // the folder already in use (eg. setup again after Start over):
            // nothing to move or restart, setup carries on
            var trim = function(p) { return String(p).replace(/\/+$/, ""); };
            if (Internal.settings.rootChosen && trim(dir_path) === trim(Internal.settings.rootDir)) {
                console.log("Pegasus X folder: the one in use picked again, kept");
                root.leave();
                return;
            }
            root.chosen = dir_path;
            root.error = "";
            root.step = "picked";
            dialog.choice = 0;
            dialog.focus = true;
        }
    }
}
