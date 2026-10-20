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


// Pegasus X: hide games and collections from every theme, show them again,
// and keep the Android collections together. Changes are applied when the
// screen is left, by reading the game list again from the cache (quick).
FocusScope {
    id: root

    signal close

    anchors.fill: parent
    enabled: focus
    visible: 0 < (x + width) && x < Window.window.width

    property var hidden: Internal.settings.hiddenItems()   // { collections, games }
    property bool mergeAndroid: Internal.settings.mergeAndroid
    property bool changed: false
    property string message: ""

    property string collName: ""      // the open collection, empty: the list
    property bool collVisible: false  // it's in the library right now
    property var collGames: []        // [{ title, path, key }] of its loaded games

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
    function isGameHidden(path) {
        for (var i = 0; i < hidden.games.length; i++) {
            if (hidden.games[i].path === path)
                return true;
        }
        return false;
    }
    function reloadHidden() {
        hidden = Internal.settings.hiddenItems();
        changed = true;
    }

    // the collections Pegasus shows now, and the hidden ones it doesn't
    readonly property var collections: {
        var out = [];
        var seen = {};
        for (var i = 0; i < api.collections.count; i++) {
            var c = api.collections.get(i);
            seen[c.name] = true;
            out.push({ "name": c.name, "games": c.games.count, "loaded": true });
        }
        for (var h = 0; h < hidden.collections.length; h++) {
            if (!seen[hidden.collections[h]])
                out.push({ "name": hidden.collections[h], "games": 0, "loaded": false });
        }
        out.sort(function(a, b) { return a.name.localeCompare(b.name); });
        return out;
    }

    function openCollection(name, loaded) {
        collName = name;
        collVisible = loaded;
        collGames = loaded ? Internal.settings.collectionGames(api.collections, name) : [];
        message = "";
        list.currentIndex = 1;
        list.positionViewAtBeginning();
    }
    function backToList() {
        var name = collName;
        collName = "";
        message = "";
        for (var i = 0; i < collections.length; i++) {
            if (collections[i].name === name) {
                list.currentIndex = i + 2;  // below the Android row and the section
                return;
            }
        }
        list.currentIndex = 0;
    }
    function leave() {
        if (changed)
            Internal.settings.reloadLibrary();
        root.close();
    }

    Keys.onPressed: {
        if (api.keys.isCancel(event) && !event.isAutoRepeat) {
            event.accepted = true;
            if (root.collName !== "")
                root.backToList();
            else
                root.leave();
        }
    }

    PegasusUtils.HorizontalSwipeArea {
        anchors.fill: parent
        onSwipeRight: root.collName !== "" ? root.backToList() : root.leave()
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: root.collName !== "" ? root.backToList() : root.leave()
    }

    ScreenHeader {
        id: header
        text: (root.collName !== "" ? qsTr("Library > %1").arg(root.collName) : qsTr("Settings > Library")) + api.tr
        z: 2
    }


    // keeps the selected row when the rows are rebuilt
    function keepPosition(change) {
        var index = list.currentIndex;
        var y = list.contentY;
        change();
        list.currentIndex = index;
        list.contentY = y;
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
            if (root.collName === "") {
                out.push({ "kind": "android", "text": qsTr("Combine Android collections") + api.tr,
                           "value": root.mergeAndroid ? qsTr("On") + api.tr : qsTr("Off") + api.tr });
                out.push({ "kind": "section", "text": qsTr("Collections") + api.tr });
                for (var i = 0; i < root.collections.length; i++) {
                    var c = root.collections[i];
                    var hiddenColl = root.isCollectionHidden(c.name);
                    var hiddenGames = root.hiddenGamesOf(c.name).length;
                    var value = hiddenColl ? qsTr("Hidden") + api.tr
                        : hiddenGames > 0 ? qsTr("%1 games · %2 hidden").arg(c.games).arg(hiddenGames) + api.tr
                        : qsTr("%1 games").arg(c.games) + api.tr;
                    out.push({ "kind": "collection", "text": c.name, "value": value, "hidden": hiddenColl, "coll": c });
                }
                return out;
            }

            var collHidden = root.isCollectionHidden(root.collName);
            out.push({ "kind": "section", "text": qsTr("Collection") + api.tr });
            out.push({ "kind": "hidecoll", "text": qsTr("Hide collection") + api.tr,
                       "value": collHidden ? qsTr("Hidden") + api.tr : qsTr("Shown") + api.tr, "hidden": collHidden });

            var games = [];
            var known = {};
            for (var g = 0; g < root.collGames.length; g++) {
                known[root.collGames[g].key] = true;
                games.push({ "title": root.collGames[g].title, "path": root.collGames[g].key });
            }
            var hiddenList = root.hiddenGamesOf(root.collName);
            for (var hg = 0; hg < hiddenList.length; hg++) {
                if (!known[hiddenList[hg].path])
                    games.push({ "title": hiddenList[hg].title, "path": hiddenList[hg].path });
            }
            games.sort(function(a, b) { return a.title.localeCompare(b.title); });

            out.push({ "kind": "section", "text": qsTr("Games · %1 hidden").arg(hiddenList.length) + api.tr });
            if (!root.collVisible)
                out.push({ "kind": "info", "text": qsTr("Show the collection to list all of its games here.") + api.tr, "value": "" });
            for (var r = 0; r < games.length; r++) {
                var gameHidden = root.isGameHidden(games[r].path);
                out.push({ "kind": "game", "text": games[r].title, "path": games[r].path, "hidden": gameHidden,
                           "value": gameHidden ? qsTr("Hidden") + api.tr : qsTr("Shown") + api.tr });
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

        function activate(index) {
            if (!selectable(index))
                return;
            var row = rows[index];
            if (row.kind === "android") {
                root.keepPosition(function() {
                    Internal.settings.mergeAndroid = !root.mergeAndroid;
                    root.changed = true;
                });
                root.message = root.mergeAndroid
                    ? qsTr("Your Android collections become one \"Android\", games first.") + api.tr
                    : qsTr("Your Android collections stay as they are.") + api.tr;
            }
            else if (row.kind === "collection") {
                root.openCollection(row.coll.name, row.coll.loaded);
            }
            else if (row.kind === "hidecoll") {
                root.keepPosition(function() {
                    Internal.settings.setCollectionHidden(root.collName, !row.hidden);
                    root.reloadHidden();
                });
            }
            else if (row.kind === "game") {
                root.keepPosition(function() {
                    Internal.settings.setGameHidden(row.path, row.text, [root.collName], !row.hidden);
                    root.reloadHidden();
                });
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
                && ((ListView.isCurrentItem && ListView.view.activeFocus) || rowMouse.containsMouse)

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
                color: parent.isInfo ? "#999" : (modelData.hidden ? "#999" : "#eee")
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(22)
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                width: parent.isInfo ? parent.width - vpx(60) : parent.width * 0.55
                elide: Text.ElideRight
            }
            Text {
                visible: !parent.isSection && !parent.isInfo
                text: modelData.value || ""
                color: modelData.hidden ? "#e8b06a" : "#c0c0c0"
                font.family: globalFonts.sans
                font.pixelSize: vpx(22)
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
                    list.activate(index);
                }
            }
        }
    }

    Text {
        id: messageBar
        text: root.message !== "" ? root.message
            : root.changed ? qsTr("Changes are applied when you leave this screen.") + api.tr
            : qsTr("Hidden games and collections are left out of every theme. Nothing on your storage changes.") + api.tr
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
}
