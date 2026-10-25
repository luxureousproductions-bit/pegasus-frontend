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


import QtQuick 2.6


// Pegasus X: a dialog in the style of Pegasus' own (see dialogs/GenericOkDialog):
// a title bar, a text area (with more items under the text, if given) and a
// row of buttons. ◂ ▸ pick a button, A presses it (picked), B cancels.
FocusScope {
    id: root

    property string title: ""
    property string text: ""
    property var buttons: []          // their labels
    property int choice: 0
    property bool busy: false         // working: no buttons, B is "cancelled" still
    property real dialogWidth: Math.min(parent.width * 0.8, vpx(760))
    default property alias content: extra.data

    readonly property int textSize: vpx(18)
    readonly property int titleTextSize: vpx(20)

    signal picked(int index)
    signal cancelled()

    anchors.fill: parent
    enabled: focus
    visible: focus
    z: 5

    onButtonsChanged: choice = Math.min(choice, Math.max(0, buttons.length - 1))

    Keys.onPressed: {
        if (event.isAutoRepeat)
            return;
        if (api.keys.isAccept(event)) {
            event.accepted = true;
            if (!busy && buttons.length > 0)
                root.picked(choice);
        }
        else if (api.keys.isCancel(event)) {
            event.accepted = true;
            root.cancelled();
        }
    }
    Keys.onLeftPressed: choice = Math.max(0, choice - 1)
    Keys.onRightPressed: choice = Math.min(buttons.length - 1, choice + 1)

    Rectangle {
        anchors.fill: parent
        color: "#000"
        opacity: 0.5
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: if (!root.busy) root.cancelled()
        }
    }

    Column {
        anchors.centerIn: parent
        width: root.dialogWidth

        // title bar
        Rectangle {
            width: parent.width
            height: root.titleTextSize * 2.25
            color: "#333"
            visible: root.title !== ""
            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: root.titleTextSize * 0.75
                anchors.right: parent.right
                anchors.rightMargin: root.titleTextSize * 0.75
                text: root.title
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: root.titleTextSize
                font.bold: true
                elide: Text.ElideRight
            }
            MouseArea { anchors.fill: parent }
        }

        // text area
        Rectangle {
            width: parent.width
            height: body.height + root.textSize * 2
            color: "#555"
            MouseArea { anchors.fill: parent }

            Column {
                id: body
                anchors.centerIn: parent
                width: parent.width - root.textSize * 2
                spacing: root.textSize * 0.6

                Text {
                    width: parent.width
                    visible: text !== ""
                    text: root.text
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: root.textSize
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1.1
                }
                Column {
                    id: extra
                    width: parent.width
                    spacing: root.textSize * 0.5
                }
            }
        }

        // button row
        Row {
            width: parent.width
            visible: !root.busy && root.buttons.length > 0
            Repeater {
                model: root.buttons
                delegate: Rectangle {
                    readonly property bool on: index === root.choice || buttonMouse.containsMouse
                    width: parent.width / root.buttons.length
                    height: root.textSize * 2.25
                    color: on ? "#4ae" : "#666"
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: root.textSize
                    }
                    MouseArea {
                        id: buttonMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            root.choice = index;
                            root.picked(index);
                        }
                    }
                }
            }
        }
    }
}
