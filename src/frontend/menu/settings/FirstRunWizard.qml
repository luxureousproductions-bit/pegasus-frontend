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
// Settings has for it; B goes back a step. Where it is is kept outside the
// Pegasus X folder (Settings.wizardStep), so a folder picked in step 1 (which
// needs a restart) goes on from step 2.
FocusScope {
    id: root

    signal close

    property int step: 1
    property bool libraryChanged: false   // game folders or an import: read again before step 4

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
        if (n === 4 && libraryChanged) {
            libraryChanged = false;
            Internal.settings.reloadLibrary();
        }
        if (n === 4)
            refreshStatus();
    }
    function finish() {
        Internal.settings.setWizardStep(0);
        Internal.settings.setRootSetupAsked();
        root.close();
    }

    property var status: ({})
    function refreshStatus() { status = Internal.settings.libraryStatus(api.collections); }
    Connections {
        target: Internal.scanner
        function onRunningChanged() { if (!Internal.scanner.running && root.step === 4) root.refreshStatus(); }
    }

    readonly property var stepNames: [
        qsTr("Where to keep things") + api.tr,
        qsTr("Your game folders") + api.tr,
        qsTr("Import gamelist") + api.tr,
        qsTr("Complete my library") + api.tr,
    ]
    readonly property string title: {
        switch (step) {
            case 1: return qsTr("Where Pegasus X keeps things") + api.tr;
            case 2: return qsTr("Where are your games?") + api.tr;
            case 3: return qsTr("Import gamelist") + api.tr;
            case 4: return qsTr("Complete my library") + api.tr;
        }
        return "";
    }
    readonly property string body: {
        switch (step) {
            case 1:
                return qsTr("Settings, themes, game details and scraped art all live in one folder. Inside the app is fine to start; a folder of your own (eg. PegasusX on an SD card) survives an uninstall and is easy to reach.") + api.tr;
            case 2:
                var n = Internal.settings.gameDirs.length;
                return (n === 0 ? qsTr("Add the folders your games are in (eg. ROMs). Pegasus X finds the systems inside them.")
                                : qsTr("%1 game folders added. Add more, or go on.").arg(n)) + api.tr;
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
            case 1: return qsTr("Picking a folder of your own closes Pegasus X once your data is moved; setup goes on from step 2 when you open it again.") + api.tr;
            case 3: return qsTr("Imports ES-DE folders: its gamelists, media and the emulators it uses. ES-DE itself stays as it is.") + api.tr;
            case 4: return qsTr("Only empty spots are filled. Games it isn't sure about wait in Settings > Library > Needs your check.") + api.tr;
        }
        return "";
    }
    readonly property var options: {
        switch (step) {
            case 1: return [
                { "act": "keep", "text": qsTr("Keep it inside the app") + api.tr, "value": qsTr("recommended to start") + api.tr },
                { "act": "pick", "text": qsTr("Pick a folder...") + api.tr, "value": "" },
            ];
            case 2: return [
                { "act": "dirs", "text": qsTr("Add game folders...") + api.tr, "value": "" },
                { "act": "next", "text": qsTr("Next") + api.tr, "value": "" },
            ];
            case 3: return [
                { "act": "import", "text": qsTr("Import gamelist...") + api.tr, "value": "" },
                { "act": "next", "text": qsTr("Skip") + api.tr, "value": "" },
            ];
            case 4:
                var st = status || {};
                var some = !Internal.scanner.running && (st.any || 0) > 0;
                return some ? [
                    { "act": "complete", "text": qsTr("Start and go to my games") + api.tr, "value": qsTr("%1 games").arg(st.any) + api.tr },
                    { "act": "finish", "text": qsTr("Skip, go to my games") + api.tr, "value": "" },
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
            case "pick":
                // done there: it closes Pegasus X; setup goes on from step 2
                Internal.settings.setWizardStep(2);
                openSub("RootFolderSetup.qml", { "firstRun": true });
                break;
            case "dirs":
                openSub("GameDirEditor.qml", {});
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
                finish();
                break;
            case "finish":
                finish();
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
        else if (from.indexOf("GameDirEditor") >= 0) {
            libraryChanged = true;
            list.currentIndex = 1;  // Next
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
                    root.finish();
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

    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: vpx(16)
        anchors.horizontalCenter: parent.horizontalCenter
        text: (root.step > 1 ? qsTr("A choose · B previous step") : qsTr("A choose · B skip setup")) + api.tr
              + qsTr(" · Each step can be done later in Settings") + api.tr
        color: "#ddd"
        font.family: globalFonts.sans
        font.pixelSize: vpx(16)
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
