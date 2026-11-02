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
//
// Pegasus X: suggestions. Give it a list of words (`suggestions`: genres you
// have, developers, game names...) and the ones matching what's being typed
// appear in a row above the keys, a few at a time: up from the top row goes
// to them, left/right picks, A puts it in. With `suggestionSep` ("," or
// "\n") the text is a list, and only the item the cursor is on is matched
// and replaced; with "word" only the word the cursor is in (`suggestions`
// then being words, the most used first); without it the whole text is.
FocusScope {
    id: root

    property Item target: null
    property bool multiLine: false   // an Enter key next to the space bar
    property bool allowClipboard: true
    property var suggestions: []     // words to offer (strings)
    property string suggestionSep: ""  // "" whole text, "," or "\n" a list, "word" the word typed
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
    property int row: 1              // -1: the suggestion row
    property int col: 0
    readonly property int clipRow: 5
    property int sugIndex: 0         // which suggestion, when row is -1

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

    // up from the top row: the suggestions when there are any, else out
    function goUp() {
        if (sugBar.items.length > 0) {
            row = -1;
            if (sugIndex >= sugBar.items.length)
                sugIndex = 0;
        }
        else root.leaveUp();
    }

    Keys.onUpPressed: {
        event.accepted = true;
        if (row === -1) { root.leaveUp(); return; }
        if (col === -1 || col === 10) {
            var s = sideSlot(row);
            if (row === clipRow) { row = 4; col = col === -1 ? 0 : 9; return; }
            if (s > 0) row = slotRow(s - 1);
            else goUp();
            return;
        }
        if (row === clipRow) { row = 4; col = col === 0 ? 0 : 9; return; }
        if (row > 0) row--;
        else goUp();
    }
    Keys.onDownPressed: {
        event.accepted = true;
        if (row === -1) { row = 0; if (col === -1 || col === 10) row = slotRow(0); return; }
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
        if (row === -1) { if (sugIndex > 0) sugIndex--; return; }
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
        if (row === -1) { if (sugIndex < sugBar.items.length - 1) sugIndex++; return; }
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
        if (api.keys.isAccept(event)) {
            event.accepted = true;
            if (row === -1) sugBar.apply(sugIndex);
            else press(keyAt(row, col));
            return;
        }
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
        color: "#222"
        border.color: "#555"
        border.width: vpx(1)
        MouseArea { anchors.fill: parent }  // taps between keys stay here
    }

    // -- Suggestions ---------------------------------------------------------------------

    // what's typed: the whole text, or the list item the cursor is on
    // letters (accented too), digits, and ' or - inside a word
    readonly property var wordRx: new RegExp("[\\w\\u00C0-\\u024F'\\-]")
    function wordChar(ch) {
        return ch !== undefined && ch !== "" && wordRx.test(ch);
    }
    function termBounds() {
        var t = target ? target.text : "";
        var c = target ? target.cursorPosition : 0;
        if (suggestionSep === "")
            return { "start": 0, "end": t.length };
        if (suggestionSep === "word") {
            var ws = c, we = c;
            while (ws > 0 && wordChar(t.charAt(ws - 1))) ws--;
            while (we < t.length && wordChar(t.charAt(we))) we++;
            return { "start": ws, "end": we };
        }
        var start = t.lastIndexOf(suggestionSep, c - 1) + 1;
        var end = t.indexOf(suggestionSep, c);
        return { "start": start, "end": end < 0 ? t.length : end };
    }

    // a suggested word written the way it's being typed: "Mar" -> "Mario",
    // "MAR" -> "MARIO", "mar" keeps the word's own ("Mario", "march")
    function caseLike(word, typed) {
        if (typed.length > 1 && typed === typed.toUpperCase() && typed !== typed.toLowerCase())
            return word.toUpperCase();
        var first = typed.charAt(0);
        if (first !== first.toLowerCase())
            return word.charAt(0).toUpperCase() + word.substring(1);
        return word;
    }

    Connections {
        target: root.target
        function onTextChanged() { sugBar.update(); }
        function onCursorPositionChanged() { sugBar.update(); }
    }
    onSuggestionsChanged: sugBar.update()
    onTargetChanged: sugBar.update()

    FontMetrics {
        id: sugMetrics
        font.family: globalFonts.sans
        font.pixelSize: vpx(17)
    }

    Column {
        id: layout
        x: root.panelPad
        y: root.panelPad
        spacing: root.keyGap

        // the suggestions, a row of them above the keys (none: no row)
        Item {
            id: sugBar

            property var items: []
            property bool pending: false
            readonly property var lowered: {
                var out = [];
                var src = root.suggestions || [];
                for (var i = 0; i < src.length; i++)
                    out.push(String(src[i]).toLowerCase());
                return out;
            }

            visible: items.length > 0
            width: root.gridW + (root.sideW + root.keyGap) * 2
            height: vpx(42)

            // once per change, after the key is handled
            function update() {
                if (pending)
                    return;
                pending = true;
                Qt.callLater(refresh);
            }
            function refresh() {
                pending = false;
                var src = root.suggestions || [];
                if (!root.target || src.length === 0) {
                    if (items.length > 0) items = [];
                    if (root.row === -1) root.row = 0;
                    return;
                }
                var t = root.target.text;
                var b = root.termBounds();
                var raw = t.substring(b.start, b.end).trim();
                var typed = raw.toLowerCase();
                var wordMode = root.suggestionSep === "word";
                // words: something typed first, and only words that start with it
                if (wordMode && typed.length === 0) {
                    if (items.length > 0) items = [];
                    if (root.row === -1) root.row = 0;
                    return;
                }
                // the other items of a list: already there
                var have = {};
                if (root.suggestionSep !== "" && !wordMode) {
                    var lines = (t.substring(0, b.start) + t.substring(b.end)).split(root.suggestionSep);
                    for (var l = 0; l < lines.length; l++)
                        have[lines[l].trim().toLowerCase()] = true;
                }
                var starts = [], inside = [];
                var max = 8;
                for (var i = 0; i < lowered.length && starts.length < max; i++) {
                    var g = lowered[i];
                    if (have[g] || g === typed)
                        continue;
                    if (typed === "" || g.indexOf(typed) === 0)
                        starts.push(wordMode ? root.caseLike(src[i], raw) : src[i]);
                    else if (!wordMode && inside.length < max && typed.length >= 2 && g.indexOf(typed) > 0)
                        inside.push(src[i]);
                }
                var all = starts.concat(inside);
                var next = [];
                var used = 0;
                for (var w = 0; w < all.length; w++) {
                    var chip = Math.min(sugMetrics.advanceWidth(all[w]) + vpx(24), vpx(260)) + vpx(8);
                    if (used + chip > width)
                        break;
                    used += chip;
                    next.push(all[w]);
                }
                if (JSON.stringify(next) !== JSON.stringify(items))
                    items = next;
                if (root.sugIndex >= items.length)
                    root.sugIndex = Math.max(0, items.length - 1);
                if (items.length === 0 && root.row === -1)
                    root.row = 0;
            }
            // the term becomes the suggestion; a list gets room for the next item
            function apply(index) {
                var name = items[index];
                if (name === undefined || !root.target)
                    return;
                var t = root.target.text;
                var b = root.termBounds();
                var sep = root.suggestionSep;
                if (sep === "word") {
                    // the word, then a space (unless one follows already)
                    var tail = t.substring(b.end);
                    var gap = tail.charAt(0) === " " ? "" : " ";
                    root.target.text = t.substring(0, b.start) + name + gap + tail;
                    root.target.cursorPosition = b.start + name.length + 1;
                    root.sugIndex = 0;
                    root.row = 0;
                    return;
                }
                var lead = (sep === "," && b.start > 0) ? " " : "";
                var before = t.substring(0, b.start) + lead + name;
                var after = sep === "" ? "" : (sep === "," ? ", " : "\n");
                if (b.end >= t.length) {
                    root.target.text = before + after;
                    root.target.cursorPosition = before.length + after.length;
                }
                else {
                    root.target.text = before + t.substring(b.end);
                    root.target.cursorPosition = before.length + (sep === "" ? 0 : 1);
                }
                root.sugIndex = 0;
                root.row = 0;
            }

            Row {
                anchors.centerIn: parent
                spacing: vpx(8)
                Repeater {
                    model: sugBar.items
                    delegate: Rectangle {
                        readonly property bool highlighted: root.activeFocus && root.row === -1 && index === root.sugIndex
                        width: Math.min(sugText.implicitWidth + vpx(24), vpx(260))
                        height: sugBar.height
                        color: highlighted ? root.keyFillSel : root.keyFill
                        border.color: root.keyFillSel
                        border.width: highlighted ? 0 : vpx(1)
                        Text {
                            id: sugText
                            width: Math.min(implicitWidth, vpx(236))
                            text: modelData
                            elide: Text.ElideRight
                            color: parent.highlighted ? root.textSel : "#9dd"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(17)
                            anchors.centerIn: parent
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: sugBar.apply(index)
                        }
                    }
                }
            }
        }

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
                        color: sel ? root.keyFillSel : ((modelData.k === "CMD_SHIFT" && root.shifted) ? "#555" : root.keyFillCmd)
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
                        color: sel ? root.keyFillSel : (modelData.k === "CMD_DONE" ? "#555" : root.keyFillCmd)
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
