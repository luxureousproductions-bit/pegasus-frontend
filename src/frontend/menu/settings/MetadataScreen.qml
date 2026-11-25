// Pegasus Frontend
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
import "GenreKey.js" as GenreKey
import QtQuick 2.6


// Pegasus X: Settings > Gaming > Library > Metadata (Genres was its first
// tab alone). Three tabs, LB / RB or a tap: Genres, Publishers, Developers.
// Each lists its values with how many games have them (one-offs in amber),
// Search..., Only ... with 1 game and Sort (only what the list shows); a
// value lists its games (a game opens in Library), and its spellings that
// differ in ways you can't see side by side.
// At the top of each tab:
// - Suggested merges: values that look like one name (the same company
//   once punctuation, accents, "The", Inc / Ltd / ... are left out:
//   near-certain; once a region too, "of America", "Europe"...: regional;
//   1-2 letters off or one inside the other: maybe; a division, "Sega AM2",
//   never with "Sega"), looked for in the background. Nothing merges on its
//   own: a suggestion opens Merge, where A ticks the spellings, "Show as"
//   picks the name (the most used to start with), and a confirm merges.
// - Merged: the merges, each undone with A (and a confirm).
// A merge is an alias (metadata/aliases.json, MetadataAliases.h): the games
// show the one name at once, themes too, and after a restart or a rescrape;
// no game file is written. Genres keeps "Change this genre in all N games",
// which does write the games' files (their spellings as the files have them,
// so a merge's name never goes into a file it didn't come from).
FocusScope {
    id: root

    signal close
    signal openGame(var game)

    anchors.fill: parent

    property int tab: 0
    readonly property var fields: [ "genre", "publisher", "developer" ]
    readonly property string field: fields[tab]
    property string page: "values"   // values, games, pick, confirm (Change this genre...), suggestions, merge, merged
    property string genre: ""        // the value whose games are listed (as shown)
    property string genreKey: ""     // ...and its key (every spelling of it)
    property var allGenres: []       // [{ key, name, count, spellings }], A to Z (Settings.metadataValues)
    property var genreList: []       // ...as shown: searched, filtered, sorted (GenreKey.view)
    property string query: ""        // Search...: part of a name
    property bool onlyOnes: false    // Only ... with 1 game
    property string order: "az"      // Sort: az (A to Z), most, fewest (games)
    property var gameList: []        // [{ title, coll, obj, spelling }] of `genre`
    property var spellingList: []    // [{ name, count }] of `genre`, most games first
    property string target: ""       // Change this genre...: what it becomes
    property var plan: ({ "change": [], "same": 0 })  // ...the games it changes (saveGamesGenres' items), and how many keep theirs
    property string message: ""      // what the last change did (or its error)
    property bool pickedFromList: false  // the confirm page came from the pick page (not a spelling)
    // suggested merges and merges
    property var suggestions: []     // [{ level, games, values: [{ key, name, count }] }]
    property bool looking: false     // looked for, not ready yet
    property var merged: []          // [{ to, key, from: [{ name, count }], games }]
    property var mergeValues: []     // Merge: [{ key, name, count }]
    property var mergeTicked: ({})   // key -> true
    property string mergeShown: ""   // the name they show as
    property var mergeGames: []      // the games that change ([{ title, coll }])
    property int mergeLevel: -1      // the suggestion's level (-1: none)
    property var undoGroup: null     // Merged: the one to undo

    readonly property var tabNames: [ qsTr("Genres") + api.tr, qsTr("Publishers") + api.tr, qsTr("Developers") + api.tr ]
    readonly property string plural: tabNames[tab]
    function one(n) {
        return (tab === 0 ? (n === 1 ? qsTr("1 genre") : qsTr("%1 genres").arg(n))
              : tab === 1 ? (n === 1 ? qsTr("1 publisher") : qsTr("%1 publishers").arg(n))
              : (n === 1 ? qsTr("1 developer") : qsTr("%1 developers").arg(n))) + api.tr;
    }
    function games(n) { return (n === 1 ? qsTr("1 game") : qsTr("%1 games").arg(n)) + api.tr; }
    function levelText(level) {
        return (level === 0 ? qsTr("near-certain") : level === 1 ? qsTr("regional") : qsTr("maybe")) + api.tr;
    }
    // the log's prefix ("Genres: " as Genres always wrote)
    function tag() { return [ "Genres", "Publishers", "Developers" ][tab] + ": "; }

    // every value of the tab (spellings that differ in ways you can't see
    // count as one; the log shows how they differ), and its suggestions,
    // looked for in the background
    function readGenres() {
        var out = Internal.settings.metadataValues(api.allGames, field);
        for (var r = 0; r < out.length; r++) {
            var sp = out[r].spellings;
            if (sp.length < 2)
                continue;
            var parts = [];
            for (var s = 0; s < sp.length; s++)
                parts.push("\"" + sp[s].name + "\" (" + sp[s].count + "): " + GenreKey.codes(sp[s].name));
            console.log(tag() + "\"" + out[r].name + "\" counts " + sp.length + " spellings as one: " + parts.join(" | "));
        }
        allGenres = out;
        // (the log: the first values, so what's shown can be checked)
        var shown = [];
        for (var n = 0; n < out.length && n < 40; n++)
            shown.push(out[n].name + " (" + out[n].count + ")");
        console.log(tag() + out.length + " values: " + shown.join(", ") + (out.length > 40 ? ", ..." : ""));
        merged = Internal.settings.metadataMerged(field);
        looking = true;
        suggestions = [];
        Internal.settings.metadataSuggest(field);
        applyView();
    }
    Connections {
        target: Internal.settings
        function onMetadataSuggested(f, found, ms) {
            if (f !== root.field)
                return;
            // (the list keeps its place: a new model starts it at the top)
            var at = list.currentIndex;
            root.suggestions = found;
            root.looking = false;
            list.currentIndex = Math.min(at, list.count - 1);
            if (!list.selectable(list.currentIndex))
                list.jumpTo(false);
            var parts = [];
            for (var i = 0; i < found.length; i++) {
                var names = [];
                for (var v = 0; v < found[i].values.length; v++)
                    names.push(found[i].values[v].name);
                parts.push(root.levelText(found[i].level) + ": " + names.join(" + "));
            }
            console.log(root.tag() + "suggested merges (" + ms + " ms): " + (parts.length > 0 ? parts.join(" | ") : "none"));
        }
    }
    // the list shown: the search, the one-game filter and the sort applied
    // (only the view: no value is left out for good, nothing is written)
    function applyView() {
        genreList = GenreKey.view(allGenres, query, onlyOnes, order);
        if (query !== "" || onlyOnes || order !== "az") {
            var names = [];
            for (var i = 0; i < genreList.length; i++)
                names.push(genreList[i].name + " (" + genreList[i].count + ")");
            console.log(tag() + "search \"" + query + "\", only 1 game " + onlyOnes + ", sort " + order + ": " + names.join(", "));
        }
    }
    function orderText(o) {
        return (o === "most" ? qsTr("Most games first") : o === "fewest" ? qsTr("Fewest games first") : qsTr("A to Z")) + api.tr;
    }
    function readGames(key) {
        gameList = Internal.settings.metadataGames(field, key);
        var sp = [];
        for (var r = 0; r < allGenres.length; r++) {
            if (allGenres[r].key === key)
                sp = allGenres[r].spellings;
        }
        spellingList = sp;
    }
    // Change this genre...: every game of `genre` with the genre replaced by
    // `to` (GenreKey.replace: its other genres as they are), from the genres
    // as its file has them (a merge's name stays out of its file); the games
    // it doesn't change are counted
    function makePlan(to) {
        var change = [];
        var same = 0;
        var shownOf = function(name) { return Internal.settings.shownValue("genre", name); };
        for (var i = 0; i < gameList.length; i++) {
            var g = gameList[i].obj;
            var names = Internal.settings.rawValues(g, "genre");
            var now = GenreKey.replace(names, genreKey, to, shownOf);
            if (GenreKey.same(names, now)) {
                same++;
                continue;
            }
            // saved where Library saves the game: its collection, its file
            // (an Android app: its key)
            var files = g.files;
            change.push({ "obj": g, "coll": gameList[i].coll, "title": g.title, "genre": now,
                          "path": files && files.count > 0 ? files.get(0).path : Internal.settings.gameKey(g) });
        }
        return { "change": change, "same": same };
    }
    function openConfirm(to) {
        target = String(to).trim();
        if (target === "")
            return;
        plan = makePlan(target);
        page = "confirm";
        list.jumpTo(false);
        list.positionViewAtBeginning();
    }
    function applyPlan() {
        var items = plan.change;
        var result = Internal.settings.saveGamesGenres(items);
        var n = result.saved || 0;
        var spellingOnly = GenreKey.key(target) === genreKey;
        if (result.error)
            message = qsTr("Changed in %1 of %2 games. Not saved: %3").arg(n).arg(items.length).arg(result.error) + api.tr;
        else if (spellingOnly)
            message = (n === 1 ? qsTr("\"%1\" is now spelled \"%2\" in 1 more game.").arg(genre).arg(target)
                               : qsTr("\"%1\" is now spelled \"%2\" in %3 more games.").arg(genre).arg(target).arg(n)) + api.tr;
        else
            message = (n === 1 ? qsTr("\"%1\" is now \"%2\" in 1 game.").arg(genre).arg(target)
                               : qsTr("\"%1\" is now \"%2\" in %3 games.").arg(genre).arg(target).arg(n)) + api.tr;
        console.log("Genres: \"" + genre + "\" -> \"" + target + "\": " + n + " of " + items.length + " games saved"
                    + (result.error ? ", error: " + result.error : ""));
        // (as after a game saved in Library: the loaded games show it at
        // once, themes too; no read of the library)
        // the list as it is now, on the genre it became
        page = "values";
        genreKey = "";
        gameList = [];
        spellingList = [];
        readGenres();
        list.jumpTo(false);
        select("genre", target);
    }

    // a game's spelling as its number in spellingList (1 = the most used)
    function spellingNumber(name) {
        for (var i = 0; i < spellingList.length; i++) {
            if (spellingList[i].name === name)
                return i + 1;
        }
        return 0;
    }

    // -- Merge -------------------------------------------------------------------------------
    // a suggestion's values, all ticked, shown as the most used
    function openMerge(values, level) {
        mergeValues = values;
        mergeLevel = level;
        var t = {};
        for (var i = 0; i < values.length; i++)
            t[values[i].key] = true;
        mergeTicked = t;
        mergeShown = values.length > 0 ? values[0].name : "";
        readMergeGames();
        message = "";
        page = "merge";
        list.jumpTo(false);
        list.positionViewAtBeginning();
    }
    function tickedValues() {
        var out = [];
        for (var i = 0; i < mergeValues.length; i++) {
            if (mergeTicked[mergeValues[i].key])
                out.push(mergeValues[i]);
        }
        return out;
    }
    // the games whose name changes: those of the ticked values but the shown one
    function readMergeGames() {
        var out = [];
        var t = tickedValues();
        for (var i = 0; i < t.length; i++) {
            if (t[i].name === mergeShown)
                continue;
            var gs = Internal.settings.metadataGames(field, t[i].key);
            for (var g = 0; g < gs.length; g++)
                out.push({ "title": gs[g].title, "coll": gs[g].coll + " · " + gs[g].spelling });
        }
        mergeGames = out;
    }
    function toggleMerge(key) {
        var t = {};
        for (var k in mergeTicked)
            t[k] = mergeTicked[k];
        t[key] = !t[key];
        mergeTicked = t;
        // the shown name: still a ticked one
        var ticked = tickedValues();
        var still = false;
        for (var i = 0; i < ticked.length; i++)
            still = still || ticked[i].name === mergeShown;
        if (!still && ticked.length > 0)
            mergeShown = ticked[0].name;
        readMergeGames();
    }
    // Show as: the next ticked name
    function nextShown() {
        var ticked = tickedValues();
        if (ticked.length === 0)
            return;
        var at = 0;
        for (var i = 0; i < ticked.length; i++) {
            if (ticked[i].name === mergeShown)
                at = (i + 1) % ticked.length;
        }
        mergeShown = ticked[at].name;
        readMergeGames();
    }
    function doMerge() {
        var ticked = tickedValues();
        var keys = [];
        var names = [];
        for (var i = 0; i < ticked.length; i++) {
            keys.push(ticked[i].key);
            names.push(ticked[i].name);
        }
        var result = Internal.settings.metadataMerge(api.allGames, field, keys, mergeShown);
        console.log(tag() + "merge " + names.join(" + ") + " shown as \"" + mergeShown + "\": "
                    + (result.error ? "not made, " + result.error : result.spellings + " spellings, " + result.games + " games"));
        if (result.error)
            message = qsTr("Not merged: %1").arg(result.error) + api.tr;
        else
            message = qsTr("\"%1\" is shown for %2 now (%3). Your game files are as they were; Merged can undo it.")
                .arg(mergeShown).arg(names.join(", ")).arg(games(result.games)) + api.tr;
        var shown = mergeShown;
        page = "values";
        readGenres();
        list.jumpTo(false);
        select("genre", shown);
    }
    function openMerged() {
        merged = Internal.settings.metadataMerged(field);
        message = "";
        page = "merged";
        list.jumpTo(false);
        list.positionViewAtBeginning();
    }
    function doUndo() {
        var g = undoGroup;
        if (!g)
            return;
        var result = Internal.settings.metadataUndo(api.allGames, field, g.to);
        console.log(tag() + "undo \"" + g.to + "\": " + (result.error ? result.error : result.spellings + " spellings, " + result.games + " games"));
        readGenres();
        merged = Internal.settings.metadataMerged(field);
        message = result.error ? qsTr("Not undone: %1").arg(result.error) + api.tr
                               : qsTr("\"%1\" is undone: its spellings show as they are again.").arg(g.to) + api.tr;
        list.jumpTo(false);
    }

    function switchTab(to) {
        if (to < 0 || to > 2 || to === tab)
            return;
        tab = to;
        start();
        console.log("Metadata: tab " + tag().slice(0, -2));
    }

    readonly property var rows: {
        var out = [];
        var i;
        if (page === "pick") {
            out.push({ "kind": "info", "value": "",
                       "text": (gameList.length === 1 ? qsTr("Replace \"%1\" in its game with:").arg(genre)
                                : qsTr("Replace \"%1\" in its %2 games with:").arg(genre).arg(gameList.length)) + api.tr });
            out.push({ "kind": "type", "text": qsTr("Type a genre...") + api.tr, "value": "" });
            for (i = 0; i < allGenres.length; i++) {
                if (allGenres[i].key === genreKey)
                    continue;
                out.push({ "kind": "target", "text": allGenres[i].name, "value": games(allGenres[i].count) });
            }
            return out;
        }
        if (page === "confirm") {
            var spellingOnly = GenreKey.key(target) === genreKey;
            out.push({ "kind": "info", "value": "",
                       "text": (spellingOnly ? qsTr("Use the spelling \"%2\" for \"%1\"") : qsTr("Replace \"%1\" with \"%2\""))
                               .arg(genre).arg(target) + api.tr });
            var nc = plan.change.length;
            out.push({ "kind": "info", "value": "",
                       "text": (nc === 0 ? qsTr("No game changes") : nc === 1 ? qsTr("1 game changes") : qsTr("%1 games change").arg(nc)) + api.tr
                               + (plan.same > 0 ? qsTr(" · %1 already as it would be").arg(plan.same) + api.tr : "")
                               + (nc > 0 ? qsTr(" · their other genres stay") + api.tr : "") });
            out.push({ "kind": "cancel", "text": qsTr("Keep as is") + api.tr, "value": "" });
            if (nc > 0) {
                out.push({ "kind": "apply", "text": (nc === 1 ? qsTr("Change 1 game") : qsTr("Change %1 games").arg(nc)) + api.tr,
                           "value": "" });
                out.push({ "kind": "info", "text": qsTr("The games that change:") + api.tr, "value": "" });
                // (they can be moved to, so a long list scrolls)
                for (i = 0; i < nc; i++)
                    out.push({ "kind": "affected", "text": plan.change[i].title, "value": plan.change[i].coll });
            }
            return out;
        }
        if (page === "suggestions") {
            out.push({ "kind": "info", "value": "",
                       "text": (looking ? qsTr("Looking for names that may be one...")
                                : suggestions.length === 0 ? qsTr("No suggestions: no two names here look like one.")
                                : qsTr("%1 suggestions. Nothing merges until you open one and confirm.").arg(suggestions.length)) + api.tr });
            for (i = 0; i < suggestions.length; i++) {
                var sg = suggestions[i];
                var names = [];
                for (var v = 0; v < sg.values.length && v < 4; v++)
                    names.push(sg.values[v].name);
                out.push({ "kind": "suggestion", "index": i,
                           "text": names.join("  ·  ") + (sg.values.length > 4 ? qsTr("  +%1").arg(sg.values.length - 4) + api.tr : ""),
                           "value": levelText(sg.level) + " · " + games(sg.games), "one": sg.level === 2 });
            }
            return out;
        }
        if (page === "merge") {
            var ticked = tickedValues();
            out.push({ "kind": "info", "value": "",
                       "text": qsTr("Show these as one name. Your game files stay as they are; Merged can undo it.") + api.tr
                               + (mergeLevel >= 0 ? " (" + levelText(mergeLevel) + ")" : "") });
            out.push({ "kind": "shown", "text": qsTr("Show as") + api.tr, "value": "“" + mergeShown + "”" });
            out.push({ "kind": "info", "text": qsTr("Spellings · A ticks or unticks:") + api.tr, "value": "" });
            for (i = 0; i < mergeValues.length; i++) {
                var mv = mergeValues[i];
                out.push({ "kind": "mtick", "key": mv.key, "ticked": !!mergeTicked[mv.key],
                           "text": (mergeTicked[mv.key] ? "☑  " : "☐  ") + mv.name,
                           "value": games(mv.count) + (mv.name === mergeShown ? qsTr(" · shown") + api.tr : "") });
            }
            out.push({ "kind": "cancel", "text": qsTr("Keep as is") + api.tr, "value": "" });
            if (ticked.length >= 2)
                out.push({ "kind": "domerge", "text": qsTr("Merge %1 spellings").arg(ticked.length) + api.tr,
                           "value": games(mergeGames.length) + qsTr(" change name") + api.tr });
            else
                out.push({ "kind": "info", "text": qsTr("Tick two or more to merge them.") + api.tr, "value": "" });
            if (mergeGames.length > 0) {
                out.push({ "kind": "info", "text": qsTr("The games that show \"%1\" instead:").arg(mergeShown) + api.tr, "value": "" });
                for (i = 0; i < mergeGames.length; i++)
                    out.push({ "kind": "affected", "text": mergeGames[i].title, "value": mergeGames[i].coll });
            }
            return out;
        }
        if (page === "merged") {
            out.push({ "kind": "info", "value": "",
                       "text": (merged.length === 0 ? qsTr("Nothing merged yet.")
                                : qsTr("Shown as one name; your game files have the spellings on the right. A undoes one.")) + api.tr });
            if (message !== "")
                out.push({ "kind": "info", "text": message, "value": "" });
            for (i = 0; i < merged.length; i++) {
                var from = [];
                for (var f = 0; f < merged[i].from.length; f++)
                    from.push(merged[i].from[f].name + " (" + merged[i].from[f].count + ")");
                out.push({ "kind": "mgroup", "index": i, "text": merged[i].to, "value": from.join(", ") });
            }
            return out;
        }
        if (page === "games") {
            out.push({ "kind": "info", "text": games(gameList.length), "value": "" });
            if (field === "genre")
                out.push({ "kind": "change", "value": "",
                           "text": (gameList.length === 1 ? qsTr("Change this genre in its game...") : qsTr("Change this genre in all %1 games...").arg(gameList.length)) + api.tr });
            // its spellings side by side: each as written (what differs from
            // the first in amber, a space as · there), how many games, and in
            // words what differs, with the character codes
            var many = spellingList.length > 1;
            if (many) {
                out.push({ "kind": "info", "text": qsTr("%1 spellings, counted as one here:").arg(spellingList.length) + api.tr,
                           "value": "" });
                var main = spellingList[0].name;
                for (i = 0; i < spellingList.length; i++) {
                    var sc = spellingList[i].count;
                    out.push({ "kind": "spelling", "name": spellingList[i].name,
                               "text": (i + 1) + "&nbsp;&nbsp;" + GenreKey.markup(spellingList[i].name, i === 0 ? "" : main),
                               "note": i === 0 ? qsTr("the most used") + api.tr : GenreKey.note(spellingList[i].name, main),
                               "value": games(sc), "one": sc === 1 });
                }
                out.push({ "kind": "info", "text": qsTr("Its games:") + api.tr, "value": "" });
            }
            for (i = 0; i < gameList.length; i++) {
                var num = many ? spellingNumber(gameList[i].spelling) : 0;
                out.push({ "kind": "game", "text": gameList[i].title, "index": i,
                           "value": gameList[i].coll + (num > 0 ? qsTr(" · spelling %1").arg(num) + api.tr : "") });
            }
            return out;
        }
        var ones = 0;
        for (i = 0; i < allGenres.length; i++) {
            if (allGenres[i].count === 1)
                ones++;
        }
        var narrowed = query !== "" || onlyOnes;
        out.push({ "kind": "info", "text": (allGenres.length === 0 ? qsTr("None in your library yet") + api.tr
                                            : qsTr("%1 · %2 with only one game").arg(one(allGenres.length)).arg(ones) + api.tr)
                                           + (narrowed ? qsTr(" · %1 shown").arg(genreList.length) + api.tr : ""),
                   "value": "" });
        if (message !== "")
            out.push({ "kind": "info", "text": message, "value": "" });
        // suggestions and merges first, then what the list shows (search,
        // filter, sort): A opens or switches
        out.push({ "kind": "suggested", "text": qsTr("Suggested merges") + api.tr,
                   "value": (looking ? qsTr("looking...") : suggestions.length === 0 ? qsTr("none") : String(suggestions.length)) + api.tr,
                   "one": !looking && suggestions.length > 0 });
        out.push({ "kind": "merged", "text": qsTr("Merged") + api.tr,
                   "value": merged.length === 0 ? qsTr("none") + api.tr : one(merged.length) });
        if (allGenres.length > 0) {
            out.push({ "kind": "search", "text": (tab === 0 ? qsTr("Search genres...") : tab === 1 ? qsTr("Search publishers...") : qsTr("Search developers...")) + api.tr,
                       "value": query !== "" ? "“" + query + "”" : "" });
            out.push({ "kind": "ones", "text": (tab === 0 ? qsTr("Only genres with 1 game") : tab === 1 ? qsTr("Only publishers with 1 game") : qsTr("Only developers with 1 game")) + api.tr,
                       "value": (onlyOnes ? qsTr("On") : qsTr("Off")) + api.tr });
            out.push({ "kind": "sort", "text": qsTr("Sort") + api.tr, "value": orderText(order) });
            if (genreList.length === 0)
                out.push({ "kind": "info", "text": qsTr("Nothing matches.") + api.tr, "value": "" });
        }
        for (i = 0; i < genreList.length; i++) {
            var c = genreList[i].count;
            var nSp = genreList[i].spellings.length;
            out.push({ "kind": "genre", "text": genreList[i].name, "key": genreList[i].key, "one": c === 1,
                       "value": games(c) + (nSp > 1 ? qsTr(" · %1 spellings").arg(nSp) + api.tr : "") });
        }
        return out;
    }

    // the first row of a kind
    function selectKind(kind) {
        for (var i = 0; i < rows.length; i++) {
            if (rows[i].kind === kind) {
                list.currentIndex = i;
                list.positionViewAtIndex(i, ListView.Center);
                return;
            }
        }
    }
    // a row by its text (a value: by its key, as the spelling shown may change)
    function select(kind, text) {
        for (var i = 0; i < rows.length; i++) {
            if (rows[i].kind === kind && (kind === "genre" ? rows[i].key === GenreKey.key(text) : rows[i].text === text)) {
                list.currentIndex = i;
                list.positionViewAtIndex(i, ListView.Center);
                return;
            }
        }
        list.jumpTo(false);
    }

    // back from a game in Library (or opened again): counts and lists as they are now
    function refresh() {
        readGenres();
        // the open value as it's shown now (its most used spelling)
        for (var g = 0; g < allGenres.length; g++) {
            if (allGenres[g].key === genreKey)
                genre = allGenres[g].name;
        }
        if (page === "games") {
            var at = list.currentIndex >= 0 && list.currentIndex < rows.length ? rows[list.currentIndex].text : "";
            readGames(genreKey);
            if (gameList.length === 0) {
                page = "values";
                select("genre", genre);
                return;
            }
            select("game", at);
            return;
        }
        var name = list.currentIndex >= 0 && list.currentIndex < rows.length ? rows[list.currentIndex].text : "";
        select("genre", name);
    }

    // opened (from Library): Genres first
    function open() {
        tab = 0;
        start();
    }
    // the tab's values as they are now, from the top
    function start() {
        page = "values";
        genre = "";
        genreKey = "";
        gameList = [];
        spellingList = [];
        message = "";
        query = "";
        onlyOnes = false;
        order = "az";
        readGenres();
        list.jumpTo(false);
        list.positionViewAtBeginning();
    }

    function activate(row) {
        if (row.kind === "genre") {
            message = "";
            genre = row.text;
            genreKey = row.key;
            readGames(genreKey);
            page = "games";
            list.jumpTo(false);
        }
        else if (row.kind === "game") {
            root.openGame(gameList[row.index].obj);
        }
        else if (row.kind === "change") {
            message = "";
            page = "pick";
            list.jumpTo(false);
            list.positionViewAtBeginning();
        }
        else if (row.kind === "spelling") {
            if (field !== "genre")
                return;
            // this spelling for every game of the genre
            message = "";
            pickedFromList = false;
            openConfirm(row.name);
        }
        else if (row.kind === "target") {
            pickedFromList = true;
            openConfirm(row.text);
        }
        else if (row.kind === "type") {
            typeDialog.open("change");
        }
        else if (row.kind === "cancel") {
            back();
        }
        else if (row.kind === "apply") {
            applyPlan();
        }
        else if (row.kind === "search") {
            typeDialog.open("search");
        }
        else if (row.kind === "ones") {
            onlyOnes = !onlyOnes;
            applyView();
            select("ones", row.text);
        }
        else if (row.kind === "sort") {
            order = order === "az" ? "most" : order === "most" ? "fewest" : "az";
            applyView();
            select("sort", row.text);
        }
        else if (row.kind === "suggested") {
            message = "";
            page = "suggestions";
            list.jumpTo(false);
            list.positionViewAtBeginning();
        }
        else if (row.kind === "suggestion") {
            var sg = suggestions[row.index];
            openMerge(sg.values, sg.level);
        }
        else if (row.kind === "merged") {
            openMerged();
        }
        else if (row.kind === "mtick") {
            var at = list.currentIndex;
            toggleMerge(row.key);
            list.currentIndex = at;
            list.positionViewAtIndex(at, ListView.Contain);
        }
        else if (row.kind === "shown") {
            nextShown();
            selectKind("shown");
        }
        else if (row.kind === "domerge") {
            mergeDialog.show();
        }
        else if (row.kind === "mgroup") {
            undoGroup = merged[row.index];
            undoDialog.show();
        }
    }
    function back() {
        // Change this genre...: back to the genre's page (from the confirm
        // page: to the pick page if that's where it came from)
        if (page === "confirm" || page === "pick") {
            var fromPick = page === "confirm" && pickedFromList;
            page = fromPick ? "pick" : "games";
            if (fromPick)
                select("target", target);
            else
                list.jumpTo(false);
            return;
        }
        if (page === "merge") {
            // nothing merged: the suggestions again
            page = "suggestions";
            list.jumpTo(false);
            return;
        }
        if (page === "suggestions" || page === "merged") {
            var from = page === "suggestions" ? "suggested" : "merged";
            page = "values";
            for (var i = 0; i < rows.length; i++) {
                if (rows[i].kind === from)
                    list.currentIndex = i;
            }
            return;
        }
        if (page === "games") {
            page = "values";
            readGenres();
            select("genre", genre);
            return;
        }
        root.close();
    }

    Keys.onPressed: {
        if (event.isAutoRepeat)
            return;
        if (api.keys.isCancel(event)) {
            event.accepted = true;
            back();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#222"
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: root.back()
        }
    }

    ScreenHeader {
        id: header
        text: (root.page === "games" ? qsTr("Metadata > %1 > %2").arg(root.plural).arg(root.genre)
            : root.page === "pick" ? qsTr("Metadata > Genres > %1 > Change to").arg(root.genre)
            : root.page === "confirm" ? qsTr("Metadata > Genres > %1 > Change").arg(root.genre)
            : root.page === "suggestions" ? qsTr("Metadata > %1 > Suggested merges").arg(root.plural)
            : root.page === "merge" ? qsTr("Metadata > %1 > Merge").arg(root.plural)
            : root.page === "merged" ? qsTr("Metadata > %1 > Merged").arg(root.plural)
            : qsTr("Library > Metadata")) + api.tr
        z: 2
    }

    // the tabs: LB / RB (on a tab's list), or a tap
    Row {
        id: tabBar
        anchors.top: header.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: list.width
        height: vpx(40)
        visible: root.page === "values"
        Repeater {
            model: root.tabNames
            delegate: Item {
                width: tabBar.width / 3
                height: tabBar.height
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    color: index === root.tab ? "#eee" : "#888"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(19)
                    font.bold: index === root.tab
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: vpx(3)
                    color: index === root.tab ? "#3aa" : "#444"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        list.focus = true;
                        root.switchTab(index);
                    }
                }
            }
        }
    }

    ListView {
        id: list

        anchors.top: tabBar.visible ? tabBar.bottom : header.bottom
        anchors.topMargin: tabBar.visible ? vpx(8) : 0
        anchors.bottom: hints.top
        anchors.bottomMargin: vpx(8)
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.7
        clip: true
        focus: true
        model: root.rows

        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: height * 0.3
        preferredHighlightEnd: height * 0.7
        // focus moves at once (no slide to catch up with a held D-pad)
        highlightMoveDuration: 0

        // (every row but the info ones: a spelling (A: that spelling for the
        // whole genre) and a game that changes (A: nothing) too, so the
        // d-pad scrolls through them)
        function selectable(i) {
            return i >= 0 && i < count && root.rows[i].kind !== "info";
        }
        // quick moves in a long list: LB / RB 10 rows (stopping at the ends), LT top, RT bottom
        function jumpTo(end) {
            var i = end ? count - 1 : 0;
            while (i >= 0 && i < count && !selectable(i))
                i += end ? -1 : 1;
            if (i >= 0 && i < count)
                currentIndex = i;
            else
                currentIndex = 0;
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
        Keys.onPressed: {
            // a tab's list: LB / RB the tabs; elsewhere 10 rows
            if (api.keys.isPrevPage(event)) {
                event.accepted = true;
                if (root.page === "values") root.switchTab(root.tab - 1); else jumpBy(-10);
                return;
            }
            if (api.keys.isNextPage(event)) {
                event.accepted = true;
                if (root.page === "values") root.switchTab(root.tab + 1); else jumpBy(10);
                return;
            }
            if (api.keys.isPageUp(event)) { event.accepted = true; jumpTo(false); return; }
            if (api.keys.isPageDown(event)) { event.accepted = true; jumpTo(true); return; }
            if (api.keys.isAccept(event) && !event.isAutoRepeat && selectable(currentIndex)) {
                event.accepted = true;
                root.activate(root.rows[currentIndex]);
            }
        }

        delegate: Item {
            readonly property bool isInfo: modelData.kind === "info"
            readonly property bool isSpelling: modelData.kind === "spelling"
            readonly property bool highlighted: ListView.isCurrentItem && list.activeFocus && !isInfo

            width: ListView.view.width
            height: isInfo ? vpx(42) : (isSpelling ? vpx(76) : vpx(22) * 2.4)

            Rectangle {
                width: parent.width
                height: vpx(3)
                anchors.bottom: parent.bottom
                color: "#3aa"
                visible: parent.highlighted
            }
            Text {
                id: rowLabel
                text: modelData.text
                // a spelling: its marks (GenreKey.markup escapes the text);
                // the rest as plain text (a title may have a "<")
                textFormat: parent.isSpelling ? Text.StyledText : Text.PlainText
                color: parent.isInfo ? "#999" : "#eee"
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(21)
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.verticalCenter: parent.isSpelling ? undefined : parent.verticalCenter
                anchors.top: parent.isSpelling ? parent.top : undefined
                anchors.topMargin: vpx(10)
                width: parent.isInfo ? parent.width - vpx(60) : parent.width * 0.6
                elide: Text.ElideRight
            }
            // a spelling: how it differs from the first, in words and codes
            Text {
                visible: parent.isSpelling
                text: modelData.note || ""
                color: "#999"
                font.family: globalFonts.sans
                font.pixelSize: vpx(16)
                anchors.left: rowLabel.left
                anchors.leftMargin: vpx(28)
                anchors.top: rowLabel.bottom
                anchors.topMargin: vpx(6)
                width: parent.width - vpx(90)
                elide: Text.ElideRight
            }
            Text {
                visible: !parent.isInfo || modelData.value !== ""
                text: modelData.value + (["genre", "search", "change", "type", "target", "suggested", "merged", "suggestion"].indexOf(modelData.kind) >= 0 ? "   ›" : "")
                color: modelData.one ? "#e8b06a" : "#c0c0c0"
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(21)
                anchors.left: rowLabel.right
                anchors.leftMargin: vpx(16)
                anchors.right: parent.right
                anchors.rightMargin: vpx(30)
                anchors.verticalCenter: parent.isSpelling ? undefined : parent.verticalCenter
                anchors.top: parent.isSpelling ? parent.top : undefined
                anchors.topMargin: vpx(10)
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
            }
            MouseArea {
                anchors.fill: parent
                enabled: !parent.isInfo
                onClicked: {
                    list.currentIndex = index;
                    list.focus = true;
                    root.activate(modelData);
                }
            }
        }
    }

    // one hint bar, the pictures of the other Pegasus X screens
    RoundPromptBar {
        id: hints
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        visible: !typeDialog.visible && !mergeDialog.visible && !undoDialog.visible
        text: (root.page === "values" ? qsTr("A open · LB / RB tabs · LT / RT top / bottom · B back")
             : root.page === "merge" ? qsTr("A tick / choose · LB / RB 10 rows · LT / RT top / bottom · B back")
             : qsTr("A choose · LB / RB 10 rows · LT / RT top / bottom · B back")) + api.tr
    }

    // Merge: asked once more (Cancel first)
    PegasusDialog {
        id: mergeDialog
        z: 6
        function show() {
            choice = 0;
            focus = true;
        }
        title: qsTr("Merge?") + api.tr
        text: {
            var t = root.tickedValues();
            var names = [];
            for (var i = 0; i < t.length; i++)
                names.push(t[i].name);
            return qsTr("Show %1 as \"%2\"? %3 show the new name. Your game files stay as they are, and Merged can undo it.")
                .arg(names.join(", ")).arg(root.mergeShown).arg(root.games(root.mergeGames.length)) + api.tr;
        }
        buttons: [ qsTr("Cancel") + api.tr, qsTr("Merge") + api.tr ]
        onPicked: {
            focus = false;
            list.focus = true;
            if (index === 1)
                root.doMerge();
        }
        onCancelled: {
            focus = false;
            list.focus = true;
        }
    }
    // Merged: undo, asked once more
    PegasusDialog {
        id: undoDialog
        z: 6
        function show() {
            choice = 0;
            focus = true;
        }
        title: qsTr("Undo this merge?") + api.tr
        text: {
            var g = root.undoGroup;
            if (!g)
                return "";
            var from = [];
            for (var i = 0; i < g.from.length; i++)
                from.push(g.from[i].name);
            return qsTr("%1 show as they are again, not as \"%2\".").arg(from.join(", ")).arg(g.to) + api.tr;
        }
        buttons: [ qsTr("Cancel") + api.tr, qsTr("Undo") + api.tr ]
        onPicked: {
            focus = false;
            list.focus = true;
            if (index === 1)
                root.doUndo();
        }
        onCancelled: {
            focus = false;
            list.focus = true;
        }
    }

    // -- Change this genre... > Type a genre... ---------------------------------------------
    // Pegasus' own keyboard (d-pad and A, or touch), the genres you have as
    // suggestions; Start (or Done) takes it, B goes back to the list
    FocusScope {
        id: typeDialog

        anchors.fill: parent
        enabled: focus
        visible: focus
        z: 4

        property string mode: "change"   // change (Type a genre...), search (Search genres...)
        property string before: ""       // search: the search when it opened (B puts it back)
        function open(how) {
            mode = how;
            before = root.query;
            typeInput.text = how === "search" ? root.query : "";
            typeInput.cursorPosition = typeInput.text.length;
            typeKeyboard.row = 1;
            typeKeyboard.col = 0;
            focus = true;
            typeKeyboard.focus = true;
        }
        // search: the list narrows as you type
        function typed() {
            if (!visible || mode !== "search")
                return;
            root.query = typeInput.text;
            root.applyView();
        }
        function finish() {
            var text = typeInput.text.trim();
            if (mode === "search") {
                root.query = text;  // (empty: the whole list again)
                root.applyView();
                list.focus = true;
                root.selectKind("search");
                return;
            }
            if (text === "")
                return;
            list.focus = true;
            root.pickedFromList = true;
            root.openConfirm(text);
        }
        function cancel() {
            if (mode === "search") {
                root.query = before;
                root.applyView();
            }
            list.focus = true;
            if (mode === "search")
                root.selectKind("search");
        }
        function genreNames() {
            var out = [];
            for (var i = 0; i < root.allGenres.length; i++)
                out.push(root.allGenres[i].name);
            return out;
        }

        Keys.onPressed: {
            if (api.keys.isCancel(event) && !event.isAutoRepeat) {
                event.accepted = true;
                typeDialog.cancel();
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
            height: typeColumn.height + vpx(40)
            anchors.horizontalCenter: parent.horizontalCenter
            y: vpx(30)
            color: "#444"
            Column {
                id: typeColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(20)
                spacing: vpx(12)
                Text {
                    width: parent.width
                    text: (typeDialog.mode === "search" ? qsTr("Search %1: part of a name").arg(root.plural.toLowerCase())
                                                        : qsTr("Replace \"%1\" with").arg(root.genre)) + api.tr
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
                        id: typeInput
                        anchors.fill: parent
                        anchors.leftMargin: vpx(14)
                        anchors.rightMargin: vpx(14)
                        verticalAlignment: TextInput.AlignVCenter
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(20)
                        cursorVisible: typeDialog.visible
                        activeFocusOnPress: false  // no Android keyboard: Pegasus' own is below
                        clip: true
                        onTextChanged: typeDialog.typed()
                    }
                }
            }
        }
        OnScreenKeyboard {
            id: typeKeyboard
            anchors.bottom: parent.bottom
            anchors.bottomMargin: vpx(12)
            anchors.horizontalCenter: parent.horizontalCenter
            keyH: vpx(40)
            target: typeInput
            suggestions: typeDialog.visible && typeDialog.mode === "change" ? typeDialog.genreNames() : []
            onDone: typeDialog.finish()
        }
    }
}
