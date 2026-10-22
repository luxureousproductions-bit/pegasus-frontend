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
    signal pickFolder(string dir_path)
    signal newFolderRequested(string parent_path)
    readonly property string currentFolder: folderModel.folder

    anchors.fill: parent

    // Pegasus X: every file is listed (the ones that can't be picked greyed
    // out); a folder opens at its first entry, and backing out lands on the
    // folder you left instead of keeping the old row number
    Component.onCompleted: root.showDrives()

    // Pegasus X: every browse starts on the drives; `path` (eg. the game's
    // media folder) becomes the "Suggested" jump
    property string startFolder: ""
    function openAt(path) {
        startFolder = path || "";
        showDrives();
        list.focus = true;
    }
    function showDrives() {
        folderModel.cd("/:pegasusx-drives:");  // no such folder: the drive list
        root.resetPosition();
    }
    // the folder it was last used in, for next time
    function remember() {
        if (folderModel.folder !== "" && folderModel.folder !== "/")
            Internal.settings.lastBrowseFolder = folderModel.folder;
    }

    // Pegasus X: quick jumps, in a row above the list (d-pad up from the
    // top of the list, or Y): the drives, one folder up (LB), the last folder
    // (RB), a suggested one, the ES-DE folder and Pegasus X's data folder
    property string dataFolder: Internal.settings.dataDir
    readonly property var shortcuts: {
        var out = [];
        if (pickFolders) {
            out.push({ "t": qsTr("Use this folder") + api.tr, "act": "use" });
            out.push({ "t": qsTr("New folder...") + api.tr, "act": "new" });
        }
        out.push({ "t": qsTr("Drives") + api.tr, "act": "drives" });
        out.push({ "t": qsTr("Up a folder  LB") + api.tr, "act": "up" });
        if (Internal.settings.lastBrowseFolder !== "")
            out.push({ "t": qsTr("Last folder  RB") + api.tr, "act": "go", "path": Internal.settings.lastBrowseFolder });
        if (startFolder !== "")
            out.push({ "t": qsTr("Suggested") + api.tr, "act": "go", "path": startFolder });
        if (Internal.settings.esdeImportDir !== "")
            out.push({ "t": qsTr("ES-DE") + api.tr, "act": "go", "path": Internal.settings.esdeImportDir });
        if (dataFolder !== "")
            out.push({ "t": qsTr("Pegasus X data") + api.tr, "act": "go", "path": dataFolder });
        return out;
    }
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
    function resetPosition() {
        list.currentIndex = list.count > 1 ? 1 : 0;
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
            event.accepted = true;
            root.remember();
            root.cancel();
        }
        else if (api.keys.isPrevPage(event)) {
            event.accepted = true;
            root.enter("..");
        }
        else if (api.keys.isNextPage(event)) {
            // RB: back to the last folder
            event.accepted = true;
            if (Internal.settings.lastBrowseFolder !== "") {
                folderModel.cd(Internal.settings.lastBrowseFolder);
                root.resetPosition();
                list.focus = true;
            }
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
        height: parent.height * 0.9
        width: height * 1.5
        color: "#444"
        radius: vpx(8)

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
                    text: folderModel.folder + "   ·   " + qsTr("LB up · RB last folder · Y quick jumps") + api.tr
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
                    radius: vpx(4)
                    color: highlighted ? "#3aa" : "#2b2b2b"
                    border.color: "#3aa"
                    border.width: highlighted ? 0 : vpx(1)
                    Text {
                        id: jumpText
                        anchors.centerIn: parent
                        text: modelData.t
                        color: parent.highlighted ? "#111" : "#9dd"
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
                anchors.bottom: parent.bottom
                color: "#333"

                ListView {
                    id: list
                    anchors.fill: parent
                    clip: true

                    model: folderModel
                    delegate: fileDelegate
                    // up from the top row: the quick jumps
                    Keys.onUpPressed: {
                        if (currentIndex <= 0)
                            jumpRow.focus = true;
                        else
                            decrementCurrentIndex();
                    }

                    focus: true
                    highlightRangeMode: ListView.ApplyRange
                    preferredHighlightBegin: height * 0.5 - vpx(18) * 1.25
                    preferredHighlightEnd: height * 0.5 + vpx(18) * 1.25
                    highlightMoveDuration: 0
                }
            }
        }
    }

    Component {
        id: fileDelegate

        Rectangle {
            readonly property bool highlighted: ListView.isCurrentItem || mouseArea.containsMouse

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
