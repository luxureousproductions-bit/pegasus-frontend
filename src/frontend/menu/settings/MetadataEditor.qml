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

    property string page: "collections"  // collections, hidden, collection, game
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

    // Pegasus X: what the library looked like when Library opened; leaving with
    // the same (eg. hidden and shown again) reads nothing again
    property string libStart: ""
    property bool scrapedHere: false
    function libState() {
        return JSON.stringify({ "h": Internal.settings.hiddenItems(), "k": Internal.settings.androidKinds(),
                                "m": Internal.settings.mergeAndroid });
    }

    Component.onCompleted: {
        libStart = libState();
        Internal.settings.libraryOpen = true;  // a finished scrape waits for you to leave
        list.currentIndex = 1;  // the first option, below the title
        // the first time: offer to set up the data folder
        if (Internal.settings.dataDir === "" && !Internal.settings.dataDirAsked)
            askDataFolder.start();
    }
    Timer {
        id: askDataFolder
        interval: 300
        onTriggered: {
            Internal.settings.dataDirAsked = true;
            chooser.openFor(qsTr("Set up a Pegasus X data folder? It's where art you pick (and later, scraped art) is kept, in a folder you name, anywhere you like.") + api.tr, [
                { "act": "now", "text": qsTr("Set it up now") + api.tr },
                { "act": "later", "text": qsTr("Later (Library > Data folder)") + api.tr },
            ], function(act) { if (act === "now") root.pickDataFolder(); });
        }
    }

    // -- Library: hidden games and collections, Android sections -------------------

    property var hidden: Internal.settings.hiddenItems()   // { collections, games }
    property var kinds: Internal.settings.androidKinds()   // game key -> moved section
    property bool mergeAndroid: Internal.settings.mergeAndroid
    property bool libChanged: false      // hidden / moved / merged: read again on the way out

    function isCollectionHidden(name) {
        return hidden.collections.indexOf(name) >= 0;
    }
    function hiddenGamesOf(name) {
        var out = [];
        for (var i = 0; i < hidden.games.length; i++) {
            if (hidden.games[i].collections.indexOf(name) >= 0)
                out.push(hidden.games[i]);
        }
        return out;
    }
    function isGameHidden(key) {
        for (var i = 0; i < hidden.games.length; i++) {
            if (hidden.games[i].path === key)
                return true;
        }
        return false;
    }
    function setCollectionHidden(name, hide) {
        keepPosition(function() {
            Internal.settings.setCollectionHidden(name, hide);
            hidden = Internal.settings.hiddenItems();
        });
        libChanged = true;
        message = hide ? qsTr("\"%1\" is hidden from every theme once you leave Library.").arg(name) + api.tr
                       : qsTr("\"%1\" comes back once you leave Library.").arg(name) + api.tr;
    }
    function setGameHidden(key, title, hide) {
        keepPosition(function() {
            Internal.settings.setGameHidden(key, title, [collName], hide);
            hidden = Internal.settings.hiddenItems();
        });
        libChanged = true;
        message = hide ? qsTr("\"%1\" is hidden from every theme once you leave Library.").arg(title) + api.tr
                       : qsTr("\"%1\" comes back once you leave Library.").arg(title) + api.tr;
    }
    // -- Launch commands: a list to pick from, a custom one at the bottom ---------

    property var choices: []          // the open collection's emulators
    property string choicesFor: ""
    // Android entries: the app a game opens, by name
    property var launchInfo: ({})     // the open game's { command, package }
    property var apps: []             // installed apps, read once
    function loadApps() {
        if (apps.length === 0)
            apps = Internal.settings.androidApps();
    }
    function packageOf(command) {
        var m = /-n\s+([\w.]+)\//.exec(String(command || ""));
        return m ? m[1] : "";
    }
    function appLabel(pkg) {
        if (!pkg)
            return game ? String(current("title") || game.title) : "";
        loadApps();
        for (var i = 0; i < apps.length; i++) {
            if (apps[i]["package"] === pkg)
                return apps[i].label;
        }
        return pkg;
    }
    function androidLaunchPicker(field) {
        loadApps();
        var currentCmd = String(current("launch") || "");
        var mark = function(text, on) { return on ? text + "   ✓" : text; };
        var entries = [];
        var own = game && game["package"] ? game["package"] : "";
        entries.push({ "act": "global", "text": mark((own ? qsTr("Default: %1").arg(appLabel(own))
                                                          : qsTr("Default: %1").arg(appLabel(launchInfo["package"]))) + api.tr,
                                                     currentCmd === "") });
        var list_apps = apps.slice().sort(function(a, b) { return root.byName(a.label, b.label); });
        for (var i = 0; i < list_apps.length; i++) {
            var app = list_apps[i];
            var cmd = Internal.settings.androidAppCommand(app.component);
            entries.push({ "act": "cmd", "command": cmd,
                           "text": mark(app.label + "   (" + app["package"] + ")", currentCmd !== "" && packageOf(currentCmd) === app["package"]) });
        }
        entries.push({ "act": "custom", "text": qsTr("Custom command...") + api.tr });
        chooser.openFor(field.label, entries, function(act, entry) {
            if (act === "global")
                root.setEdit("launch", "");
            else if (act === "cmd")
                root.setEdit("launch", entry.command);
            else if (act === "custom")
                fieldDialog.openFor(field);
        });
    }
    function launchPicker(field) {
        if (page === "game" && isAndroidCollection(collObj)) {
            androidLaunchPicker(field);
            return;
        }
        if (choicesFor !== collName) {
            choices = Internal.settings.emulatorChoices(collName, collDetails.shortname || "", collDetails.romRoot || "");
            choicesFor = collName;
        }
        var currentCmd = String(current("launch") || "");
        var entries = [];
        var mark = function(text, on) { return on ? text + "   ✓" : text; };
        if (page === "game") {
            entries.push({ "act": "global", "text": mark(qsTr("Default: %1").arg(collectionLaunchLabel) + api.tr, currentCmd === "") });
        }
        else if (collDetails.defaultLabel) {
            entries.push({ "act": "global", "text": mark(qsTr("Default: %1 (until you pick one)").arg(collDetails.defaultLabel) + api.tr, currentCmd === "") });
        }
        // the command your metadata had before Pegasus X changed it
        var original = Internal.settings.originalLaunch(collName, page === "game" ? (game.path || game.key) : "");
        if (original.known && original.command !== "")
            entries.push({ "act": "cmd", "command": original.command,
                           "text": mark(qsTr("Your metadata's command") + api.tr,
                                        currentCmd !== "" && sameCommand(original.command, currentCmd)) });
        var anyInstalled = false;
        for (var i = 0; i < choices.length; i++) {
            var c = choices[i];
            if (!c.installed)
                continue;
            anyInstalled = true;
            entries.push({ "act": "cmd", "command": c.command,
                           "text": mark(c.label + (c.custom ? qsTr(" · yours") + api.tr
                                                  : c.recommended ? qsTr(" · recommended") + api.tr : ""),
                                        currentCmd !== "" && sameCommand(c.command, currentCmd)) });
        }
        // a system the list doesn't know: the apps in Android's Emulators section
        if (!anyInstalled) {
            var emus = libraryEmulators();
            for (var e = 0; e < emus.length; e++)
                entries.push({ "act": "cmd", "command": emus[e].command,
                               "text": mark(emus[e].label + qsTr(" · Android emulator") + api.tr,
                                            currentCmd !== "" && sameCommand(emus[e].command, currentCmd)) });
        }
        for (var m = 0; m < choices.length; m++) {
            if (!choices[m].installed)
                entries.push({ "act": "cmd", "command": choices[m].command,
                               "text": mark(choices[m].label + qsTr(" · not installed") + api.tr,
                                            currentCmd !== "" && sameCommand(choices[m].command, currentCmd)) });
        }
        entries.push({ "act": "custom", "text": qsTr("Custom command... (and save it to this list)") + api.tr });
        var yours = choices.filter(function(x) { return x.custom; });
        if (yours.length > 0)
            entries.push({ "act": "forget", "text": qsTr("Remove one of yours from the list...") + api.tr });
        chooser.openFor(field.label, entries, function(act, entry) {
            if (act === "global")
                root.setEdit("launch", "");
            else if (act === "cmd")
                root.setEdit("launch", (root.page === "game" && entry.command === root.collectionLaunch) ? "" : entry.command);
            else if (act === "custom")
                fieldDialog.openFor(field);
            else if (act === "forget")
                root.forgetEmulator(yours);
        });
    }
    // the apps in Android's Emulators section (Settings > Library), opened with the game file
    function libraryEmulators() {
        loadApps();
        var out = [];
        var seen = {};
        var names = ["Android", "Android Games"];
        for (var n = 0; n < names.length; n++) {
            var list_in = Internal.settings.collectionGames(api.collections, names[n]);
            for (var i = 0; i < list_in.length; i++) {
                var e = list_in[i];
                if (kindOf(e) !== "Emulator" || !e["package"] || seen[e["package"]])
                    continue;
                seen[e["package"]] = true;
                for (var a = 0; a < apps.length; a++) {
                    if (apps[a]["package"] === e["package"]) {
                        out.push({ "label": e.title, "command": "am start --user 0 -n " + apps[a].component
                                   + " -a android.intent.action.VIEW -d {file.documenturi}" });
                        break;
                    }
                }
            }
        }
        return out;
    }
    // your own emulators: a name and a command, listed for the collection's
    // system (every collection of it), saved in Pegasus X's config
    function saveEmulator(label, command) {
        var error = Internal.settings.saveCustomEmulator(collName, collDetails.shortname || "", label, command, false);
        if (error !== "") {
            message = error;
            return;
        }
        choicesFor = "";  // read again
        setEdit("launch", (page === "game" && command === collectionLaunch) ? "" : command);
        message = qsTr("\"%1\" is in the emulator list of this system now.").arg(label) + api.tr;
    }
    function forgetEmulator(yours) {
        var entries = yours.map(function(x) { return { "act": "forget", "label": x.label, "text": x.label }; });
        chooser.openFor(qsTr("Remove which one?") + api.tr, entries, function(act, entry) {
            Internal.settings.saveCustomEmulator(root.collName, root.collDetails.shortname || "", entry.label, "", true);
            root.choicesFor = "";
            root.message = qsTr("\"%1\" is off the list.").arg(entry.label) + api.tr;
        });
    }
    // -- Scraping one game --------------------------------------------------------
    property bool scrapingGame: false
    // set while a page is filled in: the list's rows are made once at the end,
    // not again for every property set on the way (that's what made opening a
    // collection pause)
    property bool building: false
    function scrapeThisGame() {
        if (scrapingGame)
            return;
        var s = Internal.settings.scraperSettings();
        var sourceName = s.source === "igdb" ? "IGDB" : s.source === "steamgriddb" ? "SteamGridDB" : "ScreenScraper";
        chooser.openFor(qsTr("Scrape \"%1\" from %2").arg(current("title")).arg(sourceName) + api.tr, [
            { "act": "fill", "text": qsTr("Fill what's empty") + api.tr },
            { "act": "replace", "text": qsTr("Replace with what's found") + api.tr },
            { "act": "source", "text": qsTr("Scrape from: %1  (change)").arg(sourceName) + api.tr },
            { "act": "options", "text": qsTr("Scraper options... (content, pictures, accounts)") + api.tr },
        ], function(act) {
            if (act === "source") {
                var next = s.source === "screenscraper" ? "igdb" : s.source === "igdb" ? "steamgriddb" : "screenscraper";
                Internal.settings.setScraperSetting("source", next);
                Qt.callLater(root.scrapeThisGame);
                return;
            }
            if (act === "options") {
                scraperLoader.active = true;
                scraperLoader.focus = true;
                return;
            }
            var error = Internal.settings.scrapeGame(root.game.obj, root.gameColl(), act === "replace");
            if (error !== "") {
                root.message = error;
                return;
            }
            root.keepPosition(function() { root.scrapingGame = true; });
            root.message = qsTr("Scraping \"%1\"...").arg(root.current("title")) + api.tr;
        });
    }
    Connections {
        target: Internal.settings
        function onScraperFinished(state) {
            if (state.saved > 0)
                root.scrapedHere = true;
            if (!root.scrapingGame)
                return;
            root.keepPosition(function() { root.scrapingGame = false; });
            if (state.saved > 0) {
                root.anySaved = true;
                // the game was updated in place: its page shows what was saved now
                if (root.page === "game" && root.game && root.game.obj) {
                    // the game's removed-art list was updated with it
                    if (root.hiddenArtSaved[root.game.path] !== undefined) {
                        var hm = Object.assign({}, root.hiddenArtSaved);
                        delete hm[root.game.path];
                        root.hiddenArtSaved = hm;
                    }
                    var index = list.currentIndex;
                    root.openGame(root.game);
                    list.currentIndex = Math.min(index, list.count - 1);
                    list.positionViewAtIndex(list.currentIndex, ListView.Center);
                    root.missingMap[root.game.path] = root.missingOf(root.game.obj);
                    root.missingMapChanged();
                }
                root.message = qsTr("Scraped: %1").arg(state.result) + api.tr;
            }
            else {
                root.message = qsTr("Nothing saved: %1").arg(state.stopReason || state.result) + api.tr;
            }
        }
    }

    // starts a game with a command, to see that it works
    function testWith(command, entry) {
        if (!entry || command === "") {
            message = qsTr("Pick an emulator first.") + api.tr;
            return;
        }
        var error = Internal.settings.testEmulatorCommand(command, entry.path);
        message = error === "" ? qsTr("Started \"%1\".").arg(entry.title) + api.tr : error;
    }
    function sameCommand(a, b) {
        return String(a).replace(/\s+/g, " ").trim() === String(b).replace(/\s+/g, " ").trim();
    }

    // -- Pegasus X's data folder: picked art is copied there --------------------

    property string dataDir: Internal.settings.dataDir

    // ES-DE's list of Android emulators for each system (A updates it)
    property string emuListInfo: Internal.settings.emulatorListInfo()
    property bool emuListUpdating: false
    function updateEmulatorList() {
        if (emuListUpdating)
            return;
        emuListUpdating = true;
        keepPosition(function() { emuListInfo = qsTr("Downloading the latest from ES-DE...") + api.tr; });
        Internal.settings.updateEmulatorList();
    }
    Timer {
        running: true
        interval: 800
        onTriggered: {
            var age = Internal.settings.emulatorListAgeDays();
            if (age < 0 || age >= 7)
                root.updateEmulatorList();
        }
    }
    Connections {
        target: Internal.settings
        function onEmulatorListUpdated(ok, text) {
            root.emuListUpdating = false;
            root.keepPosition(function() { root.emuListInfo = text; });
            root.choicesFor = "";  // read again with the new list
        }
    }
    // the folder each kind of art goes in (ES-DE's names)
    readonly property var artDataFolders: ({
        "art:video": "videos", "art:screenshot": "screenshots", "art:titlescreen": "titlescreens",
        "art:fanart": "fanart", "art:box3d": "3dboxes", "art:box2d": "covers", "art:boxback": "backcovers",
        "art:physical": "physicalmedia", "art:miximage": "miximages", "art:logo": "marquees",
        "art:manual": "manuals", "cart:background": "system", "cart:logo": "system",
    })
    function storeArt(key, path) {
        if (dataDir === "")
            return path;
        var second = key.indexOf(p2) === 0;
        var plain = artBase(second ? key.substring(p2.length) : key);
        // named after the game's file, as the scraper names its files (an
        // Android app has no file: its title)
        var gameFile = (page === "game" && game && game.path && game.path.indexOf("android:") !== 0)
            ? String(game.path).split("/").pop().replace(/\.[^.]+$/, "") : "";
        var name = plain === "cart:background" ? "background"
                 : plain === "cart:logo" ? "logo"
                 : (gameFile || String(current("title") || (game ? game.title : "art")));
        return Internal.settings.importArt(path, second ? coll2Name : gameColl(), artDataFolders[plain] || "art", name);
    }
    function pickDataFolder() {
        filePicker.pickFolders = true;
        filePicker.fileExtensions = [];
        filePicker.openAt(dataDir);
        filePicker.focus = true;
    }

    function isAndroidCollection(c) {
        if (!c)
            return false;
        var n = String(c.name).toLowerCase();
        var sn = String(c.shortName || "").toLowerCase();
        return n === "android" || n === "android games" || sn === "android" || sn === "androidgames";
    }
    function kindOf(g) {
        return kinds[g.key] || g.kind || "Game";
    }
    // what to do with an Android entry
    function androidActions(g) {
        var hiddenNow = isGameHidden(g.key);
        var k = kindOf(g);
        var entries = [ { "act": "edit", "text": qsTr("Edit details and art...") + api.tr },
                        { "act": hiddenNow ? "show" : "hide", "text": hiddenNow ? qsTr("Show it again") + api.tr : qsTr("Hide") + api.tr } ];
        if (k !== "Game") entries.push({ "act": "Game", "text": qsTr("Move to Games") + api.tr });
        if (k !== "Application") entries.push({ "act": "Application", "text": qsTr("Move to Apps") + api.tr });
        if (k !== "Emulator") entries.push({ "act": "Emulator", "text": qsTr("Move to Emulators") + api.tr });
        if (kinds[g.key]) entries.push({ "act": "auto", "text": qsTr("Back to automatic") + api.tr });
        chooser.openFor(g.title, entries, function(act) {
            if (act === "edit")
                root.openGame(g);
            else if (act === "hide" || act === "show")
                root.setGameHidden(g.key, g.title, act === "hide");
            else {
                var live = false;
                root.keepPosition(function() {
                    Internal.settings.setAndroidKind(g.key, act === "auto" ? "" : act);
                    root.kinds = Internal.settings.androidKinds();
                    // into its section now, themes and Emulators included
                    live = act !== "auto" && Internal.settings.applyAndroidKind(api.collections, g.obj, act);
                });
                if (live) {
                    if (root.coll2Name !== "")
                        g.coll = act === "Game" ? root.coll2Name : root.collName;
                    root.message = qsTr("\"%1\" moved.").arg(g.title) + api.tr;
                }
                else {
                    root.libChanged = true;
                    root.message = qsTr("\"%1\" moves once you leave Library.").arg(g.title) + api.tr;
                }
            }
        });
    }

    // -- What can be edited ------------------------------------------------------

    readonly property var gameFields: [
        { "key": "title", "label": qsTr("Title") + api.tr, "type": "text" },
        { "key": "developer", "label": qsTr("Developer") + api.tr, "type": "list" },
        { "key": "publisher", "label": qsTr("Publisher") + api.tr, "type": "list" },
        { "key": "genre", "label": qsTr("Genres") + api.tr, "type": "list", "picks": true },
        { "key": "description", "label": qsTr("Description") + api.tr, "type": "multi" },
        { "key": "launch", "label": qsTr("Emulator") + api.tr, "type": "command" },
        { "key": "release", "label": qsTr("Release date") + api.tr, "type": "date" },
        { "key": "players", "label": qsTr("Players") + api.tr, "type": "number" },
        { "key": "rating", "label": qsTr("Rating") + api.tr, "type": "rating" },
    ]
    readonly property var collectionFields: [
        { "key": "name", "label": qsTr("Name") + api.tr, "type": "text" },
        { "key": "shortname", "label": qsTr("Short name") + api.tr, "type": "text" },
        { "key": "extensions", "label": qsTr("Extensions") + api.tr, "type": "text" },
        { "key": "launch", "label": qsTr("Emulator") + api.tr, "type": "command" },
    ]
    property var collDetails: ({})   // the open collection's own lines (launch, extensions, default)
    // Android with Merge off: Android Games is edited on the same page, its
    // keys start with p2 ("g:name", "g:cart:logo")
    readonly property string p2: "g:"
    property string coll2Name: ""
    property var coll2Obj: null
    property var coll2Details: ({})
    readonly property var coll2Fields: [
        { "key": "g:name", "label": qsTr("Name") + api.tr, "type": "text" },
        { "key": "g:shortname", "label": qsTr("Short name") + api.tr, "type": "text" },
    ]
    // -- Missing art: games without a box, logo, screenshot or video --------------
    property bool missingOnly: false
    property var missingMap: ({})    // game path -> ["box", "logo", ...]
    function missingOf(obj) {
        var a = obj ? obj.assets : null;
        if (!a)
            return [];
        var out = [];
        if (a.boxFrontList.length === 0) out.push(qsTr("box") + api.tr);
        if (a.logoList.length === 0) out.push(qsTr("logo") + api.tr);
        if (a.screenshotList.length === 0) out.push(qsTr("screenshot") + api.tr);
        if (a.videoList.length === 0) out.push(qsTr("video") + api.tr);
        return out;
    }
    // the open game, with what you picked (before the library is read again)
    function missingNow() {
        var out = [];
        if (!hasArt("art:box2d") && !hasArt("art:box3d")) out.push(qsTr("box") + api.tr);
        if (!hasArt("art:logo")) out.push(qsTr("logo") + api.tr);
        if (!hasArt("art:screenshot")) out.push(qsTr("screenshot") + api.tr);
        if (!hasArt("art:video")) out.push(qsTr("video") + api.tr);
        return out;
    }

    // the collection a game is saved in (on the Android page, Android or Android Games)
    function gameColl() {
        return (page === "game" && game && game.coll) ? game.coll : collName;
    }
    function isAndroidGamesCollection(c) {
        if (!c)
            return false;
        return String(c.name).toLowerCase() === "android games" || String(c.shortName || "").toLowerCase() === "androidgames";
    }
    function findCollection(test) {
        for (var i = 0; i < api.collections.count; i++) {
            var c = api.collections.get(i);
            if (test(c))
                return c;
        }
        return null;
    }
    property var gameCommands: ({})  // game path -> its own launch command
    // the command a game uses when it has none of its own
    readonly property string collectionLaunch: {
        var own = (page === "collection" && ("launch" in edits)) ? edits["launch"]
                : ("launch" in savedColl) ? savedColl["launch"] : (collDetails.launch || "");
        return own !== "" ? own : (collDetails.defaultCommand || "");
    }
    property var savedColl: ({})     // the collection's values saved this time
    // what the collection launches with, by name: "GameNative"
    readonly property string collectionLaunchLabel: {
        var own = (page === "collection" && ("launch" in edits)) ? edits["launch"]
                : ("launch" in savedColl) ? savedColl["launch"] : (collDetails.launch || "");
        if (own === "")
            return collDetails.defaultLabel ? collDetails.defaultLabel : qsTr("none set") + api.tr;
        for (var i = 0; i < choices.length; i++) {
            if (sameCommand(choices[i].command, own))
                return choices[i].label;
        }
        return qsTr("the collection's custom command") + api.tr;
    }
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
    // every file of a kind has its own key: "art:screenshot#0", "art:screenshot#1"
    function artBase(key) {
        var s = String(key);
        var at = s.indexOf("#");
        return at < 0 ? s : s.substring(0, at);
    }
    function artKind(key) {
        var base = artBase(key);
        for (var i = 0; i < artKinds.length; i++) {
            if (artKinds[i][0] === base)
                return artKinds[i];
        }
        return null;
    }
    // the keys of one kind, in order (files the game has, and ones you added)
    function artKeysOf(base) {
        var found = {};
        var maps = [orig, saved, edits];
        for (var m = 0; m < maps.length; m++) {
            for (var k in maps[m]) {
                if (k.indexOf(base + "#") === 0)
                    found[k] = true;
            }
        }
        return Object.keys(found).sort(function(a, b) {
            return parseInt(a.substring(base.length + 1), 10) - parseInt(b.substring(base.length + 1), 10);
        });
    }
    function hasArt(base) {
        var keys = artKeysOf(base);
        for (var i = 0; i < keys.length; i++) {
            if (current(keys[i]) !== "")
                return true;
        }
        return false;
    }
    function artFilter(key) {
        key = artBase(key);
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
            // each file its own row; the boxes: the 3D ones, the 2D ones
            var n = 0;
            for (var j = 0; j < files.length; j++) {
                if (kind[4] === "3d" && !is3d(files[j])) continue;
                if (kind[4] === "2d" && is3d(files[j])) continue;
                o[kind[0] + "#" + n] = files[j];
                n++;
            }
        }
        origSlots = slots;
        return o;
    }
    function readCollection(c) {
        var o = {
            "name": c.name,
            "shortname": collDetails.shortname || c.shortName || "",
            "extensions": collDetails.extensions || "",
            "launch": collDetails.launch || "",
            // the system tile's art, read by themes (XboxOSv2's tiles use it)
            "cart:background": localPath(c.assets ? c.assets.background : ""),
            "cart:logo": localPath(c.assets ? c.assets.logo : ""),
        };
        if (coll2Obj) {
            o[p2 + "name"] = coll2Obj.name;
            o[p2 + "shortname"] = coll2Details.shortname || coll2Obj.shortName || "";
            o[p2 + "cart:background"] = localPath(coll2Obj.assets ? coll2Obj.assets.background : "");
            o[p2 + "cart:logo"] = localPath(coll2Obj.assets ? coll2Obj.assets.logo : "");
        }
        return o;
    }

    // -- Pages -----------------------------------------------------------------------

    readonly property var collections: {
        var out = [];
        var seen = {};
        // Android Games is on Android's page whenever both are in the library
        // (the Merge switch changes that once you leave Library)
        var android = findCollection(function(x) { return isAndroidCollection(x) && !isAndroidGamesCollection(x); });
        var androidGames = android ? findCollection(isAndroidGamesCollection) : null;
        for (var i = 0; i < api.collections.count; i++) {
            var c = api.collections.get(i);
            seen[c.name] = true;
            if (androidGames && c === androidGames)
                continue;  // on Android's page
            var n = c.games.count + ((androidGames && c === android) ? androidGames.games.count : 0);
            out.push({ "name": c.name, "count": n, "obj": c, "loaded": true });
        }
        // hidden ones aren't in the library any more
        for (var h = 0; h < hidden.collections.length; h++) {
            if (!seen[hidden.collections[h]])
                out.push({ "name": hidden.collections[h], "count": 0, "obj": null, "loaded": false });
        }
        out.sort(function(a, b) { return root.byName(a.name, b.name); });
        return out;
    }

    function openCollection(entry) {
        if (!entry.loaded) {
            chooser.openFor(qsTr("\"%1\" is hidden.").arg(entry.name) + api.tr,
                [ { "act": "show", "text": qsTr("Show it again") + api.tr },
                  { "act": "stay", "text": qsTr("Keep it hidden") + api.tr } ],
                function(act) { if (act === "show") root.setCollectionHidden(entry.name, false); });
            return;
        }
        building = true;
        collName = entry.name;
        collObj = entry.obj;
        // Android, Merge off: Android Games too, on this page
        var second = (isAndroidCollection(entry.obj) && !isAndroidGamesCollection(entry.obj))
            ? findCollection(isAndroidGamesCollection) : null;
        coll2Obj = second;
        coll2Name = second ? second.name : "";
        coll2Details = second ? Internal.settings.collectionDetails(second.name) : ({});
        var tag = function(list_in, name) {
            for (var t = 0; t < list_in.length; t++)
                list_in[t].coll = name;
            return list_in;
        };
        var list_games = tag(Internal.settings.collectionGames(api.collections, entry.name), entry.name);
        if (second)
            list_games = list_games.concat(tag(Internal.settings.collectionGames(api.collections, second.name), second.name));
        list_games.sort(function(a, b) { return root.byName(a.title, b.title); });
        var miss = {};
        for (var mi = 0; mi < list_games.length; mi++)
            miss[list_games[mi].path] = missingOf(list_games[mi].obj);
        missingMap = miss;
        missingOnly = false;
        games = list_games;
        savedTitles = {};
        savedColl = {};
        collDetails = Internal.settings.collectionDetails(entry.name);
        themeArt = Internal.settings.themeSystemArt(entry.name, collDetails.shortname || entry.obj.shortName || "");
        coll2ThemeArt = second ? Internal.settings.themeSystemArt(second.name, coll2Details.shortname || second.shortName || "") : ({});
        choices = Internal.settings.emulatorChoices(entry.name, collDetails.shortname || "", collDetails.romRoot || "");
        choicesFor = entry.name;
        var cmds = Internal.settings.gameEmulators(entry.name);
        if (second) {
            var cmds2 = Internal.settings.gameEmulators(second.name);
            for (var ck in cmds2)
                cmds[ck] = cmds2[ck];
        }
        gameCommands = cmds;
        orig = readCollection(entry.obj);
        saved = {};
        edits = {};
        loc = Internal.settings.metadataLocation(entry.name, "");
        message = "";
        page = "collection";
        building = false;
        list.currentIndex = 1;
        list.positionViewAtBeginning();
    }
    function openGame(entry) {
        building = true;
        game = entry;
        launchInfo = isAndroidCollection(collObj) ? Internal.settings.gameLaunchInfo(entry.obj) : ({});
        orig = readGame(entry.obj);
        saved = {};
        edits = {};
        loc = Internal.settings.metadataLocation(entry.coll || collName, entry.path || entry.key);
        message = "";
        page = "game";
        building = false;
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
        building = true;
        edits = {};
        message = "";
        if (page === "game") {
            var path = game ? game.path : "";
            page = "collection";
            orig = readCollection(collObj);
            saved = savedColl;
            loc = Internal.settings.metadataLocation(collName, "");
            building = false;
            for (var i = 0; i < list.rows.length; i++) {
                if (list.rows[i].kind === "game" && list.rows[i].game.path === path) {
                    list.currentIndex = i;
                    list.positionViewAtIndex(i, ListView.Center);
                    return;
                }
            }
            return;
        }
        building = false;
        if (page === "hidden") {
            page = "collections";
            list.currentIndex = 2;
            return;
        }
        if (page === "collection") {
            var name = collName;
            page = "collections";
            collName = "";
            coll2Name = "";
            coll2Obj = null;
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
        Internal.settings.libraryOpen = false;
        if (anySaved || scrapedHere || (libChanged && libState() !== libStart))
            Internal.settings.reloadLibrary();
        root.close();
    }

    // edits -> what's written: art goes back into its slot's whole list,
    // replacing only the file that was shown (the others, eg. more
    // screenshots, stay)
    // art you removed: "<kind>|<file>", kept in the game's entry so the file
    // stays out even when another metadata file (eg. an ES-DE import, or a
    // media folder) lists it too
    function hiddenArtNow() {
        var list = [];
        if (game && game.obj && game.obj.extra && game.obj.extra["pegasusx-hidden-art"]) {
            var had = game.obj.extra["pegasusx-hidden-art"];
            list = Array.isArray(had) ? had.slice() : [String(had)];
        }
        if (game && hiddenArtSaved[game.path])
            list = hiddenArtSaved[game.path].slice();
        return list;
    }
    property var hiddenArtSaved: ({})   // game path -> its list, saved this time
    function toChanges() {
        var out = {};
        var slots = {};
        var hiddenArt = hiddenArtNow();
        var hiddenBefore = JSON.stringify(hiddenArt);
        for (var key in edits) {
            if (key.indexOf("art:") !== 0) {
                out[key] = edits[key];
                continue;
            }
            var kind = artKind(key);
            if (!kind)
                continue;
            var slot = kind[3];
            if (!(slot in slots))
                slots[slot] = (origSlots[slot] || []).slice();
            var list = slots[slot];
            var before = (key in saved) ? saved[key] : orig[key];
            var at = before ? list.indexOf(before) : -1;
            var value = edits[key];
            if (kind[3].indexOf("assets.") === 0) {
                var kindName = kind[3].substring(7);
                // a file you pick again comes back
                if (value !== "")
                    hiddenArt = hiddenArt.filter(function(h) { return h !== kindName + "|" + value; });
                // the one it replaces, or that you removed, stays out
                if (before && value !== before && String(before).indexOf("image://") !== 0
                        && hiddenArt.indexOf(kindName + "|" + before) < 0)
                    hiddenArt.push(kindName + "|" + before);
            }
            if (at >= 0) {
                if (value === "")
                    list.splice(at, 1);
                else
                    list[at] = value;
            }
            else if (value !== "") {
                // a first one becomes the main one (but the 2D box stays it);
                // another of a kind goes after the ones there
                if (kind[4] === "3d" || list.length > 0 && artKeysOf(kind[0]).indexOf(key) > 0)
                    list.push(value);
                else
                    list.unshift(value);
            }
        }
        for (var s in slots)
            out[s] = slots[s].filter(function(f) { return f && String(f).indexOf("image://") !== 0; });
        if (JSON.stringify(hiddenArt) !== hiddenBefore)
            out["x-pegasusx-hidden-art"] = hiddenArt;
        // one text: a new description replaces the summary too
        if ("description" in out)
            out["summary"] = "";
        return out;
    }

    // the collection's art keys as written in its metadata
    readonly property var collectionArt: [
        ["cart:background", qsTr("Tile background") + api.tr, "assets.background"],
        ["cart:logo", qsTr("Logo") + api.tr, "assets.logo"],
    ]
    // prefix "": the open collection's changes; p2: Android Games'
    function collectionChanges(prefix) {
        var out = {};
        for (var key in edits) {
            var mine = prefix !== "" ? key.indexOf(prefix) === 0 : key.indexOf(p2) !== 0;
            if (!mine)
                continue;
            var plain = key.substring(prefix.length);
            var mapped = plain;
            for (var i = 0; i < collectionArt.length; i++) {
                if (collectionArt[i][0] === plain)
                    mapped = collectionArt[i][2];
            }
            out[mapped] = edits[key];
        }
        return out;
    }

    function save() {
        if (!dirty)
            return true;
        var error = "";
        if (page === "game") {
            error = Internal.settings.saveGameMetadata(gameColl(), game.path || game.key, current("title"), toChanges());
        }
        else {
            var mainChanges = collectionChanges("");
            if (Object.keys(mainChanges).length > 0)
                error = Internal.settings.saveCollectionMetadata(collName, mainChanges);
            var secondChanges = coll2Name !== "" ? collectionChanges(p2) : {};
            if (error === "" && Object.keys(secondChanges).length > 0)
                error = Internal.settings.saveCollectionMetadata(coll2Name, secondChanges);
        }
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
        if (page === "game" && ("x-pegasusx-hidden-art" in written)) {
            var hiddenMap = Object.assign({}, hiddenArtSaved);
            hiddenMap[game.path] = written["x-pegasusx-hidden-art"];
            hiddenArtSaved = hiddenMap;
        }
        var where = loc.file || "";
        keepPosition(function() {
            saved = merged;
            edits = {};
            if (page === "game") {
                var missNow = Object.assign({}, missingMap);
                missNow[game.path] = root.missingNow();
                missingMap = missNow;
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
                if ((p2 + "name") in merged)
                    coll2Name = merged[p2 + "name"];
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
        loc = Internal.settings.metadataLocation(gameColl(), page === "game" ? (game.path || game.key) : "");
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
        text: (page === "collections" ? qsTr("Settings > Library")
             : page === "hidden" ? qsTr("Library > Hidden")
             : page === "collection" ? qsTr("Library > %1").arg(root.collName)
             : qsTr("%1 > %2").arg(root.collName).arg(root.current("title"))) + api.tr
        z: 2
    }


    ListView {
        id: list

        anchors.top: header.bottom
        anchors.bottom: messageBar.top
        // a game's or a collection's page: the list on the left, its art beside it
        x: artPreview.split ? parent.width * 0.05 : (parent.width - width) / 2
        width: artPreview.split ? parent.width * 0.5 : parent.width * 0.7
        clip: true
        focus: true

        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: height * 0.3
        preferredHighlightEnd: height * 0.7
        highlightMoveDuration: 150

        readonly property var rows: {
            if (root.building)
                return [];
            var out = [];
            var i;
            if (root.page === "collections") {
                out.push({ "kind": "section", "text": qsTr("Options") + api.tr });
                var hiddenCount = root.hidden.collections.length + root.hidden.games.length;
                out.push({ "kind": "emulist", "text": qsTr("Emulator list") + api.tr, "value": root.emuListInfo });
                out.push({ "kind": "datadir", "text": qsTr("Data folder") + api.tr,
                           "value": root.dataDir !== "" ? root.dataDir : qsTr("not set: art stays where you pick it") + api.tr,
                           "muted": root.dataDir === "" });
                out.push({ "kind": "hiddenlist", "text": qsTr("Hidden games and collections") + api.tr,
                           "value": hiddenCount > 0 ? qsTr("%1 hidden").arg(hiddenCount) + api.tr : qsTr("none") + api.tr,
                           "muted": hiddenCount === 0 });
                out.push({ "kind": "section", "text": qsTr("Scraper") + api.tr });
                out.push({ "kind": "scraper", "text": qsTr("Scraper...") + api.tr, "value": "" });
                out.push({ "kind": "section", "text": qsTr("Collections") + api.tr });
                for (i = 0; i < root.collections.length; i++) {
                    var c = root.collections[i];
                    var hiddenHere = root.hiddenGamesOf(c.name).length;
                    var collHidden = root.isCollectionHidden(c.name);
                    out.push({ "kind": "coll", "text": c.name, "coll": c, "hidden": collHidden,
                               "value": collHidden ? qsTr("Hidden") + api.tr
                                      : hiddenHere > 0 ? qsTr("%1 games · %2 hidden").arg(c.count).arg(hiddenHere) + api.tr
                                      : qsTr("%1 games").arg(c.count) + api.tr });
                }
                return out;
            }
            if (root.page === "hidden") {
                out.push({ "kind": "section", "text": qsTr("Collections") + api.tr });
                for (i = 0; i < root.hidden.collections.length; i++)
                    out.push({ "kind": "unhidecoll", "text": root.hidden.collections[i], "value": qsTr("Hidden") + api.tr, "hidden": true });
                if (root.hidden.collections.length === 0)
                    out.push({ "kind": "info", "text": qsTr("None") + api.tr, "value": "" });
                out.push({ "kind": "section", "text": qsTr("Games") + api.tr });
                var hg = root.hidden.games.slice().sort(function(a, b) { return root.byName(a.title, b.title); });
                for (i = 0; i < hg.length; i++)
                    out.push({ "kind": "unhidegame", "text": hg[i].title, "entry": hg[i], "hidden": true,
                               "value": (hg[i].collections.length > 0 ? hg[i].collections[0] + " · " : "") + qsTr("Hidden") + api.tr });
                if (hg.length === 0)
                    out.push({ "kind": "info", "text": qsTr("None") + api.tr, "value": "" });
                return out;
            }

            var fields = root.page === "game" ? root.gameFields : root.collectionFields;
            if (root.page === "game") {
                var gHidden = root.isGameHidden(root.game.key);
                out.push({ "kind": "section", "text": qsTr("In your library") + api.tr });
                out.push({ "kind": "hidegame", "text": qsTr("Show in themes") + api.tr, "hidden": gHidden,
                           "value": gHidden ? qsTr("Hidden") + api.tr : qsTr("Shown") + api.tr });
                if (root.isAndroidCollection(root.collObj)) {
                    var gk = root.kindOf(root.game);
                    out.push({ "kind": "section_android", "text": qsTr("Android section") + api.tr,
                               "value": (gk === "Application" ? qsTr("Apps") : gk === "Emulator" ? qsTr("Emulators") : qsTr("Games")) + api.tr });
                }
            }
            out.push({ "kind": "section", "text": (root.page === "game" ? qsTr("Details") : qsTr("Collection")) + api.tr });
            if (root.page === "collection") {
                var cHidden = root.isCollectionHidden(root.collName);
                out.push({ "kind": "hidecoll", "text": qsTr("Show in themes") + api.tr, "hidden": cHidden,
                           "value": cHidden ? qsTr("Hidden") + api.tr : qsTr("Shown") + api.tr });
                if (root.isAndroidCollection(root.collObj))
                    out.push({ "kind": "mergeandroid", "text": qsTr("Android games") + api.tr,
                               "value": root.mergeAndroid ? qsTr("Inside Android (Games section)") + api.tr
                                                          : qsTr("Their own collection: Android Games") + api.tr });
            }
            for (i = 0; i < fields.length; i++) {
                var f = fields[i];
                if (root.page === "collection" && root.isAndroidCollection(root.collObj)
                        && (f.key === "launch" || f.key === "extensions"))
                    continue;
                var value = root.current(f.key);
                var text = root.shown(f, value);
                var empty = f.empty || "—";
                if (f.key === "launch" && root.page === "game" && root.isAndroidCollection(root.collObj)) {
                    var shownCmd = text !== "" ? text : (root.launchInfo.command || "");
                    if (shownCmd !== "") {
                        text = root.appLabel(root.packageOf(shownCmd) || root.launchInfo["package"]) + "  ·  " + shownCmd;
                        empty = text;
                    }
                    else {
                        empty = qsTr("No app") + api.tr;
                    }
                }
                else if (f.key === "launch") {
                    empty = root.page === "game"
                        ? qsTr("Default: %1").arg(root.collectionLaunchLabel) + api.tr
                        : (root.collDetails.defaultLabel
                            ? qsTr("Default: %1").arg(root.collDetails.defaultLabel) + api.tr
                            : qsTr("none, pick one in Emulators") + api.tr);
                }
                out.push({ "kind": "field", "field": f,
                           "text": (f.key === "launch" && root.page === "game" && root.isAndroidCollection(root.collObj))
                                   ? qsTr("App") + api.tr : f.label,
                           "value": text !== "" ? text : empty,
                           "muted": text === "", "edited": f.key in root.edits, "done": f.key in root.saved });
                // try it right away
                if (f.key === "launch" && !root.isAndroidCollection(root.collObj)
                        && (root.page === "game" || root.games.length > 0))
                    out.push({ "kind": "test", "text": root.page === "game" ? qsTr("Test this game") + api.tr
                                                                            : qsTr("Test with a game") + api.tr,
                               "value": root.page === "game" ? "" : root.games[0].title });
                // the game's file, below its title
                if (root.page === "game" && f.key === "title")
                    out.push({ "kind": "info", "text": qsTr("File") + api.tr, "value": root.fileShown() });
            }

            if (root.page === "collection") {
                out.push({ "kind": "section", "text": qsTr("System tile") + api.tr });
                for (i = 0; i < root.collectionArt.length; i++) {
                    var ca = root.collectionArt[i];
                    var caValue = root.current(ca[0]);
                    out.push({ "kind": "art", "text": ca[1], "key": ca[0], "icon": false,
                               "value": caValue === "" ? root.themeArtText(ca[0]) : root.fileName(caValue),
                               "muted": caValue === "", "edited": ca[0] in root.edits, "done": ca[0] in root.saved });
                }
                if (root.coll2Name !== "") {
                    out.push({ "kind": "section", "text": qsTr("Android Games collection") + api.tr });
                    var c2Hidden = root.isCollectionHidden(root.coll2Name);
                    out.push({ "kind": "hidecoll2", "text": qsTr("Show in themes") + api.tr, "hidden": c2Hidden,
                               "value": c2Hidden ? qsTr("Hidden") + api.tr : qsTr("Shown") + api.tr });
                    for (i = 0; i < root.coll2Fields.length; i++) {
                        var f2 = root.coll2Fields[i];
                        var v2 = root.shown(f2, root.current(f2.key));
                        out.push({ "kind": "field", "text": f2.label, "field": f2, "value": v2 !== "" ? v2 : "—",
                                   "muted": v2 === "", "edited": f2.key in root.edits, "done": f2.key in root.saved });
                    }
                    for (i = 0; i < root.collectionArt.length; i++) {
                        var key2 = root.p2 + root.collectionArt[i][0];
                        var a2 = root.current(key2);
                        out.push({ "kind": "art", "text": qsTr("Tile %1").arg(root.collectionArt[i][1].toLowerCase()) + api.tr,
                                   "key": key2, "icon": false,
                                   "value": a2 === "" ? root.themeArtText(key2) : root.fileName(a2),
                                   "muted": a2 === "", "edited": key2 in root.edits, "done": key2 in root.saved });
                    }
                }
            }
            if (root.page === "game") {
                out.push({ "kind": "section", "text": qsTr("Art and manual") + api.tr });
                for (i = 0; i < root.artKinds.length; i++) {
                    var kind = root.artKinds[i];
                    var kindKeys = root.artKeysOf(kind[0]);
                    for (var kk = 0; kk < kindKeys.length; kk++) {
                        var akey = kindKeys[kk];
                        var art = root.current(akey);
                        if (art === "" && !(akey in root.edits))
                            continue;
                        var isIcon = String(art).indexOf("image://") === 0;
                        out.push({ "kind": "art", "text": kindKeys.length > 1 ? kind[1] + " " + (kk + 1) : kind[1],
                                   "key": akey, "icon": isIcon,
                                   "value": art === "" ? qsTr("removed") + api.tr : isIcon ? qsTr("the app's icon") + api.tr : root.fileName(art),
                                   "muted": art === "" || isIcon, "edited": akey in root.edits, "done": akey in root.saved });
                    }
                }
                out.push({ "kind": "addart", "text": qsTr("Add art or a manual...") + api.tr, "value": "" });
                out.push({ "kind": "scrapegame", "text": qsTr("Scrape this game...") + api.tr,
                           "value": root.scrapingGame ? qsTr("scraping...") + api.tr : qsTr("with the Scraper's settings") + api.tr,
                           "muted": true });
            }

            var savedIn = root.loc.file ? root.loc.file.split("/").slice(-2).join("/") : "";
            if (root.loc.isNew)
                savedIn = qsTr("a new entry of yours (%1)").arg(savedIn) + api.tr;
            else if (root.loc.isImport)
                savedIn = savedIn + qsTr(" · kept when you rescan") + api.tr;
            out.push({ "kind": "info", "text": qsTr("Saved in") + api.tr, "value": savedIn });
            out.push({ "kind": "save", "text": root.dirty ? qsTr("Save changes") + api.tr : qsTr("No changes to save") + api.tr, "value": "" });

            if (root.page === "collection") {
                var android = root.isAndroidCollection(root.collObj);
                var groups = android
                    ? [ { "kind": "Game", "text": qsTr("Games") + api.tr, "rows": [] },
                        { "kind": "Application", "text": qsTr("Apps") + api.tr, "rows": [] },
                        { "kind": "Emulator", "text": qsTr("Emulators") + api.tr, "rows": [] } ]
                    : [ { "kind": "", "text": qsTr("Games") + api.tr, "rows": [] } ];
                var missingCount = 0;
                for (i = 0; i < root.games.length; i++) {
                    if ((root.missingMap[root.games[i].path] || []).length > 0)
                        missingCount++;
                }
                out.push({ "kind": "section", "text": qsTr("Show") + api.tr });
                out.push({ "kind": "missingfilter", "text": qsTr("Games to list") + api.tr,
                           "value": root.missingOnly ? qsTr("Missing art only · %1").arg(missingCount) + api.tr
                                                     : qsTr("All · %1 missing art").arg(missingCount) + api.tr });
                var known = {};
                for (i = 0; i < root.games.length; i++) {
                    var g = root.games[i];
                    known[g.key] = true;
                    var gMissing = root.missingMap[g.path] || [];
                    if (root.missingOnly && gMissing.length === 0)
                        continue;
                    var title = root.savedTitles[g.path] || g.title;
                    var gameHidden = root.isGameHidden(g.key);
                    var row = { "kind": "game", "text": android ? title + "  (" + root.sourceLabel(g.source) + ")" : title,
                                "game": g, "hidden": gameHidden,
                                "value": gameHidden ? qsTr("Hidden") + api.tr
                                       : root.missingOnly ? qsTr("no %1").arg(gMissing.join(" · ")) + api.tr
                                       : (root.savedTitles[g.path] !== undefined ? qsTr("edited · ") + api.tr : "")
                                         + (android ? qsTr("Shown") + api.tr : root.sourceLabel(g.source)),
                                "done": root.savedTitles[g.path] !== undefined };
                    var gi = 0;
                    if (android) {
                        var k = root.kindOf(g);
                        gi = k === "Application" ? 1 : k === "Emulator" ? 2 : 0;
                    }
                    groups[gi].rows.push(row);
                }
                // hidden before this time: not in the library any more
                var gone = root.hiddenGamesOf(root.collName);
                if (root.coll2Name !== "")
                    gone = gone.concat(root.hiddenGamesOf(root.coll2Name));
                for (i = 0; i < gone.length; i++) {
                    if (!root.missingOnly && !known[gone[i].path])
                        groups[0].rows.push({ "kind": "unhidegame", "text": gone[i].title, "entry": gone[i], "hidden": true,
                                              "value": qsTr("Hidden") + api.tr });
                }
                for (var gs = 0; gs < groups.length; gs++) {
                    if (android && groups[gs].rows.length === 0)
                        continue;
                    groups[gs].rows.sort(function(a, b) { return root.byName(a.text, b.text); });
                    var hiddenN = groups[gs].rows.filter(function(r) { return r.hidden; }).length;
                    out.push({ "kind": "section", "text": groups[gs].text + " · " + groups[gs].rows.length
                               + (hiddenN > 0 ? qsTr(" · %1 hidden").arg(hiddenN) + api.tr : "") });
                    out = out.concat(groups[gs].rows);
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
            else if (row.kind === "game") {
                if (root.isAndroidCollection(root.collObj))
                    root.androidActions(row.game);
                else
                    root.openGame(row.game);
            }
            else if (row.kind === "mergeandroid") {
                root.keepPosition(function() {
                    Internal.settings.mergeAndroid = !root.mergeAndroid;
                });
                root.libChanged = true;
                root.message = root.mergeAndroid
                    ? qsTr("Android games move into Android once you leave Library.") + api.tr
                    : qsTr("Android games get their own collection once you leave Library.") + api.tr;
            }
            else if (row.kind === "hiddenlist") {
                root.page = "hidden";
                root.message = "";
                list.currentIndex = 1;
                list.positionViewAtBeginning();
            }
            else if (row.kind === "unhidecoll")
                root.setCollectionHidden(row.text, false);
            else if (row.kind === "unhidegame") {
                Internal.settings.setGameHidden(row.entry.path, row.entry.title, row.entry.collections, false);
                root.keepPosition(function() { root.hidden = Internal.settings.hiddenItems(); });
                root.libChanged = true;
                root.message = qsTr("\"%1\" comes back once you leave Library.").arg(row.entry.title) + api.tr;
            }
            else if (row.kind === "hidecoll")
                root.setCollectionHidden(root.collName, !row.hidden);
            else if (row.kind === "missingfilter")
                root.keepPosition(function() { root.missingOnly = !root.missingOnly; });
            else if (row.kind === "hidecoll2")
                root.setCollectionHidden(root.coll2Name, !row.hidden);
            else if (row.kind === "hidegame")
                root.setGameHidden(root.game.key, root.current("title"), !row.hidden);
            else if (row.kind === "section_android")
                root.androidActions(root.game);
            else if (row.kind === "field") {
                if (row.field.type === "command")
                    root.launchPicker(row.field);
                else
                    fieldDialog.openFor(row.field);
            }
            else if (row.kind === "test") {
                if (root.page === "game")
                    root.testWith(String(root.current("launch") || "") || root.collectionLaunch, root.game);
                else
                    root.testWith(root.collectionLaunch, root.games[0]);
            }
            else if (row.kind === "scraper") {
                scraperLoader.active = true;
                scraperLoader.focus = true;
            }
            else if (row.kind === "scrapegame")
                root.scrapeThisGame();
            else if (row.kind === "emulist")
                root.updateEmulatorList();
            else if (row.kind === "datadir")
                root.pickDataFolder();
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
            else if (api.keys.isFilters(event)) {
                // Y: hide or show a game
                event.accepted = true;
                var row = currentIndex >= 0 && currentIndex < rows.length ? rows[currentIndex] : null;
                if (row && row.kind === "game")
                    root.setGameHidden(row.game.key, row.game.title, !row.hidden);
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
                color: parent.highlighted ? "#3aa" : (root.dirty ? "#555" : "#333")
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
                color: parent.isInfo ? "#999" : modelData.hidden ? "#999" : "#eee"
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
                     : modelData.hidden ? "#e8b06a"
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
            : root.page === "collections" ? qsTr("Edit, hide and sort your collections and games. Saved in Pegasus metadata files, so every theme shows it.") + api.tr
            : root.page === "hidden" ? qsTr("A shows it again.") + api.tr
            : root.page === "collection" ? qsTr("A edit · Y hide / show a game · X undo a change · B back") + api.tr
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


    // -- The art panel: beside the list on a game's and a collection's page -------

    function toUrl(file) {
        var f = String(file || "");
        if (f === "")
            return "";
        if (/^(image|file|https?|qrc):/.test(f))
            return f;
        return f.indexOf("/") === 0 ? "file://" + f : "";
    }
    function firstOf(list) {
        return (list && list.length > 0) ? String(list[0]) : "";
    }
    // what the panel shows for a row: { file, title, edited }
    // the theme's own system art (Settings.themeSystemArt): Android Games'
    // when the key starts with p2
    property var themeArt: ({})
    property var coll2ThemeArt: ({})
    function themeArtFor(key) {
        var k = String(key);
        var art = k.indexOf(p2) === 0 ? coll2ThemeArt : themeArt;
        return (k.indexOf("background") >= 0 ? art.background : art.logo) || "";
    }
    function themeArtText(key) {
        var f = themeArtFor(key);
        return f !== "" ? qsTr("the theme's own · %1").arg(fileName(f)) + api.tr : qsTr("the theme's own (none found)") + api.tr;
    }
    function previewFor(row) {
        if (!row)
            return {};
        if (row.kind === "art") {
            var own = String(current(row.key) || "");
            // a system tile without art of its own: the theme's own
            if (own === "" && String(row.key).indexOf("cart:") >= 0) {
                var themeFile = themeArtFor(row.key);
                if (themeFile !== "")
                    return { "file": themeFile, "title": row.text + qsTr(" · the theme's own") + api.tr };
            }
            return { "file": own, "title": row.text, "edited": !!row.edited, "own": true };
        }
        if (page === "game") {
            var bases = ["art:box2d", "art:box3d", "art:screenshot", "art:titlescreen", "art:fanart", "art:miximage", "art:logo"];
            for (var b = 0; b < bases.length; b++) {
                var keys = artKeysOf(bases[b]);
                for (var k = 0; k < keys.length; k++) {
                    var f = String(current(keys[k]) || "");
                    if (f !== "")
                        return { "file": f, "title": artKind(keys[k])[1], "edited": keys[k] in edits };
                }
            }
            return { "file": "", "title": "" };
        }
        if (page === "collection") {
            if ((row.kind === "game") && row.game && row.game.obj && row.game.obj.assets) {
                var a = row.game.obj.assets;
                return { "file": firstOf(a.boxFrontList) || firstOf(a.screenshotList) || firstOf(a.logoList) || firstOf(a.backgroundList),
                         "title": row.game.title };
            }
            var tile = String(current("cart:background") || current("cart:logo") || "");
            if (tile === "")
                tile = themeArt.background || themeArt.logo || "";
            return { "file": tile, "title": qsTr("System tile") + api.tr };
        }
        return {};
    }

    Item {
        id: artPreview

        readonly property bool split: root.page === "game" || root.page === "collection"
        readonly property var row: (list.currentIndex >= 0 && list.currentIndex < list.rows.length) ? list.rows[list.currentIndex] : null
        readonly property var shown: root.previewFor(row)
        readonly property string file: shown.file || ""
        readonly property bool isVideo: /\.(mp4|webm|mkv|avi|mov)$/i.test(file)
        readonly property bool isDoc: /\.(pdf|cbz|cbr)$/i.test(file)
        // every picture of the open game, as rows of the list
        readonly property var thumbs: {
            var out = [];
            if (root.page !== "game")
                return out;
            for (var i = 0; i < list.rows.length; i++) {
                var r = list.rows[i];
                if (r.kind === "art" && String(root.current(r.key) || "") !== "")
                    out.push({ "index": i, "key": r.key, "label": r.text, "file": String(root.current(r.key)) });
            }
            return out;
        }

        visible: split
        anchors.left: list.right
        anchors.leftMargin: vpx(28)
        anchors.right: parent.right
        anchors.rightMargin: parent.width * 0.04
        anchors.top: list.top
        anchors.topMargin: vpx(20)
        anchors.bottom: messageBar.top
        anchors.bottomMargin: vpx(10)

        Rectangle {
            id: frame
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: caption.top
            anchors.bottomMargin: vpx(8)
            color: "#333"
            border.color: artPreview.shown.edited ? "#e8b06a" : "#444"
            border.width: vpx(2)

            Image {
                id: bigImage
                anchors.fill: parent
                anchors.margins: vpx(14)
                source: (artPreview.isVideo || artPreview.isDoc) ? "" : root.toUrl(artPreview.file)
                cache: false  // a re-scraped file keeps its name: show the new one
                sourceSize.width: vpx(700)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
                opacity: status === Image.Ready ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 180 } }
            }
            // a video or a manual: a tile with what it is
            Column {
                anchors.centerIn: parent
                spacing: vpx(10)
                visible: artPreview.isVideo || artPreview.isDoc || artPreview.file === "" || bigImage.status === Image.Error
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: vpx(84)
                    height: width
                    color: "transparent"
                    border.color: artPreview.file === "" ? "#444" : "#3aa"
                    border.width: vpx(3)
                    Text {
                        anchors.centerIn: parent
                        text: artPreview.isVideo ? "▶" : artPreview.isDoc ? "PDF" : artPreview.file === "" ? "—" : "?"
                        color: artPreview.file === "" ? "#555" : "#5cc"
                        font.family: globalFonts.sans
                        font.pixelSize: artPreview.isDoc ? vpx(22) : vpx(34)
                        font.bold: true
                    }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: artPreview.isVideo ? qsTr("Video") + api.tr
                        : artPreview.isDoc ? qsTr("Manual") + api.tr
                        : artPreview.file === "" ? qsTr("No picture") + api.tr
                        : qsTr("Can't show this file") + api.tr
                    color: "#888"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(17)
                }
            }
            // which picture this is
            Rectangle {
                visible: !!artPreview.shown.title
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: vpx(10)
                width: kindText.implicitWidth + vpx(20)
                height: vpx(30)
                color: "#cc1d2a30"
                Text {
                    id: kindText
                    anchors.centerIn: parent
                    text: artPreview.shown.title || ""
                    color: "#9dd"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(15)
                }
            }
            Rectangle {
                visible: !!artPreview.shown.edited
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: vpx(10)
                width: newText.implicitWidth + vpx(20)
                height: vpx(30)
                color: "#e8b06a"
                Text {
                    id: newText
                    anchors.centerIn: parent
                    text: qsTr("New · not saved") + api.tr
                    color: "#1a1a1a"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(15)
                }
            }
        }

        Text {
            id: caption
            anchors.bottom: strip.visible ? strip.top : parent.bottom
            anchors.bottomMargin: strip.visible ? vpx(10) : 0
            width: parent.width
            text: artPreview.file === "" || artPreview.file.indexOf("image://") === 0 ? ""
                : root.fileName(root.localPath(artPreview.file))
            color: "#bbb"
            elide: Text.ElideMiddle
            horizontalAlignment: Text.AlignHCenter
            font.family: globalFonts.sans
            font.pixelSize: vpx(15)
        }

        // every picture of the game: the one you're on is outlined, A on a
        // row edits it, touch one to go to its row
        ListView {
            id: strip
            visible: artPreview.thumbs.length > 1
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width, contentWidth)
            height: vpx(64)
            orientation: ListView.Horizontal
            spacing: vpx(8)
            interactive: contentWidth > parent.width
            clip: true
            model: artPreview.thumbs
            currentIndex: {
                for (var i = 0; i < artPreview.thumbs.length; i++) {
                    if (artPreview.thumbs[i].index === list.currentIndex)
                        return i;
                }
                return -1;
            }
            highlightRangeMode: ListView.ApplyRange
            preferredHighlightBegin: width * 0.3
            preferredHighlightEnd: width * 0.7
            delegate: Rectangle {
                readonly property bool isOn: index === strip.currentIndex
                readonly property bool vid: /\.(mp4|webm|mkv|avi|mov|pdf|cbz|cbr)$/i.test(modelData.file)
                width: vpx(88)
                height: strip.height
                color: "#1e2224"
                border.color: isOn ? "#3aa" : "#444"
                border.width: isOn ? vpx(3) : vpx(1)
                opacity: isOn ? 1 : 0.75
                Image {
                    anchors.fill: parent
                    anchors.margins: vpx(4)
                    visible: !parent.vid
                    source: parent.vid ? "" : root.toUrl(modelData.file)
                    cache: false  // a re-scraped file keeps its name: show the new one
                    sourceSize.width: vpx(160)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    clip: true
                }
                Text {
                    anchors.centerIn: parent
                    visible: parent.vid
                    text: /\.(pdf|cbz|cbr)$/i.test(modelData.file) ? "PDF" : "▶"
                    color: "#5cc"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(20)
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        list.currentIndex = modelData.index;
                        list.focus = true;
                    }
                }
            }
        }
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
        else if (page === "collection") {
            startDir = loc.file ? dirOf(loc.file) : "";
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
            var names = artFolders[artBase(key)] || [];
            for (var ri = 0; ri < roots.length; ri++) {
                for (var ni = 0; ni < names.length; ni++)
                    candidates.push(roots[ri] + "/" + names[ni]);
            }
            startDir = Internal.settings.firstExistingDir(candidates) || gameDir;
        }
        filePicker.pickFolders = false;
        filePicker.fileExtensions = artFilter(key);
        filePicker.openAt(startDir);
        filePicker.focus = true;
    }
    function addArt() {
        var entries = [];
        for (var i = 0; i < artKinds.length; i++) {
            var base = artKinds[i][0];
            var keys = artKeysOf(base);
            var next = 0;
            for (var k = 0; k < keys.length; k++)
                next = Math.max(next, parseInt(keys[k].substring(base.length + 1), 10) + 1);
            entries.push({ "act": base + "#" + next,
                           "text": hasArt(base) ? qsTr("%1 (another one)").arg(artKinds[i][1]) + api.tr : artKinds[i][1] });
        }
        chooser.openFor(qsTr("What kind?") + api.tr, entries, function(act) { root.pickArt(act); });
    }

    FilePicker {
        id: filePicker
        z: 3  // over the page's header

        fileNames: []
        fileExtensions: [".png", ".jpg"]
        infoText: qsTr("Pick the file. It's written relative to the metadata file when it's in the same folder.") + api.tr

        enabled: focus
        visible: opacity > 0.01
        opacity: focus ? 1.0 : 0.0
        Behavior on opacity { PropertyAnimation { duration: 300 } }

        onCancel: list.focus = true
        dataFolder: root.dataDir
        onPickFile: {
            if (pickFolders)
                return;
            // into the data folder, if there is one
            root.setEdit(root.artKey, root.storeArt(root.artKey, file_path));
            list.focus = true;
        }
        onPickFolder: {
            Internal.settings.dataDir = dir_path;
            root.dataDir = dir_path;
            root.message = qsTr("Art you pick from now on is copied into %1.").arg(dir_path) + api.tr;
            pickFolders = false;
            list.focus = true;
        }
        onNewFolderRequested: nameDialog.openFor(parent_path)
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
                cb(e.act, e);
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
        // a launch command wraps over several lines too (it's one line when saved)
        readonly property bool multiLine: !!field && (field.type === "multi" || field.type === "list" || field.type === "command")
        readonly property bool isCommand: !!field && field.type === "command"

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
                text = root.page !== "game" ? (root.collDetails.defaultCommand || "")
                     : root.isAndroidCollection(root.collObj) ? (root.launchInfo.command || "")
                     : root.collectionLaunch;
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
            suggestBar.current = 0;
            suggestBar.items = [];
            kb.target = multiLine ? multiInput : lineInput;
            kb.multiLine = multiLine && !isCommand;
            kb.row = 1;
            kb.col = 0;
            focus = true;
            kb.focus = true;
            suggestBar.update();
        }
        // the command, under a name, into the system's emulator list
        function saveToList() {
            var command = multiInput.text.replace(/\s*\n\s*/g, " ").trim();
            if (command === "") {
                hint.text = qsTr("Type the command first.") + api.tr;
                return;
            }
            closeDialog();
            nameDialog.openForEmulator(command);
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
                value = (f.type === "command" ? raw.replace(/\s*\n\s*/g, " ") : raw).trim();
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
                if ((f.key === "title" || f.key === "name" || f.key === "g:name") && value === "") {
                    hint.text = qsTr("The name can't be empty.") + api.tr;
                    return;
                }
                // the same as what it uses anyway: nothing of its own
                if (f.type === "command") {
                    if (root.page === "game" && !root.isAndroidCollection(root.collObj) && value === root.collectionLaunch)
                        value = "";
                    if (root.page === "game" && root.isAndroidCollection(root.collObj) && value === (root.launchInfo.command || ""))
                        value = "";
                    if (root.page === "collection" && (root.collDetails.launch || "") === "" && value === (root.collDetails.defaultCommand || ""))
                        value = "";
                }
            }
            closeDialog();
            root.setEdit(f.key, value);
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

        // Pegasus' own keyboard, along the bottom: d-pad and A, or touch
        OnScreenKeyboard {
            id: kb
            anchors.bottom: parent.bottom
            anchors.bottomMargin: vpx(12)
            anchors.horizontalCenter: parent.horizontalCenter
            z: 1
            keyH: vpx(40)
            onDone: fieldDialog.finish()
            onLeaveUp: (suggestBar.visible ? suggestBar : actionRow).focus = true
        }

        // genres you have, right above the keyboard, matching the line you're
        // typing (a few at a time: no long row to scroll through)
        FocusScope {
            id: suggestBar

            readonly property bool wanted: !!fieldDialog.field && !!fieldDialog.field.picks
            readonly property var lowered: root.genres.map(function(x) { return String(x).toLowerCase(); })
            property var items: []
            property int current: 0
            property bool pending: false

            visible: wanted && items.length > 0
            anchors.bottom: kb.top
            anchors.bottomMargin: vpx(8)
            anchors.horizontalCenter: kb.horizontalCenter
            width: kb.width
            height: vpx(42)
            z: 1

            // once per change of text or cursor, after the key is handled
            function update() {
                if (!wanted || pending)
                    return;
                pending = true;
                Qt.callLater(refresh);
            }
            function lineBounds() {
                var t = multiInput.text;
                var c = multiInput.cursorPosition;
                var start = t.lastIndexOf("\n", c - 1) + 1;
                var end = t.indexOf("\n", c);
                return { "start": start, "end": end < 0 ? t.length : end };
            }
            function refresh() {
                pending = false;
                if (!wanted) {
                    items = [];
                    return;
                }
                var t = multiInput.text;
                var b = lineBounds();
                var typed = t.substring(b.start, b.end).trim().toLowerCase();
                // the other lines: already in the list
                var have = {};
                var lines = (t.substring(0, b.start) + t.substring(b.end)).split("\n");
                for (var l = 0; l < lines.length; l++)
                    have[lines[l].trim().toLowerCase()] = true;
                var starts = [], inside = [];
                var max = 7;
                for (var i = 0; i < lowered.length && starts.length < max; i++) {
                    var g = lowered[i];
                    if (have[g] || g === typed)
                        continue;
                    if (typed === "" || g.indexOf(typed) === 0)
                        starts.push(root.genres[i]);
                    else if (inside.length < max && g.indexOf(typed) > 0)
                        inside.push(root.genres[i]);
                }
                var all = starts.concat(inside);
                var next = [];
                var used = 0;
                for (var w = 0; w < all.length; w++) {
                    var chip = Math.min(suggestMetrics.advanceWidth(all[w]) + vpx(24), vpx(220)) + vpx(8);
                    if (used + chip > width)
                        break;
                    used += chip;
                    next.push(all[w]);
                }
                if (JSON.stringify(next) !== JSON.stringify(items))
                    items = next;
                if (current >= items.length)
                    current = Math.max(0, items.length - 1);
                if (items.length === 0 && activeFocus)
                    kb.focus = true;
            }
            // the line you're on becomes the genre, then a new line for the next
            function apply(index) {
                var name = items[index];
                if (name === undefined)
                    return;
                var t = multiInput.text;
                var b = lineBounds();
                var before = t.substring(0, b.start) + name;
                if (b.end >= t.length) {
                    multiInput.text = before + "\n";
                    multiInput.cursorPosition = before.length + 1;
                }
                else {
                    multiInput.text = before + t.substring(b.end);
                    multiInput.cursorPosition = before.length + 1;
                }
                current = 0;
                kb.focus = true;
            }

            Keys.onLeftPressed: { if (current > 0) current--; }
            Keys.onRightPressed: { if (current < items.length - 1) current++; }
            Keys.onUpPressed: actionRow.focus = true
            Keys.onDownPressed: kb.focus = true
            Keys.onPressed: {
                if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                    event.accepted = true;
                    apply(current);
                }
            }

            FontMetrics {
                id: suggestMetrics
                font.family: globalFonts.sans
                font.pixelSize: vpx(17)
            }
            Row {
                anchors.centerIn: parent
                spacing: vpx(8)
                Repeater {
                    model: suggestBar.items
                    delegate: Rectangle {
                        readonly property bool highlighted: suggestBar.activeFocus && index === suggestBar.current
                        width: Math.min(suggestText.implicitWidth + vpx(24), vpx(220))
                        height: suggestBar.height
                        color: highlighted ? "#3aa" : "#333"
                        border.color: "#3aa"
                        border.width: highlighted ? 0 : vpx(1)
                        Text {
                            id: suggestText
                            width: Math.min(implicitWidth, vpx(196))
                            text: modelData
                            elide: Text.ElideRight
                            color: parent.highlighted ? "#111" : "#9dd"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(17)
                            anchors.centerIn: parent
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: suggestBar.apply(index)
                        }
                    }
                }
            }
        }

        Rectangle {
            width: Math.min(parent.width * 0.8, vpx(900))
            height: dialogColumn.height + vpx(40)
            anchors.horizontalCenter: parent.horizontalCenter
            y: vpx(14)
            color: "#444"

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
                        activeFocusOnPress: false  // no Android keyboard: Pegasus' own is below
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
                    height: fieldDialog.isCommand
                        ? Math.min(Math.max(vpx(170), multiInput.contentHeight + vpx(24)), root.height * 0.32)
                        : Math.min(Math.max(vpx(110), multiInput.contentHeight + vpx(24)), root.height * 0.2)

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
                            onTextChanged: suggestBar.update()
                            onCursorPositionChanged: suggestBar.update()
                            onCursorRectangleChanged: multiFlick.ensureVisible(cursorRectangle)
                            activeFocusOnPress: false
                            // the d-pad leaves the box from its last line
                            Keys.onDownPressed: {
                                if (cursorRectangle.y + cursorRectangle.height >= contentHeight - 2) {
                                    kb.focus = true;
                                    event.accepted = true;
                                }
                                else {
                                    event.accepted = false;
                                }
                            }
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
                    model: (fieldDialog.isCommand && !(root.page === "game" && root.isAndroidCollection(root.collObj)))
                        ? [ { "name": "cancel", "text": qsTr("Cancel") + api.tr },
                            { "name": "clear", "text": qsTr("Clear") + api.tr },
                            { "name": "savelist", "text": qsTr("Save to the list...") + api.tr } ]
                        : [ { "name": "cancel", "text": qsTr("Cancel") + api.tr },
                            { "name": "clear", "text": qsTr("Clear") + api.tr } ]
                    KeyNavigation.down: suggestBar.visible ? suggestBar : kb

                    Keys.onPressed: {
                        if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                            event.accepted = true;
                            var name = model[currentIndex].name;
                            if (name === "done")
                                fieldDialog.finish();
                            else if (name === "savelist")
                                fieldDialog.saveToList();
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
                        color: highlighted ? "#3aa" : (modelData.name === "done" ? "#555" : "#333")
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
                                if (modelData.name === "savelist")
                                    fieldDialog.saveToList();
                                else if (modelData.name === "done")
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

    // -- The scraper (its own screen, over Library) ---------------------------------

    Loader {
        id: scraperLoader
        anchors.fill: parent
        z: 7
        active: false
        enabled: focus
        visible: active && focus
        source: "ScraperScreen.qml"
        onLoaded: item.focus = true
        onFocusChanged: if (item && focus) item.focus = true
    }
    Connections {
        target: scraperLoader.item
        function onClose() {
            list.focus = true;
            // after the signal is done: the screen that sent it goes
            Qt.callLater(function() { scraperLoader.active = false; });
        }
    }

    // -- The scraper's matches to pick from (interactive mode, one game) -----------

    ScraperChoice {
        id: scraperChoice
        z: 9
        onFinished: {
            if (scraperLoader.active)
                scraperLoader.focus = true;
            else
                list.focus = true;
        }
    }
    Connections {
        target: Internal.settings
        function onScraperNeedsChoice(choice) { scraperChoice.openFor(choice); }
    }
    // waiting since before Library opened (scraping went on after you left)
    Timer {
        running: true
        interval: 300
        onTriggered: {
            var pending = Internal.settings.scraperPendingChoice();
            if (pending.candidates !== undefined)
                scraperChoice.openFor(pending);
        }
    }

    // -- A name for a new folder ---------------------------------------------------

    FocusScope {
        id: nameDialog

        property string parentPath: ""
        property string command: ""   // naming an emulator of yours, not a folder
        anchors.fill: parent
        enabled: focus
        visible: focus
        z: 6

        function back() {
            (command !== "" ? list : filePicker).focus = true;
        }
        function openForEmulator(cmd) {
            command = cmd;
            nameInput.text = "";
            nameKeyboard.row = 1;
            nameKeyboard.col = 0;
            focus = true;
            nameKeyboard.focus = true;
        }
        function openFor(path) {
            command = "";
            parentPath = path;
            nameInput.text = "";
            nameKeyboard.row = 1;
            nameKeyboard.col = 0;
            focus = true;
            nameKeyboard.focus = true;
        }
        function finish() {
            if (command !== "") {
                var label = nameInput.text.trim();
                if (label === "")
                    return;
                var cmd = command;
                list.focus = true;
                root.saveEmulator(label, cmd);
                return;
            }
            var name = nameInput.text.trim().replace(/[\\/:*?<>|\u0022]/g, "_");
            if (name === "")
                return;
            var path = parentPath + "/" + name;
            if (Internal.settings.makeDir(path))
                filePicker.openAt(path);
            filePicker.focus = true;
        }

        Keys.onPressed: {
            if (api.keys.isCancel(event) && !event.isAutoRepeat) {
                event.accepted = true;
                nameDialog.back();
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.5
            MouseArea { anchors.fill: parent }
        }
        Rectangle {
            width: Math.min(parent.width * 0.7, vpx(800))
            height: nameColumn.height + vpx(40)
            anchors.horizontalCenter: parent.horizontalCenter
            y: vpx(30)
            color: "#444"
            Column {
                id: nameColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(20)
                spacing: vpx(12)
                Text {
                    width: parent.width
                    text: (nameDialog.command !== ""
                           ? qsTr("A name for it in the list, eg. Xendroid Fork")
                           : qsTr("New folder in %1").arg(nameDialog.parentPath)) + api.tr
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(18)
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                }
                Rectangle {
                    width: parent.width
                    height: vpx(52)
                    color: "#333"
                    border.color: "#3aa"
                    border.width: vpx(2)
                    TextInput {
                        id: nameInput
                        anchors.fill: parent
                        anchors.leftMargin: vpx(14)
                        anchors.rightMargin: vpx(14)
                        verticalAlignment: TextInput.AlignVCenter
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(20)
                        cursorVisible: nameDialog.visible
                        activeFocusOnPress: false
                        clip: true
                    }
                }
            }
        }
        OnScreenKeyboard {
            id: nameKeyboard
            anchors.bottom: parent.bottom
            anchors.bottomMargin: vpx(12)
            anchors.horizontalCenter: parent.horizontalCenter
            keyH: vpx(40)
            target: nameInput
            onDone: nameDialog.finish()
        }
    }
}
