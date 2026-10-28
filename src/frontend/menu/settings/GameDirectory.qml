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
import "qrc:/qmlutils" as PegasusUtils
import QtQuick 2.6


// Pegasus X: Settings > Game directory. The systems in
// <Pegasus X folder>/gamedirectory (one metadata file each), Add systems
// (pick systems, then the ROM folder they're in), Import gamelist (ES-DE),
// and the ROM folders watched for new games.
FocusScope {
    id: root

    signal close
    signal openEsdeImport

    property string page: "main"     // main, add, found
    property bool addOnly: false     // opened for Add systems alone (the first-start setup)
    property string message: ""
    property bool changed: false     // the library is read again on the way out
    property bool working: false
    property var systems: []         // Settings.gameDirectorySystems
    property var watched: []         // Settings.watchedRomFolders
    property var known: []           // Settings.knownSystems (Add systems)
    property var picked: ({})        // system name -> true
    property string romFolder: ""
    property var matches: []         // Settings.matchSystems
    property string folderFor: ""    // a system without a folder: picking one for it
    property int pickedCount: Object.keys(picked).length

    anchors.fill: parent
    enabled: focus
    visible: focus

    Component.onCompleted: {
        refresh();
        if (addOnly)
            openAdd();
    }

    function refresh() {
        systems = Internal.settings.gameDirectorySystems();
        watched = Internal.settings.watchedRomFolders();
    }
    function keepPosition(fn) {
        var i = list.currentIndex;
        fn();
        list.currentIndex = Math.min(i, Math.max(0, list.count - 1));
    }
    function openAdd() {
        known = Internal.settings.knownSystems();
        picked = {};
        romFolder = "";
        matches = [];
        page = "add";
        message = "";
        list.currentIndex = 0;
        list.positionViewAtBeginning();
    }
    function togglePicked(name) {
        var next = Object.assign({}, picked);
        if (next[name]) delete next[name]; else next[name] = true;
        keepPosition(function() { picked = next; });
    }
    // a whole ROM folder: every system found in it (no picking)
    function addRomFolder() {
        picked = {};
        folderFor = "";
        filePicker.openAt(Internal.settings.lastBrowseFolder);
        filePicker.focus = true;
    }
    function removeWatched(folder) {
        chooser.openFor(qsTr("Remove %1? Its games leave the game directory (their entries are set aside in backup/, nothing on the card is touched) and it's no longer watched.").arg(folder) + api.tr, [
            { "act": "remove", "text": qsTr("Remove the folder") + api.tr },
            { "act": "forget", "text": qsTr("Only stop watching it (keep the games)") + api.tr },
            { "act": "stay", "text": qsTr("Keep it") + api.tr },
        ], function(act) {
            if (act === "stay")
                return;
            var n = Internal.settings.removeRomFolder(folder, act === "remove");
            root.changed = root.changed || act === "remove";
            root.message = act === "remove" ? qsTr("%1 games set aside.").arg(n) + api.tr : qsTr("No longer watched.") + api.tr;
            root.keepPosition(root.refresh);
        });
    }
    function pickRomFolder() {
        if (pickedCount === 0) {
            message = qsTr("Tick the systems first.") + api.tr;
            return;
        }
        folderFor = "";
        filePicker.openAt(romFolder !== "" ? romFolder : Internal.settings.lastBrowseFolder);
        filePicker.focus = true;
    }
    function showMatches(folder) {
        romFolder = folder;
        matches = Internal.settings.matchSystems(folder, Object.keys(picked));
        page = "found";
        message = "";
        list.currentIndex = 0;
        list.positionViewAtBeginning();
    }
    readonly property int foundCount: {
        var n = 0;
        for (var i = 0; i < matches.length; i++)
            if (matches[i].games > 0) n++;
        return n;
    }
    function importFound() {
        var names = [];
        for (var i = 0; i < matches.length; i++)
            if (matches[i].games > 0) names.push(matches[i].name);
        if (names.length === 0) {
            message = qsTr("None of the picked systems has a folder here.") + api.tr;
            return;
        }
        working = true;
        message = qsTr("Reading %1 systems...").arg(names.length) + api.tr;
        Internal.settings.importSystems(romFolder, names);
    }
    function pickFolderFor(name) {
        folderFor = name;
        filePicker.openAt(romFolder !== "" ? romFolder : Internal.settings.lastBrowseFolder);
        filePicker.focus = true;
    }
    Connections {
        target: Internal.settings
        function onRomFolderImportFinished(result) {
            root.working = false;
            if (result.error) {
                root.message = result.error;
                return;
            }
            root.changed = true;
            root.message = qsTr("Done: %1 systems, %2 games in the game directory.").arg(result.systems).arg(result.games) + api.tr;
            root.keepPosition(function() {
                root.refresh();
                if (root.page === "found")
                    root.matches = Internal.settings.matchSystems(root.romFolder, Object.keys(root.picked));
            });
            if (root.page === "found" && root.foundCount > 0) {
                // the systems are in: back to the list
                root.page = "main";
                root.list_top();
            }
        }
    }
    function list_top() {
        list.currentIndex = 0;
        list.positionViewAtBeginning();
    }
    function reimport(entry) {
        working = true;
        message = qsTr("Reading %1's folders again...").arg(entry.name) + api.tr;
        Internal.settings.reimportSystem(entry.key);
    }
    function remove(entry) {
        chooser.openFor(qsTr("Remove %1 from the game directory? Its file goes to backup/%2/ (nothing is deleted), and its ROM folders are no longer watched.").arg(entry.name).arg(entry.key) + api.tr, [
            { "act": "remove", "text": qsTr("Remove") + api.tr },
            { "act": "stay", "text": qsTr("Keep it") + api.tr },
        ], function(act) {
            if (act !== "remove")
                return;
            var error = Internal.settings.removeSystem(entry.key);
            root.message = error !== "" ? error : qsTr("%1 removed from the game directory.").arg(entry.name) + api.tr;
            root.keepPosition(root.refresh);
        });
    }
    function back() {
        if (filePicker.focus) {
            filePicker.focus = false;
            list.focus = true;
            return;
        }
        if (page === "found") {
            page = pickedCount > 0 ? "add" : "main";
            message = "";
            list_top();
            return;
        }
        if (page === "add" && !addOnly) {
            page = "main";
            message = "";
            list_top();
            return;
        }
        if (changed)
            Internal.settings.reloadLibrary();
        root.close();
    }

    Rectangle {
        anchors.fill: parent
        color: "#222"
    }
    PegasusUtils.HorizontalSwipeArea {
        anchors.fill: parent
        onSwipeRight: root.back()
    }

    ScreenHeader {
        id: header
        text: (root.page === "main" ? qsTr("Settings > Game directory")
             : qsTr("Game directory > Add systems")) + api.tr
        z: 2
    }

    ListView {
        id: list
        anchors.top: header.bottom
        anchors.bottom: messageBar.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.7
        clip: true
        focus: true
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: height * 0.3
        preferredHighlightEnd: height * 0.7
        highlightMoveDuration: 150

        readonly property var rows: {
            var out = [];
            var i;
            if (root.page === "main") {
                var total = 0;
                for (i = 0; i < root.systems.length; i++)
                    total += root.systems[i].games;
                out.push({ "kind": "section", "text": qsTr("PegasusX/gamedirectory · %1 systems · %2 games").arg(root.systems.length).arg(total) + api.tr });
                out.push({ "kind": "addfolder", "text": qsTr("Add a ROM folder...") + api.tr, "value": qsTr("every system found in it ›") + api.tr });
                out.push({ "kind": "add", "text": qsTr("Add systems...") + api.tr, "value": qsTr("pick systems, then their ROM folder ›") + api.tr });
                out.push({ "kind": "esde", "text": qsTr("Import gamelist...") + api.tr, "value": qsTr("ES-DE: details, art, emulators") + api.tr,
                           "muted": !Internal.settings.esdeImportSupported });
                out.push({ "kind": "section", "text": qsTr("Systems") + api.tr });
                for (i = 0; i < root.systems.length; i++) {
                    var s = root.systems[i];
                    var where = s.folders.length > 0 ? root.shortFolder(s.folders[0]) + (s.folders.length > 1 ? " +" + (s.folders.length - 1) : "") : "";
                    out.push({ "kind": "system", "text": s.name, "entry": s,
                               "sub": (s.shortName !== "" && s.shortName !== s.name.toLowerCase()) ? "· " + s.shortName : "",
                               "value": (s.games === 1 ? qsTr("1 game") : qsTr("%1 games").arg(s.games)) + api.tr
                                      + (where !== "" ? " · " + where : "")
                                      + (s.source !== "" ? " · " + (s.source === "gamelist" ? qsTr("gamelist") : qsTr("own metadata")) + api.tr : ""),
                               "done": s.source !== "" });
                }
                if (root.systems.length === 0)
                    out.push({ "kind": "info", "text": qsTr("No systems yet: Add systems, or Import gamelist.") + api.tr, "value": "" });
                out.push({ "kind": "section", "text": qsTr("ROM folders watched for new games") + api.tr });
                var byFolder = {};
                for (i = 0; i < root.watched.length; i++) {
                    var w = root.watched[i];
                    var parent = w.folder.replace(/\/[^\/]+\/?$/, "");
                    byFolder[parent] = (byFolder[parent] || 0) + 1;
                }
                var parents = Object.keys(byFolder).sort();
                for (i = 0; i < parents.length; i++)
                    out.push({ "kind": "watched", "text": parents[i], "folder": parents[i],
                               "value": (byFolder[parents[i]] === 1 ? qsTr("1 system") : qsTr("%1 systems").arg(byFolder[parents[i]])) + api.tr });
                if (parents.length > 0)
                    out.push({ "kind": "rescan", "text": qsTr("Read them again now") + api.tr, "value": root.working ? qsTr("working...") + api.tr : "" });
                else
                    out.push({ "kind": "info", "text": qsTr("None") + api.tr, "value": "" });
                return out;
            }
            if (root.page === "add") {
                out.push({ "kind": "next", "text": root.pickedCount > 0
                    ? qsTr("Next: pick the ROM folder for these %1 ›").arg(root.pickedCount) + api.tr
                    : qsTr("Tick your systems, then pick their ROM folder") + api.tr, "value": "" });
                out.push({ "kind": "section", "text": qsTr("%1 systems").arg(root.known.length) + api.tr });
                for (i = 0; i < root.known.length; i++) {
                    var k = root.known[i];
                    out.push({ "kind": "pick", "text": k.fullName, "name": k.name, "check": !!root.picked[k.name],
                               "value": k.shortName + (k.inDirectory ? " · " + qsTr("already in your directory") + api.tr
                                                                     : " · " + String(k.extensions).replace(/\./g, "").split(/\s+/).slice(0, 4).join(" ")),
                               "muted": k.inDirectory });
                }
                return out;
            }
            // found
            out.push({ "kind": "section", "text": (root.pickedCount === 0 ? qsTr("ROM folder: %1 · every system found in it") : qsTr("ROM folder: %1")).arg(root.romFolder) + api.tr });
            if (root.matches.length === 0)
                out.push({ "kind": "info", "text": qsTr("No system folders found here (they're named like snes, psx, gba). Pick the folder that has them, or use Add systems and pick each folder.") + api.tr, "value": "" });
            for (i = 0; i < root.matches.length; i++) {
                var m = root.matches[i];
                if (m.games > 0) {
                    var bits = [ root.shortFolder(m.folders[0]), (m.games === 1 ? qsTr("1 game") : qsTr("%1 games").arg(m.games)) + api.tr ];
                    if (m.ownMetadata) bits.push(qsTr("own metadata file found") + api.tr);
                    if (m.gamelist) bits.push(qsTr("gamelist.xml found") + api.tr);
                    if (m.mediaFolder) bits.push(qsTr("media folder found") + api.tr);
                    out.push({ "kind": "match", "text": m.fullName, "entry": m, "value": bits.join(" · "), "done": true });
                }
                else {
                    out.push({ "kind": "match", "text": m.fullName, "entry": m,
                               "value": qsTr("no folder called %1 here · A picks one").arg(m.folderName || m.name) + api.tr, "warn": true });
                }
            }
            out.push({ "kind": "section", "text": qsTr("What happens") + api.tr });
            out.push({ "kind": "info", "text": qsTr("The games go into gamedirectory/<system>. Your own metadata file is read once (its details and art first) and never written to. A media folder's art is linked by path. Games already in the directory keep what they have.") + api.tr, "value": "" });
            out.push({ "kind": "import", "text": root.working ? qsTr("Reading...") + api.tr
                       : (root.foundCount === 1 ? qsTr("Import 1 system") : qsTr("Import %1 systems").arg(root.foundCount)) + api.tr, "value": "" });
            return out;
        }
        model: rows

        function selectable(i) {
            return i >= 0 && i < rows.length && rows[i].kind !== "section" && rows[i].kind !== "info";
        }
        // quick moves in a long list: LB top, RB bottom, LT/RT 10 rows
        function jumpTo(end) {
            var i = end ? count - 1 : 0;
            while (i >= 0 && i < count && !selectable(i))
                i += end ? -1 : 1;
            if (i >= 0 && i < count)
                currentIndex = i;
        }
        function jumpBy(delta) {
            var step = delta > 0 ? 1 : -1;
            var i = currentIndex, moved = 0, last = currentIndex;
            while (moved < Math.abs(delta)) {
                i += step;
                if (i < 0 || i >= count)
                    break;
                if (selectable(i)) {
                    last = i;
                    moved++;
                }
            }
            currentIndex = last;
        }
        function pageKeys(event) {
            if (api.keys.isPrevPage(event)) jumpTo(false);
            else if (api.keys.isNextPage(event)) jumpTo(true);
            else if (api.keys.isPageUp(event)) jumpBy(-10);
            else if (api.keys.isPageDown(event)) jumpBy(10);
            else return false;
            event.accepted = true;
            return true;
        }
        Keys.onUpPressed: {
            var i = currentIndex - 1;
            while (i >= 0 && !selectable(i)) i--;
            if (i >= 0) currentIndex = i;
        }
        Keys.onDownPressed: {
            var i = currentIndex + 1;
            while (i < count && !selectable(i)) i++;
            if (i < count) currentIndex = i;
        }
        onModelChanged: {
            if (!selectable(currentIndex)) {
                var i = currentIndex + 1;
                while (i < count && !selectable(i)) i++;
                if (i < count) currentIndex = i;
            }
        }
        function activate(index) {
            if (!selectable(index) || root.working)
                return;
            var row = rows[index];
            if (row.kind === "add") root.openAdd();
            else if (row.kind === "addfolder") root.addRomFolder();
            else if (row.kind === "watched") root.removeWatched(row.folder);
            else if (row.kind === "esde") root.openEsdeImport();
            else if (row.kind === "system") root.reimport(row.entry);
            else if (row.kind === "rescan") { root.working = true; root.message = qsTr("Reading the watched folders again...") + api.tr; Internal.settings.rescanRomFolders(); }
            else if (row.kind === "next") root.pickRomFolder();
            else if (row.kind === "pick") root.togglePicked(row.name);
            else if (row.kind === "match") { if (row.entry.games > 0) root.importFound(); else root.pickFolderFor(row.entry.name); }
            else if (row.kind === "import") root.importFound();
        }
        Keys.onPressed: {
            if (pageKeys(event))
                return;
            if (event.isAutoRepeat)
                return;
            if (api.keys.isAccept(event)) {
                event.accepted = true;
                activate(currentIndex);
            }
            else if (api.keys.isCancel(event)) {
                event.accepted = true;
                root.back();
            }
            else if (api.keys.isFilters(event)) {
                // Y: a system out of the directory
                event.accepted = true;
                var row = selectable(currentIndex) ? rows[currentIndex] : null;
                if (row && row.kind === "system")
                    root.remove(row.entry);
                else if (row && row.kind === "watched")
                    root.removeWatched(row.folder);
            }
        }

        delegate: Item {
            readonly property bool isSection: modelData.kind === "section"
            readonly property bool isInfo: modelData.kind === "info"
            readonly property bool isPick: modelData.kind === "pick"
            readonly property bool highlighted: !isSection && !isInfo
                && ((ListView.isCurrentItem && ListView.view.activeFocus) || rowMouse.containsMouse)
            width: ListView.view.width
            height: isSection ? sectionText.height : (isInfo ? infoText.height + vpx(16) : isPick ? vpx(44) : vpx(22) * 2.4)

            Text {
                id: sectionText
                visible: parent.isSection
                text: modelData.text
                color: "#bbb"
                font.family: globalFonts.sans
                font.pixelSize: vpx(19)
                topPadding: font.pixelSize * (index === 0 ? 0.75 : 1.5)
                bottomPadding: font.pixelSize * 0.25
                width: parent.width
                elide: Text.ElideRight
            }
            Text {
                id: infoText
                visible: parent.isInfo
                text: modelData.text
                color: "#999"
                font.family: globalFonts.sans
                font.pixelSize: vpx(17)
                wrapMode: Text.WordWrap
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                lineHeight: 1.2
            }
            Rectangle {
                width: parent.width
                height: vpx(3)
                anchors.bottom: parent.bottom
                color: "#3aa"
                visible: parent.highlighted
            }
            Rectangle {
                visible: parent.isPick
                width: vpx(18)
                height: vpx(18)
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                color: "transparent"
                border.color: "#ddd"
                border.width: vpx(2)
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: vpx(4)
                    color: "#ddd"
                    visible: !!modelData.check
                }
            }
            Text {
                id: rowLabel
                visible: !parent.isSection && !parent.isInfo
                text: modelData.text + (modelData.sub ? " <font color='#888' size='2'>" + modelData.sub + "</font>" : "")
                textFormat: modelData.sub ? Text.RichText : Text.PlainText
                color: modelData.muted ? "#999" : "#eee"
                font.family: globalFonts.sans
                font.pixelSize: parent.isPick ? vpx(19) : vpx(21)
                anchors.left: parent.left
                anchors.leftMargin: parent.isPick ? vpx(62) : vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width * 0.45
                elide: Text.ElideRight
            }
            Text {
                visible: !parent.isSection && !parent.isInfo
                text: modelData.value || ""
                color: modelData.warn ? "#e9a23b" : modelData.done ? "#9d9" : modelData.muted ? "#888" : "#c0c0c0"
                font.family: globalFonts.sans
                font.pixelSize: parent.isPick ? vpx(16) : vpx(19)
                anchors.left: rowLabel.right
                anchors.leftMargin: vpx(16)
                anchors.right: parent.right
                anchors.rightMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideMiddle
            }
            MouseArea {
                id: rowMouse
                anchors.fill: parent
                enabled: !parent.isSection && !parent.isInfo
                hoverEnabled: true
                onClicked: {
                    list.currentIndex = index;
                    list.focus = true;
                    list.activate(index);
                }
            }
        }
    }

    function shortFolder(path) {
        var p = String(path || "").replace(/\/+$/, "");
        var parts = p.split("/");
        return parts.length >= 2 ? parts[parts.length - 2] + "/" + parts[parts.length - 1] : p;
    }

    Text {
        id: messageBar
        text: root.message !== "" ? root.message
            : root.page === "main" ? qsTr("A open / re-import a system · Y remove a system or a ROM folder · B back") + api.tr
            : root.page === "add" ? qsTr("A tick · B back") + api.tr
            : qsTr("A import / pick a folder · B back") + api.tr
        height: implicitHeight
        color: "#ddd"
        font.family: globalFonts.sans
        font.pixelSize: vpx(17)
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.7
        topPadding: vpx(10)
        bottomPadding: vpx(16)
    }

    // the ROM folder (for the picked systems), or one system's folder
    FilePicker {
        id: filePicker
        z: 3
        enabled: focus
        visible: focus
        pickFolders: true
        allowNewFolder: false
        fileExtensions: []
        infoText: root.folderFor !== ""
            ? qsTr("Pick the folder the %1 games are in.").arg(root.folderFor) + api.tr
            : qsTr("Pick the folder your ROMs are in: the one with the systems' folders inside (snes, psx, ...), or one system's folder.") + api.tr
        onCancel: { focus = false; list.focus = true; }
        onPickFolder: {
            focus = false;
            list.focus = true;
            if (root.folderFor !== "") {
                root.working = true;
                root.message = qsTr("Reading %1 as %2...").arg(dir_path).arg(root.folderFor) + api.tr;
                Internal.settings.importFolderAs(dir_path, root.folderFor);
                root.folderFor = "";
                return;
            }
            root.showMatches(dir_path);
        }
    }

    // a yes/no with a short list (as Library's)
    PegasusOptionsDialog {
        id: chooser
        z: 4
        property var callback: null
        function openFor(question, entries, cb) {
            text = question;
            rows = entries.map(function(e) { return { "text": e.text, "value": "", "act": e.act }; });
            callback = cb;
            focus = true;
        }
        title: qsTr("Game directory") + api.tr
        closeLabel: qsTr("Cancel") + api.tr
        onActivated: {
            var act = rows[index].act;
            focus = false;
            list.focus = true;
            if (callback) callback(act);
        }
        onClosed: { focus = false; list.focus = true; }
    }
}
