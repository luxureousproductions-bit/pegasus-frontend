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

    // Pegasus X: pick the folder itself (browse into it, then "Use this
    // folder"), or one of the files named in fileNames/fileExtensions, in
    // which case pick() gets the file's path. Every file is shown, so you can
    // see what a folder has, but only those files can be picked.
    property bool folderMode: false
    property var fileNames: ["metadata.pegasus.txt", "metadata.txt"]
    property var fileExtensions: [".metadata.pegasus.txt", ".metadata.txt"]

    // a folder to open first, eg. a system's folder
    property string startFolder: ""
    onStartFolderChanged: {
        if (startFolder !== "") {
            folderModel.cd(startFolder);
            root.resetPosition();
        }
    }

    signal cancel
    signal pick(string dir_path)

    Component.onCompleted: root.resetPosition()

    // a new folder starts at its first entry (not "..")
    function resetPosition() {
        list.currentIndex = list.count > 1 ? 1 : 0;
        list.positionViewAtIndex(list.currentIndex, ListView.Center);
    }

    // into a folder: start at the top; back out: land on the folder we left
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

    anchors.fill: parent

    Keys.onPressed: {
        if (api.keys.isCancel(event) && !event.isAutoRepeat) {
            event.accepted = true;
            root.cancel();
        }
    }


    FolderListModel {
        id: folderModel
        files: root.fileNames
        extensions: root.fileExtensions
        showAllFiles: true
    }

    // the root lists the drives, which are not a sensible pick
    readonly property bool canUseFolder: folderMode && folderModel.folder !== "/" && folderModel.folder !== ""

    function useFolder() {
        if (canUseFolder)
            root.pick(folderModel.folder);
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
            text: root.folderMode
                ? (root.fileNames.length > 0 || root.fileExtensions.length > 0
                    ? qsTr("Open the folder you want, then select \"Use this folder\",\n"
                         + "or select one of the files in bold.")
                    : qsTr("Open the folder you want, then select \"Use this folder\"."))
                : qsTr("Select a collection file below (metadata.pegasus.txt or metadata.txt).\n"
                     + "The directory of the file will then be added to the list.")
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
                    text: folderModel.folder
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
            Rectangle {
                id: useButton

                readonly property bool highlighted: activeFocus || useMouse.containsMouse

                visible: root.folderMode
                width: parent.width
                height: visible ? useLabel.height : 0
                anchors.top: path.bottom
                color: highlighted ? "#3aa" : "#2a5f5f"
                opacity: root.canUseFolder ? 1.0 : 0.4

                KeyNavigation.down: list
                Keys.onPressed: {
                    if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                        event.accepted = true;
                        root.useFolder();
                    }
                }

                Text {
                    id: useLabel
                    text: qsTr("Use this folder") + api.tr
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    lineHeight: 2.5

                    color: parent.highlighted ? "#111" : "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(18)
                    font.bold: true
                }

                MouseArea {
                    id: useMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.useFolder()
                    cursorShape: Qt.PointingHandCursor
                }
            }
            Rectangle {
                width: parent.width
                anchors.top: useButton.bottom
                anchors.bottom: parent.bottom
                color: "#333"

                ListView {
                    id: list
                    anchors.fill: parent
                    clip: true

                    model: folderModel
                    delegate: fileDelegate

                    // up from the first entry reaches "Use this folder"
                    Keys.onUpPressed: {
                        if (root.folderMode && currentIndex <= 0)
                            useButton.focus = true;
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
                if (!pickable)
                    return;
                if (root.folderMode)
                    root.pick(folderModel.folder + "/" + name);
                else
                    root.pick(folderModel.folder);
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
