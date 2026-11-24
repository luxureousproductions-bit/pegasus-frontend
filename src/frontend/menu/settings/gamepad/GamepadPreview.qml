// Pegasus Frontend
// Copyright (C) 2017-2021  Mátyás Mustoha
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

// Pegasus X: assets/gamepad/preview.png is a flat drawing of an Xbox Series
// style controller (offset sticks, d-pad lower left, A B X Y, View / Guide /
// Menu, bumpers and triggers), 512 x 340 as the old one; fieldPoints are
// where each button is on it (fractions of the width and height).
Image {
    property string currentField: ""

    readonly property var fieldPoints: {
        'x': [0.6602, 0.3529],
        'y': [0.7266, 0.2529],
        'a': [0.7266, 0.4529],
        'b': [0.7930, 0.3529],
        'start': [0.5664, 0.3559],
        'guide': [0.5000, 0.2353],
        'select': [0.4336, 0.3559],
        'lsb': [0.2734, 0.3529],
        'lsx': [0.2734, 0.3529],
        'lsy': [0.2734, 0.3529],
        'rsb': [0.6289, 0.6059],
        'rsx': [0.6289, 0.6059],
        'rsy': [0.6289, 0.6059],
        'lt': [0.2734, 0.0588],
        'lb': [0.2266, 0.1235],
        'rt': [0.7266, 0.0588],
        'rb': [0.7734, 0.1235],
        'dpup': [0.3809, 0.5324],
        'dpdown': [0.3809, 0.6794],
        'dpleft': [0.3320, 0.6059],
        'dpright': [0.4297, 0.6059],
    }
    readonly property var verticalAxes: ['lsy', 'rsy']
    readonly property var horizontalAxes: ['lsx', 'rsx']

    readonly property double currentPointX: currentField ? fieldPoints[currentField][0] * width : 0
    readonly property double currentPointY: currentField ? fieldPoints[currentField][1] * height : 0
    readonly property bool currentFieldIsAxis: currentField && (horizontalAxes.includes(currentField) || verticalAxes.includes(currentField))


    source: "qrc:/frontend/assets/gamepad/preview.png"
    fillMode: Image.PreserveAspectFit


    Rectangle {
        id: selectionCenter

        color: "#3cc"
        width: vpx(20)
        height: width
        radius: width * 0.5

        visible: currentField
        x: currentPointX - radius
        y: currentPointY - radius
    }

    Rectangle {
        id: selectionAxis

        color: selectionCenter.color
        width: selectionCenter.width * 2
        height: vpx(3)

        visible: currentFieldIsAxis
        x: currentPointX - width / 2
        y: currentPointY - height / 2

        transformOrigin: Item.Center
        rotation: verticalAxes.includes(currentField) ? 90 : 0
    }
}
