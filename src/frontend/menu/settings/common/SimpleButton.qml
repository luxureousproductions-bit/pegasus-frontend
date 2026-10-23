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


import QtQuick 2.0


FocusScope {
    id: root

    property alias label: label.text
    // Pegasus X: a line or two under the name, like the on/off options have
    property alias desc: description.text

    readonly property int fontSize: vpx(22)
    readonly property int horizontalPadding: vpx(30)

    signal activate()


    width: parent.width
    height: description.text ? labelContainer.height + fontSize * 1.25 : fontSize * 2.5
    opacity: enabled ? 1.0 : 0.25

    Keys.onPressed: {
        if (api.keys.isAccept(event) && !event.isAutoRepeat) {
            event.accepted = true;
            root.activate();
        }
    }


    Rectangle {
        id: underline

        width: parent.width
        height: vpx(3)
        anchors.bottom: parent.bottom

        color: "#3aa"
        visible: parent.focus || mouseArea.containsMouse
    }

    Column {
        id: labelContainer
        anchors.left: parent.left
        anchors.leftMargin: horizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width * 0.85 - horizontalPadding
        spacing: fontSize * 0.25

        Text {
            id: label

            color: "#eee"
            font.pixelSize: fontSize
            font.family: globalFonts.sans
        }
        Text {
            id: description

            visible: text !== ""
            width: parent.width
            color: "#999"
            font.pixelSize: fontSize * 0.8
            font.family: globalFonts.sans
            wrapMode: Text.WordWrap
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: activate()
        cursorShape: Qt.PointingHandCursor
    }
}
