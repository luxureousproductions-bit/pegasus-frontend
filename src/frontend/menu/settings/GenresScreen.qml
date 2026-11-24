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


// Pegasus X: Settings > Gaming > Library > Genres. Every genre in the library
// with how many games have it (one-offs in amber, so they stand out); a genre
// lists its games, and a game opens in Library to fix its genre there. A
// genre with more than one spelling lists them above its games, side by
// side, what differs marked. Above the list: Search genres..., Only genres
// with 1 game and Sort (A to Z, most or fewest games): they only change what
// the list shows, for as long as Genres is open. On a genre's
// page, "Change this genre..." (or A on one of its spellings) replaces it in
// all its games at once, after a page that shows which games change: the
// one thing here that writes (each game's other genres stay).
FocusScope {
    id: root

    signal close
    signal openGame(var game)

    anchors.fill: parent

    property string page: "genres"   // genres, games, pick, confirm (Change this genre...)
    property string genre: ""        // the one whose games are listed (as shown)
    property string genreKey: ""     // ...and its key (GenreKey.key: every spelling of it)
    property var allGenres: []       // [{ key, name, count, spellings }], A to Z (GenreKey.group)
    property var genreList: []       // ...as shown: searched, filtered, sorted (GenreKey.view)
    property string query: ""        // Search genres...: part of a name
    property bool onlyOnes: false    // Only genres with 1 game
    property string order: "az"      // Sort: az (A to Z), most, fewest (games)
    property var gameList: []        // [{ title, coll, obj, spelling }] of `genre`
    property var spellingList: []    // [{ name, count }] of `genre`, most games first (GenreKey.group)
    property string target: ""       // Change this genre...: what it becomes
    property var plan: ({ "change": [], "same": 0 })  // ...the games it changes (saveGamesGenres' items), and how many keep theirs
    property string message: ""      // what the last change did (or its error)
    property bool pickedFromList: false  // the confirm page came from the pick page (not a spelling)

    // the genres Android uses to sort apps, as the keyboard's suggestions leave out
    function skipped(low) { return low === "application" || low === "emulator"; }

    // spellings of one genre that differ in ways you can't see (an apostrophe,
    // a space, letter case) count as one; the log shows how they differ
    function readGenres() {
        var perGame = [];
        var all = api.allGames;
        for (var i = 0; i < all.count; i++)
            perGame.push(all.get(i).genreList || []);
        var out = GenreKey.group(perGame, skipped);
        for (var r = 0; r < out.length; r++) {
            var sp = out[r].spellings;
            if (sp.length < 2)
                continue;
            var parts = [];
            for (var s = 0; s < sp.length; s++)
                parts.push("\"" + sp[s].name + "\" (" + sp[s].count + "): " + GenreKey.codes(sp[s].name));
            console.log("Genres: \"" + out[r].name + "\" counts " + sp.length + " spellings as one: " + parts.join(" | "));
        }
        allGenres = out;
        applyView();
    }
    // the list shown: the search, the one-game filter and the sort applied
    // (only the view: no genre is left out for good, nothing is written)
    function applyView() {
        genreList = GenreKey.view(allGenres, query, onlyOnes, order);
        if (query !== "" || onlyOnes || order !== "az") {
            var names = [];
            for (var i = 0; i < genreList.length; i++)
                names.push(genreList[i].name + " (" + genreList[i].count + ")");
            console.log("Genres: search \"" + query + "\", only 1 game " + onlyOnes + ", sort " + order + ": " + names.join(", "));
        }
    }
    function orderText(o) {
        return (o === "most" ? qsTr("Most games first") : o === "fewest" ? qsTr("Fewest games first") : qsTr("A to Z")) + api.tr;
    }
    function readGames(key) {
        var out = [];
        var all = api.allGames;
        for (var i = 0; i < all.count; i++) {
            var g = all.get(i);
            var names = g.genreList || [];
            for (var n = 0; n < names.length; n++) {
                if (GenreKey.key(names[n]) === key) {
                    var colls = g.collections;
                    out.push({ "title": g.title, "obj": g, "spelling": String(names[n]).trim(),
                               "coll": colls && colls.count > 0 ? colls.get(0).name : "" });
                    break;
                }
            }
        }
        out.sort(function(a, b) { return a.title.toLowerCase().localeCompare(b.title.toLowerCase()); });
        gameList = out;
        var sp = [];
        for (var r = 0; r < allGenres.length; r++) {
            if (allGenres[r].key === key)
                sp = allGenres[r].spellings;
        }
        spellingList = sp;
    }
    // Change this genre...: every game of `genre` with the genre replaced by
    // `to` (GenreKey.replace: its other genres as they are); the games it
    // doesn't change are counted
    function makePlan(to) {
        var change = [];
        var same = 0;
        for (var i = 0; i < gameList.length; i++) {
            var g = gameList[i].obj;
            var names = g.genreList || [];
            var now = GenreKey.replace(names, genreKey, to);
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
        page = "genres";
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
                var tc = allGenres[i].count;
                out.push({ "kind": "target", "text": allGenres[i].name,
                           "value": (tc === 1 ? qsTr("1 game") : qsTr("%1 games").arg(tc)) + api.tr });
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
        if (page === "games") {
            out.push({ "kind": "info", "text": (gameList.length === 1 ? qsTr("1 game") : qsTr("%1 games").arg(gameList.length)) + api.tr,
                       "value": "" });
            out.push({ "kind": "change", "value": "",
                       "text": (gameList.length === 1 ? qsTr("Change this genre in its game...") : qsTr("Change this genre in all %1 games...").arg(gameList.length)) + api.tr });
            // its spellings side by side: each as written (what differs from
            // the first in amber, a space as · there), how many games, and in
            // words what differs, with the character codes
            var many = spellingList.length > 1;
            if (many) {
                out.push({ "kind": "info", "text": qsTr("%1 spellings, counted as one genre here:").arg(spellingList.length) + api.tr,
                           "value": "" });
                var main = spellingList[0].name;
                for (i = 0; i < spellingList.length; i++) {
                    var sc = spellingList[i].count;
                    out.push({ "kind": "spelling", "name": spellingList[i].name,
                               "text": (i + 1) + "&nbsp;&nbsp;" + GenreKey.markup(spellingList[i].name, i === 0 ? "" : main),
                               "note": i === 0 ? qsTr("the most used") + api.tr : GenreKey.note(spellingList[i].name, main),
                               "value": (sc === 1 ? qsTr("1 game") : qsTr("%1 games").arg(sc)) + api.tr, "one": sc === 1 });
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
        out.push({ "kind": "info", "text": (allGenres.length === 0 ? qsTr("No genres in your library yet")
                                            : qsTr("%1 genres · %2 with only one game").arg(allGenres.length).arg(ones)) + api.tr
                                           + (narrowed ? qsTr(" · %1 shown").arg(genreList.length) + api.tr : ""),
                   "value": "" });
        if (message !== "")
            out.push({ "kind": "info", "text": message, "value": "" });
        // what the list shows (search, filter, sort): A opens or switches
        if (allGenres.length > 0) {
            out.push({ "kind": "search", "text": qsTr("Search genres...") + api.tr,
                       "value": query !== "" ? "\u201C" + query + "\u201D" : "" });
            out.push({ "kind": "ones", "text": qsTr("Only genres with 1 game") + api.tr,
                       "value": (onlyOnes ? qsTr("On") : qsTr("Off")) + api.tr });
            out.push({ "kind": "sort", "text": qsTr("Sort") + api.tr, "value": orderText(order) });
            if (genreList.length === 0)
                out.push({ "kind": "info", "text": qsTr("No genre matches.") + api.tr, "value": "" });
        }
        for (i = 0; i < genreList.length; i++) {
            var c = genreList[i].count;
            var nSp = genreList[i].spellings.length;
            out.push({ "kind": "genre", "text": genreList[i].name, "key": genreList[i].key, "one": c === 1,
                       "value": (c === 1 ? qsTr("1 game") : qsTr("%1 games").arg(c)) + api.tr
                              + (nSp > 1 ? qsTr(" · %1 spellings").arg(nSp) + api.tr : "") });
        }
        return out;
    }

    // a row by its text (a genre: by its key, as the spelling shown may change)
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
        // the open genre as it's shown now (its most used spelling)
        for (var g = 0; g < allGenres.length; g++) {
            if (allGenres[g].key === genreKey)
                genre = allGenres[g].name;
        }
        if (page === "games") {
            var at = list.currentIndex >= 0 && list.currentIndex < rows.length ? rows[list.currentIndex].text : "";
            readGames(genreKey);
            if (gameList.length === 0) {
                page = "genres";
                select("genre", genre);
                return;
            }
            select("game", at);
            return;
        }
        var name = list.currentIndex >= 0 && list.currentIndex < rows.length ? rows[list.currentIndex].text : "";
        select("genre", name);
    }

    // opened from the Scraper: the genres as they are now, from the top
    function start() {
        page = "genres";
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
        if (page === "games") {
            page = "genres";
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
        text: root.page === "games" ? qsTr("Genres > %1").arg(root.genre) + api.tr
            : root.page === "pick" ? qsTr("Genres > %1 > Change to").arg(root.genre) + api.tr
            : root.page === "confirm" ? qsTr("Genres > %1 > Change").arg(root.genre) + api.tr
            : qsTr("Library > Genres") + api.tr
        z: 2
    }

    ListView {
        id: list

        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.bottomMargin: vpx(20)
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
            if (api.keys.isPrevPage(event)) { event.accepted = true; jumpBy(-10); return; }
            if (api.keys.isNextPage(event)) { event.accepted = true; jumpBy(10); return; }
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
                width: parent.width * 0.6
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
                text: modelData.value + (["genre", "search", "change", "type", "target"].indexOf(modelData.kind) >= 0 ? "   ›" : "")
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
                root.select("search", qsTr("Search genres...") + api.tr);
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
                root.select("search", qsTr("Search genres...") + api.tr);
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
                    text: (typeDialog.mode === "search" ? qsTr("Search genres: part of a name")
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
