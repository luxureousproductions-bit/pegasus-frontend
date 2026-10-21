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


// Pegasus X: Pegasus' own on-screen keyboard, laid out like XboxOSv2's (and
// the Xbox one): a letter grid between two columns of command keys, with the
// controller buttons drawn on them. Works with the controller and touch, and
// types into `target` (a TextInput or TextEdit) at its cursor, so Android's
// keyboard never has to open.
//
//     [ #+=  LT ] [ 1 2 3 4 5 6 7 8 9 0 ] [ ⌫     X ]
//     [ ←    LB ] [ q w e r t y u i o p ] [ →    RB ]
//     [         ] [ a s d f g h j k l ' ] [          ]
//     [ ⇧    L3 ] [ z x c v b n m , . ? ] [ Done  ≡ ]
//     [         ] [ space            Y  ] [          ]
//                 [ Copy ]     [ Paste ]
//
// A types · X deletes · Y space · LB/RB move the cursor · LT/RT change page
// · L3 capitals · Start done. B is left to the host (it closes the dialog).
FocusScope {
    id: root

    property Item target: null
    property bool multiLine: false   // an Enter key next to the space bar
    property bool allowClipboard: true
    signal done
    signal leaveUp                   // the d-pad went up from the top row
    signal leaveDown                 // ... down from the bottom row

    // sizes
    property real keyW: vpx(62)
    property real keyH: vpx(46)
    property real keyGap: vpx(6)
    readonly property real sideW: keyW * 1.6
    readonly property real gridW: keyW * 10 + keyGap * 9
    readonly property real panelPad: vpx(14)
    implicitWidth: gridW + (sideW + keyGap) * 2 + panelPad * 2
    implicitHeight: layout.height + panelPad * 2
    width: implicitWidth
    height: implicitHeight

    // Pegasus' settings colours
    readonly property color keyFill: "#333"
    readonly property color keyFillCmd: "#3c3c3c"
    readonly property color keyFillSel: "#3aa"
    readonly property color textSel: "#111"

    // -- Pages ------------------------------------------------------------------------

    property int page: 0         // 0 letters, 1 symbols, 2 accents
    property bool shifted: false

    readonly property var letterRows: [
        ["1","2","3","4","5","6","7","8","9","0"],
        ["q","w","e","r","t","y","u","i","o","p"],
        ["a","s","d","f","g","h","j","k","l","'"],
        ["z","x","c","v","b","n","m",",",".","?"],
    ]
    readonly property var symbolRows: [
        ["1","2","3","4","5","6","7","8","9","0"],
        ["!","@","#","$","%","^","&","*","(",")"],
        ["-","_","=","+","[","]","{","}","\\","|"],
        [";",":","\"","/","<",">","~","`","€","£"],
    ]
    readonly property var accentRows: [
        ["à","á","â","ã","ä","å","æ","ç","è","é"],
        ["ê","ë","ì","í","î","ï","ñ","ò","ó","ô"],
        ["õ","ö","ø","ù","ú","û","ü","ý","ÿ","ß"],
        ["œ","¢","¥","™","©","®","°","¡","¿","·"],
    ]
    readonly property var rows: page === 1 ? symbolRows : (page === 2 ? accentRows : letterRows)
    readonly property string pageLabel: page === 0 ? "#+=" : (page === 1 ? "áé" : "ABC")

    // -- Where the highlight is ---------------------------------------------------------
    // col -1 = left column, 0..9 = the grid, 10 = right column; row 0..3 the
    // letters, 4 the space bar, 5 the clipboard keys.
    property int row: 1
    property int col: 0
    readonly property int clipRow: 5

    // the side columns hold three keys each; the lower two span two rows
    function sideSlot(r) { return r <= 0 ? 0 : (r <= 2 ? 1 : 2); }
    function slotRow(s) { return s === 0 ? 0 : (s === 1 ? 1 : 3); }

    function keyAt(r, c) {
        if (c === -1) {
            var sl = sideSlot(r);
            return sl === 0 ? "CMD_PAGE" : (sl === 1 ? "CMD_LEFT" : "CMD_SHIFT");
        }
        if (c === 10) {
            var sr = sideSlot(r);
            return sr === 0 ? "CMD_DEL" : (sr === 1 ? "CMD_RIGHT" : "CMD_DONE");
        }
        if (r === clipRow)
            return c === 0 ? "CMD_COPY" : "CMD_PASTE";
        if (r === 4)
            return (multiLine && c >= 8) ? "CMD_NEWLINE" : "CMD_SPACE";
        return rows[r][c];
    }
    function selCentre(r, c) { return activeFocus && row === r && col === c; }
    function selSpace() { return activeFocus && row === 4 && col >= 0 && col <= 9 && !(multiLine && col >= 8); }
    function selNewline() { return activeFocus && row === 4 && multiLine && col >= 8 && col <= 9; }
    function selSide(left, slot) { return activeFocus && col === (left ? -1 : 10) && row !== clipRow && sideSlot(row) === slot; }
    function selClip(c) { return activeFocus && row === clipRow && col === c; }

    // -- Typing into the target ---------------------------------------------------------

    function insert(text) {
        if (!target)
            return;
        var pos = target.cursorPosition;
        target.insert(pos, text);
        target.cursorPosition = pos + text.length;
    }
    function backspace() {
        if (!target)
            return;
        var pos = target.cursorPosition;
        if (pos > 0) {
            target.remove(pos - 1, pos);
            target.cursorPosition = pos - 1;
        }
    }
    function caretLeft() { if (target) target.cursorPosition = Math.max(0, target.cursorPosition - 1); }
    function caretRight() { if (target) target.cursorPosition = Math.min(target.length, target.cursorPosition + 1); }

    // the system clipboard, through a hidden text field
    TextInput {
        id: clipProxy
        width: 0
        height: 0
        opacity: 0
        activeFocusOnPress: false
    }
    function doCopy() {
        if (!target)
            return;
        clipProxy.text = target.text;
        clipProxy.selectAll();
        clipProxy.copy();
        clipProxy.deselect();
    }
    function doPaste() {
        clipProxy.text = "";
        clipProxy.paste();
        if (clipProxy.text.length > 0)
            insert(clipProxy.text);
    }

    function press(k) {
        switch (k) {
            case "CMD_PAGE": page = (page + 1) % 3; return;
            case "CMD_LEFT": caretLeft(); return;
            case "CMD_RIGHT": caretRight(); return;
            case "CMD_SHIFT": shifted = !shifted; return;
            case "CMD_DEL": backspace(); return;
            case "CMD_DONE": root.done(); return;
            case "CMD_SPACE": insert(" "); return;
            case "CMD_NEWLINE": insert("\n"); return;
            case "CMD_COPY": doCopy(); return;
            case "CMD_PASTE": doPaste(); return;
        }
        insert(shifted ? k.toUpperCase() : k);
        if (shifted)
            shifted = false;  // one capital, like a phone keyboard
    }

    // -- The controller -------------------------------------------------------------------

    Keys.onUpPressed: {
        event.accepted = true;
        if (col === -1 || col === 10) {
            var s = sideSlot(row);
            if (row === clipRow) { row = 4; col = col === -1 ? 0 : 9; return; }
            if (s > 0) row = slotRow(s - 1);
            else root.leaveUp();
            return;
        }
        if (row === clipRow) { row = 4; col = col === 0 ? 0 : 9; return; }
        if (row > 0) row--;
        else root.leaveUp();
    }
    Keys.onDownPressed: {
        event.accepted = true;
        if (col === -1 || col === 10) {
            var s = sideSlot(row);
            if (s < 2) row = slotRow(s + 1);
            else if (allowClipboard) { row = clipRow; col = col === 10 ? 1 : 0; }
            else root.leaveDown();
            return;
        }
        if (row === clipRow) { root.leaveDown(); return; }
        if (row < 4) { row++; return; }
        if (allowClipboard) { row = clipRow; col = col >= 5 ? 1 : 0; }
        else root.leaveDown();
    }
    Keys.onLeftPressed: {
        event.accepted = true;
        if (col === -1) return;
        if (row === clipRow) { if (col > 0) col--; return; }
        if (col === 10) { col = row === 4 ? (multiLine ? 8 : 9) : 9; return; }
        if (row === 4) {
            if (multiLine && col >= 8) { col = 7; return; }
            col = -1; row = slotRow(sideSlot(row)); return;
        }
        if (col === 0) { col = -1; row = slotRow(sideSlot(row)); return; }
        col--;
    }
    Keys.onRightPressed: {
        event.accepted = true;
        if (col === 10) return;
        if (row === clipRow) { if (col < 1) col++; return; }
        if (col === -1) { col = 0; return; }
        if (row === 4) {
            if (multiLine && col < 8) { col = 8; return; }
            col = 10; row = slotRow(sideSlot(row)); return;
        }
        if (col === 9) { col = 10; row = slotRow(sideSlot(row)); return; }
        col++;
    }

    Keys.onPressed: {
        // a real keyboard types as usual (desktop, or one plugged in)
        if (event.key === Qt.Key_Backspace) {
            event.accepted = true;
            backspace();
            return;
        }
        if (event.text && event.text.length === 1 && event.text.charCodeAt(0) >= 0x20
            && !(event.modifiers & Qt.ControlModifier) && !(event.modifiers & Qt.AltModifier)) {
            event.accepted = true;
            insert(event.text);
            return;
        }
        if (event.isAutoRepeat)
            return;
        if (startKeys.indexOf(event.key) !== -1) { event.accepted = true; root.done(); return; }
        if (shiftKeys.indexOf(event.key) !== -1) { event.accepted = true; shifted = !shifted; return; }
        if (api.keys.isAccept(event)) { event.accepted = true; press(keyAt(row, col)); return; }
        if (api.keys.isDetails(event)) { event.accepted = true; backspace(); return; }
        if (api.keys.isFilters(event)) { event.accepted = true; insert(" "); return; }
        if (api.keys.isPrevPage(event)) { event.accepted = true; caretLeft(); return; }
        if (api.keys.isNextPage(event)) { event.accepted = true; caretRight(); return; }
        if (api.keys.isPageUp(event) || api.keys.isPageDown(event)) { event.accepted = true; press("CMD_PAGE"); return; }
    }
    // Pegasus has no names for these buttons: their raw codes (Start, left
    // stick click), plus the keyboard keys that mean the same
    readonly property var startKeys: [ 1048587, Qt.Key_Return, Qt.Key_Enter ]
    readonly property var shiftKeys: [ 1048582, Qt.Key_Shift ]

    // -- Drawing ----------------------------------------------------------------------------

    // a controller badge on a command key
    function badgeText(k) {
        switch (k) {
            case "CMD_PAGE": return "LT";
            case "CMD_LEFT": return "LB";
            case "CMD_SHIFT": return "L3";
            case "CMD_DEL": return "X";
            case "CMD_RIGHT": return "RB";
            case "CMD_DONE": return "≡";
            case "CMD_SPACE": return "Y";
        }
        return "";
    }

    Rectangle {
        anchors.fill: parent
        radius: vpx(8)
        color: "#222"
        border.color: "#555"
        border.width: vpx(1)
        MouseArea { anchors.fill: parent }  // taps between keys stay here
    }

    Column {
        id: layout
        x: root.panelPad
        y: root.panelPad
        spacing: root.keyGap

        Row {
            spacing: root.keyGap

            // left column
            Column {
                spacing: root.keyGap
                Repeater {
                    model: [ { "k": "CMD_PAGE", "slot": 0, "h": 1 }, { "k": "CMD_LEFT", "slot": 1, "h": 2 }, { "k": "CMD_SHIFT", "slot": 2, "h": 2 } ]
                    Rectangle {
                        readonly property bool sel: root.selSide(true, modelData.slot)
                        width: root.sideW
                        height: root.keyH * modelData.h + root.keyGap * (modelData.h - 1)
                        radius: vpx(6)
                        color: sel ? root.keyFillSel : ((modelData.k === "CMD_SHIFT" && root.shifted) ? "#2a5f5f" : root.keyFillCmd)
                        Column {
                            anchors.centerIn: parent
                            spacing: vpx(4)
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.k === "CMD_PAGE" ? root.pageLabel : modelData.k === "CMD_LEFT" ? "‹" : "⇧"
                                color: parent.parent.sel ? root.textSel : "#eee"
                                font.family: globalFonts.sans
                                font.pixelSize: modelData.k === "CMD_LEFT" ? vpx(34) : vpx(20)
                                font.bold: modelData.k === "CMD_PAGE"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: root.badgeText(modelData.k)
                                color: parent.parent.sel ? root.textSel : "#9a9a9a"
                                font.family: globalFonts.sans
                                font.pixelSize: vpx(13)
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: { root.col = -1; root.row = root.slotRow(modelData.slot); root.press(modelData.k); }
                        }
                    }
                }
            }

            // the letters and the space bar
            Column {
                spacing: root.keyGap
                Repeater {
                    model: 4
                    Row {
                        readonly property int rowIndex: index
                        spacing: root.keyGap
                        Repeater {
                            model: root.rows[parent.rowIndex]
                            Rectangle {
                                readonly property bool sel: root.selCentre(parent.rowIndex, index)
                                width: root.keyW
                                height: root.keyH
                                radius: vpx(4)
                                color: sel ? root.keyFillSel : root.keyFill
                                Text {
                                    anchors.centerIn: parent
                                    text: root.shifted ? modelData.toUpperCase() : modelData
                                    color: parent.sel ? root.textSel : "#eee"
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
                    Rectangle {
                        readonly property bool sel: root.selSpace()
                        width: root.multiLine ? root.keyW * 8 + root.keyGap * 7 : root.gridW
                        height: root.keyH
                        radius: vpx(4)
                        color: sel ? root.keyFillSel : root.keyFillCmd
                        Row {
                            anchors.centerIn: parent
                            spacing: vpx(10)
                            Text {
                                text: qsTr("space") + api.tr
                                color: parent.parent.sel ? root.textSel : "#eee"
                                font.family: globalFonts.sans
                                font.pixelSize: vpx(18)
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Y"
                                color: parent.parent.sel ? root.textSel : "#9a9a9a"
                                font.family: globalFonts.sans
                                font.pixelSize: vpx(13)
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: { root.row = 4; root.col = 0; root.press("CMD_SPACE"); }
                        }
                    }
                    Rectangle {
                        readonly property bool sel: root.selNewline()
                        visible: root.multiLine
                        width: root.keyW * 2 + root.keyGap
                        height: root.keyH
                        radius: vpx(4)
                        color: sel ? root.keyFillSel : root.keyFillCmd
                        Text {
                            anchors.centerIn: parent
                            text: "↵ " + qsTr("new line") + api.tr
                            color: parent.sel ? root.textSel : "#eee"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(16)
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: { root.row = 4; root.col = 8; root.press("CMD_NEWLINE"); }
                        }
                    }
                }
            }

            // right column
            Column {
                spacing: root.keyGap
                Repeater {
                    model: [ { "k": "CMD_DEL", "slot": 0, "h": 1 }, { "k": "CMD_RIGHT", "slot": 1, "h": 2 }, { "k": "CMD_DONE", "slot": 2, "h": 2 } ]
                    Rectangle {
                        readonly property bool sel: root.selSide(false, modelData.slot)
                        width: root.sideW
                        height: root.keyH * modelData.h + root.keyGap * (modelData.h - 1)
                        radius: vpx(6)
                        color: sel ? root.keyFillSel : (modelData.k === "CMD_DONE" ? "#2a5f5f" : root.keyFillCmd)
                        Column {
                            anchors.centerIn: parent
                            spacing: vpx(4)
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.k === "CMD_DEL" ? "← " + qsTr("del") + api.tr
                                    : modelData.k === "CMD_RIGHT" ? "›" : qsTr("Done") + api.tr
                                color: parent.parent.sel ? root.textSel : "#eee"
                                font.family: globalFonts.sans
                                font.pixelSize: modelData.k === "CMD_RIGHT" ? vpx(34) : vpx(18)
                                font.bold: modelData.k === "CMD_DONE"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: root.badgeText(modelData.k)
                                color: parent.parent.sel ? root.textSel : "#9a9a9a"
                                font.family: globalFonts.sans
                                font.pixelSize: vpx(13)
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: { root.col = 10; root.row = root.slotRow(modelData.slot); root.press(modelData.k); }
                        }
                    }
                }
            }
        }

        // copy / paste
        Row {
            visible: root.allowClipboard
            x: root.sideW + root.keyGap
            spacing: root.keyGap
            Repeater {
                model: [ { "k": "CMD_COPY", "t": qsTr("Copy all") + api.tr }, { "k": "CMD_PASTE", "t": qsTr("Paste") + api.tr } ]
                Rectangle {
                    readonly property bool sel: root.selClip(index)
                    width: (root.gridW - root.keyGap) / 2
                    height: root.keyH * 0.85
                    radius: vpx(4)
                    color: sel ? root.keyFillSel : root.keyFillCmd
                    Text {
                        anchors.centerIn: parent
                        text: modelData.t
                        color: parent.sel ? root.textSel : "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(16)
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { root.row = root.clipRow; root.col = index; root.press(modelData.k); }
                    }
                }
            }
        }
    }
}
