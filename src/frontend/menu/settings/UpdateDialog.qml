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


// Pegasus X: Settings > Check for updates, in the style of Pegasus' dialogs:
// the newest build published on GitHub, downloaded and installed over this one.
FocusScope {
    id: root

    signal close

    anchors.fill: parent
    enabled: focus
    visible: focus

    readonly property var updater: Internal.settings.updater
    readonly property string ustate: updater.state
    readonly property var info: updater.info
    property int choice: 0

    Component.onCompleted: updater.check()

    // the buttons for each state: [label, action]
    readonly property var buttons: {
        if (ustate === "available")
            return info.canInstall ? [[qsTr("Download and install") + api.tr, "download"], [qsTr("Not now") + api.tr, "close"]]
                                   : [[qsTr("Ok") + api.tr, "close"]];
        if (ustate === "downloading")
            return [[qsTr("Stop") + api.tr, "cancel"]];
        if (ustate === "ready")
            return [[qsTr("Install") + api.tr, "install"], [qsTr("Close") + api.tr, "close"]];
        if (ustate === "error")
            return [[qsTr("Try again") + api.tr, "check"], [qsTr("Close") + api.tr, "close"]];
        if (ustate === "checking")
            return [[qsTr("Close") + api.tr, "close"]];
        return [[qsTr("Ok") + api.tr, "close"]];
    }
    onButtonsChanged: choice = 0

    function act(what) {
        if (what === "download") updater.download();
        else if (what === "install") updater.install();
        else if (what === "cancel") updater.cancel();
        else if (what === "check") updater.check();
        else root.close();
    }

    Keys.onPressed: {
        if (event.isAutoRepeat)
            return;
        if (api.keys.isCancel(event)) {
            event.accepted = true;
            if (ustate === "downloading")
                updater.cancel();
            root.close();
        }
        else if (api.keys.isAccept(event)) {
            event.accepted = true;
            act(buttons[Math.min(choice, buttons.length - 1)][1]);
        }
    }
    Keys.onLeftPressed: choice = Math.max(0, choice - 1)
    Keys.onRightPressed: choice = Math.min(buttons.length - 1, choice + 1)

    function mb(bytes) { return (bytes / 1048576).toFixed(1); }

    Rectangle {
        anchors.fill: parent
        color: "#000"
        opacity: 0.5
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: root.close()
        }
    }

    Column {
        id: dialog
        anchors.centerIn: parent
        width: Math.min(parent.height * 0.9, parent.width * 0.8)

        MouseArea {
            width: parent.width
            height: 0
        }

        // title bar
        Rectangle {
            width: parent.width
            height: vpx(20) * 2.25
            color: "#333"
            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: vpx(15)
                text: qsTr("Check for updates") + api.tr
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(20)
                font.bold: true
            }
        }

        // text area
        Rectangle {
            width: parent.width
            height: body.height + vpx(18) * 3
            color: "#555"

            Column {
                id: body
                anchors.centerIn: parent
                width: parent.width - vpx(36)
                spacing: vpx(12)

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(18)
                    text: {
                        var current = qsTr("This is Pegasus X build %1.").arg(root.info.current) + api.tr;
                        switch (root.ustate) {
                            case "checking": return current + "\n" + qsTr("Looking for a newer build...") + api.tr;
                            case "uptodate": return current + "\n" + qsTr("It's the newest one.") + api.tr;
                            case "available":
                                return current + "\n" + qsTr("Build %1 is out.").arg(root.info.latest) + api.tr
                                    + (root.info.canInstall ? "" : "\n" + qsTr("Get it from the GitHub release (updates install on Android and Windows).") + api.tr);
                            case "downloading":
                                return qsTr("Downloading build %1... %2 of %3 MB").arg(root.info.latest)
                                    .arg(root.mb(root.info.received)).arg(root.mb(root.info.size)) + api.tr;
                            case "ready":
                                return root.info.error !== "" ? root.info.error
                                    : qsTr("Build %1 is downloaded: Android's installer asks to install it over this one. Your settings, library and themes stay.").arg(root.info.latest) + api.tr;
                            case "error": return root.info.error;
                        }
                        return current;
                    }
                }
                // what's new: the release notes (the commit's message)
                Text {
                    width: parent.width
                    visible: (root.ustate === "available" || root.ustate === "uptodate") && text !== ""
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    maximumLineCount: 4
                    elide: Text.ElideRight
                    color: "#ccc"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(16)
                    text: String(root.info.notes || "").split("\n").filter(function(l) {
                        return l.indexOf("build:") !== 0 && l.indexOf("commit:") !== 0 && l.trim() !== "";
                    }).join("\n")
                }
                // how far the download is
                Rectangle {
                    width: parent.width
                    height: vpx(10)
                    color: "#333"
                    visible: root.ustate === "downloading"
                    Rectangle {
                        height: parent.height
                        color: "#4ae"
                        width: parent.width * (root.info.size > 0 ? Math.min(1, root.info.received / root.info.size) : 0)
                    }
                }
            }
        }

        // button row
        Row {
            width: parent.width
            Repeater {
                model: root.buttons
                delegate: Rectangle {
                    readonly property bool on: index === root.choice || buttonMouse.containsMouse
                    width: parent.width / root.buttons.length
                    height: vpx(18) * 2.25
                    color: on ? "#4ae" : "#666"
                    Text {
                        anchors.centerIn: parent
                        text: modelData[0]
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(18)
                    }
                    MouseArea {
                        id: buttonMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.act(modelData[1])
                    }
                }
            }
        }
    }
}
