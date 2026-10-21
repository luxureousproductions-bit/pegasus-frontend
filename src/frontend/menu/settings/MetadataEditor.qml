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
        { "key": "developer", "label": qsTr("Developer") + api.tr, "type": "list" },
        { "key": "publisher", "label": qsTr("Publisher") + api.tr, "type": "list" },
        { "key": "genre", "label": qsTr("Genres") + api.tr, "type": "list", "picks": true },
        { "key": "description", "label": qsTr("Description") + api.tr, "type": "multi" },
        { "key": "launch", "label": qsTr("Launch command") + api.tr, "type": "command" },
        { "key": "release", "label": qsTr("Release date") + api.tr, "type": "date" },
        { "key": "players", "label": qsTr("Players") + api.tr, "type": "number" },
        { "key": "rating", "label": qsTr("Rating") + api.tr, "type": "rating" },
    ]
    readonly property var collectionFields: [
        { "key": "name", "label": qsTr("Name") + api.tr, "type": "text" },
        { "key": "shortname", "label": qsTr("Short name") + api.tr, "type": "text" },
        { "key": "extensions", "label": qsTr("Extensions") + api.tr, "type": "text" },
        { "key": "launch", "label": qsTr("Launch command") + api.tr, "type": "command" },
    ]
    property var collDetails: ({})   // the open collection's own lines (launch, extensions, default)
    property var gameCommands: ({})  // game path -> its own launch command
    // the command a game uses when it has none of its own
    readonly property string collectionLaunch: {
        var own = (page === "collection" && ("launch" in edits)) ? edits["launch"]
                : ("launch" in savedColl) ? savedColl["launch"] : (collDetails.launch || "");
        return own !== "" ? own : (collDetails.defaultCommand || "");
    }
    property var savedColl: ({})     // the collection's values saved this time
    // The names XboxOSv2's media carousel uses: editor key, label, the
    // Assets list it's read from, the metadata key it's written to, and for
    // the two boxes sharing one Pegasus slot, which of them ("3d" / "2d").
    // The files can sit in any folder Pegasus X knows (ES-DE, Skraper, ...).
    readonly property var artKinds: [
        ["art:video", qsTr("Videos") + api.tr, "videoList", "assets.video", ""],
        ["art:screenshot", qsTr("Screenshot") + api.tr, "screenshotList", "assets.screenshot", ""],
        ["art:titlescreen", qsTr("Title Screen") + api.tr, "titlescreenList", "assets.titlescreen", ""],
        ["art:fanart", qsTr("Fanart") + api.tr, "backgroundList", "assets.background", ""],
        ["art:box3d", qsTr("3D Box") + api.tr, "boxFrontList", "assets.box_front", "3d"],
        ["art:box2d", qsTr("2D Box") + api.tr, "boxFrontList", "assets.box_front", "2d"],
        ["art:boxback", qsTr("Back Box") + api.tr, "boxBackList", "assets.box_back", ""],
        ["art:physical", qsTr("Physical Media") + api.tr, "cartridgeList", "assets.cartridge", ""],
        ["art:miximage", qsTr("Miximage") + api.tr, "steamList", "assets.steam", ""],
        ["art:logo", qsTr("Logo") + api.tr, "logoList", "assets.logo", ""],
        ["art:manual", qsTr("Manual") + api.tr, "", "x-manual", ""],
    ]
    function is3d(path) {
        return /box3d|box_3d|3dbox/i.test(String(path));
    }
    property var origSlots: ({})  // metadata key -> every file the game has there now
    function artFilter(key) {
        if (key === "art:video")
            return [".mp4", ".webm", ".mkv", ".avi", ".mov"];
        if (key === "art:manual")
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
        o["developer"] = (g.developerList || []).slice();
        o["publisher"] = (g.publisherList || []).slice();
        o["genre"] = (g.genreList || []).filter(function(x) {
            var l = String(x).toLowerCase();
            return l !== "application" && l !== "emulator";
        });
        o["release"] = (g.release && !isNaN(g.release.getTime())) ? Qt.formatDate(g.release, "yyyy-MM-dd") : "";
        o["players"] = g.players > 1 ? String(g.players) : "";
        o["rating"] = g.rating > 0 ? Math.round(g.rating * 100) + "%" : "";
        // one text: the description, else the summary
        o["description"] = g.description || g.summary || "";
        o["launch"] = gameCommands[game.path] || "";
        var slots = {};
        for (var i = 0; i < artKinds.length; i++) {
            var kind = artKinds[i];
            var files = [];
            if (kind[2] !== "")
                files = (g.assets && g.assets[kind[2]]) ? Array.prototype.slice.call(g.assets[kind[2]]) : [];
            else if (g.extra && g.extra.manual)
                files = Array.isArray(g.extra.manual) ? g.extra.manual.slice() : [g.extra.manual];
            files = files.map(function(f) { return root.localPath(f); });
            slots[kind[3]] = files;
            // the one shown: for the boxes, the 3D or the 2D one
            var shownFile = "";
            for (var j = 0; j < files.length; j++) {
                if (kind[4] === "3d" && !is3d(files[j])) continue;
                if (kind[4] === "2d" && is3d(files[j])) continue;
                shownFile = files[j];
                break;
            }
            o[kind[0]] = shownFile;
        }
        origSlots = slots;
        return o;
    }
    function readCollection(c) {
        return {
            "name": c.name,
            "shortname": collDetails.shortname || c.shortName || "",
            "extensions": collDetails.extensions || "",
            "launch": collDetails.launch || "",
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
        savedColl = {};
        collDetails = Internal.settings.collectionDetails(entry.name);
        gameCommands = Internal.settings.gameEmulators(entry.name);
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
            saved = savedColl;
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

    // edits -> what's written: art goes back into its slot's whole list,
    // replacing only the file that was shown (the others, eg. more
    // screenshots, stay)
    function toChanges() {
        var out = {};
        var slots = {};
        for (var key in edits) {
            if (key.indexOf("art:") !== 0) {
                out[key] = edits[key];
                continue;
            }
            var kind = null;
            for (var i = 0; i < artKinds.length; i++) {
                if (artKinds[i][0] === key)
                    kind = artKinds[i];
            }
            if (!kind)
                continue;
            var slot = kind[3];
            if (!(slot in slots))
                slots[slot] = (origSlots[slot] || []).slice();
            var list = slots[slot];
            var before = (key in saved) ? saved[key] : orig[key];
            var at = before ? list.indexOf(before) : -1;
            var value = edits[key];
            if (at >= 0) {
                if (value === "")
                    list.splice(at, 1);
                else
                    list[at] = value;
            }
            else if (value !== "") {
                if (kind[4] === "3d")
                    list.push(value);      // the 2D box stays the main one
                else
                    list.unshift(value);
            }
        }
        for (var s in slots)
            out[s] = slots[s].filter(function(f) { return f && String(f).indexOf("image://") !== 0; });
        // one text: a new description replaces the summary too
        if ("description" in out)
            out["summary"] = "";
        return out;
    }

    function save() {
        if (!dirty)
            return true;
        var error = page === "game"
            ? Internal.settings.saveGameMetadata(collName, game.path || game.key, current("title"), toChanges())
            : Internal.settings.saveCollectionMetadata(collName, edits);
        if (error !== "") {
            message = error;
            return false;
        }
        var written = toChanges();
        var slotsNow = Object.assign({}, origSlots);
        for (var w in written) {
            if (w.indexOf("assets.") === 0 || w === "x-manual")
                slotsNow[w] = written[w];
        }
        origSlots = slotsNow;
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
                if ("launch" in merged) {
                    var cmds = Object.assign({}, gameCommands);
                    cmds[game.path] = merged["launch"];
                    gameCommands = cmds;
                    // the game uses it right away
                    Internal.settings.setLiveLaunchCommand(game.obj, merged["launch"] !== "" ? merged["launch"] : root.collectionLaunch);
                }
            }
            else {
                savedColl = merged;
                if ("name" in merged)
                    collName = merged["name"];
                if ("launch" in merged) {
                    // the collection's games without their own use it right away
                    for (var gi = 0; gi < games.length; gi++) {
                        if (!gameCommands[games[gi].path])
                            Internal.settings.setLiveLaunchCommand(games[gi].obj, root.collectionLaunch);
                    }
                }
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
                var empty = f.empty || "—";
                if (f.key === "launch") {
                    empty = root.page === "game"
                        ? qsTr("the collection's") + api.tr
                        : (root.collDetails.defaultLabel
                            ? qsTr("recommended: %1").arg(root.collDetails.defaultLabel) + api.tr
                            : qsTr("none, pick one in Emulators") + api.tr);
                }
                out.push({ "kind": "field", "text": f.label, "field": f,
                           "value": text !== "" ? text : empty,
                           "muted": text === "", "edited": f.key in root.edits, "done": f.key in root.saved });
                // the game's file, below its title
                if (root.page === "game" && f.key === "title")
                    out.push({ "kind": "info", "text": qsTr("File") + api.tr, "value": root.fileShown() });
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
    // the folder names each kind of art can have (ES-DE, Skraper, Pegasus...)
    readonly property var artFolders: ({
        "art:video": ["videos", "video"],
        "art:screenshot": ["screenshots", "screenshot", "snap", "snaps"],
        "art:titlescreen": ["titlescreens", "screenshottitle", "titlescreen"],
        "art:fanart": ["fanart", "fanarts", "background", "backgrounds"],
        "art:box3d": ["3dboxes", "box3d", "3dbox", "box3d-front"],
        "art:box2d": ["covers", "box2dfront", "box2d", "boxfront"],
        "art:boxback": ["backcovers", "box2dback", "boxback"],
        "art:physical": ["physicalmedia", "support", "cartridge", "cartridges", "disc"],
        "art:miximage": ["miximages", "miximage"],
        "art:logo": ["wheel", "marquees", "marquee", "logos", "logo"],
        "art:manual": ["manuals", "manual"],
    })
    // "./game.steam": the game's file, from the folder of its metadata file
    function fileShown() {
        var path = game ? String(game.path || game.key || "") : "";
        if (path.indexOf("/") !== 0)
            return path;
        var base = loc.file ? dirOf(loc.file) : "";
        if (base !== "" && path.indexOf(base + "/") === 0)
            return "./" + path.substring(base.length + 1);
        return path;
    }
    function dirOf(path) {
        var s = String(path || "");
        return s.substring(0, s.lastIndexOf("/"));
    }
    function pickArt(key) {
        artKey = key;
        var currentFile = current(key);
        var startDir = "";
        if (currentFile && String(currentFile).indexOf("/") === 0) {
            startDir = dirOf(currentFile);
        }
        else {
            // that kind's folder, by any of its names, next to the game's other
            // art (eg. ES-DE's downloaded_media/<system>/) or in its ROM folder
            var roots = [];
            for (var slot in origSlots) {
                var files = origSlots[slot] || [];
                for (var i = 0; i < files.length; i++) {
                    if (String(files[i]).indexOf("/") === 0) {
                        var r = dirOf(dirOf(files[i]));
                        if (roots.indexOf(r) < 0)
                            roots.push(r);
                    }
                }
            }
            var gameDir = (game && game.path && String(game.path).indexOf("/") === 0) ? dirOf(game.path) : "";
            if (gameDir !== "") {
                roots.push(gameDir + "/media");
                roots.push(gameDir);
            }
            var candidates = [];
            var names = artFolders[key] || [];
            for (var ri = 0; ri < roots.length; ri++) {
                for (var ni = 0; ni < names.length; ni++)
                    candidates.push(roots[ri] + "/" + names[ni]);
            }
            startDir = Internal.settings.firstExistingDir(candidates) || gameDir;
        }
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
            // a launch command starts from the one in use, to change from there
            if (f.type === "command" && text === "")
                text = root.page === "game" ? root.collectionLaunch : (root.collDetails.defaultCommand || "");
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
            kb.target = multiLine ? multiInput : lineInput;
            kb.multiLine = multiLine;
            kb.row = 1;
            kb.col = 0;
            focus = true;
            kb.focus = true;
        }
        function closeDialog() {
            Qt.inputMethod.hide();
            lineInput.focus = false;
            multiInput.focus = false;
            kb.focus = false;
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
                // the same as what it uses anyway: nothing of its own
                if (f.type === "command") {
                    if (root.page === "game" && value === root.collectionLaunch)
                        value = "";
                    if (root.page === "collection" && (root.collDetails.launch || "") === "" && value === (root.collDetails.defaultCommand || ""))
                        value = "";
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
            y: vpx(14)
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
                    KeyNavigation.down: kb

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
                        cursorVisible: fieldDialog.visible
                        anchors.fill: parent
                        anchors.leftMargin: vpx(14)
                        anchors.rightMargin: vpx(14)
                        inputMethodHints: fieldDialog.field && (fieldDialog.field.type === "number" || fieldDialog.field.type === "rating")
                            ? Qt.ImhDigitsOnly : Qt.ImhNone
                        onActiveFocusChanged: if (activeFocus) Qt.inputMethod.show()
                        Keys.onReturnPressed: fieldDialog.finish()
                        Keys.onEnterPressed: fieldDialog.finish()
                        KeyNavigation.down: kb
                    }
                }

                // several lines (lists: one item each)
                FocusScope {
                    id: multiBox
                    visible: fieldDialog.multiLine
                    width: parent.width
                    height: Math.min(Math.max(vpx(110), multiInput.contentHeight + vpx(24)), root.height * 0.24)

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
                            cursorVisible: fieldDialog.visible
                            onCursorRectangleChanged: multiFlick.ensureVisible(cursorRectangle)
                            onActiveFocusChanged: if (activeFocus) Qt.inputMethod.show()
                            // the d-pad leaves the box from its last line
                            Keys.onDownPressed: {
                                if (cursorRectangle.y + cursorRectangle.height >= contentHeight - 2) {
                                    (fieldDialog.field && fieldDialog.field.picks && root.genres.length > 0 ? pickRow : kb).focus = true;
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
                    KeyNavigation.down: kb
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

                // Pegasus' own keyboard: d-pad and A, or touch
                OnScreenKeyboard {
                    id: kb
                    width: parent.width
                    onDone: fieldDialog.finish()
                    onLeaveUp: {
                        if (pickRow.visible)
                            pickRow.focus = true;
                    }
                    onLeaveDown: actionRow.focus = true
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
                    KeyNavigation.up: kb

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
