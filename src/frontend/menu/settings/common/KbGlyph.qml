// Pegasus X
// Copyright (C) 2026  Esteban (MisterE88)
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


// Pegasus X: a picture on an on-screen keyboard key (the XBSX-Experience
// theme's keyboard art in assets/keyboard, or its button art in
// assets/gamepad/x360), drawn at twice its size for a sharp edge. If it
// can't be loaded, `fallback` is shown as words instead (what the key said
// before; give the item its width and height, the words centred on it).
Item {
    id: root

    property string source: ""
    property string fallback: ""
    property real fallbackSize: vpx(13)
    property bool fallbackBold: false
    property color color: "#fff"

    readonly property bool loaded: icon.status === Image.Ready

    Image {
        id: icon
        visible: root.loaded
        anchors.fill: parent
        source: root.source
        sourceSize.width: Math.round(root.width * 2)
        sourceSize.height: Math.round(root.height * 2)
        fillMode: Image.PreserveAspectFit
        smooth: true
    }
    Text {
        id: label
        visible: !root.loaded
        anchors.centerIn: parent
        text: root.fallback
        color: root.color
        font.family: globalFonts.sans
        font.pixelSize: root.fallbackSize
        font.bold: root.fallbackBold
    }
}
