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
// metadata, logs, art as media/<system>/<kind>). Shown at the first start,
// and from Settings > Pegasus X folder. Changing it means starting Pegasus X
// again, so the last step closes the app.
FocusScope {
    id: root

    signal close

    property bool firstRun: false
    property string step: "intro"      // intro, picked, moving, done
    property string chosen: ""
    property string error: ""
    property var contents: ({})

    anchors.fill: parent
    enabled: focus
    visible: focus

    Component.onCompleted: contents = Internal.settings.rootDirContents()

    Connections {
        target: Internal.settings
        function onRootDirChanged(error) {
            if (root.step !== "moving")
                return;
            root.error = error;
            root.step = error === "" ? "done" : "picked";
            dialog.choice = 0;
        }
    }

    function pickFolder() {
        filePicker.pickFolders = true;
        filePicker.fileExtensions = [];
        filePicker.openAt(Internal.settings.rootChosen ? Internal.settings.rootDir : "");
        filePicker.focus = true;
    }
    function apply(move) {
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
                case "done": return [ qsTr("Close Pegasus X") + api.tr ];
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
                        + qsTr("Everything Pegasus X keeps (settings, themes, game metadata, scraped art, logs) lives in one folder. Right now that's %1 (%2), inside the app, which goes when the app is uninstalled and is awkward to reach. You can pick a folder of your own instead, like PegasusX on the internal storage or an SD card: art goes to media/<system>/<kind> in it, themes to themes/.")
                            .arg(Internal.settings.rootChosen ? now : qsTr("the app's own storage") + api.tr).arg(what) + api.tr;
                case "picked":
                    return (root.error !== "" ? root.error + "\n\n" : "")
                        + qsTr("Pegasus X folder: %1\n\n\"Move my data\" copies what's in %2 (%3) there: your settings, themes, metadata and art, so nothing changes but the place. \"Start fresh\" leaves it behind.")
                            .arg(root.chosen).arg(now).arg(what) + api.tr;
                case "moving":
                    return qsTr("Copying your settings, themes, metadata and art to %1...\n\nThis can take a while; Pegasus X keeps running.").arg(root.chosen) + api.tr;
                case "done":
                    return qsTr("Done. Pegasus X uses %1 from its next start: it closes now, open it again.").arg(root.chosen) + api.tr;
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
                    Internal.settings.quitApp();
                    break;
            }
        }
        onCancelled: {
            if (root.step === "done")
                Internal.settings.quitApp();
            else if (root.step !== "moving")
                root.leave();
        }
    }

    FilePicker {
        id: filePicker
        z: 2
        enabled: focus
        visible: focus
        onCancel: dialog.focus = true
        onPickFolder: {
            root.chosen = dir_path;
            root.error = "";
            root.step = "picked";
            dialog.choice = 0;
            dialog.focus = true;
        }
    }
}
