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
import QtQuick.Window 2.2


// Pegasus X: edit the details, art and files of collections and games, saved
// in Pegasus metadata files so every theme shows them. Where an edit goes:
// your own metadata file in place, an ES-DE import (marked, so a rescan keeps
// it), or for a game Pegasus found by itself, a new entry in Pegasus X's own
// file (see EsdeProvider.h, save_game_metadata).
FocusScope {
    id: root

    signal close

    anchors.fill: parent
    enabled: focus
    visible: 0 < (x + width) && x < Window.window.width

    property string page: "collections"  // collections, collection, game
    property string message: ""
    property bool anySaved: false        // the library is read again on the way out

    property string collName: ""
    property var collObj: null
    property var games: []               // [{ title, path, key, source, obj }]
    property var game: null              // the open game, one of `games`
    property var loc: ({})               // where it's saved: { file, isImport, isNew }

    // the open game's or collection's values: as found, saved this time, not saved yet
    property var orig: ({})
    property var saved: ({})
    property var edits: ({})
    readonly property bool dirty: Object.keys(edits).length > 0
    property var savedTitles: ({})       // game path -> title saved this time
    property var genres: []              // every genre in the library, for quick picks

    Component.onCompleted: list.currentIndex = 1  // the first collection, below the title

    // -- What can be edited ------------------------------------------------------

    readonly property var gameFields: [
        { "key": "title", "label": qsTr("Title") + api.tr, "type": "text" },
        { "key": "sort-by", "label": qsTr("Sort title") + api.tr, "type": "text", "empty": qsTr("same as the title") + api.tr },
        { "key": "developer", "label": qsTr("Developer") + api.tr, "type": "list" },
        { "key": "publisher", "label": qsTr("Publisher") + api.tr, "type": "list" },
        { "key": "genre", "label": qsTr("Genres") + api.tr, "type": "list", "picks": true },
        { "key": "release", "label": qsTr("Release date") + api.tr, "type": "date" },
        { "key": "players", "label": qsTr("Players") + api.tr, "type": "number" },
        { "key": "rating", "label": qsTr("Rating") + api.tr, "type": "rating" },
        { "key": "summary", "label": qsTr("Summary") + api.tr, "type": "multi" },
        { "key": "description", "label": qsTr("Description") + api.tr, "type": "multi" },
    ]
    readonly property var collectionFields: [
        { "key": "name", "label": qsTr("Name") + api.tr, "type": "text" },
        { "key": "shortname", "label": qsTr("Short name") + api.tr, "type": "text" },
        { "key": "summary", "label": qsTr("Summary") + api.tr, "type": "multi" },
        { "key": "description", "label": qsTr("Description") + api.tr, "type": "multi" },
    ]
    // metadata key, label, Assets property
    readonly property var artKinds: [
        ["assets.box_front", qsTr("Box front") + api.tr, "boxFront"],
        ["assets.box_back", qsTr("Box back") + api.tr, "boxBack"],
        ["assets.box_full", qsTr("Full box") + api.tr, "boxFull"],
        ["assets.cartridge", qsTr("Cartridge / disc") + api.tr, "cartridge"],
        ["assets.logo", qsTr("Logo") + api.tr, "logo"],
        ["assets.marquee", qsTr("Marquee") + api.tr, "marquee"],
        ["assets.screenshot", qsTr("Screenshot") + api.tr, "screenshot"],
        ["assets.titlescreen", qsTr("Title screen") + api.tr, "titlescreen"],
        ["assets.background", qsTr("Background") + api.tr, "background"],
        ["assets.banner", qsTr("Banner") + api.tr, "banner"],
        ["assets.tile", qsTr("Tile") + api.tr, "tile"],
        ["assets.poster", qsTr("Poster") + api.tr, "poster"],
        ["assets.video", qsTr("Video") + api.tr, "video"],
        ["x-manual", qsTr("Manual") + api.tr, ""],
    ]
    function artFilter(key) {
        if (key === "assets.video")
            return [".mp4", ".webm", ".mkv", ".avi", ".mov"];
        if (key === "x-manual")
            return [".pdf", ".cbz", ".cbr", ".jpg", ".png"];
        return [".png", ".jpg", ".jpeg", ".webp", ".gif", ".bmp", ".svg"];
    }

    // -- Values --------------------------------------------------------------------

    function current(key) {
        if (key in edits) return edits[key];
        if (key in saved) return saved[key];
        return orig[key] !== undefined ? orig[key] : "";
    }
    function same(a, b) {
        return JSON.stringify(a) === JSON.stringify(b);
    }
    function setEdit(key, value) {
        var next = Object.assign({}, edits);
        var base = (key in saved) ? saved[key] : (orig[key] !== undefined ? orig[key] : "");
        if (same(value, base))
            delete next[key];
        else
            next[key] = value;
        keepPosition(function() { edits = next; });
    }
    function undoEdit(key) {
        if (!(key in edits))
            return false;
        var next = Object.assign({}, edits);
        delete next[key];
        keepPosition(function() { edits = next; });
        return true;
    }
    function shown(field, value) {
        if (Array.isArray(value))
            return value.join(", ");
        var text = String(value || "");
        if (field && field.type === "multi")
            text = text.split("\n")[0];
        return text;
    }
    function localPath(url) {
        var s = String(url || "");
        if (s.indexOf("file://") === 0)
            s = decodeURIComponent(s.substring(7));
        return s;
    }
    function fileName(path) {
        var s = String(path || "");
        return s.substring(s.lastIndexOf("/") + 1);
    }
    function sourceLabel(source) {
        if (source === "user") return qsTr("Game directory") + api.tr;
        if (source === "esde") return qsTr("ES-DE") + api.tr;
        return qsTr("Pegasus") + api.tr;
    }
    function byName(a, b) {
        var x = String(a).toLowerCase(), y = String(b).toLowerCase();
        return x < y ? -1 : (x > y ? 1 : 0);
    }

    function readGame(g) {
        var o = {};
        o["title"] = g.title;
        o["sort-by"] = (g.sortBy && g.sortBy !== g.title) ? g.sortBy : "";
        o["developer"] = (g.developerList || []).slice();
        o["publisher"] = (g.publisherList || []).slice();
        o["genre"] = (g.genreList || []).filter(function(x) {
            var l = String(x).toLowerCase();
            return l !== "application" && l !== "emulator";
        });
        o["release"] = (g.release && !isNaN(g.release.getTime())) ? Qt.formatDate(g.release, "yyyy-MM-dd") : "";
        o["players"] = g.players > 1 ? String(g.players) : "";
        o["rating"] = g.rating > 0 ? Math.round(g.rating * 100) + "%" : "";
        o["summary"] = g.summary || "";
        o["description"] = g.description || "";
        for (var i = 0; i < artKinds.length; i++) {
            var value = "";
            if (artKinds[i][2] !== "")
                value = g.assets ? g.assets[artKinds[i][2]] : "";
            else if (g.extra && g.extra.manual)
                value = Array.isArray(g.extra.manual) ? g.extra.manual[0] : g.extra.manual;
            o[artKinds[i][0]] = localPath(value);
        }
        return o;
    }
    function readCollection(c) {
        return {
            "name": c.name,
            "shortname": c.shortName || "",
            "summary": c.summary || "",
            "description": c.description || "",
        };
    }

    // -- Pages -----------------------------------------------------------------------

    readonly property var collections: {
        var out = [];
        for (var i = 0; i < api.collections.count; i++) {
            var c = api.collections.get(i);
            out.push({ "name": c.name, "count": c.games.count, "obj": c });
        }
        out.sort(function(a, b) { return root.byName(a.name, b.name); });
        return out;
    }

    function openCollection(entry) {
        collName = entry.name;
        collObj = entry.obj;
        var list_games = Internal.settings.collectionGames(api.collections, entry.name);
        list_games.sort(function(a, b) { return root.byName(a.title, b.title); });
        games = list_games;
        savedTitles = {};
        orig = readCollection(entry.obj);
        saved = {};
        edits = {};
        loc = Internal.settings.metadataLocation(entry.name, "");
        message = "";
        page = "collection";
        list.currentIndex = 1;
        list.positionViewAtBeginning();
    }
    function openGame(entry) {
        game = entry;
        orig = readGame(entry.obj);
        saved = {};
        edits = {};
        loc = Internal.settings.metadataLocation(collName, entry.path || entry.key);
        message = "";
        page = "game";
        list.currentIndex = 1;
        list.positionViewAtBeginning();
    }
    // back one page; with unsaved changes, asks first
    function back(force) {
        if (dirty && !force) {
            chooser.openFor(qsTr("Save your changes to \"%1\"?").arg(page === "game" ? current("title") : collName) + api.tr, [
                { "act": "save", "text": qsTr("Save") + api.tr },
                { "act": "discard", "text": qsTr("Don't save") + api.tr },
                { "act": "stay", "text": qsTr("Keep editing") + api.tr },
            ], function(act) {
                if (act === "save") {
                    if (root.save())
                        root.back(true);
                }
                else if (act === "discard") {
                    root.back(true);
                }
            });
            return;
        }
        edits = {};
        message = "";
        if (page === "game") {
            var path = game ? game.path : "";
            page = "collection";
            orig = readCollection(collObj);
            saved = {};
            loc = Internal.settings.metadataLocation(collName, "");
            for (var i = 0; i < list.rows.length; i++) {
                if (list.rows[i].kind === "game" && list.rows[i].game.path === path) {
                    list.currentIndex = i;
                    list.positionViewAtIndex(i, ListView.Center);
                    return;
                }
            }
            return;
        }
        if (page === "collection") {
            var name = collName;
            page = "collections";
            collName = "";
            for (var j = 0; j < list.rows.length; j++) {
                if (list.rows[j].text === name) {
                    list.currentIndex = j;
                    list.positionViewAtIndex(j, ListView.Center);
                    return;
                }
            }
            list.currentIndex = 0;
            return;
        }
        if (anySaved)
            Internal.settings.reloadLibrary();
        root.close();
    }

    function save() {
        if (!dirty)
            return true;
        var error = page === "game"
            ? Internal.settings.saveGameMetadata(collName, game.path || game.key, current("title"), edits)
            : Internal.settings.saveCollectionMetadata(collName, edits);
        if (error !== "") {
            message = error;
            return false;
        }
        var merged = Object.assign({}, saved);
        for (var key in edits)
            merged[key] = edits[key];
        var where = loc.file || "";
        keepPosition(function() {
            saved = merged;
            edits = {};
            if (page === "game") {
                var titles = Object.assign({}, savedTitles);
                titles[game.path] = root.current("title");
                savedTitles = titles;
            }
            else if ("name" in merged) {
                collName = merged["name"];
            }
        });
        anySaved = true;
        message = qsTr("Saved in %1. Every theme shows it once you leave Edit metadata.").arg(where.split("/").slice(-2).join("/")) + api.tr;
        loc = Internal.settings.metadataLocation(collName, page === "game" ? (game.path || game.key) : "");
        return true;
    }

    // keeps the selected row when the rows are rebuilt
    function keepPosition(change) {
        var index = list.currentIndex;
        var y = list.contentY;
        change();
        list.currentIndex = index;
        list.contentY = y;
    }

    Keys.onPressed: {
        if (event.isAutoRepeat)
            return;
        if (api.keys.isCancel(event)) {
            event.accepted = true;
            root.back(false);
        }
    }

    PegasusUtils.HorizontalSwipeArea {
        anchors.fill: parent
        onSwipeRight: root.back(false)
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: root.back(false)
    }

    ScreenHeader {
        id: header
        text: (page === "collections" ? qsTr("Settings > Edit metadata")
             : page === "collection" ? qsTr("Edit metadata > %1").arg(root.collName)
             : qsTr("%1 > %2").arg(root.collName).arg(root.current("title"))) + api.tr
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
            if (root.page === "collections") {
                out.push({ "kind": "section", "text": qsTr("Collections") + api.tr });
                for (i = 0; i < root.collections.length; i++) {
                    var c = root.collections[i];
                    out.push({ "kind": "coll", "text": c.name, "value": qsTr("%1 games").arg(c.count) + api.tr, "coll": c });
                }
                return out;
            }

            var fields = root.page === "game" ? root.gameFields : root.collectionFields;
            out.push({ "kind": "section", "text": (root.page === "game" ? qsTr("Details") : qsTr("Collection")) + api.tr });
            for (i = 0; i < fields.length; i++) {
                var f = fields[i];
                var value = root.current(f.key);
                var text = root.shown(f, value);
                out.push({ "kind": "field", "text": f.label, "field": f,
                           "value": text !== "" ? text : (f.empty || "—"),
                           "muted": text === "", "edited": f.key in root.edits, "done": f.key in root.saved });
            }

            if (root.page === "game") {
                out.push({ "kind": "section", "text": qsTr("Art and manual") + api.tr });
                for (i = 0; i < root.artKinds.length; i++) {
                    var kind = root.artKinds[i];
                    var art = root.current(kind[0]);
                    if (art === "" && !(kind[0] in root.edits))
                        continue;
                    var isIcon = String(art).indexOf("image://") === 0;
                    out.push({ "kind": "art", "text": kind[1], "key": kind[0], "icon": isIcon,
                               "value": art === "" ? qsTr("removed") + api.tr : isIcon ? qsTr("the app's icon") + api.tr : root.fileName(art),
                               "muted": art === "" || isIcon, "edited": kind[0] in root.edits, "done": kind[0] in root.saved });
                }
                out.push({ "kind": "addart", "text": qsTr("Add art or a manual...") + api.tr, "value": "" });
                out.push({ "kind": "info", "text": qsTr("Launch") + api.tr, "value": qsTr("set in Settings > Emulators") + api.tr });
            }

            var savedIn = root.loc.file ? root.loc.file.split("/").slice(-2).join("/") : "";
            if (root.loc.isNew)
                savedIn = qsTr("a new entry of yours (%1)").arg(savedIn) + api.tr;
            else if (root.loc.isImport)
                savedIn = savedIn + qsTr(" · kept when you rescan") + api.tr;
            out.push({ "kind": "info", "text": qsTr("Saved in") + api.tr, "value": savedIn });
            out.push({ "kind": "save", "text": root.dirty ? qsTr("Save changes") + api.tr : qsTr("No changes to save") + api.tr, "value": "" });

            if (root.page === "collection") {
                out.push({ "kind": "section", "text": qsTr("Games · %1").arg(root.games.length) + api.tr });
                for (i = 0; i < root.games.length; i++) {
                    var g = root.games[i];
                    var title = root.savedTitles[g.path] || g.title;
                    out.push({ "kind": "game", "text": title, "game": g,
                               "value": (root.savedTitles[g.path] !== undefined ? qsTr("edited · ") + api.tr : "") + root.sourceLabel(g.source),
                               "done": root.savedTitles[g.path] !== undefined });
                }
            }
            return out;
        }
        model: rows

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
        onModelChanged: {
            if (!selectable(currentIndex)) {
                var i = currentIndex + 1;
                while (i < count && !selectable(i))
                    i++;
                if (i < count)
                    currentIndex = i;
            }
        }

        function activate(index) {
            if (!selectable(index))
                return;
            var row = rows[index];
            if (row.kind === "coll")
                root.openCollection(row.coll);
            else if (row.kind === "game")
                root.openGame(row.game);
            else if (row.kind === "field")
                fieldDialog.openFor(row.field);
            else if (row.kind === "art")
                root.pickArt(row.key);
            else if (row.kind === "addart")
                root.addArt();
            else if (row.kind === "save") {
                if (root.dirty)
                    root.save();
            }
        }
        // X: undo a field's change, or remove a picture
        function secondary(index) {
            if (!selectable(index))
                return;
            var row = rows[index];
            if (row.kind === "field") {
                if (row.field.key && root.undoEdit(row.field.key))
                    root.message = qsTr("Undone.") + api.tr;
            }
            else if (row.kind === "art" && !row.icon) {
                if (!root.undoEdit(row.key))
                    root.setEdit(row.key, "");
            }
        }

        Keys.onPressed: {
            if (event.isAutoRepeat)
                return;
            if (api.keys.isAccept(event)) {
                event.accepted = true;
                activate(currentIndex);
            }
            else if (api.keys.isDetails(event)) {
                event.accepted = true;
                secondary(currentIndex);
            }
        }

        delegate: Item {
            readonly property bool isSection: modelData.kind === "section"
            readonly property bool isInfo: modelData.kind === "info"
            readonly property bool isSave: modelData.kind === "save"
            readonly property bool highlighted: !isSection && !isInfo
                && ((ListView.isCurrentItem && ListView.view.activeFocus) || rowMouse.containsMouse)

            width: ListView.view.width
            height: isSection ? sectionText.height : (isInfo ? vpx(44) : isSave ? vpx(64) : vpx(22) * 2.4)

            Text {
                id: sectionText
                visible: parent.isSection
                text: modelData.text
                color: "#bbb"
                font.family: globalFonts.sans
                font.pixelSize: vpx(22)
                topPadding: font.pixelSize * (index === 0 ? 0.75 : 1.5)
                bottomPadding: font.pixelSize * 0.25
            }

            // the Save button
            Rectangle {
                visible: parent.isSave
                anchors.centerIn: parent
                width: Math.min(parent.width * 0.5, vpx(360))
                height: vpx(48)
                radius: vpx(6)
                color: parent.highlighted ? "#3aa" : (root.dirty ? "#2a5f5f" : "#333")
                Text {
                    anchors.centerIn: parent
                    text: modelData.text
                    color: parent.parent.highlighted ? "#111" : (root.dirty ? "#eee" : "#888")
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(20)
                    font.bold: root.dirty
                }
            }

            Rectangle {
                width: parent.width
                height: vpx(3)
                anchors.bottom: parent.bottom
                color: "#3aa"
                visible: parent.highlighted && !parent.isSave
            }
            Text {
                id: rowLabel
                visible: !parent.isSection && !parent.isSave
                text: modelData.text
                color: parent.isInfo ? "#999" : "#eee"
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(21)
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width * 0.4
                elide: Text.ElideRight
            }
            Text {
                visible: !parent.isSection && !parent.isSave
                text: modelData.value || ""
                color: parent.isInfo ? "#999"
                     : modelData.edited ? "#5cc"
                     : modelData.done ? "#9d9"
                     : modelData.muted ? "#888" : "#c0c0c0"
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(21)
                anchors.left: rowLabel.right
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

    Text {
        id: messageBar
        text: root.message !== "" ? root.message
            : root.page === "collections" ? qsTr("Pick a collection to edit it or one of its games.") + api.tr
            : root.page === "collection" ? qsTr("A edit · X undo a change · B back") + api.tr
            : qsTr("A edit · X undo a change or remove a picture · B back") + api.tr
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


    // -- Art -------------------------------------------------------------------------

    property string artKey: ""
    function pickArt(key) {
        artKey = key;
        var currentFile = current(key);
        var startDir = "";
        if (currentFile && String(currentFile).indexOf("/") === 0)
            startDir = String(currentFile).substring(0, String(currentFile).lastIndexOf("/"));
        else if (game && game.path && String(game.path).indexOf("/") === 0)
            startDir = String(game.path).substring(0, String(game.path).lastIndexOf("/"));
        filePicker.fileExtensions = artFilter(key);
        filePicker.openAt(startDir);
        filePicker.focus = true;
    }
    function addArt() {
        var entries = [];
        for (var i = 0; i < artKinds.length; i++) {
            var value = current(artKinds[i][0]);
            if (value === "" && !(artKinds[i][0] in edits))
                entries.push({ "act": artKinds[i][0], "text": artKinds[i][1] });
        }
        chooser.openFor(qsTr("What kind?") + api.tr, entries, function(act) { root.pickArt(act); });
    }

    FilePicker {
        id: filePicker

        fileNames: []
        fileExtensions: [".png", ".jpg"]
        infoText: qsTr("Pick the file. It's written relative to the metadata file when it's in the same folder.") + api.tr

        enabled: focus
        visible: opacity > 0.01
        opacity: focus ? 1.0 : 0.0
        Behavior on opacity { PropertyAnimation { duration: 300 } }

        onCancel: list.focus = true
        onPickFile: {
            root.setEdit(root.artKey, file_path);
            list.focus = true;
        }
    }


    // -- One choice from a short list: unsaved changes, kind of art -----------------

    FocusScope {
        id: chooser

        property string title: ""
        property var entries: []
        property var callback: null

        anchors.fill: parent
        enabled: focus
        visible: focus
        z: 5

        function openFor(text, list_entries, cb) {
            title = text;
            entries = list_entries;
            callback = cb;
            chooserList.currentIndex = 0;
            focus = true;
        }
        function done(index) {
            var cb = callback;
            var e = entries[index];
            list.focus = true;
            if (cb && e)
                cb(e.act);
        }

        Keys.onPressed: {
            if (event.isAutoRepeat)
                return;
            if (api.keys.isCancel(event)) {
                event.accepted = true;
                list.focus = true;
            }
            else if (api.keys.isAccept(event)) {
                event.accepted = true;
                done(chooserList.currentIndex);
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.5
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: list.focus = true
            }
        }
        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.5, vpx(560))

            Rectangle {
                width: parent.width
                height: chooserTitle.height
                color: "#444"
                Text {
                    id: chooserTitle
                    width: parent.width
                    text: chooser.title
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(18)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    padding: font.pixelSize
                }
            }
            Rectangle {
                width: parent.width
                height: Math.min(chooserList.count * vpx(48), root.height * 0.5)
                color: "#333"
                ListView {
                    id: chooserList
                    anchors.fill: parent
                    clip: true
                    focus: true
                    keyNavigationWraps: true
                    model: chooser.entries
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: vpx(48)
                        color: ListView.isCurrentItem ? "#585858" : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: modelData.text
                            color: "#eee"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(18)
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: chooser.done(index)
                        }
                    }
                }
            }
        }
    }


    // -- Editing one field ----------------------------------------------------------

    FocusScope {
        id: fieldDialog

        property var field: null
        readonly property bool multiLine: !!field && (field.type === "multi" || field.type === "list")

        anchors.fill: parent
        enabled: focus
        visible: focus
        z: 4

        function openFor(f) {
            field = f;
            var value = root.current(f.key);
            var text = Array.isArray(value) ? value.join("\n") : String(value || "");
            if (f.type === "rating")
                text = text.replace("%", "");
            hint.text = "";
            if (multiLine) {
                multiInput.text = text;
                multiInput.cursorPosition = text.length;
            }
            else {
                lineInput.text = text;
                lineInput.cursorPosition = text.length;
            }
            if (f.picks && root.genres.length === 0)
                root.genres = Internal.settings.libraryGenres(api.allGames);
            focus = true;
            (multiLine ? multiBox : lineBox).focus = true;
        }
        function closeDialog() {
            Qt.inputMethod.hide();
            lineInput.focus = false;
            multiInput.focus = false;
            list.focus = true;
        }
        function finish() {
            var f = field;
            var raw = multiLine ? multiInput.text : lineInput.text;
            var value;
            if (f.type === "list") {
                value = raw.split("\n").map(function(x) { return x.trim(); }).filter(function(x) { return x !== ""; });
            }
            else if (f.type === "multi") {
                value = raw.replace(/\s+$/, "");
            }
            else {
                value = raw.trim();
                if (f.type === "number" && value !== "" && !/^\d+$/.test(value)) {
                    hint.text = qsTr("Type a number, eg. 2.") + api.tr;
                    return;
                }
                if (f.type === "rating" && value !== "") {
                    var n = parseInt(value, 10);
                    if (isNaN(n) || n < 0 || n > 100) {
                        hint.text = qsTr("Type a rating from 0 to 100.") + api.tr;
                        return;
                    }
                    value = n + "%";
                }
                if (f.type === "date" && value !== "" && !/^\d{4}(-\d{2}(-\d{2})?)?$/.test(value)) {
                    hint.text = qsTr("Type the date as YYYY-MM-DD (or just the year).") + api.tr;
                    return;
                }
                if ((f.key === "title" || f.key === "name") && value === "") {
                    hint.text = qsTr("The name can't be empty.") + api.tr;
                    return;
                }
            }
            closeDialog();
            root.setEdit(f.key, value);
        }
        function addPick(text) {
            var lines = multiInput.text.split("\n").map(function(x) { return x.trim(); }).filter(function(x) { return x !== ""; });
            for (var i = 0; i < lines.length; i++) {
                if (lines[i].toLowerCase() === text.toLowerCase())
                    return;
            }
            lines.push(text);
            multiInput.text = lines.join("\n");
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
            opacity: 0.45
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: fieldDialog.closeDialog()
            }
        }

        Rectangle {
            width: Math.min(parent.width * 0.8, vpx(900))
            height: dialogColumn.height + vpx(40)
            anchors.horizontalCenter: parent.horizontalCenter
            y: vpx(30)
            color: "#444"
            radius: vpx(8)

            MouseArea { anchors.fill: parent }

            Column {
                id: dialogColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(20)
                spacing: vpx(12)

                Text {
                    width: parent.width
                    text: fieldDialog.field
                        ? (fieldDialog.field.type === "list"
                            ? qsTr("%1, one per line").arg(fieldDialog.field.label)
                            : fieldDialog.field.type === "date" ? qsTr("%1 (YYYY-MM-DD)").arg(fieldDialog.field.label)
                            : fieldDialog.field.type === "rating" ? qsTr("%1, from 0 to 100").arg(fieldDialog.field.label)
                            : fieldDialog.field.label) + api.tr
                        : ""
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(19)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                // one line
                FocusScope {
                    id: lineBox
                    visible: !fieldDialog.multiLine
                    width: parent.width
                    height: vpx(52)
                    KeyNavigation.down: actionRow

                    Rectangle {
                        anchors.fill: parent
                        color: "#333"
                        border.color: lineBox.activeFocus ? "#3aa" : "#333"
                        border.width: vpx(2)
                    }
                    TextInput {
                        id: lineInput
                        focus: true
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(20)
                        clip: true
                        verticalAlignment: TextInput.AlignVCenter
                        anchors.fill: parent
                        anchors.leftMargin: vpx(14)
                        anchors.rightMargin: vpx(14)
                        inputMethodHints: fieldDialog.field && (fieldDialog.field.type === "number" || fieldDialog.field.type === "rating")
                            ? Qt.ImhDigitsOnly : Qt.ImhNone
                        onActiveFocusChanged: if (activeFocus) Qt.inputMethod.show()
                        Keys.onReturnPressed: fieldDialog.finish()
                        Keys.onEnterPressed: fieldDialog.finish()
                        KeyNavigation.down: actionRow
                    }
                }

                // several lines (lists: one item each)
                FocusScope {
                    id: multiBox
                    visible: fieldDialog.multiLine
                    width: parent.width
                    height: Math.min(Math.max(vpx(150), multiInput.contentHeight + vpx(24)), root.height * 0.42)

                    Rectangle {
                        anchors.fill: parent
                        color: "#333"
                        border.color: multiBox.activeFocus ? "#3aa" : "#333"
                        border.width: vpx(2)
                    }
                    Flickable {
                        id: multiFlick
                        anchors.fill: parent
                        anchors.margins: vpx(12)
                        clip: true
                        contentHeight: multiInput.contentHeight
                        contentWidth: width
                        function ensureVisible(r) {
                            if (contentY >= r.y)
                                contentY = r.y;
                            else if (contentY + height <= r.y + r.height)
                                contentY = r.y + r.height - height;
                        }
                        TextEdit {
                            id: multiInput
                            focus: true
                            width: multiFlick.width
                            color: "#eee"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(18)
                            wrapMode: TextEdit.Wrap
                            onCursorRectangleChanged: multiFlick.ensureVisible(cursorRectangle)
                            onActiveFocusChanged: if (activeFocus) Qt.inputMethod.show()
                            // the d-pad leaves the box from its last line
                            Keys.onDownPressed: {
                                if (cursorRectangle.y + cursorRectangle.height >= contentHeight - 2) {
                                    (fieldDialog.field && fieldDialog.field.picks && root.genres.length > 0 ? pickRow : actionRow).focus = true;
                                    event.accepted = true;
                                }
                                else {
                                    event.accepted = false;
                                }
                            }
                        }
                    }
                }

                // quick picks: genres already in the library
                ListView {
                    id: pickRow
                    visible: !!fieldDialog.field && !!fieldDialog.field.picks && root.genres.length > 0
                    width: parent.width
                    height: visible ? vpx(36) : 0
                    orientation: ListView.Horizontal
                    spacing: vpx(8)
                    clip: true
                    model: visible ? root.genres : []
                    KeyNavigation.up: multiBox
                    KeyNavigation.down: actionRow
                    Keys.onPressed: {
                        if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                            event.accepted = true;
                            fieldDialog.addPick(model[currentIndex]);
                        }
                    }
                    delegate: Rectangle {
                        readonly property bool highlighted: ListView.view.activeFocus && ListView.isCurrentItem
                        width: pickText.implicitWidth + vpx(20)
                        height: ListView.view.height
                        radius: vpx(4)
                        color: highlighted ? "#3aa" : "#333"
                        border.color: "#3aa"
                        border.width: highlighted ? 0 : vpx(1)
                        Text {
                            id: pickText
                            text: modelData
                            color: parent.highlighted ? "#111" : "#9dd"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(15)
                            anchors.centerIn: parent
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: fieldDialog.addPick(modelData)
                        }
                    }
                }

                Text {
                    id: hint
                    width: parent.width
                    visible: text !== ""
                    color: "#e8b06a"
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
                        { "name": "done", "text": qsTr("Done") + api.tr },
                        { "name": "cancel", "text": qsTr("Cancel") + api.tr },
                        { "name": "clear", "text": qsTr("Clear") + api.tr },
                    ]
                    KeyNavigation.up: fieldDialog.multiLine
                        ? (pickRow.visible ? pickRow : multiBox) : lineBox

                    Keys.onPressed: {
                        if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                            event.accepted = true;
                            var name = model[currentIndex].name;
                            if (name === "done")
                                fieldDialog.finish();
                            else if (name === "cancel")
                                fieldDialog.closeDialog();
                            else {
                                lineInput.text = "";
                                multiInput.text = "";
                            }
                        }
                    }

                    delegate: Rectangle {
                        readonly property bool highlighted: ListView.view.activeFocus && ListView.isCurrentItem
                        width: actionText.implicitWidth + vpx(40)
                        height: ListView.view.height
                        radius: vpx(6)
                        color: highlighted ? "#3aa" : (modelData.name === "done" ? "#2a5f5f" : "#333")
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
                            onClicked: {
                                if (modelData.name === "done")
                                    fieldDialog.finish();
                                else if (modelData.name === "cancel")
                                    fieldDialog.closeDialog();
                                else {
                                    lineInput.text = "";
                                    multiInput.text = "";
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
