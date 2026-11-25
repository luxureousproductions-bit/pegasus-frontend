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
import QtQuick 2.6


// Pegasus X: the first start, in four steps: where to keep things, your game
// folders, import gamelist, complete my library. Each step opens the screen
// Settings has for it; B goes back a step. Where it is is kept in the app's
// own storage (Settings.wizardStep, SetupState.h), so a folder picked in step 1
// (Pegasus X starts again) goes on from step 2. No theme is loaded while it's
// open (main.qml); the library is read here, for step 4, and when it ends.
// Finished or skipped, it never opens again by itself (Settings.finishSetup):
// only Run setup again or Start over bring it back.
FocusScope {
    id: root

    signal close

    property int step: 1
    property int tick: 0   // bumped when a step's screen closes, so the texts read again
    property bool libraryChanged: false   // game folders or an import: read again before step 4
    property bool appsPicked: false       // Android apps to import: read again by the end of setup too
    property bool readOnce: false         // the library read by setup this session
    // Pegasus data already in the Pegasus X folder (after Android's Clear data,
    // or another Pegasus's): step 1 asks to use it or start fresh
    readonly property string found: Internal.settings.setupFoundData()
    property bool wiping: false

    anchors.fill: parent
    enabled: focus
    visible: focus

    Component.onCompleted: {
        step = Math.max(1, Internal.settings.wizardStep());
        Internal.settings.setWizardStep(step);
    }

    function goTo(n) {
        step = n;
        Internal.settings.setWizardStep(n);
        list.currentIndex = 0;
        list.focus = true;
        if (n === 4 && (libraryChanged || !readOnce))
            readLibrary();
        if (n === 4)
            refreshStatus();
    }
    function readLibrary() {
        libraryChanged = false;
        readOnce = true;
        Internal.settings.reloadLibrary();
    }
    // finished (or skipped: B on step 1): never again by itself; the theme loads
    function finish(skipped) {
        // the library as setup left it (it isn't read at a start with setup open)
        if (libraryChanged || !readOnce)
            readLibrary();
        Internal.settings.setRootSetupAsked();
        Internal.settings.finishSetup(!!skipped);
        root.close();
    }

    property var status: ({})
    function refreshStatus() { status = Internal.settings.libraryStatus(api.collections); }
    Connections {
        target: Internal.scanner
        function onRunningChanged() { if (!Internal.scanner.running && root.step === 4) root.refreshStatus(); }
    }

    readonly property var stepNames: [
        qsTr("App Data Folder") + api.tr,
        qsTr("Systems and ROM folders") + api.tr,
        qsTr("Import gamelist") + api.tr,
        qsTr("Complete my library") + api.tr,
    ]
    readonly property string title: {
        switch (step) {
            case 1: return (root.found !== "" ? qsTr("Pegasus X data found") : qsTr("Where Pegasus X keeps things")) + api.tr;
            case 2: return qsTr("Which systems do you have?") + api.tr;
            case 3: return qsTr("Import gamelist") + api.tr;
            case 4: return qsTr("Complete my library") + api.tr;
        }
        return "";
    }
    readonly property string body: {
        var t = tick;
        switch (step) {
            case 1:
                if (root.found !== "") {
                    var c = Internal.settings.rootDirContents();
                    return qsTr("%1 already has Pegasus data in it (%2 files, %3 MB): settings, game folders, theme settings or scraped art. Use it as it is (setup goes on with it), or start fresh.")
                        .arg(root.found).arg(c.files || 0).arg((c.mb || 0).toFixed(1)) + api.tr;
                }
                return qsTr("Settings, themes, game details and scraped art all live in one folder. Inside the app is fine to start; a folder of your own (eg. PegasusX on an SD card) survives an uninstall and is easy to reach.") + api.tr;
            case 2:
                var n = Internal.settings.gameDirectorySystems().length;
                return (n === 0 ? qsTr("Pick the folder your ROMs are in and Pegasus X finds your systems in it, or tick your systems first, then their folder. Systems are found by folder name (snes, psx, ...) and the games' file types. Your ROM folders are only read.")
                                : qsTr("%1 systems in your game directory. Add more, or go on.").arg(n)) + api.tr;
            case 3:
                return qsTr("Already set up another front end? Bring its games, details and art in, so there's nothing to do again.") + api.tr;
            case 4:
                if (Internal.scanner.running)
                    return qsTr("Reading your games...") + api.tr;
                var st = status || {};
                if ((st.games || 0) === 0)
                    return qsTr("No games yet: add game folders in step 2, or finish and add them later in Settings.") + api.tr;
                if ((st.any || 0) === 0)
                    return qsTr("%1 games, all with a box, a screenshot and a description.").arg(st.games) + api.tr;
                return qsTr("%1 games, %2 missing a box, a screenshot or a description. Pegasus X can look them up now, in the background: free pictures by checksum and name, plus anything your logins add.")
                    .arg(st.games).arg(st.any) + api.tr;
        }
        return "";
    }
    // small print under the text
    readonly property string fine: {
        switch (step) {
            case 1: return (root.found !== ""
                ? qsTr("Start fresh deletes what Start over deletes, there and in the app's own folder; your ROMs, ES-DE's folder and your themes stay. Pegasus X starts again.")
                : qsTr("Picking a folder of your own copies your data there, then Pegasus X starts again and setup goes on from step 2.")) + api.tr;
            case 3: return qsTr("Imports ES-DE folders: its gamelists, media and the emulators it uses. ES-DE itself stays as it is.") + api.tr;
            case 4: return qsTr("Only empty spots are filled. Games it isn't sure about wait in Settings > Library > Needs your check.") + api.tr;
        }
        return "";
    }
    readonly property var options: {
        var t = tick;
        switch (step) {
            case 1: return root.found !== "" ? [
                { "act": "use", "text": qsTr("Use it") + api.tr, "value": root.found },
                { "act": "fresh", "text": qsTr("Start fresh...") + api.tr, "value": qsTr("deletes it") + api.tr },
                { "act": "pick", "text": qsTr("Pick another folder...") + api.tr, "value": "" },
            ] : [
                // (setup run again, or after Start over: the folder in use is kept)
                Internal.settings.rootChosen
                    ? { "act": "keep", "text": qsTr("Keep using this folder") + api.tr, "value": Internal.settings.rootDir }
                    : { "act": "keep", "text": qsTr("Keep it inside the app") + api.tr, "value": qsTr("recommended to start") + api.tr },
                { "act": "pick", "text": qsTr("Pick a folder...") + api.tr, "value": "" },
            ];
            case 2:
                var have = Internal.settings.gameDirectorySystems().length;
                var two = [
                    { "act": "romfolder", "text": qsTr("Pick my ROMs folder...") + api.tr, "value": qsTr("finds the systems in it") + api.tr },
                    { "act": "dirs", "text": qsTr("Add systems...") + api.tr, "value": have === 0 ? qsTr("nothing added yet") + api.tr : "" },
                ];
                // Android apps to import (a new install: none until you pick)
                if (Qt.platform.os === "android" || Internal.settings.androidAppChoices().length > 0) {
                    var picked = Internal.settings.androidAppSelection().length;
                    two.push({ "act": "apps", "text": qsTr("Android apps to import...") + api.tr,
                               "value": (picked === 0 ? (Internal.settings.androidAppSelectionKnown() ? qsTr("none yet") : qsTr("every app"))
                                         : picked === 1 ? qsTr("1 app") : qsTr("%1 apps").arg(picked)) + api.tr });
                }
                two.push({ "act": "next", "text": qsTr("Next") + api.tr, "value": "" });
                return two;
            case 3: return [
                { "act": "import", "text": qsTr("Import gamelist...") + api.tr, "value": "" },
                { "act": "next", "text": qsTr("Skip") + api.tr, "value": "" },
            ];
            case 4:
                var st = status || {};
                var some = !Internal.scanner.running && (st.any || 0) > 0;
                return some ? [
                    { "act": "complete", "text": qsTr("Start and go to my games") + api.tr, "value": qsTr("%1 games").arg(st.any) + api.tr },
                    { "act": "skip4", "text": qsTr("Skip, go to my games") + api.tr, "value": "" },
                ] : [
                    { "act": "finish", "text": qsTr("Go to my games") + api.tr, "value": "" },
                ];
        }
        return [];
    }
    function act(a) {
        switch (a) {
            case "keep":
                Internal.settings.setRootSetupAsked();
                goTo(2);
                break;
            case "use":
                // the data found, as it is: setup goes on with it (its systems in step 2)
                console.log("Setup: the Pegasus data found in " + root.found + " is used");
                Internal.settings.setRootSetupAsked();
                goTo(2);
                break;
            case "fresh":
                freshDialog.show();
                break;
            case "pick":
                // done there: it closes Pegasus X; setup goes on from step 2
                Internal.settings.setWizardStep(2);
                openSub("RootFolderSetup.qml", { "firstRun": true });
                break;
            case "dirs":
                openSub("GameDirectory.qml", { "addOnly": true });
                break;
            case "romfolder":
                openSub("GameDirectory.qml", { "addOnly": true, "folderFirst": true });
                break;
            case "apps":
                openSub("AndroidAppPicker.qml", {});
                break;
            case "import":
                openSub("EsdeImport.qml", {});
                break;
            case "next":
                goTo(step + 1);
                break;
            case "complete":
                var error = Internal.settings.completeLibrary(api.collections,
                    { "boxes": true, "shots": true, "details": true, "fanart": false, "videos": false });
                if (error !== "")
                    console.warn("Complete my library: " + error);
                finish(false);
                break;
            case "finish":
            case "skip4":
                finish(false);
                break;
        }
    }
    function openSub(file, props) {
        subscreen.setSource(file, props);
        subscreen.focus = true;
    }
    function subClosed() {
        var from = String(subscreen.source);
        subscreen.focus = false;
        list.focus = true;
        Qt.callLater(function() { subscreen.source = ""; });
        if (from.indexOf("RootFolderSetup") >= 0) {
            goTo(2);
        }
        else if (from.indexOf("GameDirectory") >= 0) {
            libraryChanged = true;
            tick++;  // the texts read the directory again
            list.currentIndex = list.count - 1;  // Next
        }
        else if (from.indexOf("AndroidAppPicker") >= 0) {
            libraryChanged = true;  // read again before step 4
            appsPicked = true;
            tick++;  // its row's count
            list.currentIndex = list.count - 1;  // Next
        }
        else if (from.indexOf("EsdeImport") >= 0) {
            libraryChanged = true;
            goTo(4);
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#222"
    }

    ScreenHeader {
        id: header
        text: qsTr("Welcome to Pegasus X") + api.tr
    }

    // the steps: done, this one, still to come
    Row {
        id: steps
        anchors.top: header.bottom
        anchors.topMargin: vpx(22)
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.8
        spacing: vpx(2)
        Repeater {
            model: root.stepNames
            delegate: Rectangle {
                width: (steps.width - steps.spacing * 3) / 4
                height: vpx(42)
                color: index + 1 === root.step ? "#3aa" : index + 1 < root.step ? "#2a4a4a" : "#333"
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: vpx(12)
                    anchors.right: parent.right
                    anchors.rightMargin: vpx(8)
                    text: (index + 1) + " · " + modelData
                    color: index + 1 === root.step ? "#fff" : index + 1 < root.step ? "#cee" : "#999"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(16)
                    elide: Text.ElideRight
                }
            }
        }
    }

    Column {
        id: texts
        anchors.top: steps.bottom
        anchors.topMargin: vpx(30)
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.7
        spacing: vpx(12)
        Text {
            width: parent.width
            text: root.title
            color: "#eee"
            font.family: globalFonts.sans
            font.pixelSize: vpx(26)
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            text: root.body
            color: "#bbb"
            font.family: globalFonts.sans
            font.pixelSize: vpx(18)
            lineHeight: 1.2
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            visible: text !== ""
            text: root.fine
            color: "#888"
            font.family: globalFonts.sans
            font.pixelSize: vpx(15)
            wrapMode: Text.WordWrap
        }
    }

    ListView {
        id: list
        anchors.top: texts.bottom
        anchors.topMargin: vpx(22)
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.7
        height: count * (vpx(56) + spacing)
        spacing: vpx(6)
        focus: true
        interactive: false
        model: root.options
        keyNavigationWraps: false
        // Pegasus X: focus moves at once (no slide to catch up with a held D-pad)
        highlightMoveDuration: 0

        // (not interactive: the arrows are handled here)
        Keys.onUpPressed: if (currentIndex > 0) currentIndex--
        Keys.onDownPressed: if (currentIndex < count - 1) currentIndex++
        Keys.onPressed: {
            if (event.isAutoRepeat)
                return;
            if (api.keys.isAccept(event)) {
                event.accepted = true;
                if (currentIndex >= 0 && currentIndex < root.options.length)
                    root.act(root.options[currentIndex].act);
            }
            else if (api.keys.isCancel(event)) {
                event.accepted = true;
                if (root.step > 1)
                    root.goTo(root.step - 1);
                else
                    root.finish(true);
            }
        }

        delegate: Rectangle {
            readonly property bool on: (ListView.isCurrentItem && ListView.view.activeFocus) || rowMouse.containsMouse
            width: ListView.view.width
            height: vpx(56)
            color: "#2b2b2b"
            Text {
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                text: modelData.text
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(20)
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                text: modelData.value
                color: "#bbb"
                font.family: globalFonts.sans
                font.pixelSize: vpx(18)
            }
            Rectangle {
                width: parent.width
                height: vpx(3)
                anchors.bottom: parent.bottom
                color: "#3aa"
                visible: parent.on
            }
            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    list.currentIndex = index;
                    root.act(modelData.act);
                }
            }
        }
    }

    RoundPromptBar {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        // (a step's screen, eg. Game directory and its folder picker, has its own)
        visible: !subscreen.visible && !freshDialog.visible
        items: [
            { "keys": ["A"], "text": qsTr("choose") + api.tr },
            { "keys": ["B"], "text": (root.step > 1 ? qsTr("previous step") : qsTr("skip setup")) + api.tr },
        ]
    }

    // Start fresh (data found): asked once more, Cancel focused; then it's
    // deleted as Start over does and Pegasus X starts again
    PegasusDialog {
        id: freshDialog
        z: 6
        function show() {
            root.wiping = false;
            choice = 0;
            focus = true;
        }
        title: qsTr("Start fresh?") + api.tr
        busy: root.wiping
        text: root.wiping
            ? qsTr("Deleting... Pegasus X starts again when it's done.") + api.tr
            : qsTr("Pegasus X's data in %1 is deleted: settings, logins, game directory, theme settings, scraped art. Your ROMs, ES-DE's folder and your themes stay. This can't be undone.").arg(root.found) + api.tr
        buttons: [ qsTr("Cancel") + api.tr, qsTr("Delete and start fresh") + api.tr ]
        onPicked: {
            if (index !== 1) {
                focus = false;
                list.focus = true;
                return;
            }
            root.wiping = true;
            console.log("Setup: start fresh: deleting the Pegasus data in " + root.found);
            Internal.settings.setupStartFresh();
        }
        onCancelled: {
            if (root.wiping)
                return;
            focus = false;
            list.focus = true;
        }
    }

    // the screen a step opens (Settings' own)
    Loader {
        id: subscreen
        anchors.fill: parent
        z: 5
        enabled: focus
        visible: status === Loader.Ready && focus
        onLoaded: item.focus = true
    }
    Connections {
        target: subscreen.item
        ignoreUnknownSignals: true
        function onClose() { root.subClosed(); }
    }
}
