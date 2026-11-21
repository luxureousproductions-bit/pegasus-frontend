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
import QtQuick 2.6


// Pegasus X: Settings > Gaming > Scraper > Genres. Every genre in the library
// with how many games have it (one-offs in amber, so they stand out); a genre
// lists its games, and a game opens in Library to fix its genre there.
// Only shows: nothing is merged or rewritten here.
FocusScope {
    id: root

    signal close
    signal openGame(var game)

    anchors.fill: parent

    property string page: "genres"   // genres, games
    property string genre: ""        // the one whose games are listed
    property var genreList: []       // [{ name, count }], A to Z
    property var gameList: []        // [{ title, coll, obj }] of `genre`

    // the genres Android uses to sort apps, as the keyboard's suggestions leave out
    function skipped(low) { return low === "application" || low === "emulator"; }

    function readGenres() {
        var counts = {};
        var all = api.allGames;
        for (var i = 0; i < all.count; i++) {
            var names = all.get(i).genreList || [];
            var seen = {};
            for (var n = 0; n < names.length; n++) {
                var name = String(names[n]).trim();
                if (name === "" || seen[name] || skipped(name.toLowerCase()))
                    continue;
                seen[name] = true;
                counts[name] = (counts[name] || 0) + 1;
            }
        }
        var out = [];
        for (var key in counts)
            out.push({ "name": key, "count": counts[key] });
        out.sort(function(a, b) {
            var c = a.name.toLowerCase().localeCompare(b.name.toLowerCase());
            return c !== 0 ? c : a.name.localeCompare(b.name);
        });
        genreList = out;
    }
    function readGames(name) {
        var out = [];
        var all = api.allGames;
        for (var i = 0; i < all.count; i++) {
            var g = all.get(i);
            var names = g.genreList || [];
            for (var n = 0; n < names.length; n++) {
                if (String(names[n]).trim() === name) {
                    var colls = g.collections;
                    out.push({ "title": g.title, "obj": g,
                               "coll": colls && colls.count > 0 ? colls.get(0).name : "" });
                    break;
                }
            }
        }
        out.sort(function(a, b) { return a.title.toLowerCase().localeCompare(b.title.toLowerCase()); });
        gameList = out;
    }

    readonly property var rows: {
        var out = [];
        var i;
        if (page === "games") {
            out.push({ "kind": "info", "text": (gameList.length === 1 ? qsTr("1 game") : qsTr("%1 games").arg(gameList.length)) + api.tr,
                       "value": "" });
            for (i = 0; i < gameList.length; i++)
                out.push({ "kind": "game", "text": gameList[i].title, "value": gameList[i].coll, "index": i });
            return out;
        }
        var ones = 0;
        for (i = 0; i < genreList.length; i++) {
            if (genreList[i].count === 1)
                ones++;
        }
        out.push({ "kind": "info", "text": (genreList.length === 0 ? qsTr("No genres in your library yet")
                                            : qsTr("%1 genres · %2 with only one game").arg(genreList.length).arg(ones)) + api.tr,
                   "value": "" });
        for (i = 0; i < genreList.length; i++) {
            var c = genreList[i].count;
            out.push({ "kind": "genre", "text": genreList[i].name, "one": c === 1,
                       "value": (c === 1 ? qsTr("1 game") : qsTr("%1 games").arg(c)) + api.tr });
        }
        return out;
    }

    function select(kind, text) {
        for (var i = 0; i < rows.length; i++) {
            if (rows[i].kind === kind && rows[i].text === text) {
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
        if (page === "games") {
            var at = list.currentIndex >= 0 && list.currentIndex < rows.length ? rows[list.currentIndex].text : "";
            readGames(genre);
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
        gameList = [];
        readGenres();
        list.jumpTo(false);
        list.positionViewAtBeginning();
    }

    function activate(row) {
        if (row.kind === "genre") {
            genre = row.text;
            readGames(genre);
            page = "games";
            list.jumpTo(false);
        }
        else if (row.kind === "game") {
            root.openGame(gameList[row.index].obj);
        }
    }
    function back() {
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
        text: root.page === "games" ? qsTr("Genres > %1").arg(root.genre) + api.tr : qsTr("Scraper > Genres") + api.tr
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

        function selectable(i) {
            return i >= 0 && i < count && root.rows[i].kind !== "info";
        }
        // quick moves in a long list: LB top, RB bottom, LT/RT 10 rows
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
            if (api.keys.isPrevPage(event)) { event.accepted = true; jumpTo(false); return; }
            if (api.keys.isNextPage(event)) { event.accepted = true; jumpTo(true); return; }
            if (api.keys.isPageUp(event)) { event.accepted = true; jumpBy(-10); return; }
            if (api.keys.isPageDown(event)) { event.accepted = true; jumpBy(10); return; }
            if (api.keys.isAccept(event) && !event.isAutoRepeat && selectable(currentIndex)) {
                event.accepted = true;
                root.activate(root.rows[currentIndex]);
            }
        }

        delegate: Item {
            readonly property bool isInfo: modelData.kind === "info"
            readonly property bool highlighted: ListView.isCurrentItem && list.activeFocus && !isInfo

            width: ListView.view.width
            height: isInfo ? vpx(42) : vpx(22) * 2.4

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
                color: parent.isInfo ? "#999" : "#eee"
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(21)
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width * 0.6
                elide: Text.ElideRight
            }
            Text {
                text: modelData.value + (modelData.kind === "genre" ? "   ›" : "")
                color: modelData.one ? "#e8b06a" : "#c0c0c0"
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(21)
                anchors.left: rowLabel.right
                anchors.leftMargin: vpx(16)
                anchors.right: parent.right
                anchors.rightMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
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
}
