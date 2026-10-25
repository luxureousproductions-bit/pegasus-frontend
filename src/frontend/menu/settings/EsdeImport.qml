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
    // Pegasus X: the merge setting when the screen opened; flipped back is no change
    property bool mergeTaken: false
    property bool mergeStart: false
    property bool otherChange: false   // an import, rescan or delete

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

    // the systems are counted in the background (it reads every ROM folder),
    // so the screen can show that it's loading
    property bool loadingSystems: true
    property int keepIndex: -1  // the row to come back to once the list is read again
    function reloadSystems() {
        loadingSystems = true;
        systems = [];
        picked = {};
        Internal.settings.requestEsdeImportSystems();
    }
    Connections {
        target: Internal.settings
        function onEsdeImportSystemsReady(list) {
            // A to Z by the name you see (the list arrives sorted; kept so here too)
            var sorted = list.slice();
            sorted.sort(function(a, b) { return a.fullname.toLowerCase().localeCompare(b.fullname.toLowerCase()); });
            root.systems = sorted;
            root.loadingSystems = false;
            if (root.keepIndex >= 0) {
                list.currentIndex = Math.min(root.keepIndex, root.rows.length - 1);
                root.keepIndex = -1;
            }
        }
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

    // Three pages, each one list so the controller just goes up and down:
    //   menu:   Import, Rescan, Delete
    //   import: the ES-DE folder, the Android option, "all systems", each
    //           system, then Import
    //   rescan: the systems imported already; A imports that one again
    //   delete: the systems imported already; A twice deletes its import
    property string page: "menu"
    property bool mergeAndroid: Internal.settings.mergeAndroid
    property string confirmDelete: ""   // the system waiting for a second A
    readonly property var scanned: {
        var out = [];
        for (var j = 0; j < systems.length; j++) {
            if (systems[j].importedFiles && systems[j].importedFiles.length > 0)
                out.push(systems[j]);
        }
        return out;
    }
    readonly property var rows: {
        var out = [];
        if (page === "menu") {
            out.push({ "kind": "nav", "to": "import", "text": qsTr("Import") + api.tr,
                       "note": qsTr("pick systems to import from ES-DE") + api.tr });
            out.push({ "kind": "nav", "to": "rescan", "text": qsTr("Rescan") + api.tr,
                       "note": loadingSystems ? "" : qsTr("%1 imported").arg(scanned.length) + api.tr });
            out.push({ "kind": "nav", "to": "delete", "text": qsTr("Delete") + api.tr,
                       "note": loadingSystems ? "" : qsTr("%1 imported").arg(scanned.length) + api.tr });
            return out;
        }
        if (page === "rescan" || page === "delete") {
            for (var r = 0; r < scanned.length; r++)
                out.push({ "kind": page, "sys": scanned[r] });
            return out;
        }
        out.push({ "kind": "folder" });
        out.push({ "kind": "use" });
        for (var k = 0; k < systems.length; k++) {
            if (systems[k].name === "androidgames")
                out.push({ "kind": "androidgames" });
        }
        if (systems.length > 0)
            out.push({ "kind": "all" });
        for (var i = 0; i < systems.length; i++)
            out.push({ "kind": "system", "sys": systems[i] });
        out.push({ "kind": "import" });
        return out;
    }
    function openPage(name) {
        page = name;
        message = "";
        confirmDelete = "";
        list.currentIndex = 0;
        list.positionViewAtBeginning();
    }

    function rescan(sys) {
        message = "";
        progress = 0;
        progressSystem = "";
        Internal.settings.startEsdeImport([sys.name]);
    }
    function deleteImport(sys) {
        if (confirmDelete !== sys.name) {
            confirmDelete = sys.name;
            message = qsTr("Press A again to delete the import of %1. Your ROMs, ES-DE's art and your own metadata files stay.").arg(sys.fullname) + api.tr;
            return;
        }
        confirmDelete = "";
        var removed = Internal.settings.deleteEsdeImport(sys.folders);
        otherChange = otherChange || removed > 0;
        imported = imported || removed > 0;
        message = removed > 0 ? qsTr("Deleted the import of %1.").arg(sys.fullname) + api.tr
                              : qsTr("Nothing to delete for %1.").arg(sys.fullname) + api.tr;
        var keep = list.currentIndex;
        reloadSystems();
        keepIndex = keep;
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
                root.otherChange = true;
                root.imported = true;
                root.message = qsTr("Imported %1 games from %2 systems.").arg(result.games).arg(result.systems) + api.tr
                    + (result.error !== "" ? " " + result.error : "");
            }
            else {
                root.message = result.error !== "" ? result.error : qsTr("Nothing was imported.") + api.tr;
            }
            // the Scanned imports list shows what's there now
            var keep = list.currentIndex;
            root.reloadSystems();
            root.keepIndex = keep;
        }
    }

    function activate(index) {
        if (index < 0 || index >= rows.length)
            return;
        var row = rows[index];
        if (row.kind === "folder")
            filePicker.focus = true;
        else if (row.kind === "use") {
            var use = Internal.settings.esdeImportUse;
            Internal.settings.esdeImportUse = use === "media" ? "fill" : use === "fill" ? "replace" : "media";
            otherChange = true;
            imported = true;  // applied when the library is read again
        }
        else if (row.kind === "androidgames") {
            if (!mergeTaken) {
                mergeStart = root.mergeAndroid;
                mergeTaken = true;
            }
            Internal.settings.mergeAndroid = !root.mergeAndroid;
            // asks to reload on the way out, unless it's back as it was
            imported = otherChange || Internal.settings.mergeAndroid !== mergeStart;
        }
        else if (row.kind === "nav")
            openPage(row.to);
        else if (row.kind === "rescan")
            rescan(row.sys);
        else if (row.kind === "delete")
            deleteImport(row.sys);
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
            if (root.page !== "menu") {
                var from = root.page;
                root.openPage("menu");
                list.currentIndex = from === "import" ? 0 : from === "rescan" ? 1 : 2;
            }
            else {
                root.closeMaybe();
            }
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

        MouseArea {
            anchors.fill: parent
        }

        Text {
            id: info
            text: root.page === "rescan"
                ? qsTr("Pick a system to import again from ES-DE: new games, details and art come in, "
                     + "and the emulators you picked are kept.") + api.tr
                : root.page === "delete"
                ? qsTr("Pick a system to delete its import. Your ROMs, ES-DE's art and your own metadata files stay.") + api.tr
                : qsTr("Turns ES-DE's game details, art and manuals into Pegasus metadata files, "
                     + "written into each system's ROM folder. Your own metadata files are not changed.") + api.tr
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

                onCurrentIndexChanged: root.confirmDelete = ""
                function selectable(i) {
                    return i >= 0 && i < root.rows.length && root.rows[i].kind !== "section";
                }
                Keys.onUpPressed: {
                    var i = currentIndex - 1;
                    while (i >= 0 && !selectable(i))
                        i--;
                    if (i >= 0)
                        currentIndex = i;
                }
                Keys.onDownPressed: {
                    var i = currentIndex + 1;
                    while (i < count && !selectable(i))
                        i++;
                    if (i < count)
                        currentIndex = i;
                }

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
                    readonly property bool isSection: modelData.kind === "section"
                    readonly property bool isScanned: modelData.kind === "rescan" || modelData.kind === "delete"
                    readonly property bool highlighted: !isSection && ListView.view.activeFocus
                                                        && (ListView.isCurrentItem || rowMouse.containsMouse)
                    readonly property bool isSystem: modelData.kind === "system"
                    readonly property bool checked: modelData.kind === "all" ? root.allPicked
                                                  : modelData.kind === "androidgames" ? root.mergeAndroid
                                                  : isSystem ? !!root.picked[modelData.sys.name] : false

                    width: ListView.view.width
                    height: isSection ? vpx(18) * 2 : vpx(18) * 2.6
                    color: isSection ? "transparent" : highlighted ? "#3aa"
                         : modelData.kind === "import" ? "#555"
                         : modelData.kind === "folder" ? "#3a3a3a" : "transparent"

                    // check box
                    Rectangle {
                        id: box
                        visible: modelData.kind === "all" || modelData.kind === "androidgames" || parent.isSystem
                        width: vpx(20)
                        height: width
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
                                case "use": return qsTr("For games in your own metadata files, use ES-DE's") + api.tr;
                                case "section": return modelData.text;
                                case "rescan":
                                case "delete": return modelData.sys.fullname;
                                case "nav": return modelData.text;
                                case "androidgames": return qsTr("Merge Android Games into Android") + api.tr;
                                case "import": return root.pickedCount > 0
                                    ? qsTr("Import %1 systems").arg(root.pickedCount) + api.tr
                                    : qsTr("Import") + api.tr;
                                default: return modelData.sys.fullname;
                            }
                        }
                        color: parent.isSection ? "#bbb" : parent.highlighted ? "#111" : "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: parent.isSection ? vpx(17) : vpx(18)
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
                            if (modelData.kind === "use") {
                                var use = Internal.settings.esdeImportUse;
                                return "◂ " + (use === "media" ? qsTr("art only")
                                     : use === "replace" ? qsTr("art and details, replacing yours")
                                     : qsTr("art, and details you don't have")) + api.tr + " ▸";
                            }
                            if (modelData.kind === "nav")
                                return modelData.note;
                            if (modelData.kind === "rescan")
                                return qsTr("%1 games").arg(modelData.sys.games) + api.tr;
                            if (modelData.kind === "delete")
                                return root.confirmDelete === modelData.sys.name
                                    ? qsTr("A again to delete") + api.tr
                                    : qsTr("%1 games").arg(modelData.sys.games) + api.tr;
                            if (!parent.isSystem)
                                return "";
                            var parts = [qsTr("%1 games").arg(modelData.sys.games)];
                            if (modelData.sys.hasGamelist)
                                parts.push(qsTr("details"));
                            if (modelData.sys.hasMedia)
                                parts.push(qsTr("art"));
                            var shortName = (modelData.sys.name === "androidgames" && root.mergeAndroid)
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
                        enabled: !parent.isSection
                        onClicked: {
                            if (list.currentIndex !== index)
                                list.currentIndex = index;
                            list.focus = true;
                            root.activate(index);
                        }
                    }
                }
            }

            Text {
                visible: (root.page === "import" && root.systems.length === 0)
                    || ((root.page === "rescan" || root.page === "delete") && root.scanned.length === 0)
                text: root.loadingSystems ? qsTr("Looking for ES-DE systems in your ROM folders...") + api.tr
                    : root.page !== "import" ? qsTr("Nothing has been imported yet. Use Import first.") + api.tr
                    : Internal.settings.esdeImportDir === ""
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
                color: "#333"

                Rectangle {
                    width: Math.max(height, parent.width * root.progress)
                    height: parent.height
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
