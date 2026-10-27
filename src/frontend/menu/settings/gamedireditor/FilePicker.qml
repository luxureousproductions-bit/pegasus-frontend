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
    // asking the host for a name to make a new one in the current folder
    property bool pickFolders: false
    property bool allowNewFolder: true   // "Create new folder..." beside "Use this folder"
    signal pickFolder(string dir_path)
    signal newFolderRequested(string parent_path)
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
    // picking a folder: the buttons at the bottom
    readonly property var folderButtons: !pickFolders ? []
        : allowNewFolder ? [ { "t": qsTr("Use this folder") + api.tr, "act": "use" }, { "t": qsTr("Create new folder...") + api.tr, "act": "new" } ]
        : [ { "t": qsTr("Use this folder") + api.tr, "act": "use" } ]
    function jump(entry) {
        if (entry.act === "use") {
            root.remember();
            root.pickFolder(folderModel.folder);
            return;
        }
        if (entry.act === "new") {
            root.newFolderRequested(folderModel.folder);
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
    function resetPosition() {
        // the first entry after ".."
        list.currentIndex = (list.count > 1 && folderModel.indexOfName("..") === 0) ? 1 : 0;
        list.positionViewAtIndex(list.currentIndex, ListView.Center);
    }
    function enter(name) {
        if (name === "..") {
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

    Keys.onPressed: {
        if (event.isAutoRepeat)
            return;
        if (api.keys.isCancel(event)) {
            // B: up a folder; on the drive list, out of the browser
            event.accepted = true;
            if (folderModel.indexOfName("..") !== 0) {
                root.remember();
                root.cancel();
            }
            else {
                root.enter("..");
                list.focus = true;
            }
        }
        else if (api.keys.isDetails(event)) {
            // X: out of the browser, from any folder
            event.accepted = true;
            root.remember();
            root.cancel();
        }
        else if (api.keys.isPrevPage(event)) {
            // LB: back to the last folder
            event.accepted = true;
            if (Internal.settings.lastBrowseFolder !== "") {
                folderModel.cd(Internal.settings.lastBrowseFolder);
                root.resetPosition();
                list.focus = true;
            }
        }
        else if (api.keys.isNextPage(event)) {
            // RB: the drives
            event.accepted = true;
            root.showDrives();
            list.focus = true;
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

            Rectangle {
                id: path
                width: parent.width
                height: pathText.height
                color: "#222"

                Text {
                    id: pathText
                    text: root.onDrives ? qsTr("Drives") + api.tr : folderModel.folder
                    width: parent.width

                    color: "#bbb"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(18)

                    lineHeight: 2.5
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: font.pixelSize; rightPadding: leftPadding
                    elide: Text.ElideMiddle
                }
            }
            ListView {
                id: jumpRow
                anchors.top: path.bottom
                anchors.topMargin: vpx(8)
                width: parent.width
                height: vpx(38)
                orientation: ListView.Horizontal
                spacing: vpx(8)
                clip: true
                model: root.shortcuts
                KeyNavigation.down: list
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
            Rectangle {
                width: parent.width
                anchors.top: jumpRow.bottom
                anchors.topMargin: vpx(8)
                anchors.bottom: buttonRow.visible ? buttonRow.top : helpLine.top
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
                    // up from the top row: the quick jumps; down from the
                    // last one: the buttons (picking a folder)
                    Keys.onUpPressed: {
                        if (currentIndex <= 0)
                            jumpRow.focus = true;
                        else
                            decrementCurrentIndex();
                    }
                    Keys.onDownPressed: {
                        if (currentIndex >= count - 1 && root.pickFolders)
                            buttonRow.focus = true;
                        else
                            incrementCurrentIndex();
                    }

                    focus: true
                    highlightRangeMode: ListView.ApplyRange
                    preferredHighlightBegin: height * 0.5 - vpx(18) * 1.25
                    preferredHighlightEnd: height * 0.5 + vpx(18) * 1.25
                    highlightMoveDuration: 0
                    onCurrentIndexChanged: previewDelay.restart()
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

            // picking a folder: Use this folder / Create new folder, square
            // buttons at the bottom as in Pegasus' dialogs
            Row {
                id: buttonRow
                visible: root.pickFolders
                anchors.bottom: helpLine.top
                anchors.bottomMargin: vpx(8)
                width: parent.width
                height: vpx(18) * 2.4
                property int current: 0
                onActiveFocusChanged: if (activeFocus) current = 0
                Keys.onLeftPressed: current = Math.max(0, current - 1)
                Keys.onRightPressed: current = Math.min(root.folderButtons.length - 1, current + 1)
                Keys.onUpPressed: list.focus = true
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
                        width: buttonRow.width / Math.max(1, root.folderButtons.length)
                        height: buttonRow.height
                        color: on ? "#4ae" : (index === 0 ? "#555" : "#666")
                        Text {
                            anchors.centerIn: parent
                            text: modelData.t
                            color: "#eee"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(18)
                            font.bold: index === 0
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

            // the buttons, as Pegasus' help line
            Rectangle {
                id: helpLine
                anchors.bottom: parent.bottom
                width: parent.width
                height: helpText.height
                color: "#222"
                Text {
                    id: helpText
                    width: parent.width
                    text: (root.onDrives
                        ? qsTr("A  Open     B  Close     LB  Last folder     Y  Quick jumps")
                        : qsTr("A  Open     B  Up     X  Close     LB  Last folder     RB  Drives     Y  Quick jumps"))
                        + (root.pickFolders ? (root.allowNewFolder ? qsTr("     ▾  Use this folder / Create new") : qsTr("     ▾  Use this folder")) : "") + api.tr
                    color: "#bbb"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(15)
                    lineHeight: 2.2
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
            }
        }
    }

    Component {
        id: fileDelegate

        Rectangle {
            readonly property bool highlighted: ListView.isCurrentItem || mouseArea.containsMouse
            readonly property string fileName: name
            readonly property bool isPicture: !isDir && /\.(png|jpe?g|webp|gif|bmp|svg)$/i.test(name)

            color: highlighted ? "#566" : "transparent"
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
                source: isDir ? "qrc:/frontend/assets/dir-icon.png" : "";

                fillMode: Image.PreserveAspectFit
                height: parent.height * 0.75
                width: height

                anchors.left: parent.left
                anchors.leftMargin: parent.height * 0.5
                anchors.top: parent.top
                anchors.topMargin: parent.height * 0.08
            }

            Text {
                id: label
                text: name
                verticalAlignment: Text.AlignVCenter
                lineHeight: 2.5

                color: (isDir || pickable) ? "#eee" : "#888"
                font.bold: !isDir && pickable
                font.family: globalFonts.sans
                font.pixelSize: vpx(18)

                anchors.left: icon.right
                anchors.right: parent.right
                leftPadding: parent.height * 0.25
                rightPadding: parent.height * 0.5
                elide: Text.ElideRight
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: pickItem()
                cursorShape: Qt.PointingHandCursor
            }
        }
    }
}
