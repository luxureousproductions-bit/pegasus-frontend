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


// Pegasus X: Android apps to import (Settings > Library: the Android
// collection, and Options; first-time setup). Every installed app that can be
// opened, with its icon and name, ticked if it's imported, and the section it
// goes into: Application, Emulator or Games. Only the ticked apps are in the
// library. Search apps... narrows the list; Select all and Deselect all act on
// the apps shown. Each change is saved at once; the library takes it when
// it's read again (leaving Library, or the next step of setup).
FocusScope {
    id: root

    signal close

    anchors.fill: parent

    property bool startOnLoad: true   // opened on its own (setup); Library calls start()
    property var apps: []             // [{ package, label, kind, selected }], A to Z
    property var shown: []            // ...the ones the search leaves (indexes into apps)
    property string query: ""         // Search apps...: part of a name or package
    property bool changed: false      // something saved: the library is read again
    property string message: ""

    readonly property var kinds: [ "Application", "Emulator", "Game" ]
    function kindText(k) {
        return (k === "Application" ? qsTr("Application") : k === "Emulator" ? qsTr("Emulator") : qsTr("Games")) + api.tr;
    }

    Component.onCompleted: if (startOnLoad) start()

    function start() {
        apps = Internal.settings.androidAppChoices();
        query = "";
        message = "";
        changed = false;
        applyView();
        list.jumpTo(false);
        list.positionViewAtBeginning();
        console.log("Android apps to import: " + apps.length + " apps, " + pickedCount() + " picked");
    }
    function applyView() {
        var q = query.trim().toLowerCase();
        var out = [];
        for (var i = 0; i < apps.length; i++) {
            if (q === "" || apps[i].label.toLowerCase().indexOf(q) >= 0 || apps[i]["package"].toLowerCase().indexOf(q) >= 0)
                out.push(i);
        }
        shown = out;
    }
    function pickedCount() {
        var n = 0;
        for (var i = 0; i < apps.length; i++) {
            if (apps[i].selected)
                n++;
        }
        return n;
    }

    // saved at once: the picked apps, each with its section (kept as it is
    // now, so an update of the app doesn't move it)
    function save(what) {
        var picked = [];
        var kindOf = {};
        for (var i = 0; i < apps.length; i++) {
            if (!apps[i].selected)
                continue;
            picked.push(apps[i]["package"]);
            kindOf[apps[i]["package"]] = apps[i].kind;
        }
        Internal.settings.setAndroidAppChoices(picked, kindOf);
        changed = true;
        message = what;
        console.log("Android apps to import: " + what + " (" + picked.length + " picked)");
    }
    // the list keeps its place when the rows are made again
    function keep(change) {
        var index = list.currentIndex;
        var y = list.contentY;
        change();
        list.currentIndex = index;
        list.contentY = y;
    }
    function setApp(i, field, value) {
        var next = apps.slice();
        var app = {};
        for (var k in next[i])
            app[k] = next[i][k];
        app[field] = value;
        next[i] = app;
        apps = next;
    }
    function toggle(i) {
        var on = !apps[i].selected;
        var app = apps[i];
        keep(function() {
            root.setApp(i, "selected", on);
            root.save((on ? qsTr("\"%1\" is imported (%2)").arg(app.label).arg(root.kindText(app.kind))
                          : qsTr("\"%1\" isn't imported").arg(app.label)) + api.tr);
        });
    }
    function nextKind(i) {
        var k = kinds[(kinds.indexOf(apps[i].kind) + 1) % kinds.length];
        var app = apps[i];
        keep(function() {
            root.setApp(i, "kind", k);
            if (app.selected)
                root.save(qsTr("\"%1\" goes into %2").arg(app.label).arg(root.kindText(k)) + api.tr);
            else
                root.message = qsTr("\"%1\" goes into %2 when it's ticked").arg(app.label).arg(root.kindText(k)) + api.tr;
        });
    }
    function setAll(on) {
        var next = apps.slice();
        var n = 0;
        for (var s = 0; s < shown.length; s++) {
            var i = shown[s];
            if (next[i].selected === on)
                continue;
            var app = {};
            for (var k in next[i])
                app[k] = next[i][k];
            app.selected = on;
            next[i] = app;
            n++;
        }
        keep(function() {
            root.apps = next;
            root.save((on ? qsTr("%1 more apps ticked").arg(n) : qsTr("%1 apps unticked").arg(n)) + api.tr);
        });
    }

    readonly property var rows: {
        var out = [];
        var narrowed = query !== "";
        out.push({ "kind": "info", "value": "",
                   "text": (apps.length === 0 ? qsTr("No apps found")
                            : qsTr("%1 apps · %2 imported").arg(apps.length).arg(pickedCount())) + api.tr
                           + (narrowed ? qsTr(" · %1 shown").arg(shown.length) + api.tr : "") });
        out.push({ "kind": "info", "value": "",
                   "text": message !== "" ? message
                         : qsTr("Only the ticked apps are in your library, each in its section. Saved at once.") + api.tr });
        if (apps.length > 0) {
            out.push({ "kind": "search", "text": qsTr("Search apps...") + api.tr,
                       "value": narrowed ? "“" + query + "”" : "" });
            out.push({ "kind": "all", "text": (narrowed ? qsTr("Select all shown") : qsTr("Select all")) + api.tr, "value": "" });
            out.push({ "kind": "none", "text": (narrowed ? qsTr("Deselect all shown") : qsTr("Deselect all")) + api.tr, "value": "" });
            if (shown.length === 0)
                out.push({ "kind": "info", "text": qsTr("No app matches.") + api.tr, "value": "" });
        }
        for (var s = 0; s < shown.length; s++) {
            var app = apps[shown[s]];
            out.push({ "kind": "app", "index": shown[s], "text": app.label, "pkg": app["package"],
                       "selected": app.selected, "value": kindText(app.kind) });
        }
        return out;
    }

    function select(kind) {
        for (var i = 0; i < rows.length; i++) {
            if (rows[i].kind === kind) {
                list.currentIndex = i;
                list.positionViewAtIndex(i, ListView.Center);
                return;
            }
        }
        list.jumpTo(false);
    }

    function activate(row) {
        if (row.kind === "app")
            toggle(row.index);
        else if (row.kind === "all")
            setAll(true);
        else if (row.kind === "none")
            setAll(false);
        else if (row.kind === "search")
            typeDialog.open();
    }
    function back() {
        if (query !== "") {
            // first the search goes, then the screen
            query = "";
            applyView();
            select("search");
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
        text: qsTr("Library > Android apps to import") + api.tr
        z: 2
    }

    ListView {
        id: list

        anchors.top: header.bottom
        anchors.bottom: prompts.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.7
        clip: true
        focus: true
        model: root.rows

        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: height * 0.3
        preferredHighlightEnd: height * 0.7
        highlightMoveDuration: 0

        function selectable(i) {
            return i >= 0 && i < count && root.rows[i].kind !== "info";
        }
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
            if (event.isAutoRepeat || !selectable(currentIndex))
                return;
            if (api.keys.isAccept(event)) {
                event.accepted = true;
                root.activate(root.rows[currentIndex]);
            }
            else if (api.keys.isDetails(event) && root.rows[currentIndex].kind === "app") {
                // X: the next section (Application, Emulator, Games)
                event.accepted = true;
                root.nextKind(root.rows[currentIndex].index);
            }
        }

        delegate: Item {
            readonly property bool isInfo: modelData.kind === "info"
            readonly property bool isApp: modelData.kind === "app"
            readonly property bool highlighted: ListView.isCurrentItem && list.activeFocus && !isInfo

            width: ListView.view.width
            height: isInfo ? vpx(42) : isApp ? vpx(60) : vpx(22) * 2.4

            Rectangle {
                width: parent.width
                height: vpx(3)
                anchors.bottom: parent.bottom
                color: "#3aa"
                visible: parent.highlighted
            }
            // the tick
            Text {
                id: tick
                visible: parent.isApp
                text: modelData.selected ? "☑" : "☐"
                color: modelData.selected ? "#3cc" : "#888"
                font.family: globalFonts.sans
                font.pixelSize: vpx(28)
                anchors.left: parent.left
                anchors.leftMargin: vpx(20)
                anchors.verticalCenter: parent.verticalCenter
            }
            // the app's icon (a letter when there's none)
            Rectangle {
                id: iconBox
                visible: parent.isApp
                width: vpx(44)
                height: vpx(44)
                anchors.left: tick.right
                anchors.leftMargin: vpx(14)
                anchors.verticalCenter: parent.verticalCenter
                color: icon.status === Image.Ready ? "transparent" : "#3a3a3a"
                Text {
                    anchors.centerIn: parent
                    visible: icon.status !== Image.Ready
                    text: String(modelData.text || "?").charAt(0).toUpperCase()
                    color: "#bbb"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(22)
                }
                Image {
                    id: icon
                    anchors.fill: parent
                    source: parent.visible ? "image://androidicons/" + modelData.pkg : ""
                    sourceSize.width: vpx(44)
                    sourceSize.height: vpx(44)
                    asynchronous: true
                    cache: true
                    fillMode: Image.PreserveAspectFit
                }
            }
            Text {
                id: rowLabel
                text: modelData.text
                color: parent.isInfo ? "#999" : (parent.isApp && !modelData.selected ? "#aaa" : "#eee")
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(21)
                anchors.left: parent.isApp ? iconBox.right : parent.left
                anchors.leftMargin: parent.isApp ? vpx(16) : vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                width: parent.isInfo ? parent.width - vpx(60) : parent.width * 0.5
                elide: Text.ElideRight
            }
            Text {
                id: rowValue
                text: modelData.value + (modelData.kind === "search" ? "   ›" : "")
                color: parent.isApp && !modelData.selected ? "#888" : "#c0c0c0"
                font.family: globalFonts.sans
                font.pixelSize: vpx(19)
                anchors.right: parent.right
                anchors.rightMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
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
            // touch: the section on the right changes it (as X does)
            MouseArea {
                visible: parent.isApp
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: rowValue.width + vpx(60)
                onClicked: {
                    list.currentIndex = index;
                    list.focus = true;
                    root.nextKind(modelData.index);
                }
            }
        }
    }

    PromptBar {
        id: prompts
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        text: qsTr("A tick / untick · X change section · LB/RB top/bottom · LT/RT 10 rows · B back") + api.tr
    }

    FocusScope {
        id: typeDialog

        anchors.fill: parent
        enabled: focus
        visible: focus
        z: 4

        property string before: ""   // the search when it opened (B puts it back)
        function open() {
            before = root.query;
            typeInput.text = root.query;
            typeInput.cursorPosition = typeInput.text.length;
            typeKeyboard.row = 1;
            typeKeyboard.col = 0;
            focus = true;
            typeKeyboard.focus = true;
        }
        function typed() {
            if (!visible)
                return;
            root.query = typeInput.text;
            root.applyView();
        }
        function finish() {
            root.query = typeInput.text.trim();  // (empty: the whole list again)
            root.applyView();
            list.focus = true;
            root.select("search");
        }
        function cancel() {
            root.query = before;
            root.applyView();
            list.focus = true;
            root.select("search");
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
                    text: qsTr("Search apps: part of a name") + api.tr
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(18)
                    horizontalAlignment: Text.AlignHCenter
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
            suggestions: []
            onDone: typeDialog.finish()
        }
    }
}
