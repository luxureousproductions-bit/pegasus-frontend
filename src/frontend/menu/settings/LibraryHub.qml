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
import "gamedireditor"
import QtQuick 2.6


// Pegasus X: the library hub. Left: where games, art and ES-DE data come from;
// right: every system with its emulator. A system opens its own page (name,
// emulator, your own emulators, hidden items).
FocusScope {
    id: root

    property bool mSettingsChanged: false
    property var systems: []

    signal close

    anchors.fill: parent

    enabled: focus
    visible: opacity > 0.001
    opacity: focus ? 1.0 : 0.0
    Behavior on opacity { PropertyAnimation { duration: 150 } }

    Component.onCompleted: refresh()

    function refresh() {
        systems = Internal.settings.emulatorSystems();
    }
    function changed() {
        mSettingsChanged = true;
        refresh();
    }

    function closeMaybe() {
        if (mSettingsChanged)
            reloadDialog.focus = true;
        else
            root.close();
    }

    // An app picked with "Other installed app" is saved as "@app:<package>:<style>:<name>",
    // one of your own emulators as "@custom:<name>"
    function appInfo(label) {
        if (!label || label.indexOf("@app:") !== 0)
            return null;
        var parts = label.split(":");
        return { "pkg": parts[1], "style": parts[2], "name": parts.slice(3).join(":") };
    }
    function styleText(style) {
        if (style === "bootpath")
            return qsTr("boot path") + api.tr;
        if (style === "view-uri")
            return qsTr("shared file") + api.tr;
        return qsTr("opens the file") + api.tr;
    }
    function labelText(label) {
        if (label && label.indexOf("@custom:") === 0)
            return label.substring(8) + qsTr(" (yours)") + api.tr;
        var app = appInfo(label);
        return app ? app.name + " (" + styleText(app.style) + ")" : label;
    }
    function currentText(sys) {
        if (!sys)
            return "";
        if (sys.current !== "")
            return labelText(sys.current);
        return qsTr("Default (%1)").arg(labelText(sys.defaultLabel)) + api.tr;
    }
    function statusText(sys) {
        if (!sys)
            return "";
        if (sys.status === "ready")
            return qsTr("Ready") + api.tr;
        if (sys.status === "missing")
            return qsTr("Not installed") + api.tr;
        return qsTr("No emulator") + api.tr;
    }
    function statusColor(sys) {
        if (sys && sys.status === "ready")
            return "#8d8";
        return "#eb7";
    }
    function shortPath(path) {
        var parts = path.split("/");
        return parts.length > 2 ? parts.slice(-2).join("/") : path;
    }
    function testResultText(result, what) {
        if (result.error === "")
            return qsTr("Started \"%1\" with %2.").arg(result.game).arg(what) + api.tr;
        if (result.game)
            return qsTr("Couldn't start \"%1\": %2").arg(result.game).arg(result.error) + api.tr;
        return result.error;
    }

    Keys.onPressed: {
        if (api.keys.isCancel(event) && !event.isAutoRepeat) {
            event.accepted = true;
            root.closeMaybe();
        }
    }


    Rectangle {
        anchors.fill: parent
        color: "#000"
        opacity: 0.4

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: root.closeMaybe()
        }
    }


    // -- The hub --------------------------------------------------------------

    Rectangle {
        id: main

        height: parent.height * 0.9
        width: Math.min(height * 1.78, parent.width * 0.96)
        anchors.centerIn: parent

        color: "#2a2b2e"
        radius: vpx(8)

        MouseArea {
            anchors.fill: parent
        }

        Text {
            id: hubTitle
            text: qsTr("Pegasus X library") + api.tr
            color: "#eee"
            font.family: globalFonts.sans
            font.pixelSize: vpx(26)
            font.bold: true

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: vpx(20)
        }
        Text {
            text: qsTr("%1 systems").arg(root.systems.length) + api.tr
            color: "#aaa"
            font.family: globalFonts.sans
            font.pixelSize: vpx(16)

            anchors.baseline: hubTitle.baseline
            anchors.right: parent.right
            anchors.rightMargin: vpx(20)
        }

        ListView {
            id: sourceList

            readonly property var entries: [
                { "text": qsTr("ROM folders") + api.tr, "note": qsTr("%1 folders, metadata files read").arg(Internal.settings.libraryRomDirs.length) + api.tr, "act": "roms" },
                { "text": qsTr("Media folders") + api.tr, "note": qsTr("%1 folders, last added wins").arg(Internal.settings.libraryMediaDirs.length) + api.tr, "act": "media" },
                { "text": qsTr("ES-DE import") + api.tr, "note": qsTr("%1 picked").arg(Internal.settings.esdeImports.length) + api.tr, "act": "esde" },
                { "text": qsTr("Skipped folders") + api.tr, "note": qsTr("%1 folders").arg(Internal.settings.ignoredDirs.length) + api.tr, "act": "ignore" },
                { "text": qsTr("Reload games") + api.tr, "note": "", "act": "reload" },
            ]

            anchors.top: hubTitle.bottom
            anchors.bottom: hubHints.top
            anchors.left: parent.left
            anchors.margins: vpx(20)
            width: parent.width * 0.28
            spacing: vpx(8)
            clip: true

            model: entries
            KeyNavigation.right: systemList

            function activate(index) {
                var act = entries[index].act;
                if (act === "reload") {
                    root.mSettingsChanged = false;
                    Internal.settings.reloadProviders();
                    return;
                }
                dirLoader.setSource("LibraryDirEditor.qml", { "mode": act });
            }

            Keys.onPressed: {
                if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                    event.accepted = true;
                    activate(currentIndex);
                }
            }

            delegate: Rectangle {
                readonly property bool highlighted: ListView.view.activeFocus && ListView.isCurrentItem

                width: ListView.view.width
                height: vpx(64)
                radius: vpx(6)
                color: highlighted ? "#3aa" : "#333"

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: vpx(14)
                    anchors.rightMargin: vpx(14)
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: vpx(2)

                    Text {
                        text: modelData.text
                        width: parent.width
                        color: parent.parent.highlighted ? "#111" : "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(18)
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: text !== ""
                        text: modelData.note
                        width: parent.width
                        color: parent.parent.highlighted ? "#123" : "#aaa"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(14)
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        sourceList.currentIndex = index;
                        sourceList.focus = true;
                        sourceList.activate(index);
                    }
                }
            }
        }

        Rectangle {
            anchors.top: hubTitle.bottom
            anchors.bottom: hubHints.top
            anchors.left: sourceList.right
            anchors.right: parent.right
            anchors.margins: vpx(20)
            color: "#222"
            radius: vpx(6)

            Text {
                text: qsTr("No systems found yet. Add ROM folders on the left, then reload.") + api.tr
                font.family: globalFonts.sans
                font.pixelSize: vpx(18)
                color: "#ccc"
                width: parent.width * 0.85
                anchors.centerIn: parent
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                visible: root.systems.length === 0
            }

            ListView {
                id: systemList

                anchors.fill: parent
                anchors.margins: vpx(8)
                clip: true
                spacing: vpx(4)

                model: root.systems
                focus: true
                highlightRangeMode: ListView.ApplyRange
                preferredHighlightBegin: height * 0.5 - vpx(30)
                preferredHighlightEnd: height * 0.5 + vpx(30)
                highlightMoveDuration: 0
                KeyNavigation.left: sourceList

                Keys.onPressed: {
                    if (event.isAutoRepeat)
                        return;
                    if (api.keys.isAccept(event)) {
                        event.accepted = true;
                        sysPage.open(currentIndex);
                    }
                    else if (api.keys.isDetails(event)) {
                        event.accepted = true;
                        if (currentIndex >= 0 && currentIndex < root.systems.length)
                            hubMessage.text = root.testResultText(
                                Internal.settings.testSystemEmulator(root.systems[currentIndex].name),
                                root.currentText(root.systems[currentIndex]));
                    }
                }

                delegate: Rectangle {
                    readonly property bool highlighted: ListView.view.activeFocus && ListView.isCurrentItem

                    width: ListView.view.width
                    height: vpx(60)
                    radius: vpx(6)
                    color: highlighted ? "#3aa" : "#2c2d30"

                    Column {
                        id: sysNameCol
                        anchors.left: parent.left
                        anchors.leftMargin: vpx(14)
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * 0.42
                        spacing: vpx(2)

                        Text {
                            text: modelData.collectionName
                            width: parent.width
                            color: parent.parent.highlighted ? "#111" : "#eee"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(18)
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: (modelData.nameSource === "metadata"
                                   ? qsTr("name from your metadata") : qsTr("ES-DE name")) + " · " + modelData.name + api.tr
                            width: parent.width
                            color: parent.parent.highlighted ? "#123" : "#999"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(13)
                            elide: Text.ElideRight
                        }
                    }

                    Text {
                        text: root.currentText(modelData)
                        color: parent.highlighted ? "#111" : (modelData.current !== "" ? "#5dd" : "#ccc")
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(16)

                        anchors.left: sysNameCol.right
                        anchors.leftMargin: vpx(12)
                        anchors.right: sysStatus.left
                        anchors.rightMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                    }

                    Text {
                        id: sysStatus
                        text: root.statusText(modelData)
                        color: parent.highlighted ? "#113" : root.statusColor(modelData)
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(15)
                        font.bold: true

                        anchors.right: parent.right
                        anchors.rightMargin: vpx(14)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            systemList.currentIndex = index;
                            systemList.focus = true;
                            sysPage.open(index);
                        }
                    }
                }
            }
        }

        Text {
            id: hubMessage
            text: ""
            visible: text !== ""
            color: "#ccc"
            font.family: globalFonts.sans
            font.pixelSize: vpx(14)
            elide: Text.ElideRight

            anchors.left: parent.left
            anchors.right: hubHints.left
            anchors.leftMargin: vpx(20)
            anchors.verticalCenter: hubHints.verticalCenter
        }

        Text {
            id: hubHints
            text: qsTr("A: open   Details button: test launch   B: back") + api.tr
            color: "#aaa"
            font.family: globalFonts.sans
            font.pixelSize: vpx(14)

            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: vpx(14)
        }
    }


    // -- A system's page --------------------------------------------------------

    FocusScope {
        id: sysPage

        property int systemIndex: -1
        readonly property var sys: (systemIndex >= 0 && systemIndex < root.systems.length)
                                   ? root.systems[systemIndex] : null
        property string message: ""

        readonly property var rows: {
            if (!sys)
                return [];
            var out = [];
            out.push({ "kind": "name", "title": qsTr("Name") + api.tr, "value": sys.collectionName,
                       "note": sys.metadataName === "" ? qsTr("ES-DE name") + api.tr
                             : (sys.nameSource === "metadata" ? qsTr("from your metadata, A: use the ES-DE name") + api.tr
                                                              : qsTr("ES-DE name, A: use \"%1\"").arg(sys.metadataName) + api.tr) });
            out.push({ "kind": "emulator", "title": qsTr("Launch with") + api.tr, "value": root.currentText(sys),
                       "note": root.statusText(sys) });
            out.push({ "kind": "test", "title": qsTr("Test with a game") + api.tr, "value": "", "note": "" });
            out.push({ "kind": "open", "title": qsTr("Open emulator") + api.tr, "value": "", "note": "" });
            for (var i = 0; i < sys.options.length; i++) {
                if (sys.options[i].custom)
                    out.push({ "kind": "custom", "title": qsTr("Your emulator") + api.tr,
                               "value": sys.options[i].label.substring(8), "note": qsTr("A: edit") + api.tr,
                               "command": sys.options[i].command });
            }
            out.push({ "kind": "add", "title": qsTr("Add emulator...") + api.tr, "value": "", "note": "" });
            out.push({ "kind": "hidden", "title": qsTr("Hidden") + api.tr, "value": "",
                       "note": qsTr("%1 skipped folders, %2 hidden games").arg(sys.ignored.length).arg(hiddenPage.hiddenGamesOf(sys).length) + api.tr });
            out.push({ "kind": "info", "title": qsTr("Game folders") + api.tr, "value": sys.folders.join("\n"), "note": "" });
            var art = [];
            for (var j = 0; j < sys.artSources.length; j++)
                art.push((j + 1) + ". " + sys.artSources[j]);
            out.push({ "kind": "info", "title": qsTr("Art folders, the last one wins (after metadata art)") + api.tr,
                       "value": art.length > 0 ? art.join("\n") : qsTr("none") + api.tr, "note": "" });
            return out;
        }

        anchors.fill: parent
        enabled: focus
        visible: focus || hiddenPage.focus || addDialog.focus || picker.focus || choice.focus
                 || gamePicker.focus || ignorePicker.focus || esdeCopy.focus

        function open(index) {
            if (index < 0 || index >= root.systems.length)
                return;
            systemIndex = index;
            message = "";
            rowList.currentIndex = 0;
            focus = true;
        }

        function activate(index) {
            if (index < 0 || index >= rows.length)
                return;
            var row = rows[index];
            switch (row.kind) {
                case "name":
                    if (sys.metadataName !== "") {
                        Internal.settings.setCollectionNameSource(sys.name, sys.nameSource === "metadata" ? "esde" : "metadata");
                        root.changed();
                    }
                    break;
                case "emulator":
                    picker.focus = true;
                    break;
                case "test":
                    message = root.testResultText(Internal.settings.testSystemEmulator(sys.name), root.currentText(sys));
                    break;
                case "open":
                    var error = Internal.settings.openSystemEmulator(sys.name);
                    message = error === "" ? qsTr("Opened.") + api.tr : error;
                    break;
                case "custom":
                    addDialog.openFor(row.value, row.command);
                    break;
                case "add":
                    addDialog.openFor("", "");
                    break;
                case "hidden":
                    hiddenPage.open();
                    break;
            }
        }

        Keys.onPressed: {
            if (event.isAutoRepeat)
                return;
            if (api.keys.isCancel(event)) {
                event.accepted = true;
                systemList.focus = true;
            }
            else if (api.keys.isAccept(event)) {
                event.accepted = true;
                activate(rowList.currentIndex);
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.5

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: systemList.focus = true
            }
        }

        Rectangle {
            width: main.width
            height: main.height
            anchors.centerIn: parent
            color: "#2a2b2e"
            radius: vpx(8)

            MouseArea {
                anchors.fill: parent
            }

            Text {
                id: sysTitle
                text: sysPage.sys ? sysPage.sys.collectionName : ""
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(26)
                font.bold: true

                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: vpx(20)
                width: parent.width * 0.6
                elide: Text.ElideRight
            }
            Text {
                text: sysPage.sys ? sysPage.sys.fullname + " · " + sysPage.sys.name : ""
                color: "#aaa"
                font.family: globalFonts.sans
                font.pixelSize: vpx(16)

                anchors.baseline: sysTitle.baseline
                anchors.right: parent.right
                anchors.rightMargin: vpx(20)
            }

            ListView {
                id: rowList

                anchors.top: sysTitle.bottom
                anchors.bottom: sysMessage.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(20)
                clip: true
                spacing: vpx(6)

                focus: true
                model: sysPage.rows
                highlightRangeMode: ListView.ApplyRange
                preferredHighlightBegin: height * 0.3
                preferredHighlightEnd: height * 0.7
                highlightMoveDuration: 0

                delegate: Rectangle {
                    readonly property bool highlighted: ListView.view.activeFocus && ListView.isCurrentItem
                    readonly property bool isInfo: modelData.kind === "info"

                    width: ListView.view.width
                    height: Math.max(vpx(54), rowValue.y + rowValue.height + vpx(10))
                    radius: vpx(6)
                    color: highlighted ? "#3aa" : (isInfo ? "#242528" : "#333")

                    Text {
                        id: rowTitle
                        text: modelData.title
                        color: parent.highlighted ? "#111" : (parent.isInfo ? "#aaa" : "#eee")
                        font.family: globalFonts.sans
                        font.pixelSize: parent.isInfo ? vpx(14) : vpx(18)
                        font.bold: !parent.isInfo

                        anchors.left: parent.left
                        anchors.leftMargin: vpx(14)
                        anchors.top: parent.top
                        anchors.topMargin: parent.isInfo ? vpx(8) : vpx(15)
                        width: parent.isInfo ? parent.width - vpx(28) : parent.width * 0.3
                        elide: Text.ElideRight
                    }
                    Text {
                        id: rowValue
                        text: modelData.value
                        color: parent.highlighted ? "#111" : "#ddd"
                        font.family: globalFonts.sans
                        font.pixelSize: parent.isInfo ? vpx(15) : vpx(18)
                        wrapMode: parent.isInfo ? Text.WrapAnywhere : Text.NoWrap
                        elide: parent.isInfo ? Text.ElideNone : Text.ElideRight

                        anchors.left: parent.isInfo ? parent.left : rowTitle.right
                        anchors.leftMargin: vpx(14)
                        anchors.right: parent.isInfo ? parent.right : rowNote.left
                        anchors.rightMargin: vpx(14)
                        anchors.top: parent.isInfo ? rowTitle.bottom : parent.top
                        anchors.topMargin: parent.isInfo ? vpx(4) : vpx(15)
                    }
                    Text {
                        id: rowNote
                        text: modelData.note
                        color: parent.highlighted ? "#123" : "#aaa"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(14)

                        anchors.right: parent.right
                        anchors.rightMargin: vpx(14)
                        anchors.top: parent.top
                        anchors.topMargin: vpx(18)
                        width: parent.isInfo ? 0 : Math.min(implicitWidth, parent.width * 0.35)
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            rowList.currentIndex = index;
                            rowList.focus = true;
                            sysPage.activate(index);
                        }
                    }
                }
            }

            Text {
                id: sysMessage
                text: sysPage.message !== "" ? sysPage.message : qsTr("A: change   B: back") + api.tr
                color: "#ccc"
                font.family: globalFonts.sans
                font.pixelSize: vpx(14)
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight

                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(16)
            }
        }
    }


    // -- Hidden: skipped folders and hidden games of a system ---------------------

    FocusScope {
        id: hiddenPage

        readonly property var sys: sysPage.sys

        function inFolders(path, folders) {
            for (var i = 0; i < folders.length; i++) {
                if (path.indexOf(folders[i] + "/") === 0)
                    return true;
            }
            return false;
        }
        function hiddenGamesOf(s) {
            var out = [];
            if (!s)
                return out;
            var all = Internal.settings.hiddenGames;
            for (var i = 0; i < all.length; i++) {
                if (inFolders(all[i], s.folders))
                    out.push(all[i]);
            }
            return out;
        }

        readonly property var rows: {
            if (!sys)
                return [];
            var out = [{ "kind": "head", "text": qsTr("Skipped folders (games, metadata and art)") + api.tr }];
            for (var i = 0; i < sys.ignored.length; i++)
                out.push({ "kind": "dir", "text": sys.ignored[i] });
            out.push({ "kind": "adddir", "text": qsTr("+ Skip a folder...") + api.tr });
            out.push({ "kind": "head", "text": qsTr("Hidden games") + api.tr });
            var games = hiddenGamesOf(sys);
            for (var j = 0; j < games.length; j++)
                out.push({ "kind": "game", "text": games[j] });
            out.push({ "kind": "addgame", "text": qsTr("+ Hide a game...") + api.tr });
            return out;
        }

        anchors.fill: parent
        enabled: focus
        visible: focus || gamePicker.focus || ignorePicker.focus

        function open() {
            hiddenList.currentIndex = 1;
            focus = true;
        }

        function activate(index) {
            if (index < 0 || index >= rows.length)
                return;
            var row = rows[index];
            if (row.kind === "dir") {
                Internal.settings.removeIgnoredDir(row.text);
                root.changed();
            }
            else if (row.kind === "game") {
                Internal.settings.unhideGame(row.text);
                root.changed();
            }
            else if (row.kind === "adddir") {
                ignorePicker.startFolder = sys.folders.length > 0 ? sys.folders[0] : "";
                ignorePicker.focus = true;
            }
            else if (row.kind === "addgame") {
                gamePicker.open();
            }
        }

        Keys.onPressed: {
            if (event.isAutoRepeat)
                return;
            if (api.keys.isCancel(event)) {
                event.accepted = true;
                sysPage.focus = true;
            }
            else if (api.keys.isAccept(event)) {
                event.accepted = true;
                activate(hiddenList.currentIndex);
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.4

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: sysPage.focus = true
            }
        }

        Rectangle {
            width: main.width * 0.8
            height: main.height * 0.9
            anchors.centerIn: parent
            color: "#2c2d30"
            radius: vpx(8)

            MouseArea {
                anchors.fill: parent
            }

            Text {
                id: hiddenTitle
                text: (hiddenPage.sys ? hiddenPage.sys.collectionName + ": " : "") + qsTr("Hidden") + api.tr
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(22)
                font.bold: true

                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                padding: vpx(16)
                elide: Text.ElideRight
            }

            ListView {
                id: hiddenList

                anchors.top: hiddenTitle.bottom
                anchors.bottom: hiddenHint.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(16)
                anchors.topMargin: 0
                clip: true

                focus: true
                model: hiddenPage.rows
                highlightRangeMode: ListView.ApplyRange
                preferredHighlightBegin: height * 0.3
                preferredHighlightEnd: height * 0.7
                highlightMoveDuration: 0

                // headers can't be selected
                Keys.onUpPressed: {
                    var i = currentIndex - 1;
                    while (i >= 0 && hiddenPage.rows[i].kind === "head")
                        i--;
                    if (i >= 0)
                        currentIndex = i;
                }
                Keys.onDownPressed: {
                    var i = currentIndex + 1;
                    while (i < count && hiddenPage.rows[i].kind === "head")
                        i++;
                    if (i < count)
                        currentIndex = i;
                }

                delegate: Rectangle {
                    readonly property bool isHead: modelData.kind === "head"
                    readonly property bool highlighted: !isHead && ListView.view.activeFocus && ListView.isCurrentItem

                    width: ListView.view.width
                    height: isHead ? vpx(40) : vpx(44)
                    radius: vpx(4)
                    color: highlighted ? "#3aa" : "transparent"

                    Text {
                        text: modelData.text
                        color: parent.highlighted ? "#111"
                             : parent.isHead ? "#aaa"
                             : (modelData.kind === "adddir" || modelData.kind === "addgame") ? "#5dd" : "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: parent.isHead ? vpx(14) : vpx(17)
                        font.bold: parent.isHead

                        anchors.left: parent.left
                        anchors.leftMargin: vpx(12)
                        anchors.right: unhideText.left
                        anchors.rightMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideMiddle
                    }
                    Text {
                        id: unhideText
                        visible: modelData.kind === "dir" || modelData.kind === "game"
                        text: qsTr("A: unhide") + api.tr
                        color: parent.highlighted ? "#123" : "#999"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(14)

                        anchors.right: parent.right
                        anchors.rightMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: !parent.isHead
                        onClicked: {
                            hiddenList.currentIndex = index;
                            hiddenList.focus = true;
                            hiddenPage.activate(index);
                        }
                    }
                }
            }

            Text {
                id: hiddenHint
                text: qsTr("Hidden items stay on your storage, Pegasus X just skips them. Reload when you're done.") + api.tr
                color: "#aaa"
                font.family: globalFonts.sans
                font.pixelSize: vpx(14)
                wrapMode: Text.WordWrap

                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(16)
            }
        }
    }


    // -- Add or edit one of your own emulators ------------------------------------

    FocusScope {
        id: addDialog

        property string oldName: ""
        property string appPackage: ""
        property string appComponent: ""
        property string message: ""

        readonly property var presets: [
            { "text": qsTr("Opens the game file") + api.tr, "cmd": "am start -n %C -a android.intent.action.VIEW -d {file.documenturi}" },
            { "text": qsTr("AetherSX2 style (boot path)") + api.tr, "cmd": "am start -n %C --activity-clear-task --activity-clear-top -a android.intent.action.MAIN -e bootPath {file.documenturi}" },
            { "text": qsTr("Shares the file") + api.tr, "cmd": "am start -n %C -a android.intent.action.VIEW -d {file.uri}" },
            { "text": qsTr("ID inside the file") + api.tr, "cmd": "am start -n %C -a android.intent.action.VIEW -e id {file.contents}" },
            { "text": qsTr("RetroArch core") + api.tr, "cmd": "am start -n com.retroarch/com.retroarch.browser.retroactivity.RetroActivityFuture -e ROM {file.path} -e LIBRETRO /data/data/com.retroarch/cores/CORE_libretro_android.so -e CONFIGFILE /storage/emulated/0/Android/data/com.retroarch/files/retroarch.cfg" },
            { "text": qsTr("Copy from an ES-DE emulator...") + api.tr, "cmd": "@copy" },
        ]
        readonly property var inserts: [ "{file.documenturi}", "{file.uri}", "{file.path}", "{file.basename}", "{file.contents}" ]
        readonly property var actions: {
            var out = [
                { "text": qsTr("Test with a game") + api.tr, "act": "test" },
                { "text": qsTr("Save") + api.tr, "act": "save" },
            ];
            if (oldName !== "")
                out.push({ "text": qsTr("Delete") + api.tr, "act": "delete" });
            out.push({ "text": qsTr("Cancel") + api.tr, "act": "cancel" });
            return out;
        }

        anchors.fill: parent
        enabled: focus
        visible: focus || (choice.focus && choice.forAdd) || esdeCopy.focus

        function openFor(name, command) {
            oldName = name;
            nameInput.text = name;
            cmdInput.text = command;
            appComponent = "";
            appPackage = "";
            var n = command.indexOf("-n ");
            if (n >= 0) {
                appComponent = command.substring(n + 3).split(" ")[0];
                appPackage = appComponent.split("/")[0];
            }
            message = "";
            nameBox.focus = true;
            focus = true;
        }
        function setApp(app) {
            appPackage = app["package"];
            appComponent = Internal.settings.appGameActivity(appPackage);
            if (nameInput.text === "")
                nameInput.text = app.label;
            if (appComponent !== "" && cmdInput.text.indexOf(" -n ") >= 0)
                cmdInput.text = cmdInput.text.replace(/ -n \S+/, " -n " + appComponent);
        }
        function usePreset(index) {
            var preset = presets[index];
            if (preset.cmd === "@copy") {
                esdeCopy.open();
                return;
            }
            var comp = appComponent !== "" ? appComponent : "PACKAGE/ACTIVITY";
            cmdInput.text = preset.cmd.replace("%C", comp);
            if (preset.cmd.indexOf("%C") >= 0 && appComponent === "")
                message = qsTr("Pick the app first, or replace PACKAGE/ACTIVITY in the command.") + api.tr;
        }
        function insert(text) {
            var pos = cmdInput.cursorPosition;
            cmdInput.text = cmdInput.text.substring(0, pos) + text + cmdInput.text.substring(pos);
            cmdInput.cursorPosition = pos + text.length;
        }
        function doAction(index) {
            var act = actions[index].act;
            if (act === "test") {
                message = root.testResultText(Internal.settings.testEmulatorCommand(sysPage.sys.name, cmdInput.text),
                                              nameInput.text !== "" ? nameInput.text : qsTr("this command") + api.tr);
            }
            else if (act === "save") {
                if (nameInput.text.trim() === "" || cmdInput.text.trim() === "") {
                    message = qsTr("Give it a name and a launch command.") + api.tr;
                    return;
                }
                Internal.settings.saveCustomEmulator(sysPage.sys.name, oldName, nameInput.text, cmdInput.text);
                Internal.settings.setSystemEmulator(sysPage.sys.name, "@custom:" + nameInput.text.trim());
                root.changed();
                sysPage.message = qsTr("Saved \"%1\" and picked it for this system.").arg(nameInput.text.trim()) + api.tr;
                sysPage.focus = true;
            }
            else if (act === "delete") {
                Internal.settings.removeCustomEmulator(sysPage.sys.name, oldName);
                root.changed();
                sysPage.focus = true;
            }
            else {
                sysPage.focus = true;
            }
        }

        Keys.onPressed: {
            if (api.keys.isCancel(event) && !event.isAutoRepeat) {
                event.accepted = true;
                sysPage.focus = true;
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.5

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: sysPage.focus = true
            }
        }

        Rectangle {
            width: main.width * 0.86
            height: addColumn.height + vpx(40)
            anchors.centerIn: parent
            color: "#2c2d30"
            radius: vpx(8)

            MouseArea {
                anchors.fill: parent
            }

            Column {
                id: addColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(20)
                spacing: vpx(10)

                Text {
                    text: (addDialog.oldName === "" ? qsTr("Add emulator") : qsTr("Edit emulator")) + api.tr
                          + (sysPage.sys ? "  ·  " + sysPage.sys.collectionName : "")
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(22)
                    font.bold: true
                }

                // name
                FocusScope {
                    id: nameBox
                    width: parent.width
                    height: vpx(48)
                    KeyNavigation.down: appButton

                    Rectangle {
                        anchors.fill: parent
                        radius: vpx(6)
                        color: "#1c1d1f"
                        border.color: nameBox.activeFocus ? "#5dd" : "#444"
                        border.width: vpx(2)
                    }
                    Text {
                        text: qsTr("Name") + api.tr
                        color: "#999"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(14)
                        anchors.left: parent.left
                        anchors.leftMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                        width: vpx(110)
                    }
                    TextInput {
                        id: nameInput
                        focus: true
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(18)
                        clip: true
                        anchors.left: parent.left
                        anchors.leftMargin: vpx(122)
                        anchors.right: parent.right
                        anchors.rightMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                        onActiveFocusChanged: if (activeFocus) Qt.inputMethod.show()
                        Keys.onReturnPressed: appButton.focus = true
                    }
                }

                // app
                Rectangle {
                    id: appButton
                    width: parent.width
                    height: vpx(48)
                    radius: vpx(6)
                    color: activeFocus ? "#3aa" : "#1c1d1f"
                    KeyNavigation.up: nameBox
                    KeyNavigation.down: presetList

                    Keys.onPressed: {
                        if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                            event.accepted = true;
                            choice.openApps(true);
                        }
                    }

                    Text {
                        text: qsTr("App") + api.tr
                        color: parent.activeFocus ? "#123" : "#999"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(14)
                        anchors.left: parent.left
                        anchors.leftMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: addDialog.appPackage !== ""
                            ? addDialog.appPackage + (addDialog.appComponent === "" ? qsTr("  (not installed)") + api.tr : "")
                            : qsTr("Pick from your installed apps...") + api.tr
                        color: parent.activeFocus ? "#111" : "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(18)
                        anchors.left: parent.left
                        anchors.leftMargin: vpx(122)
                        anchors.right: parent.right
                        anchors.rightMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideMiddle
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: choice.openApps(true)
                    }
                }

                Text {
                    text: qsTr("Start from a ready-made command") + api.tr
                    color: "#999"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(14)
                }

                ListView {
                    id: presetList
                    width: parent.width
                    height: vpx(40)
                    orientation: ListView.Horizontal
                    spacing: vpx(8)
                    clip: true
                    model: addDialog.presets
                    KeyNavigation.up: appButton
                    KeyNavigation.down: cmdBox

                    Keys.onPressed: {
                        if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                            event.accepted = true;
                            addDialog.usePreset(currentIndex);
                        }
                    }

                    delegate: Rectangle {
                        readonly property bool highlighted: ListView.view.activeFocus && ListView.isCurrentItem
                        width: presetText.implicitWidth + vpx(28)
                        height: ListView.view.height
                        radius: height / 2
                        color: highlighted ? "#3aa" : "#1c1d1f"
                        border.color: "#444"
                        border.width: highlighted ? 0 : vpx(1)

                        Text {
                            id: presetText
                            text: modelData.text
                            color: parent.highlighted ? "#111" : "#eee"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(15)
                            anchors.centerIn: parent
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: addDialog.usePreset(index)
                        }
                    }
                }

                // command
                FocusScope {
                    id: cmdBox
                    width: parent.width
                    height: Math.max(vpx(80), cmdInput.contentHeight + vpx(24))
                    KeyNavigation.up: presetList
                    KeyNavigation.down: insertList

                    Rectangle {
                        anchors.fill: parent
                        radius: vpx(6)
                        color: "#1c1d1f"
                        border.color: cmdBox.activeFocus ? "#5dd" : "#444"
                        border.width: vpx(2)
                    }
                    TextInput {
                        id: cmdInput
                        focus: true
                        color: "#e6e6e6"
                        font.family: global.fonts.mono
                        font.pixelSize: vpx(15)
                        wrapMode: TextInput.WrapAnywhere
                        anchors.fill: parent
                        anchors.margins: vpx(12)
                        onActiveFocusChanged: if (activeFocus) Qt.inputMethod.show()
                        Keys.onReturnPressed: insertList.focus = true
                        KeyNavigation.up: presetList
                        KeyNavigation.down: insertList
                    }
                }

                ListView {
                    id: insertList
                    width: parent.width
                    height: vpx(34)
                    orientation: ListView.Horizontal
                    spacing: vpx(8)
                    clip: true
                    model: addDialog.inserts
                    KeyNavigation.up: cmdBox
                    KeyNavigation.down: actionList

                    Keys.onPressed: {
                        if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                            event.accepted = true;
                            addDialog.insert(addDialog.inserts[currentIndex]);
                        }
                    }

                    delegate: Rectangle {
                        readonly property bool highlighted: ListView.view.activeFocus && ListView.isCurrentItem
                        width: insertText.implicitWidth + vpx(20)
                        height: ListView.view.height
                        radius: vpx(4)
                        color: highlighted ? "#3aa" : "#1c1d1f"
                        border.color: "#2a6b6b"
                        border.width: highlighted ? 0 : vpx(1)

                        Text {
                            id: insertText
                            text: modelData
                            color: parent.highlighted ? "#111" : "#bee"
                            font.family: global.fonts.mono
                            font.pixelSize: vpx(14)
                            anchors.centerIn: parent
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: addDialog.insert(modelData)
                        }
                    }
                }

                Text {
                    width: parent.width
                    text: addDialog.message !== "" ? addDialog.message
                        : qsTr("{file.contents} is the text inside the game file, eg. a Steam app ID.") + api.tr
                    color: "#ccc"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(14)
                    wrapMode: Text.WordWrap
                }

                ListView {
                    id: actionList
                    width: parent.width
                    height: vpx(44)
                    orientation: ListView.Horizontal
                    layoutDirection: Qt.RightToLeft
                    spacing: vpx(10)
                    model: addDialog.actions
                    KeyNavigation.up: insertList

                    Keys.onPressed: {
                        if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                            event.accepted = true;
                            addDialog.doAction(currentIndex);
                        }
                    }

                    delegate: Rectangle {
                        readonly property bool highlighted: ListView.view.activeFocus && ListView.isCurrentItem
                        width: actionText.implicitWidth + vpx(36)
                        height: ListView.view.height
                        radius: vpx(6)
                        color: highlighted ? "#3aa" : (modelData.act === "save" ? "#2a5f5f" : "#1c1d1f")

                        Text {
                            id: actionText
                            text: modelData.text
                            color: parent.highlighted ? "#111" : "#eee"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(16)
                            font.bold: true
                            anchors.centerIn: parent
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: addDialog.doAction(index)
                        }
                    }
                }
            }
        }
    }


    // -- The emulator list of a system -----------------------------------------

    FocusScope {
        id: picker

        readonly property var sys: sysPage.sys

        // the default first, then a picked app, the emulators (yours too) and "other app"
        readonly property var entries: {
            if (!sys)
                return [];
            var out = [{ "label": "", "installed": true, "kind": "default" }];
            if (root.appInfo(sys.current))
                out.push({ "label": sys.current, "installed": true, "kind": "app" });
            for (var i = 0; i < sys.options.length; i++) {
                var o = sys.options[i];
                out.push({ "label": o.label, "installed": o.installed, "retroarch": o.retroarch, "kind": "option" });
            }
            out.push({ "label": "@other", "kind": "other" });
            return out;
        }

        anchors.fill: parent
        enabled: focus
        visible: focus || (choice.focus && !choice.forAdd)

        onFocusChanged: {
            if (!focus || !sys)
                return;
            var idx = 0;
            for (var i = 1; i < entries.length; i++) {
                if (entries[i].kind !== "other" && entries[i].label === sys.current)
                    idx = i;
            }
            pickerList.currentIndex = idx;
            pickerList.positionViewAtIndex(idx, ListView.Center);
        }

        function choose(label) {
            Internal.settings.setSystemEmulator(sys.name, label);
            root.changed();
            sysPage.focus = true;
        }
        function pick(index) {
            if (index < 0 || index >= entries.length)
                return;
            var entry = entries[index];
            if (entry.kind === "other") {
                choice.openApps(false);
                return;
            }
            choose(entry.label);
        }

        Keys.onPressed: {
            if (event.isAutoRepeat)
                return;
            if (api.keys.isCancel(event)) {
                event.accepted = true;
                sysPage.focus = true;
            }
            else if (api.keys.isAccept(event)) {
                event.accepted = true;
                picker.pick(pickerList.currentIndex);
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.4

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: sysPage.focus = true
            }
        }

        Rectangle {
            width: main.width * 0.75
            height: Math.min(pickerTitle.height + pickerList.contentHeight + vpx(30), main.height)
            anchors.centerIn: parent
            color: "#2c2d30"
            radius: vpx(8)

            MouseArea {
                anchors.fill: parent
            }

            Text {
                id: pickerTitle
                text: picker.sys ? qsTr("%1: launch with").arg(picker.sys.collectionName) + api.tr : ""
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(20)

                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                padding: vpx(15)
                elide: Text.ElideRight
            }

            ListView {
                id: pickerList

                anchors.top: pickerTitle.bottom
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(15)
                anchors.topMargin: 0
                clip: true

                focus: true
                model: picker.entries
                highlightRangeMode: ListView.ApplyRange
                preferredHighlightBegin: height * 0.5 - vpx(18) * 1.25
                preferredHighlightEnd: height * 0.5 + vpx(18) * 1.25
                highlightMoveDuration: 0

                delegate: Rectangle {
                    readonly property bool highlighted: ListView.isCurrentItem || optionMouse.containsMouse
                    readonly property bool isOption: modelData.kind === "option"

                    width: ListView.view.width
                    height: vpx(18) * 2.4
                    color: highlighted ? "#3aa" : "transparent"
                    radius: vpx(4)

                    Text {
                        text: {
                            switch (modelData.kind) {
                                case "default": return qsTr("Default (%1)").arg(picker.sys ? root.labelText(picker.sys.defaultLabel) : "") + api.tr;
                                case "other": return qsTr("Other installed app...") + api.tr;
                                default: return root.labelText(modelData.label);
                            }
                        }
                        font.italic: modelData.kind === "other"
                        color: parent.highlighted ? "#111" : "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(18)

                        anchors.left: parent.left
                        anchors.leftMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * 0.58
                        elide: Text.ElideRight
                    }

                    Text {
                        visible: parent.isOption
                        text: !modelData.installed ? qsTr("not installed") + api.tr
                            : modelData.retroarch ? qsTr("RetroArch, needs the core") + api.tr
                            : qsTr("installed") + api.tr
                        color: parent.highlighted ? "#113" : (modelData.installed ? "#8d8" : "#d88")
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(14)

                        anchors.right: parent.right
                        anchors.rightMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    MouseArea {
                        id: optionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: picker.pick(index)
                    }
                }
            }
        }
    }


    // -- Pick an installed app: for "Other installed app", or for Add emulator ------

    FocusScope {
        id: choice

        property bool forAdd: false
        property string mode: ""  // "apps" or "styles"
        property var apps: []
        property var pickedApp: null

        readonly property var entries: {
            if (mode === "apps") {
                var out = [];
                for (var i = 0; i < apps.length; i++)
                    out.push({ "text": apps[i].label, "note": apps[i]["package"] });
                return out;
            }
            if (mode === "styles") {
                return [
                    { "text": qsTr("Open the game file") + api.tr, "note": qsTr("most emulators") + api.tr, "style": "view-saf" },
                    { "text": qsTr("Boot path") + api.tr, "note": "AetherSX2, NetherSX2", "style": "bootpath" },
                    { "text": qsTr("Share the file directly") + api.tr, "note": qsTr("if the others don't work") + api.tr, "style": "view-uri" },
                ];
            }
            return [];
        }

        anchors.fill: parent
        enabled: focus
        visible: focus

        onModeChanged: choiceList.currentIndex = 0

        function openApps(for_add) {
            forAdd = for_add;
            apps = Internal.settings.installedApps();
            mode = "apps";
            choiceList.currentIndex = 0;
            focus = true;
        }
        function goBack() {
            if (mode === "styles")
                mode = "apps";
            else if (forAdd) {
                addDialog.focus = true;
                appButton.forceActiveFocus();
            }
            else
                picker.focus = true;
        }
        function chose(index) {
            if (index < 0 || index >= entries.length)
                return;
            if (mode === "apps") {
                pickedApp = apps[index];
                if (forAdd) {
                    addDialog.setApp(pickedApp);
                    addDialog.focus = true;
                    presetList.forceActiveFocus();
                    return;
                }
                mode = "styles";
                return;
            }
            picker.choose("@app:" + pickedApp["package"] + ":" + entries[index].style + ":" + pickedApp.label);
        }

        Keys.onPressed: {
            if (event.isAutoRepeat)
                return;
            if (api.keys.isCancel(event)) {
                event.accepted = true;
                choice.goBack();
            }
            else if (api.keys.isAccept(event)) {
                event.accepted = true;
                choice.chose(choiceList.currentIndex);
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.4

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: choice.goBack()
            }
        }

        Rectangle {
            width: main.width * 0.75
            height: Math.min(choiceTitle.height + choiceList.contentHeight + vpx(30), main.height)
            anchors.centerIn: parent
            color: "#2c2d30"
            radius: vpx(8)

            MouseArea {
                anchors.fill: parent
            }

            Text {
                id: choiceTitle
                text: choice.mode === "styles" && choice.pickedApp
                    ? qsTr("How should %1 get the game?").arg(choice.pickedApp.label) + api.tr
                    : qsTr("Pick the emulator app") + api.tr
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(20)

                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                padding: vpx(15)
                elide: Text.ElideRight
            }

            ListView {
                id: choiceList

                anchors.top: choiceTitle.bottom
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(15)
                anchors.topMargin: 0
                clip: true

                focus: true
                model: choice.entries
                highlightRangeMode: ListView.ApplyRange
                preferredHighlightBegin: height * 0.5 - vpx(18) * 1.25
                preferredHighlightEnd: height * 0.5 + vpx(18) * 1.25
                highlightMoveDuration: 0

                delegate: Rectangle {
                    readonly property bool highlighted: ListView.isCurrentItem || choiceMouse.containsMouse

                    width: ListView.view.width
                    height: vpx(18) * 2.4
                    color: highlighted ? "#3aa" : "transparent"
                    radius: vpx(4)

                    Text {
                        text: modelData.text
                        color: parent.highlighted ? "#111" : "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(18)

                        anchors.left: parent.left
                        anchors.leftMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * 0.55
                        elide: Text.ElideRight
                    }

                    Text {
                        text: modelData.note
                        color: parent.highlighted ? "#113" : "#999"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(14)

                        anchors.right: parent.right
                        anchors.rightMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * 0.4
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideMiddle
                    }

                    MouseArea {
                        id: choiceMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: choice.chose(index)
                    }
                }
            }
        }
    }


    // -- Copy a command from one of ES-DE's emulators (Add emulator) --------------

    FocusScope {
        id: esdeCopy

        readonly property var entries: {
            var out = [];
            var s = sysPage.sys;
            if (!s)
                return out;
            for (var i = 0; i < s.options.length; i++) {
                if (!s.options[i].custom && s.options[i].command !== "")
                    out.push(s.options[i]);
            }
            return out;
        }

        anchors.fill: parent
        enabled: focus
        visible: focus

        function open() {
            copyList.currentIndex = 0;
            focus = true;
        }
        function chose(index) {
            if (index < 0 || index >= entries.length)
                return;
            cmdInput.text = entries[index].command;
            if (nameInput.text === "")
                nameInput.text = entries[index].label + qsTr(" (mine)") + api.tr;
            addDialog.focus = true;
            cmdBox.forceActiveFocus();
        }

        Keys.onPressed: {
            if (event.isAutoRepeat)
                return;
            if (api.keys.isCancel(event)) {
                event.accepted = true;
                addDialog.focus = true;
            }
            else if (api.keys.isAccept(event)) {
                event.accepted = true;
                esdeCopy.chose(copyList.currentIndex);
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.4
        }

        Rectangle {
            width: main.width * 0.75
            height: Math.min(copyTitle.height + copyList.contentHeight + vpx(30), main.height)
            anchors.centerIn: parent
            color: "#2c2d30"
            radius: vpx(8)

            Text {
                id: copyTitle
                text: qsTr("Start from which ES-DE emulator?") + api.tr
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(20)
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                padding: vpx(15)
            }

            ListView {
                id: copyList
                anchors.top: copyTitle.bottom
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(15)
                anchors.topMargin: 0
                clip: true
                focus: true
                model: esdeCopy.entries
                highlightMoveDuration: 0

                delegate: Rectangle {
                    readonly property bool highlighted: ListView.isCurrentItem
                    width: ListView.view.width
                    height: vpx(18) * 2.4
                    color: highlighted ? "#3aa" : "transparent"
                    radius: vpx(4)

                    Text {
                        text: modelData.label
                        color: parent.highlighted ? "#111" : "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(18)
                        anchors.left: parent.left
                        anchors.leftMargin: vpx(12)
                        anchors.right: parent.right
                        anchors.rightMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: esdeCopy.chose(index)
                    }
                }
            }
        }
    }


    // -- Hide a game: the system's games ------------------------------------------

    FocusScope {
        id: gamePicker

        property var games: []

        anchors.fill: parent
        enabled: focus
        visible: focus

        function open() {
            var out = [];
            var s = sysPage.sys;
            for (var i = 0; s && i < api.collections.count; i++) {
                var coll = api.collections.get(i);
                if (coll.name !== s.collectionName)
                    continue;
                for (var j = 0; j < coll.games.count; j++) {
                    var game = coll.games.get(j);
                    if (game.files.count > 0)
                        out.push({ "title": game.title, "path": game.files.get(0).path });
                }
            }
            games = out;
            gameList.currentIndex = 0;
            focus = true;
        }

        Keys.onPressed: {
            if (event.isAutoRepeat)
                return;
            if (api.keys.isCancel(event)) {
                event.accepted = true;
                hiddenPage.focus = true;
            }
            else if (api.keys.isAccept(event) && gameList.currentIndex >= 0 && gameList.currentIndex < games.length) {
                event.accepted = true;
                Internal.settings.hideGame(games[gameList.currentIndex].path);
                root.changed();
                hiddenPage.focus = true;
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.4
        }

        Rectangle {
            width: main.width * 0.75
            height: main.height * 0.85
            anchors.centerIn: parent
            color: "#2c2d30"
            radius: vpx(8)

            Text {
                id: gameTitle
                text: gamePicker.games.length > 0 ? qsTr("Hide which game?") + api.tr
                    : qsTr("No games found for this system yet. Reload first.") + api.tr
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(20)
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                padding: vpx(15)
            }

            ListView {
                id: gameList
                anchors.top: gameTitle.bottom
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(15)
                anchors.topMargin: 0
                clip: true
                focus: true
                model: gamePicker.games
                highlightRangeMode: ListView.ApplyRange
                preferredHighlightBegin: height * 0.5 - vpx(20)
                preferredHighlightEnd: height * 0.5 + vpx(20)
                highlightMoveDuration: 0

                delegate: Rectangle {
                    readonly property bool highlighted: ListView.isCurrentItem
                    width: ListView.view.width
                    height: vpx(18) * 2.2
                    color: highlighted ? "#3aa" : "transparent"
                    radius: vpx(4)

                    Text {
                        text: modelData.title
                        color: parent.highlighted ? "#111" : "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(17)
                        anchors.left: parent.left
                        anchors.leftMargin: vpx(12)
                        anchors.right: parent.right
                        anchors.rightMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            Internal.settings.hideGame(modelData.path);
                            root.changed();
                            hiddenPage.focus = true;
                        }
                    }
                }
            }
        }
    }


    // -- Skip a folder (starts in the system's folder) ---------------------------

    FilePicker {
        id: ignorePicker
        folderMode: true
        fileNames: []
        fileExtensions: []

        enabled: focus
        visible: opacity > 0.01
        opacity: focus ? 1.0 : 0.0
        Behavior on opacity { PropertyAnimation { duration: 200 } }

        onCancel: hiddenPage.focus = true
        onPick: {
            Internal.settings.addIgnoredDir(dir_path);
            root.changed();
            hiddenPage.focus = true;
        }
    }


    // -- The folder lists (ROM folders, media folders, ES-DE import, skipped) --------

    Loader {
        id: dirLoader
        anchors.fill: parent
        asynchronous: false
        enabled: item !== null
        onLoaded: item.focus = true
    }
    Connections {
        target: dirLoader.item
        function onClose() {
            dirLoader.source = "";
            root.refresh();
            sourceList.focus = true;
        }
    }


    ReloadQuestion {
        id: reloadDialog
        onAccept: {
            systemList.focus = true;
            root.mSettingsChanged = false;
            Internal.settings.reloadProviders();
        }
        onCancel: {
            systemList.focus = true;
            root.mSettingsChanged = false;
            root.close();
        }
    }
}
