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


import "../common"
import QtQuick 2.8
import Pegasus.FolderListModel 1.0


FocusScope {
    id: root

    // Pegasus X: which files can be picked (pick() gets their folder), and
    // the text above the list; the defaults pick a metadata file
    property var fileNames: ["metadata.pegasus.txt", "metadata.txt"]
    property var fileExtensions: [".metadata.pegasus.txt", ".metadata.txt"]
    property string infoText: qsTr("Select a collection file below (metadata.pegasus.txt or metadata.txt).\n"
                                   + "The directory of the file will then be added to the list.")

    signal cancel
    signal pick(string dir_path)
    signal pickFile(string file_path)  // Pegasus X: the picked file itself
    // Pegasus X: picking a folder instead of a file ("Use this folder"), and
    // making a new one in the current folder (named here, then opened)
    property bool pickFolders: false
    property bool allowNewFolder: true   // "Create new folder..." beside "Use this folder"
    signal pickFolder(string dir_path)
    signal newFolderRequested(string parent_path)  // (told after the picker made it itself)
    readonly property string currentFolder: folderModel.folder

    anchors.fill: parent

    // Pegasus X: every file is listed (the ones that can't be picked greyed
    // out); a folder opens at its first entry, and backing out lands on the
    // folder you left instead of keeping the old row number
    // Pegasus X: the drives are listed when the browser is first shown, not
    // when the screen holding it opens (Library, Game folders, ES-DE import)
    property bool listed: false
    onFocusChanged: {
        if (focus && !listed)
            root.showDrives();
    }

    // Pegasus X: every browse starts on the drives; `path` (eg. the game's
    // media folder) becomes the "Suggested" jump
    property string startFolder: ""
    // picking a picture: the highlighted one is shown beside the list
    readonly property bool picturePicker: !pickFolders && fileExtensions.indexOf(".png") >= 0
    function openAt(path) {
        startFolder = path || "";
        showDrives();
        list.focus = true;
    }
    function showDrives() {
        listed = true;
        folderModel.cd("/:pegasusx-drives:");  // no such folder: the drive list
        root.resetPosition();
    }
    // the folder it was last used in, for next time
    function remember() {
        if (folderModel.folder !== "" && folderModel.folder !== "/")
            Internal.settings.lastBrowseFolder = folderModel.folder;
    }

    // Pegasus X: quick jumps, pinned in a row above the list (d-pad up from
    // the top of the list, or Y): the Pegasus X folder, ES-DE's folder, the
    // drives, the ROM folders (game directories), the last folder, and the
    // suggested one when the screen gives one. The buttons for them are in
    // the help line at the bottom
    property string dataFolder: Internal.settings.dataDir   // kept for the screens that set it
    readonly property var shortcuts: {
        var out = [];
        if (Internal.settings.rootDir !== "")
            out.push({ "t": qsTr("Pegasus X") + api.tr, "act": "go", "path": Internal.settings.rootDir });
        if (Internal.settings.esdeImportDir !== "")
            out.push({ "t": qsTr("ES-DE") + api.tr, "act": "go", "path": Internal.settings.esdeImportDir });
        out.push({ "t": qsTr("Drives") + api.tr, "act": "drives" });
        // the ROM folders (not Pegasus X's own imports folder)
        var dirs = Internal.settings.gameDirs;
        for (var d = 0; d < dirs.length; d++) {
            var dir = String(dirs[d]);
            if (dir.indexOf("/metadata/imports") >= 0 || dir.indexOf("/gamedirectory") >= 0)
                continue;
            var dirName = dir.replace(/\/+$/, "");
            dirName = dirName.substring(dirName.lastIndexOf("/") + 1) || dirName;
            out.push({ "t": dirName, "act": "go", "path": dir, "rom": true });
        }
        if (Internal.settings.lastBrowseFolder !== "")
            out.push({ "t": qsTr("Last folder") + api.tr, "act": "go", "path": Internal.settings.lastBrowseFolder });
        if (startFolder !== "")
            out.push({ "t": qsTr("Suggested") + api.tr, "act": "go", "path": startFolder });
        return out;
    }
    // picking a folder: the buttons over the list ("Use this folder" first:
    // X, or a tap, or d-pad up to it)
    readonly property var folderButtons: !pickFolders ? []
        : allowNewFolder ? [ { "t": qsTr("Use this folder") + api.tr, "act": "use" }, { "t": qsTr("Create new folder...") + api.tr, "act": "new" } ]
        : [ { "t": qsTr("Use this folder") + api.tr, "act": "use" } ]
    // a short note over the list ("Open a folder first", "Couldn't make it")
    property string note: ""
    Timer { id: noteTimer; interval: 3500; onTriggered: root.note = "" }
    function say(text) {
        note = text;
        noteTimer.restart();
    }
    function jump(entry) {
        if (entry.act === "use" || entry.act === "new") {
            // the drive list is no folder: open one first
            if (root.onDrives) {
                say(entry.act === "use" ? qsTr("Open a drive or a folder first, then pick \"Use this folder\".") + api.tr
                                        : qsTr("Open a drive or a folder first: the new folder goes in it.") + api.tr);
                list.focus = true;
                return;
            }
        }
        if (entry.act === "use") {
            root.remember();
            root.pickFolder(folderModel.folder);
            return;
        }
        if (entry.act === "new") {
            newFolder.openIn(folderModel.folder);
            return;
        }
        if (entry.act === "up")
            enter("..");
        else if (entry.act === "drives") {
            showDrives();
        }
        else {
            folderModel.cd(entry.path);
            resetPosition();
        }
        list.focus = true;
    }
    // the drive list (it has no ".." row)
    readonly property bool onDrives: folderModel.folder !== undefined && folderModel.indexOfName("..") !== 0  // re-read when the folder changes
    // the folder's own name, and the one above it (for the "up" row)
    readonly property string folderName: {
        var f = String(folderModel.folder).replace(/\/+$/, "");
        return f.substring(f.lastIndexOf("/") + 1) || f || "/";
    }
    readonly property string parentName: {
        var f = String(folderModel.folder).replace(/\/+$/, "");
        f = f.substring(0, f.lastIndexOf("/"));
        return f.substring(f.lastIndexOf("/") + 1) || qsTr("Drives") + api.tr;
    }
    function resetPosition() {
        // the first entry after ".."
        list.currentIndex = (list.count > 1 && folderModel.indexOfName("..") === 0) ? 1 : 0;
        list.positionViewAtIndex(list.currentIndex, ListView.Center);
    }
    function enter(name) {
        if (name === "..") {
            // back up: on the folder just left
            const current = folderModel.folder;
            const child = current.substring(current.lastIndexOf("/") + 1);
            folderModel.cd(name);
            var idx = folderModel.indexOfName(child);
            if (idx < 0)
                idx = folderModel.indexOfName(current);  // a drive, in the drive list
            if (idx >= 0) {
                list.currentIndex = idx;
                list.positionViewAtIndex(idx, ListView.Center);
                return;
            }
        }
        else {
            folderModel.cd(name);
        }
        root.resetPosition();
    }
    // B: up a folder; on the drive list, out of the browser
    function goUp() {
        if (root.onDrives) {
            root.remember();
            root.cancel();
        }
        else {
            root.enter("..");
            list.focus = true;
        }
    }

    Keys.onPressed: {
        if (event.isAutoRepeat && !(api.keys.isPrevPage(event) || api.keys.isNextPage(event)))
            return;
        if (api.keys.isCancel(event)) {
            event.accepted = true;
            root.goUp();
        }
        else if (api.keys.isDetails(event)) {
            event.accepted = true;
            if (root.pickFolders) {
                // X: this folder
                root.jump(root.folderButtons[0]);
            }
            else {
                // (picking a file: out of the browser, as before)
                root.remember();
                root.cancel();
            }
        }
        else if (api.keys.isPrevPage(event) || api.keys.isNextPage(event)) {
            // LB / RB: 10 rows up / down, stopping at the ends
            event.accepted = true;
            list.focus = true;
            var to = list.currentIndex + (api.keys.isNextPage(event) ? 10 : -10);
            list.currentIndex = Math.max(0, Math.min(list.count - 1, to));
        }
        else if (api.keys.isPageUp(event) || api.keys.isPageDown(event)) {
            // LT / RT: the top / the bottom
            event.accepted = true;
            list.focus = true;
            list.currentIndex = api.keys.isPageDown(event) ? list.count - 1 : 0;
        }
        else if (api.keys.isFilters(event)) {
            event.accepted = true;
            (jumpRow.activeFocus ? list : jumpRow).focus = true;
        }
    }


    FolderListModel {
        id: folderModel
        files: root.fileNames
        extensions: root.fileExtensions
        showAllFiles: true
    }


    Rectangle {
        id: shade
        anchors.fill: parent
        color: "#000"

        opacity: 0.3

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: root.cancel()
        }
    }


    Rectangle {
        id: frame
        height: parent.height * 0.9
        width: Math.min(height * 1.5, parent.width * 0.96)
        color: "#444"

        anchors.centerIn: parent


        MouseArea {
            anchors.fill: parent
        }

        Text {
            id: info
            text: root.infoText
            color: "#ee4"
            font.family: globalFonts.sans
            font.pixelSize: vpx(18)
            lineHeight: 1.15

            anchors.top: parent.top
            width: parent.width
            padding: font.pixelSize

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
        }
        Item {
            anchors.top: info.bottom
            anchors.bottom: parent.bottom
            width: parent.width - vpx(30)
            anchors.bottomMargin: vpx(15)
            anchors.horizontalCenter: parent.horizontalCenter

            // where you are: the folder's name, its whole path under it
            Rectangle {
                id: path
                width: parent.width
                height: pathColumn.height + vpx(14)
                color: "#222"

                Column {
                    id: pathColumn
                    anchors.verticalCenter: parent.verticalCenter
                    x: vpx(18)
                    width: parent.width - vpx(36)
                    spacing: vpx(2)
                    Text {
                        width: parent.width
                        text: root.onDrives ? qsTr("Drives") + api.tr : root.folderName
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(20)
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: root.onDrives ? qsTr("Open a drive to start") + api.tr : folderModel.folder
                        color: "#9a9a9a"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(15)
                        elide: Text.ElideMiddle
                    }
                }
            }
            ListView {
                id: jumpRow
                // Pegasus X: focus moves at once (no slide to catch up with a held D-pad)
                highlightMoveDuration: 0
                anchors.top: path.bottom
                anchors.topMargin: vpx(8)
                width: parent.width
                height: vpx(38)
                orientation: ListView.Horizontal
                spacing: vpx(8)
                clip: true
                model: root.shortcuts
                KeyNavigation.down: root.pickFolders ? buttonRow : list
                Keys.onPressed: {
                    if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                        event.accepted = true;
                        root.jump(root.shortcuts[currentIndex]);
                    }
                }
                delegate: Rectangle {
                    readonly property bool highlighted: ListView.view.activeFocus && ListView.isCurrentItem
                    width: jumpText.implicitWidth + vpx(24)
                    height: ListView.view.height
                    color: highlighted ? "#4ae" : (modelData.rom ? "#2f3a3a" : "#2b2b2b")
                    Text {
                        id: jumpText
                        anchors.centerIn: parent
                        text: modelData.t
                        color: parent.highlighted ? "#111" : "#ddd"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(16)
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.jump(modelData)
                    }
                }
            }

            // picking a folder: "Use this folder" (and "Create new folder..."),
            // over the list so it's always in view
            Row {
                id: buttonRow
                visible: root.pickFolders
                anchors.top: jumpRow.bottom
                anchors.topMargin: vpx(8)
                width: parent.width
                height: visible ? vpx(18) * 2.4 : 0
                spacing: vpx(8)
                property int current: 0
                onActiveFocusChanged: if (activeFocus) current = 0
                Keys.onLeftPressed: current = Math.max(0, current - 1)
                Keys.onRightPressed: current = Math.min(root.folderButtons.length - 1, current + 1)
                Keys.onUpPressed: jumpRow.focus = true
                Keys.onDownPressed: list.focus = true
                Keys.onPressed: {
                    if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                        event.accepted = true;
                        root.jump(root.folderButtons[current]);
                    }
                }
                Repeater {
                    model: root.folderButtons
                    delegate: Rectangle {
                        readonly property bool on: (buttonRow.activeFocus && index === buttonRow.current) || buttonMouse.containsMouse
                        readonly property bool use: modelData.act === "use"
                        // "Use this folder" the wider one
                        width: root.folderButtons.length === 1 ? buttonRow.width
                             : use ? (buttonRow.width - buttonRow.spacing) * 0.62 : (buttonRow.width - buttonRow.spacing) * 0.38
                        height: buttonRow.height
                        color: on ? "#4ae" : (use ? "#2f6f6f" : "#555")
                        opacity: root.onDrives ? 0.45 : 1
                        Row {
                            anchors.centerIn: parent
                            spacing: vpx(10)
                            // its button: X
                            Image {
                                visible: parent.parent.use && status === Image.Ready
                                anchors.verticalCenter: parent.verticalCenter
                                width: vpx(26); height: vpx(26)
                                source: parent.parent.use ? "qrc:/frontend/assets/gamepad/x360/x.png" : ""
                                sourceSize.width: vpx(52)
                                sourceSize.height: vpx(52)
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.t
                                color: parent.parent.on ? "#111" : "#eee"
                                font.family: globalFonts.sans
                                font.pixelSize: vpx(18)
                                font.bold: parent.parent.use
                            }
                        }
                        MouseArea {
                            id: buttonMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.jump(modelData)
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                anchors.top: buttonRow.visible ? buttonRow.bottom : jumpRow.bottom
                anchors.topMargin: vpx(8)
                anchors.bottom: helpLine.top
                anchors.bottomMargin: vpx(8)
                color: "#333"

                ListView {
                    id: list
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    width: root.picturePicker ? parent.width * 0.6 : parent.width
                    clip: true

                    model: folderModel
                    delegate: fileDelegate
                    // up from the top row: "Use this folder" (picking a
                    // folder), else the quick jumps
                    Keys.onUpPressed: {
                        if (currentIndex <= 0)
                            (root.pickFolders ? buttonRow : jumpRow).focus = true;
                        else
                            decrementCurrentIndex();
                    }
                    Keys.onDownPressed: incrementCurrentIndex()

                    focus: true
                    boundsBehavior: Flickable.StopAtBounds
                    highlightRangeMode: ListView.ApplyRange
                    preferredHighlightBegin: height * 0.5 - vpx(18) * 1.25
                    preferredHighlightEnd: height * 0.5 + vpx(18) * 1.25
                    // the row lights at once; the list scrolls to it smoothly
                    highlight: Item {}
                    highlightMoveDuration: 120
                    highlightMoveVelocity: -1
                    onCurrentIndexChanged: previewDelay.restart()
                }
                // where the list is (a long folder)
                Rectangle {
                    visible: list.visibleArea.heightRatio < 1
                    anchors.right: list.right
                    anchors.rightMargin: vpx(3)
                    y: list.y + list.visibleArea.yPosition * list.height
                    width: vpx(4)
                    height: Math.max(vpx(24), list.visibleArea.heightRatio * list.height)
                    radius: width / 2
                    color: "#777"
                }

                // the highlighted picture, once the cursor stops on it
                Timer {
                    id: previewDelay
                    interval: 150
                    onTriggered: {
                        var item = list.currentItem;
                        preview.source = (root.picturePicker && item && item.isPicture)
                            ? "file://" + folderModel.folder + "/" + item.fileName : "";
                    }
                }
                Item {
                    visible: root.picturePicker
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: list.right
                    anchors.right: parent.right
                    anchors.margins: vpx(12)

                    Image {
                        id: preview
                        anchors.fill: parent
                        anchors.bottomMargin: previewName.height
                        fillMode: Image.PreserveAspectFit
                        sourceSize.width: vpx(400)
                        asynchronous: true
                    }
                    Text {
                        id: previewName
                        anchors.bottom: parent.bottom
                        width: parent.width
                        text: preview.source != "" && preview.status === Image.Ready
                            ? qsTr("%1 × %2").arg(preview.implicitWidth).arg(preview.implicitHeight) + api.tr : ""
                        color: "#999"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(14)
                        horizontalAlignment: Text.AlignHCenter
                        padding: vpx(4)
                    }
                }
            }

            // the buttons: the round prompts of Settings, few of them (the only
            // hint bar while the picker is open; the screen behind hides its own)
            RoundPromptBar {
                id: helpLine
                anchors.bottom: parent.bottom
                width: parent.width
                items: {
                    var out = [ { "keys": ["A"], "text": qsTr("open") + api.tr } ];
                    if (root.pickFolders && !root.onDrives)
                        out.push({ "keys": ["X"], "text": qsTr("use this folder") + api.tr });
                    out.push({ "keys": ["Y"], "text": qsTr("quick jumps") + api.tr });
                    out.push({ "keys": ["B"], "text": (root.onDrives ? qsTr("close") : qsTr("up a folder")) + api.tr });
                    return out;
                }
            }
        }
    }

    Rectangle {
        visible: root.note !== ""
        z: 3
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: vpx(70)
        width: noteText.implicitWidth + vpx(40)
        height: noteText.implicitHeight + vpx(24)
        color: "#e8b06a"
        Text {
            id: noteText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, root.width * 0.8)
            text: root.note
            color: "#1a1a1a"
            font.family: globalFonts.sans
            font.pixelSize: vpx(17)
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // -- A new folder: its name, then it's made and opened ----------------------------
    FocusScope {
        id: newFolder

        property string parentPath: ""

        anchors.fill: parent
        enabled: focus
        visible: focus
        z: 4

        function openIn(path) {
            parentPath = path;
            folderName.text = "";
            folderKeyboard.row = 1;
            folderKeyboard.col = 0;
            focus = true;
            folderKeyboard.focus = true;
        }
        function back() {
            list.focus = true;
        }
        function finish() {
            // no characters a folder name can't have
            var name = folderName.text.trim().replace(new RegExp("[\\\\/:*?<>|\"]", "g"), "_");
            if (name === "")
                return;
            var path = parentPath.replace(new RegExp("/+$"), "") + "/" + name;
            if (!Internal.settings.makeDir(path)) {
                root.say(qsTr("Couldn't make %1 here.").arg(name) + api.tr);
                back();
                return;
            }
            // into it, ready for "Use this folder"
            folderModel.cd(path);
            root.resetPosition();
            root.newFolderRequested(parentPath);
            root.say(qsTr("Made %1: \"Use this folder\" picks it.").arg(name) + api.tr);
            list.focus = true;
        }

        Keys.onPressed: {
            if (api.keys.isCancel(event) && !event.isAutoRepeat) {
                event.accepted = true;
                newFolder.back();
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.5
            MouseArea { anchors.fill: parent; onClicked: newFolder.back() }
        }
        Rectangle {
            width: Math.min(parent.width * 0.7, vpx(800))
            height: folderColumn.height + vpx(40)
            anchors.horizontalCenter: parent.horizontalCenter
            y: vpx(30)
            color: "#444"
            MouseArea { anchors.fill: parent }
            Column {
                id: folderColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(20)
                spacing: vpx(12)
                Text {
                    width: parent.width
                    text: qsTr("New folder in %1").arg(newFolder.parentPath) + api.tr
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(18)
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                }
                Rectangle {
                    width: parent.width
                    height: vpx(52)
                    color: "#333"
                    border.color: "#3aa"
                    border.width: vpx(2)
                    TextInput {
                        id: folderName
                        anchors.fill: parent
                        anchors.leftMargin: vpx(14)
                        anchors.rightMargin: vpx(14)
                        verticalAlignment: TextInput.AlignVCenter
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(20)
                        clip: true
                        cursorVisible: newFolder.visible
                        activeFocusOnPress: false
                    }
                }
                Text {
                    width: parent.width
                    text: qsTr("Start  make it and open it     B  cancel") + api.tr
                    color: "#aaa"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(15)
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
        OnScreenKeyboard {
            id: folderKeyboard
            anchors.bottom: parent.bottom
            anchors.bottomMargin: vpx(12)
            anchors.horizontalCenter: parent.horizontalCenter
            keyH: vpx(40)
            target: folderName
            onDone: newFolder.finish()
        }
    }

    Component {
        id: fileDelegate

        Rectangle {
            id: row
            readonly property bool highlighted: ListView.isCurrentItem || mouseArea.containsMouse
            readonly property string fileName: name
            readonly property bool isUp: name === ".."
            readonly property bool isPicture: !isDir && /\.(png|jpe?g|webp|gif|bmp|svg)$/i.test(name)

            // folders and files told apart: a folder has its icon and a ›,
            // a file that can be picked is bold, one that can't is grey
            color: highlighted ? "#566" : (isUp ? "#2b2b2b" : "transparent")
            width: ListView.view.width
            height: label.height

            function pickItem() {
                if (isDir) {
                    root.enter(name);
                    return;
                }
                if (pickable) {
                    root.remember();
                    root.pickFile(folderModel.folder + "/" + name);
                    root.pick(folderModel.folder);
                }
            }

            Keys.onPressed: {
                if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                    event.accepted = true;
                    pickItem();
                }
            }

            Image {
                id: icon
                visible: isDir
                source: isDir ? "qrc:/frontend/assets/dir-icon.png" : ""
                opacity: row.isUp ? 0.6 : 1

                fillMode: Image.PreserveAspectFit
                height: parent.height * 0.6
                width: height

                anchors.left: parent.left
                anchors.leftMargin: parent.height * 0.5
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                id: label
                text: row.isUp ? qsTr("Up a folder (%1)").arg(root.parentName) + api.tr : name
                verticalAlignment: Text.AlignVCenter
                lineHeight: 2.5

                color: row.isUp ? "#bbb" : (isDir || pickable) ? "#eee" : "#777"
                font.bold: !isDir && pickable
                font.italic: row.isUp
                font.family: globalFonts.sans
                font.pixelSize: vpx(18)

                anchors.left: parent.left
                anchors.leftMargin: parent.height * 0.5 + parent.height * 0.6
                anchors.right: chevron.left
                leftPadding: parent.height * 0.25
                rightPadding: parent.height * 0.25
                elide: Text.ElideRight
            }
            Text {
                id: chevron
                anchors.right: parent.right
                anchors.rightMargin: parent.height * 0.4
                anchors.verticalCenter: parent.verticalCenter
                text: isDir && !row.isUp ? "›" : ""
                color: "#999"
                font.family: globalFonts.sans
                font.pixelSize: vpx(24)
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                onPressed: row.ListView.view.currentIndex = index
                onClicked: row.pickItem()
                cursorShape: Qt.PointingHandCursor
            }
        }
    }
}
