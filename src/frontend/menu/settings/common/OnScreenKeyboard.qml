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


// Pegasus X: Pegasus' own on-screen keyboard, for the controller and touch.
// It types into `target` (a TextInput or TextEdit) at its cursor.
//   d-pad move · A type · X delete · Y space · LB/RB move the cursor
// `done` comes from its Done key; the host decides what B does.
FocusScope {
    id: root

    property Item target: null
    property bool multiLine: false   // an Enter key for new lines
    signal done
    signal leaveUp                   // the d-pad went up from the top row
    signal leaveDown                 // ... down from the bottom row

    property int page: 0             // 0 letters, 1 symbols
    property bool shifted: false
    property int row: 1
    property int col: 0

    readonly property var letterRows: [
        ["1","2","3","4","5","6","7","8","9","0"],
        ["q","w","e","r","t","y","u","i","o","p"],
        ["a","s","d","f","g","h","j","k","l","'"],
        ["z","x","c","v","b","n","m",",",".","-"],
    ]
    readonly property var symbolRows: [
        ["1","2","3","4","5","6","7","8","9","0"],
        ["!","@","#","$","%","&","*","(",")","/"],
        ["-","_","=","+",":",";","\"","?","[","]"],
        ["{","}","<",">","~","|","\\","^","€","£"],
    ]
    // the bottom row: wide keys
    readonly property var bottomKeys: multiLine
        ? [ { "k": "page", "w": 1 }, { "k": "shift", "w": 1 }, { "k": "space", "w": 3 }, { "k": "enter", "w": 1 },
            { "k": "left", "w": 1 }, { "k": "right", "w": 1 }, { "k": "back", "w": 1 }, { "k": "done", "w": 1 } ]
        : [ { "k": "page", "w": 1 }, { "k": "shift", "w": 1 }, { "k": "space", "w": 4 },
            { "k": "left", "w": 1 }, { "k": "right", "w": 1 }, { "k": "back", "w": 1 }, { "k": "done", "w": 1 } ]
    readonly property var rows: page === 1 ? symbolRows : letterRows

    readonly property real keyGap: vpx(6)
    readonly property real keyW: (width - keyGap * 9) / 10
    readonly property real keyH: vpx(46)
    height: keyH * 5 + keyGap * 4

    function label(k) {
        switch (k) {
            case "page": return page === 1 ? "abc" : "#+=";
            case "shift": return shifted ? "⇧ ON" : "⇧";
            case "space": return qsTr("space") + api.tr;
            case "enter": return "↵";
            case "left": return "←";
            case "right": return "→";
            case "back": return "⌫";
            case "done": return qsTr("Done") + api.tr;
        }
        return shifted ? k.toUpperCase() : k;
    }

    // -- Typing into the target -----------------------------------------------------

    function insert(text) {
        if (!target)
            return;
        var pos = target.cursorPosition;
        target.insert(pos, text);
        target.cursorPosition = pos + text.length;
    }
    function press(k) {
        if (!target && k !== "done")
            return;
        switch (k) {
            case "page": page = page === 1 ? 0 : 1; return;
            case "shift": shifted = !shifted; return;
            case "space": insert(" "); return;
            case "enter": insert("\n"); return;
            case "left": target.cursorPosition = Math.max(0, target.cursorPosition - 1); return;
            case "right": target.cursorPosition = Math.min(target.length, target.cursorPosition + 1); return;
            case "back":
                var pos = target.cursorPosition;
                if (pos > 0) {
                    target.remove(pos - 1, pos);
                    target.cursorPosition = pos - 1;
                }
                return;
            case "done": root.done(); return;
        }
        insert(shifted ? k.toUpperCase() : k);
        if (shifted && page === 0)
            shifted = false;  // one capital, like a phone keyboard
    }

    // -- The controller ---------------------------------------------------------------

    // the bottom key under a column, and back
    function bottomIndexForCol(c) {
        var x = 0;
        var want = c + 0.5;
        for (var i = 0; i < bottomKeys.length; i++) {
            x += bottomKeys[i].w;
            if (want <= x)
                return i;
        }
        return bottomKeys.length - 1;
    }
    function colForBottomIndex(i) {
        var x = 0;
        for (var j = 0; j < i; j++)
            x += bottomKeys[j].w;
        return Math.min(9, Math.floor(x + bottomKeys[i].w / 2));
    }

    Keys.onPressed: {
        if (api.keys.isAccept(event)) {
            event.accepted = true;
            press(row < 4 ? rows[row][col] : bottomKeys[col].k);
            return;
        }
        if (event.isAutoRepeat && !(event.key === Qt.Key_Left || event.key === Qt.Key_Right
                                    || event.key === Qt.Key_Up || event.key === Qt.Key_Down))
            return;
        if (api.keys.isDetails(event)) { event.accepted = true; press("back"); return; }
        if (api.keys.isFilters(event)) { event.accepted = true; press("space"); return; }
        if (api.keys.isPrevPage(event)) { event.accepted = true; press("left"); return; }
        if (api.keys.isNextPage(event)) { event.accepted = true; press("right"); return; }

        if (event.key === Qt.Key_Left) {
            event.accepted = true;
            if (row < 4) col = (col + 9) % 10;
            else col = (col + bottomKeys.length - 1) % bottomKeys.length;
        }
        else if (event.key === Qt.Key_Right) {
            event.accepted = true;
            if (row < 4) col = (col + 1) % 10;
            else col = (col + 1) % bottomKeys.length;
        }
        else if (event.key === Qt.Key_Up) {
            event.accepted = true;
            if (row === 0) { root.leaveUp(); return; }
            if (row === 4) { col = colForBottomIndex(col); row = 3; return; }
            row--;
        }
        else if (event.key === Qt.Key_Down) {
            event.accepted = true;
            if (row === 4) { root.leaveDown(); return; }
            if (row === 3) { col = bottomIndexForCol(col); row = 4; return; }
            row++;
        }
    }

    // -- Keys ---------------------------------------------------------------------------

    Column {
        anchors.fill: parent
        spacing: root.keyGap

        Repeater {
            model: 4
            Row {
                readonly property int rowIndex: index
                spacing: root.keyGap
                Repeater {
                    model: root.rows[parent.rowIndex]
                    Rectangle {
                        readonly property bool highlighted: root.activeFocus && root.row === parent.rowIndex && root.col === index
                        width: root.keyW
                        height: root.keyH
                        radius: vpx(4)
                        color: highlighted ? "#3aa" : "#333"
                        Text {
                            anchors.centerIn: parent
                            text: root.label(modelData)
                            color: parent.highlighted ? "#111" : "#eee"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(20)
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                root.row = parent.parent.rowIndex;
                                root.col = index;
                                root.press(modelData);
                            }
                        }
                    }
                }
            }
        }
        Row {
            spacing: root.keyGap
            Repeater {
                model: root.bottomKeys
                Rectangle {
                    readonly property bool highlighted: root.activeFocus && root.row === 4 && root.col === index
                    width: root.keyW * modelData.w + root.keyGap * (modelData.w - 1)
                    height: root.keyH
                    radius: vpx(4)
                    color: highlighted ? "#3aa"
                         : modelData.k === "done" ? "#2a5f5f"
                         : (modelData.k === "shift" && root.shifted) ? "#2a5f5f" : "#3a3a3a"
                    Text {
                        anchors.centerIn: parent
                        text: root.label(modelData.k)
                        color: parent.highlighted ? "#111" : "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(18)
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.row = 4;
                            root.col = index;
                            root.press(modelData.k);
                        }
                    }
                }
            }
        }
    }
}
