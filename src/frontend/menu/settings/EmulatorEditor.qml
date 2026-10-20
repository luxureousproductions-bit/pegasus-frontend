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
import "qrc:/qmlutils" as PegasusUtils
import QtQuick 2.6
import QtQuick.Window 2.2


// Pegasus X: the emulator of each collection, and of single games. Choices are
// written into the collection's metadata files as launch: lines.
FocusScope {
    id: root

    signal close

    anchors.fill: parent
    enabled: focus
    visible: 0 < (x + width) && x < Window.window.width

    property var collections: []
    property int collIndex: -1
    readonly property var coll: (collIndex >= 0 && collIndex < collections.length) ? collections[collIndex] : null
    property var games: []          // [{ title, path }] of the open collection
    property var gameCommands: ({}) // game path -> its own launch command
    property string message: ""
    property bool loading: true

    // the collections are read in the background (it takes a moment with a
    // big library), so the screen can show that it's loading
    Component.onCompleted: refresh()

    function refresh() {
        Internal.settings.requestEmulatorCollections();
    }
    Connections {
        target: Internal.settings
        function onEmulatorCollectionsReady(collections) {
            const first = root.loading;
            root.collections = collections;
            root.loading = false;
            if (first && collections.length > 0)
                list.currentIndex = 1;  // the first collection, below the emulator list row
            if (root.coll)
                root.loadGames();
        }
    }

    function loadGames() {
        var out = [];
        for (var i = 0; i < api.collections.count; i++) {
            var c = api.collections.get(i);
            if (c.name !== coll.name)
                continue;
            for (var j = 0; j < c.games.count; j++) {
                var game = c.games.get(j);
                if (game.files.count > 0)
                    out.push({ "title": game.title, "path": game.files.get(0).path, "obj": game });
            }
        }
        games = out;
        gameCommands = Internal.settings.gameEmulators(coll.name);
    }

    // the name of the emulator a command belongs to
    function sameCommand(a, b) {
        return a.replace(/\s+/g, " ").trim() === b.replace(/\s+/g, " ").trim();
    }
    function labelOf(c, command) {
        if (!command)
            return "";
        for (var i = 0; i < c.options.length; i++) {
            if (sameCommand(c.options[i].command, command))
                return c.options[i].label;
        }
        return qsTr("Custom command") + api.tr;
    }
    function collectionText(c) {
        if (!c.command)
            return qsTr("None yet") + api.tr;
        var text = c.currentLabel !== "" ? c.currentLabel : qsTr("Your metadata's command") + api.tr;
        if (c.currentInstalled)
            return text;
        return c.missingApp
            ? text + qsTr(" · %1 not installed").arg(c.missingApp) + api.tr
            : text + qsTr(" · not installed") + api.tr;
    }

    // the loaded games use a new command right away, no reload needed
    function applyLive(game, command) {
        if (game) {
            Internal.settings.setLiveLaunchCommand(game.obj, command !== "" ? command : (coll.command || ""));
            return;
        }
        for (var i = 0; i < games.length; i++) {
            if (!gameCommands[games[i].path])
                Internal.settings.setLiveLaunchCommand(games[i].obj, command);
        }
    }

    // the first row of the list is the emulator list itself
    property string listInfo: Internal.settings.emulatorListInfo()
    property bool listUpdating: false
    Connections {
        target: Internal.settings
        function onEmulatorListUpdated(ok, text) {
            root.listUpdating = false;
            root.listInfo = text;
            if (ok)
                root.refresh();
        }
    }
    function updateList() {
        if (listUpdating)
            return;
        listUpdating = true;
        listInfo = qsTr("Downloading the latest from ES-DE...") + api.tr;
        Internal.settings.updateEmulatorList();
    }

    function openCollection(index) {
        collIndex = index;
        message = "";
        loadGames();
        page.currentIndex = 1;
        page.focus = true;
    }
    function backToList() {
        collIndex = -1;
        message = "";
        list.focus = true;
    }

    function closeMaybe() {
        root.close();
    }

    Keys.onPressed: {
        if (api.keys.isCancel(event) && !event.isAutoRepeat) {
            event.accepted = true;
            if (root.coll)
                root.backToList();
            else
                root.closeMaybe();
        }
    }


    PegasusUtils.HorizontalSwipeArea {
        anchors.fill: parent
        onSwipeRight: root.coll ? root.backToList() : root.closeMaybe()
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: root.coll ? root.backToList() : root.closeMaybe()
    }

    ScreenHeader {
        id: header
        text: (root.coll ? qsTr("Emulators > %1").arg(root.coll.name) : qsTr("Settings > Emulators")) + api.tr
        z: 2
    }


    // -- Loading ------------------------------------------------------------------

    Column {
        visible: root.loading
        anchors.centerIn: parent
        spacing: vpx(16)

        Rectangle {
            id: spinner
            width: vpx(48)
            height: width
            radius: width / 2
            color: "transparent"
            border.color: "#3aa"
            border.width: vpx(4)
            anchors.horizontalCenter: parent.horizontalCenter

            Rectangle {
                width: vpx(12)
                height: width
                radius: width / 2
                color: "#eee"
                x: parent.width / 2 - width / 2
                y: -height / 2 + parent.border.width / 2
            }
            RotationAnimation on rotation {
                running: root.loading
                loops: Animation.Infinite
                from: 0
                to: 360
                duration: 1000
            }
        }
        Text {
            text: qsTr("Looking for your collections and emulators...") + api.tr
            color: "#ccc"
            font.family: globalFonts.sans
            font.pixelSize: vpx(20)
        }
    }


    // -- The collections ------------------------------------------------------

    ListView {
        id: list

        visible: !root.coll && !root.loading
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.7
        clip: true

        focus: true
        model: [{ "kind": "update" }].concat(root.collections)
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: height * 0.3
        preferredHighlightEnd: height * 0.7
        highlightMoveDuration: 150

        header: Text {
            width: list.width
            text: root.collections.length > 0 || root.loading
                ? qsTr("Pick the emulator of each collection, or of single games. "
                     + "It's written into the collection's metadata file.") + api.tr
                : qsTr("No collections found. Collections come from the metadata files "
                     + "in your game directories (Set game directories, ES-DE import).") + api.tr
            color: "#999"
            font.family: globalFonts.sans
            font.pixelSize: vpx(18)
            wrapMode: Text.WordWrap
            topPadding: vpx(26)
            bottomPadding: vpx(26)
            leftPadding: vpx(30)
            rightPadding: vpx(30)
        }

        Keys.onPressed: {
            if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                event.accepted = true;
                if (currentIndex === 0)
                    root.updateList();
                else
                    root.openCollection(currentIndex - 1);
            }
        }

        delegate: Item {
            readonly property bool highlighted: (ListView.isCurrentItem && ListView.view.activeFocus) || rowMouse.containsMouse

            width: ListView.view.width
            height: vpx(22) * 2.5

            Rectangle {
                width: parent.width
                height: vpx(3)
                anchors.bottom: parent.bottom
                color: "#3aa"
                visible: parent.highlighted
            }
            Text {
                id: collName
                text: modelData.kind === "update" ? qsTr("Update emulator list") + api.tr : modelData.name
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(22)
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width * 0.45
                elide: Text.ElideRight
            }
            Text {
                text: modelData.kind === "update"
                    ? root.listInfo
                    : modelData.system === "" ? qsTr("Unknown system") + api.tr : root.collectionText(modelData)
                color: modelData.kind === "update" ? "#5cc"
                     : (modelData.command && !modelData.currentInstalled) || modelData.system === "" ? "#e8b06a" : "#c0c0c0"
                font.family: globalFonts.sans
                font.pixelSize: vpx(22)
                anchors.left: collName.right
                anchors.leftMargin: vpx(16)
                anchors.right: parent.right
                anchors.rightMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    list.currentIndex = index;
                    if (index === 0)
                        root.updateList();
                    else
                        root.openCollection(index - 1);
                }
            }
        }
    }


    // -- One collection -----------------------------------------------------------

    ListView {
        id: page

        visible: !!root.coll
        enabled: visible
        anchors.top: header.bottom
        anchors.bottom: messageBar.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.7
        clip: true

        readonly property var rows: {
            var c = root.coll;
            if (!c)
                return [];
            var out = [];
            out.push({ "kind": "section", "text": qsTr("Collection") + api.tr });
            out.push({ "kind": "emulator", "text": qsTr("Emulator") + api.tr,
                       "value": c.system === "" ? qsTr("Unknown system, use a custom command") + api.tr : root.collectionText(c) });
            out.push({ "kind": "test", "text": qsTr("Test with a game") + api.tr,
                       "value": root.games.length > 0 ? root.games[0].title : qsTr("no games") + api.tr });
            var shown = [];
            for (var f = 0; f < c.files.length; f++)
                shown.push(c.files[f].split("/").slice(-2).join("/"));
            out.push({ "kind": "info", "text": qsTr("Saved in") + api.tr, "value": shown.join(", ") });

            var own = 0;
            for (var g = 0; g < root.games.length; g++) {
                if (root.gameCommands[root.games[g].path])
                    own++;
            }
            out.push({ "kind": "section", "text": qsTr("Per-game · %1 set").arg(own) + api.tr });
            for (var i = 0; i < root.games.length; i++) {
                var cmd = root.gameCommands[root.games[i].path];
                out.push({ "kind": "game", "text": root.games[i].title, "game": root.games[i],
                           "own": !!cmd, "value": cmd ? root.labelOf(c, cmd) : qsTr("Collection's emulator") + api.tr });
            }
            return out;
        }
        model: rows

        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: height * 0.3
        preferredHighlightEnd: height * 0.7
        highlightMoveDuration: 150

        // section titles and info rows can't be selected
        function selectable(i) {
            return i >= 0 && i < rows.length && rows[i].kind !== "section" && rows[i].kind !== "info";
        }
        Keys.onUpPressed: {
            var i = currentIndex - 1;
            while (i >= 0 && !selectable(i))
                i--;
            if (i >= 0)
                currentIndex = i;
        }
        Keys.onDownPressed: {
            var i = currentIndex + 1;
            while (i < count && !selectable(i))
                i++;
            if (i < count)
                currentIndex = i;
        }

        function activate(index) {
            if (!selectable(index))
                return;
            var row = rows[index];
            if (row.kind === "emulator") {
                picker.openFor(null);
            }
            else if (row.kind === "game") {
                picker.openFor(row.game);
            }
            else if (row.kind === "test") {
                if (root.games.length === 0 || !root.coll.command) {
                    root.message = qsTr("Pick an emulator first.") + api.tr;
                    return;
                }
                var error = Internal.settings.testEmulatorCommand(root.coll.command, root.games[0].path);
                root.message = error === "" ? qsTr("Started \"%1\".").arg(root.games[0].title) + api.tr : error;
            }
        }

        Keys.onPressed: {
            if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                event.accepted = true;
                activate(currentIndex);
            }
        }

        delegate: Item {
            readonly property bool isSection: modelData.kind === "section"
            readonly property bool isInfo: modelData.kind === "info"
            readonly property bool highlighted: !isSection && !isInfo
                && ((ListView.isCurrentItem && ListView.view.activeFocus) || pageMouse.containsMouse)

            width: ListView.view.width
            height: isSection ? sectionText.height : (isInfo ? vpx(44) : vpx(22) * 2.5)

            Text {
                id: sectionText
                visible: parent.isSection
                text: modelData.text
                color: "#bbb"
                font.family: globalFonts.sans
                font.pixelSize: vpx(22)
                topPadding: font.pixelSize * (index === 0 ? 0.75 : 1.75)
                bottomPadding: font.pixelSize * 0.25
            }

            Rectangle {
                width: parent.width
                height: vpx(3)
                anchors.bottom: parent.bottom
                color: "#3aa"
                visible: parent.highlighted
            }
            Text {
                id: rowLabel
                visible: !parent.isSection
                text: modelData.text
                color: parent.isInfo ? "#999" : "#eee"
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(22)
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width * 0.45
                elide: Text.ElideRight
            }
            Text {
                visible: !parent.isSection
                text: modelData.value || ""
                color: parent.isInfo ? "#999" : (modelData.own ? "#5cc" : "#c0c0c0")
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(22)
                anchors.left: rowLabel.right
                anchors.leftMargin: vpx(16)
                anchors.right: parent.right
                anchors.rightMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideMiddle
            }
            MouseArea {
                id: pageMouse
                anchors.fill: parent
                enabled: !parent.isSection && !parent.isInfo
                hoverEnabled: true
                onClicked: {
                    page.currentIndex = index;
                    page.focus = true;
                    page.activate(index);
                }
            }
        }
    }

    Text {
        id: messageBar
        text: root.message
        visible: !!root.coll
        height: visible && text !== "" ? implicitHeight : 0
        color: "#ddd"
        font.family: globalFonts.sans
        font.pixelSize: vpx(17)
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter

        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.7
        bottomPadding: text !== "" ? vpx(16) : 0
    }


    // -- Picking an emulator: Pegasus' white side panel ------------------------

    FocusScope {
        id: picker

        property var game: null  // null: the collection
        readonly property var entries: {
            var c = root.coll;
            if (!c)
                return [];
            var out = [];
            var current = game ? (root.gameCommands[game.path] || "") : (c.command || "");
            if (game)
                out.push({ "kind": "inherit", "text": qsTr("Collection's emulator") + api.tr, "current": current === "" });
            else if (c.command && c.currentLabel === "")
                out.push({ "kind": "keep", "text": qsTr("Your metadata's command") + api.tr, "current": true });

            var installed = [], missing = [];
            for (var i = 0; i < c.options.length; i++) {
                var o = c.options[i];
                var entry = { "kind": "option", "text": o.label, "command": o.command, "installed": o.installed,
                              "note": o.recommended ? qsTr("recommended") + api.tr
                                    : (o.retroarch && o.installed ? qsTr("needs the core") + api.tr : ""),
                              "current": current !== "" && root.sameCommand(o.command, current) };
                (o.installed ? installed : missing).push(entry);
            }
            if (installed.length > 0)
                out.push({ "kind": "header", "text": qsTr("Installed") + api.tr });
            out = out.concat(installed);
            if (missing.length > 0)
                out.push({ "kind": "header", "text": qsTr("Not installed") + api.tr });
            out = out.concat(missing);
            out.push({ "kind": "custom", "text": qsTr("Custom command...") + api.tr, "current": false });
            return out;
        }

        anchors.fill: parent
        enabled: focus
        visible: box.x < parent.width

        function openFor(g) {
            game = g;
            var idx = 0;
            for (var i = 0; i < entries.length; i++) {
                if (entries[i].current) {
                    idx = i;
                    break;
                }
                if (idx === 0 && entries[i].kind !== "header")
                    idx = i;
            }
            pickerList.currentIndex = idx;
            pickerList.positionViewAtIndex(idx, ListView.Center);
            focus = true;
        }
        function closePicker() {
            page.focus = true;
        }

        function apply(command) {
            var c = root.coll;
            var error = game
                ? Internal.settings.setGameEmulator(c.name, game.path, game.title, command)
                : Internal.settings.setCollectionEmulator(c.name, command);
            if (error !== "") {
                root.message = error;
            }
            else {
                root.applyLive(game, command);
                root.message = game
                    ? qsTr("Saved for \"%1\".").arg(game.title) + api.tr
                    : qsTr("Saved for %1.").arg(c.name) + api.tr;

                // show the change right away, without reading everything again
                if (game) {
                    var cmds = Object.assign({}, root.gameCommands);
                    if (command !== "")
                        cmds[game.path] = command;
                    else
                        delete cmds[game.path];
                    root.gameCommands = cmds;
                }
                else {
                    var list = root.collections.slice();
                    var updated = Object.assign({}, list[root.collIndex]);
                    updated.command = command;
                    updated.currentLabel = "";
                    updated.currentInstalled = true;
                    updated.missingApp = "";
                    for (var i = 0; i < updated.options.length; i++) {
                        if (root.sameCommand(updated.options[i].command, command)) {
                            updated.currentLabel = updated.options[i].label;
                            updated.currentInstalled = updated.options[i].installed;
                        }
                    }
                    var keepIndex = page.currentIndex;
                    list[root.collIndex] = updated;
                    root.collections = list;
                    page.currentIndex = keepIndex;
                }
            }
            closePicker();
        }

        function choose(index) {
            if (index < 0 || index >= entries.length)
                return;
            var entry = entries[index];
            if (entry.kind === "header" || entry.kind === "keep")
                return closePicker();
            if (entry.kind === "inherit")
                return apply("");
            if (entry.kind === "custom") {
                var start = game ? (root.gameCommands[game.path] || root.coll.command || "") : (root.coll.command || "");
                commandDialog.openFor(game, start);
                return;
            }
            apply(entry.command);
        }

        Keys.onPressed: {
            if (event.isAutoRepeat)
                return;
            if (api.keys.isCancel(event)) {
                event.accepted = true;
                closePicker();
            }
            else if (api.keys.isAccept(event)) {
                event.accepted = true;
                choose(pickerList.currentIndex);
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: picker.focus ? 0.3 : 0.0
            Behavior on opacity { PropertyAnimation { duration: 150 } }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: picker.closePicker()
            }
        }

        Rectangle {
            id: box

            width: Math.min(vpx(460), parent.width * 0.5)
            // below the screen's title, so it doesn't cover it
            y: header.height + vpx(12)
            height: parent.height - y - vpx(16)
            x: picker.focus ? parent.width - width - height * 0.04 : parent.width
            Behavior on x { PropertyAnimation { duration: 200; easing.type: Easing.OutCubic } }

            color: "#2b2b2b"
            radius: vpx(8)
            border.color: "#444"
            border.width: vpx(1)

            MouseArea {
                anchors.fill: parent
            }

            Item {
                anchors.fill: parent
                anchors.topMargin: parent.radius
                anchors.bottomMargin: parent.radius
                clip: true

                ListView {
                    id: pickerList

                    width: parent.width
                    height: Math.min(contentHeight, parent.height)

                    focus: true
                    model: picker.entries
                    highlightRangeMode: ListView.ApplyRange
                    preferredHighlightBegin: height * 0.5 - vpx(20)
                    preferredHighlightEnd: height * 0.5 + vpx(20)
                    highlightMoveDuration: 0

                    Keys.onUpPressed: {
                        var i = currentIndex - 1;
                        while (i >= 0 && picker.entries[i].kind === "header")
                            i--;
                        if (i >= 0)
                            currentIndex = i;
                    }
                    Keys.onDownPressed: {
                        var i = currentIndex + 1;
                        while (i < count && picker.entries[i].kind === "header")
                            i++;
                        if (i < count)
                            currentIndex = i;
                    }

                    delegate: Rectangle {
                        readonly property bool isHeader: modelData.kind === "header"
                        readonly property bool highlighted: !isHeader && ListView.isCurrentItem

                        width: ListView.view.width
                        height: isHeader ? vpx(36) : vpx(46)
                        color: highlighted ? "#3aa" : "transparent"

                        Text {
                            id: entryText
                            text: modelData.text
                            color: parent.highlighted ? "#111"
                                 : parent.isHeader ? "#999"
                                 : modelData.kind === "custom" ? "#5cc"
                                 : (modelData.kind === "option" && !modelData.installed) ? "#888" : "#eee"
                            font.family: globalFonts.sans
                            font.pixelSize: parent.isHeader ? vpx(14) : vpx(19)
                            font.bold: parent.isHeader || modelData.kind === "custom"
                            font.capitalization: parent.isHeader ? Font.AllUppercase : Font.MixedCase

                            anchors.left: parent.left
                            anchors.leftMargin: vpx(20)
                            anchors.right: noteText.left
                            anchors.rightMargin: vpx(8)
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                        }
                        Text {
                            id: noteText
                            text: modelData.current ? qsTr("current") + api.tr : (modelData.note || "")
                            color: parent.highlighted ? "#123" : "#aaa"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(14)
                            anchors.right: parent.right
                            anchors.rightMargin: vpx(20)
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        MouseArea {
                            anchors.fill: parent
                            enabled: !parent.isHeader
                            onClicked: picker.choose(index)
                        }
                    }
                }
            }
        }
    }


    // -- A custom command ----------------------------------------------------------

    FocusScope {
        id: commandDialog

        property var game: null

        anchors.fill: parent
        enabled: focus
        visible: focus

        function openFor(g, start) {
            game = g;
            cmdInput.text = start;
            dialogMessage.text = "";
            focus = true;
            cmdBox.focus = true;
        }
        function closeDialog() {
            // the keyboard and its focus go away with the dialog
            Qt.inputMethod.hide();
            cmdInput.focus = false;
            page.focus = true;
        }
        function insert(text) {
            var pos = cmdInput.cursorPosition;
            cmdInput.text = cmdInput.text.substring(0, pos) + text + cmdInput.text.substring(pos);
            cmdInput.cursorPosition = pos + text.length;
        }
        function act(name) {
            var cmd = cmdInput.text.trim();
            if (name === "cancel")
                return closeDialog();
            if (cmd === "") {
                dialogMessage.text = qsTr("Type a launch command first.") + api.tr;
                return;
            }
            if (name === "test") {
                var target = game ? game : (root.games.length > 0 ? root.games[0] : null);
                if (!target) {
                    dialogMessage.text = qsTr("This collection has no games to test with.") + api.tr;
                    return;
                }
                var error = Internal.settings.testEmulatorCommand(cmd, target.path);
                dialogMessage.text = error === "" ? qsTr("Started \"%1\".").arg(target.title) + api.tr : error;
                return;
            }
            Qt.inputMethod.hide();
            cmdInput.focus = false;
            picker.game = game;
            picker.apply(cmd);
        }

        Keys.onPressed: {
            if (api.keys.isCancel(event) && !event.isAutoRepeat) {
                event.accepted = true;
                closeDialog();
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.3

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: commandDialog.closeDialog()
            }
        }

        Rectangle {
            width: Math.min(parent.width * 0.8, vpx(960))
            height: dialogColumn.height + vpx(48)
            anchors.centerIn: parent
            color: "#444"
            radius: vpx(8)

            MouseArea {
                anchors.fill: parent
            }

            Column {
                id: dialogColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(24)
                spacing: vpx(14)

                Text {
                    width: parent.width
                    text: (commandDialog.game
                        ? qsTr("Custom command for \"%1\". It's written to the game's launch: line.").arg(commandDialog.game.title)
                        : qsTr("Custom command for %1. It's written to the collection's launch: line.").arg(root.coll ? root.coll.name : "")) + api.tr
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(18)
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                FocusScope {
                    id: cmdBox
                    width: parent.width
                    height: Math.max(vpx(96), cmdInput.contentHeight + vpx(24))
                    KeyNavigation.down: insertRow

                    Rectangle {
                        anchors.fill: parent
                        color: "#333"
                        border.color: cmdBox.activeFocus ? "#3aa" : "#333"
                        border.width: vpx(2)
                    }
                    TextInput {
                        id: cmdInput
                        focus: true
                        color: "#e6e6e6"
                        font.family: global.fonts.mono
                        font.pixelSize: vpx(16)
                        wrapMode: TextInput.WrapAnywhere
                        anchors.fill: parent
                        anchors.margins: vpx(12)
                        onActiveFocusChanged: if (activeFocus) Qt.inputMethod.show()
                        Keys.onReturnPressed: insertRow.focus = true
                        KeyNavigation.down: insertRow
                    }
                }

                ListView {
                    id: insertRow
                    width: parent.width
                    height: vpx(34)
                    orientation: ListView.Horizontal
                    spacing: vpx(8)
                    model: [ "{file.documenturi}", "{file.uri}", "{file.path}", "{file.basename}", "{file.dir}" ]
                    KeyNavigation.up: cmdBox
                    KeyNavigation.down: actionRow

                    Keys.onPressed: {
                        if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                            event.accepted = true;
                            commandDialog.insert(model[currentIndex]);
                        }
                    }

                    delegate: Rectangle {
                        readonly property bool highlighted: ListView.view.activeFocus && ListView.isCurrentItem
                        width: chipText.implicitWidth + vpx(20)
                        height: ListView.view.height
                        radius: vpx(4)
                        color: highlighted ? "#3aa" : "#333"
                        border.color: "#3aa"
                        border.width: highlighted ? 0 : vpx(1)

                        Text {
                            id: chipText
                            text: modelData
                            color: parent.highlighted ? "#111" : "#9dd"
                            font.family: global.fonts.mono
                            font.pixelSize: vpx(14)
                            anchors.centerIn: parent
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: commandDialog.insert(modelData)
                        }
                    }
                }

                Text {
                    id: dialogMessage
                    width: parent.width
                    visible: text !== ""
                    color: "#ddd"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(16)
                    wrapMode: Text.WordWrap
                }

                ListView {
                    id: actionRow
                    width: parent.width
                    height: vpx(46)
                    orientation: ListView.Horizontal
                    layoutDirection: Qt.RightToLeft
                    spacing: vpx(12)
                    model: [
                        { "name": "save", "text": qsTr("Save") + api.tr },
                        { "name": "cancel", "text": qsTr("Cancel") + api.tr },
                        { "name": "test", "text": qsTr("Test with a game") + api.tr },
                    ]
                    KeyNavigation.up: insertRow

                    Keys.onPressed: {
                        if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                            event.accepted = true;
                            commandDialog.act(model[currentIndex].name);
                        }
                    }

                    delegate: Rectangle {
                        readonly property bool highlighted: ListView.view.activeFocus && ListView.isCurrentItem
                        width: actionText.implicitWidth + vpx(40)
                        height: ListView.view.height
                        radius: vpx(6)
                        color: highlighted ? "#3aa" : (modelData.name === "save" ? "#2a5f5f" : "#333")

                        Text {
                            id: actionText
                            text: modelData.text
                            color: parent.highlighted ? "#111" : "#eee"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(18)
                            anchors.centerIn: parent
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: commandDialog.act(modelData.name)
                        }
                    }
                }
            }
        }
    }
}
