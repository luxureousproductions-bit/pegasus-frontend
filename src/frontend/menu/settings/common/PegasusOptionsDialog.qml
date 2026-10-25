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


// Pegasus X: a pop-up of options in the style of Pegasus' own dialogs (dark
// title bar, square rows, a Close button at the bottom). Each row is a label
// and its value: ▴ ▾ pick a row, A or ◂ ▸ change it (activated), B closes.
FocusScope {
    id: root

    property string title: ""
    property string text: ""            // a line above the rows, if any
    property var rows: []               // [{ text, value, cycle }] (cycle: ◂ ▸ change it)
    property string closeLabel: qsTr("Close") + api.tr
    property real dialogWidth: Math.min(parent.width * 0.8, vpx(760))

    readonly property int textSize: vpx(18)
    readonly property int titleTextSize: vpx(20)
    // the rows, then the Close button (index rows.length)
    property int current: 0

    signal activated(int index, int step)   // step: 1 (A, ▸) or -1 (◂)
    signal closed()

    anchors.fill: parent
    enabled: focus
    visible: focus
    z: 5

    onFocusChanged: if (focus) current = 0

    Keys.onPressed: {
        if (event.isAutoRepeat && (api.keys.isAccept(event) || api.keys.isCancel(event)))
            return;
        if (api.keys.isAccept(event)) {
            event.accepted = true;
            if (current >= rows.length)
                root.closed();
            else
                root.activated(current, 1);
        }
        else if (api.keys.isCancel(event)) {
            event.accepted = true;
            root.closed();
        }
    }
    Keys.onUpPressed: current = Math.max(0, current - 1)
    Keys.onDownPressed: current = Math.min(rows.length, current + 1)
    Keys.onLeftPressed: if (current < rows.length) root.activated(current, -1)
    Keys.onRightPressed: if (current < rows.length) root.activated(current, 1)

    Rectangle {
        anchors.fill: parent
        color: "#000"
        opacity: 0.5
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: root.closed()
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

        // the line above the rows
        Rectangle {
            width: parent.width
            height: intro.height + root.textSize * 1.4
            color: "#555"
            visible: root.text !== ""
            MouseArea { anchors.fill: parent }
            Text {
                id: intro
                anchors.centerIn: parent
                width: parent.width - root.textSize * 2
                text: root.text
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: root.textSize * 0.9
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        // the rows
        Repeater {
            model: root.rows
            delegate: Rectangle {
                readonly property bool on: index === root.current || rowMouse.containsMouse
                width: parent.width
                height: root.textSize * 2.4
                color: on ? "#4ae" : (index % 2 === 0 ? "#444" : "#4a4a4a")

                Text {
                    id: label
                    anchors.left: parent.left
                    anchors.leftMargin: root.textSize
                    anchors.right: value.left
                    anchors.rightMargin: root.textSize
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.text
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: root.textSize
                    elide: Text.ElideRight
                }
                Text {
                    id: value
                    anchors.right: parent.right
                    anchors.rightMargin: root.textSize
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, parent.width * 0.55)
                    text: modelData.value !== undefined && modelData.value !== ""
                        ? (parent.on && modelData.cycle ? "◂ " + modelData.value + " ▸" : modelData.value) : ""
                    color: parent.on ? "#fff" : "#bbb"
                    font.family: globalFonts.sans
                    font.pixelSize: root.textSize * 0.85
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideMiddle
                }
                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.current = index;
                        root.activated(index, 1);
                    }
                }
            }
        }

        // Close
        Rectangle {
            readonly property bool on: root.current >= root.rows.length || closeMouse.containsMouse
            width: parent.width
            height: root.textSize * 2.25
            color: on ? "#4ae" : "#666"
            Text {
                anchors.centerIn: parent
                text: root.closeLabel
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: root.textSize
            }
            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.closed()
            }
        }
    }
}
