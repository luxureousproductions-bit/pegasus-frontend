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

// Pegasus X: assets/gamepad/preview.png is an Xbox controller, front view
// (839 x 561, transparent around it), used as it is: kept to its shape and
// scaled smoothly, never stretched. fieldPoints are where each button is on
// it (fractions of the picture's width and height; in pixels beside them).
// Bumpers and triggers can't be seen from the front: LB / RB sit in the
// middle of the white shoulder pieces at the top, LT / RT on the outer ends
// of the same pieces (where the triggers are, behind them).
// (preview.svg beside it is the drawing of builds 2271-2277, no longer used)
Image {
    property string currentField: ""

    readonly property var fieldPoints: {
        'x': [0.6847, 0.3039],       // 574.5, 170.5
        'y': [0.7539, 0.2041],       // 632.5, 114.5
        'a': [0.7539, 0.4046],       // 632.5, 227
        'b': [0.8230, 0.3030],       // 690.5, 170
        'start': [0.5721, 0.3039],   // 480, 170.5 (Menu)
        'guide': [0.5012, 0.1373],   // 420.5, 77 (the Xbox button)
        'select': [0.4291, 0.3039],  // 360, 170.5 (View)
        'lsb': [0.2491, 0.3030],     // 209, 170
        'lsx': [0.2491, 0.3030],
        'lsy': [0.2491, 0.3030],
        'rsb': [0.6281, 0.5294],     // 527, 297
        'rsx': [0.6281, 0.5294],
        'rsy': [0.6281, 0.5294],
        'lt': [0.1907, 0.0749],      // 160, 42: the left shoulder piece's outer end
        'lb': [0.2765, 0.0392],      // 232, 22: the left shoulder piece's middle
        'rt': [0.8093, 0.0749],      // 679, 42
        'rb': [0.7235, 0.0392],      // 607, 22
        'dpup': [0.3731, 0.4724],    // 313, 265
        'dpdown': [0.3731, 0.6114],  // 313, 343
        'dpleft': [0.3266, 0.5419],  // 274, 304
        'dpright': [0.4184, 0.5419], // 351, 304
    }
    readonly property var verticalAxes: ['lsy', 'rsy']
    readonly property var horizontalAxes: ['lsx', 'rsx']

    // (on the picture as drawn: it keeps its shape inside this item, which is
    // only given a width, so it can be narrower or lower than the item)
    readonly property double currentPointX: currentField ? (width - paintedWidth) / 2 + fieldPoints[currentField][0] * paintedWidth : 0
    readonly property double currentPointY: currentField ? (height - paintedHeight) / 2 + fieldPoints[currentField][1] * paintedHeight : 0
    readonly property bool currentFieldIsAxis: currentField && (horizontalAxes.includes(currentField) || verticalAxes.includes(currentField))


    source: "qrc:/frontend/assets/gamepad/preview.png"
    fillMode: Image.PreserveAspectFit
    // read in at the size it's shown (a smooth, sharp scale of the picture,
    // its shape kept), and drawn smoothly
    sourceSize.width: Math.max(1, Math.round(width))
    smooth: true


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
