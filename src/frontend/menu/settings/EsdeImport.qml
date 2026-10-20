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


// Pegasus X: converts an ES-DE setup into Pegasus metadata files. For each
// picked system, every game in its ROM folders gets an entry with the details
// from ES-DE's gamelist and its art and manuals from ES-DE's media folder,
// written to esde-import.metadata.pegasus.txt in that ROM folder.
FocusScope {
    id: root

    property var systems: []
    property var picked: ({})  // system name -> true
    property string message: ""
    property bool imported: false

    // progress of a running import
    property real progress: 0
    property string progressSystem: ""
    readonly property bool running: Internal.settings.esdeImportRunning

    signal close

    anchors.fill: parent

    enabled: focus
    visible: opacity > 0.001
    opacity: focus ? 1.0 : 0.0
    Behavior on opacity { PropertyAnimation { duration: 150 } }

    Component.onCompleted: reloadSystems()

    function reloadSystems() {
        systems = Internal.settings.esdeImportSystems();
        picked = {};
    }

    readonly property int pickedCount: {
        var n = 0;
        for (var key in picked) {
            if (picked[key])
                n++;
        }
        return n;
    }
    readonly property bool allPicked: systems.length > 0 && pickedCount === systems.length

    // one list, so the controller just goes up and down: the ES-DE folder,
    // the Android option, "all systems", each system, then Import
    readonly property var rows: {
        var out = [{ "kind": "folder" }];
        var hasAndroid = false;
        for (var j = 0; j < systems.length; j++) {
            if (systems[j].name === "androidapps" || systems[j].name === "androidgames")
                hasAndroid = true;
        }
        if (hasAndroid)
            out.push({ "kind": "android" });
        if (systems.length > 0)
            out.push({ "kind": "all" });
        for (var i = 0; i < systems.length; i++)
            out.push({ "kind": "system", "sys": systems[i] });
        out.push({ "kind": "import" });
        return out;
    }

    function toggle(name) {
        var copy = Object.assign({}, picked);
        copy[name] = !copy[name];
        picked = copy;
    }
    function toggleAll() {
        var copy = {};
        if (!allPicked) {
            for (var i = 0; i < systems.length; i++)
                copy[systems[i].name] = true;
        }
        picked = copy;
    }

    function runImport() {
        var names = [];
        for (var key in picked) {
            if (picked[key])
                names.push(key);
        }
        if (names.length === 0) {
            message = qsTr("Pick at least one system first.") + api.tr;
            return;
        }
        message = "";
        progress = 0;
        progressSystem = "";
        Internal.settings.startEsdeImport(names);
    }

    Connections {
        target: Internal.settings
        function onEsdeImportProgress(done, total, system) {
            root.progress = total > 0 ? done / total : 0;
            root.progressSystem = system;
        }
        function onEsdeImportFinished(result) {
            root.progress = 1;
            if (result.files && result.files.length > 0) {
                root.imported = true;
                root.message = qsTr("Imported %1 games from %2 systems.").arg(result.games).arg(result.systems) + api.tr
                    + (result.error !== "" ? " " + result.error : "");
            }
            else {
                root.message = result.error !== "" ? result.error : qsTr("Nothing was imported.") + api.tr;
            }
        }
    }

    function activate(index) {
        if (index < 0 || index >= rows.length)
            return;
        var row = rows[index];
        if (row.kind === "folder")
            filePicker.focus = true;
        else if (row.kind === "android")
            Internal.settings.esdeCombineAndroid = !Internal.settings.esdeCombineAndroid;
        else if (row.kind === "all")
            toggleAll();
        else if (row.kind === "system")
            toggle(row.sys.name);
        else if (row.kind === "import")
            runImport();
    }

    function closeMaybe() {
        if (imported)
            reloadDialog.focus = true;
        else
            root.close();
    }

    Keys.onPressed: {
        if (root.running) {
            event.accepted = true;  // wait for the import to finish
            return;
        }
        if (api.keys.isCancel(event) && !event.isAutoRepeat) {
            event.accepted = true;
            root.closeMaybe();
        }
    }


    Rectangle {
        anchors.fill: parent
        color: "#000"
        opacity: 0.3

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: root.closeMaybe()
        }
    }

    Rectangle {
        id: main

        height: parent.height * 0.85
        width: Math.min(height * 1.5, parent.width * 0.95)
        anchors.centerIn: parent

        color: "#444"
        radius: vpx(8)

        MouseArea {
            anchors.fill: parent
        }

        Text {
            id: info
            text: qsTr("Turns ES-DE's game details, art and manuals into Pegasus metadata files, "
                     + "written into each system's ROM folder. Your own metadata files are not changed. "
                     + "Run it again to refresh.") + api.tr
            color: "#eee"
            font.family: globalFonts.sans
            font.pixelSize: vpx(17)
            lineHeight: 1.15

            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width * 0.92
            topPadding: vpx(16)
            bottomPadding: vpx(10)

            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Rectangle {
            anchors.top: info.bottom
            anchors.bottom: messageText.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: vpx(15)
            anchors.topMargin: 0
            color: "#333"

            ListView {
                id: list

                anchors.fill: parent
                clip: true

                model: root.rows
                focus: true
                highlightRangeMode: ListView.ApplyRange
                preferredHighlightBegin: height * 0.5 - vpx(18) * 1.25
                preferredHighlightEnd: height * 0.5 + vpx(18) * 1.25
                highlightMoveDuration: 0

                Keys.onPressed: {
                    if (root.running) {
                        event.accepted = true;
                        return;
                    }
                    if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                        event.accepted = true;
                        root.activate(currentIndex);
                    }
                }

                delegate: Rectangle {
                    readonly property bool highlighted: ListView.view.activeFocus
                                                        && (ListView.isCurrentItem || rowMouse.containsMouse)
                    readonly property bool isSystem: modelData.kind === "system"
                    readonly property bool checked: modelData.kind === "all" ? root.allPicked
                                                  : modelData.kind === "android" ? Internal.settings.esdeCombineAndroid
                                                  : isSystem ? !!root.picked[modelData.sys.name] : false

                    width: ListView.view.width
                    height: vpx(18) * 2.6
                    color: highlighted ? "#3aa"
                         : modelData.kind === "import" ? "#2a5f5f"
                         : modelData.kind === "folder" ? "#3a3a3a" : "transparent"

                    // check box
                    Rectangle {
                        id: box
                        visible: modelData.kind === "all" || modelData.kind === "android" || parent.isSystem
                        width: vpx(20)
                        height: width
                        radius: vpx(3)
                        color: parent.checked ? (parent.highlighted ? "#111" : "#3cc") : "transparent"
                        border.color: parent.highlighted ? "#111" : "#aaa"
                        border.width: vpx(2)

                        anchors.left: parent.left
                        anchors.leftMargin: vpx(16)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        id: mainLabel
                        text: {
                            switch (modelData.kind) {
                                case "folder":
                                    return Internal.settings.esdeImportDir !== ""
                                        ? qsTr("ES-DE folder: %1").arg(Internal.settings.esdeImportDir) + api.tr
                                        : qsTr("ES-DE folder not found, select to pick it") + api.tr;
                                case "all": return qsTr("All systems") + api.tr;
                                case "android": return qsTr("Put Android Apps and Android Games in one \"Android\" collection") + api.tr;
                                case "import": return root.pickedCount > 0
                                    ? qsTr("Import %1 systems").arg(root.pickedCount) + api.tr
                                    : qsTr("Import") + api.tr;
                                default: return modelData.sys.fullname;
                            }
                        }
                        color: parent.highlighted ? "#111" : "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(18)
                        font.bold: modelData.kind === "import" || modelData.kind === "all"

                        anchors.left: box.visible ? box.right : parent.left
                        anchors.leftMargin: vpx(14)
                        anchors.right: noteLabel.left
                        anchors.rightMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                        horizontalAlignment: modelData.kind === "import" && !box.visible ? Text.AlignHCenter : Text.AlignLeft
                        elide: Text.ElideMiddle
                    }

                    Text {
                        id: noteLabel
                        text: {
                            if (modelData.kind === "folder")
                                return qsTr("change") + api.tr;
                            if (!parent.isSystem)
                                return "";
                            var parts = [qsTr("%1 games").arg(modelData.sys.games)];
                            if (modelData.sys.hasGamelist)
                                parts.push(qsTr("details"));
                            if (modelData.sys.hasMedia)
                                parts.push(qsTr("art"));
                            var shortName = Internal.settings.esdeCombineAndroid
                                && (modelData.sys.name === "androidapps" || modelData.sys.name === "androidgames")
                                ? "android" : modelData.sys.shortName;
                            return modelData.sys.name + " → " + shortName + " · " + parts.join(" · ") + api.tr;
                        }
                        color: parent.highlighted ? "#113" : "#aaa"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(14)

                        anchors.right: parent.right
                        anchors.rightMargin: vpx(16)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            list.currentIndex = index;
                            list.focus = true;
                            root.activate(index);
                        }
                    }
                }
            }

            Text {
                visible: root.systems.length === 0
                text: Internal.settings.esdeImportDir === ""
                    ? qsTr("Pick your ES-DE folder above.") + api.tr
                    : qsTr("No ES-DE systems with games were found in your ROM folders.") + api.tr
                color: "#ccc"
                font.family: globalFonts.sans
                font.pixelSize: vpx(17)
                width: parent.width * 0.85
                anchors.centerIn: parent
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }

        Text {
            id: messageText
            text: root.message !== "" ? root.message
                : qsTr("A: select   B: back") + api.tr
            color: "#ddd"
            font.family: globalFonts.sans
            font.pixelSize: vpx(15)
            wrapMode: Text.WordWrap

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: vpx(15)
            horizontalAlignment: Text.AlignHCenter
        }
    }


    // while importing: a progress bar over the list
    Rectangle {
        anchors.fill: main
        radius: main.radius
        color: "#cc222222"
        visible: root.running

        MouseArea {
            anchors.fill: parent
        }

        Column {
            anchors.centerIn: parent
            width: parent.width * 0.7
            spacing: vpx(14)

            Text {
                text: qsTr("Importing from ES-DE...") + api.tr
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(22)
                font.bold: true
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                width: parent.width
                height: vpx(14)
                radius: height / 2
                color: "#333"

                Rectangle {
                    width: Math.max(height, parent.width * root.progress)
                    height: parent.height
                    radius: height / 2
                    color: "#3aa"
                    Behavior on width { NumberAnimation { duration: 200 } }
                }
            }

            Text {
                text: root.progressSystem !== ""
                    ? qsTr("%1%  ·  %2").arg(Math.round(root.progress * 100)).arg(root.progressSystem) + api.tr
                    : qsTr("Getting ready...") + api.tr
                color: "#ccc"
                font.family: globalFonts.sans
                font.pixelSize: vpx(16)
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }
    }


    // a different ES-DE folder: picked through its settings file
    FilePicker {
        id: filePicker

        fileNames: ["es_settings.xml"]
        fileExtensions: []
        infoText: qsTr("Open your ES-DE folder, then its \"settings\" folder, and select es_settings.xml.") + api.tr

        enabled: focus
        visible: opacity > 0.01
        opacity: focus ? 1.0 : 0.0
        Behavior on opacity { PropertyAnimation { duration: 300 } }

        onCancel: list.focus = true
        onPick: {
            // dir_path is ES-DE/settings
            var esde = dir_path.substring(0, dir_path.lastIndexOf("/"));
            Internal.settings.esdeImportDir = esde;
            root.reloadSystems();
            root.message = "";
            list.focus = true;
        }
    }


    ReloadQuestion {
        id: reloadDialog
        onAccept: {
            list.focus = true;
            root.imported = false;
            Internal.settings.reloadProviders();
        }
        onCancel: {
            list.focus = true;
            root.imported = false;
            root.close();
        }
    }
}
