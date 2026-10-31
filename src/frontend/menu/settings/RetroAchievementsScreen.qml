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
import "qrc:/qmlutils" as PegasusUtils
import QtQuick 2.6


// Pegasus X: Settings > Library > RetroAchievements. Your games found on
// RetroAchievements by their ROM's hash (rcheevos' rules), your progress
// on them, and the login it takes (your own username and web API key).
FocusScope {
    id: root

    signal close

    anchors.fill: parent
    enabled: focus
    visible: focus

    property var s: Internal.settings.raSettings()
    property var status: ({})       // Settings.raStatus
    property var run: Internal.settings.raState()       // the live progress (the match row's second line reads it)
    property bool running: !!run.running                   // what the rows depend on: changes twice a run, not every tick
    property var login: ({})        // { ok, text }
    property bool checking: false
    property string message: ""
    property string lastRun: ""
    property bool changed: false    // the library is read again on the way out

    function set(key, value) {
        Internal.settings.setRaSetting(key, value);
        var next = Object.assign({}, s);
        next[key] = value;
        keepPosition(function() { s = next; });
        if (key === "user" || key === "key")
            checkLogin();
    }
    function checkLogin() {
        checking = true;
        Internal.settings.raCheckLogin();
    }
    function refreshStatus() {
        keepPosition(function() { status = Internal.settings.raStatus(api.collections); });
    }
    function keepPosition(fn) {
        var i = list.currentIndex;
        fn();
        list.currentIndex = Math.min(i, Math.max(0, list.count - 1));
    }
    Component.onCompleted: {
        openedAt = Date.now();
        refreshStatus();
        checkLogin();
        list.currentIndex = 1;
    }
    Connections {
        target: Internal.settings
        function onRaLoginChecked(result) {
            root.checking = false;
            root.keepPosition(function() { root.login = result; });
        }
        function onRaProgress(state) {
            root.run = state;
            if (!!state.running !== root.running)
                root.keepPosition(function() { root.running = !!state.running; });
        }
        function onRaFinished(result) {
            root.run = Internal.settings.raState();
            root.keepPosition(function() { root.running = false; });
            root.refreshStatus();
            // (the loaded games took their fields in place: no library read needed)
            root.lastRun = result.error && !(result.matched > 0)
                ? result.error
                : qsTr("Done: %1 found (%2 with achievements), %3 not on RetroAchievements, %4 couldn't be checked, progress for %5")
                    .arg(result.matched).arg(result.withAchievements).arg(result.notFound).arg(result.unsupported).arg(result.progress) + api.tr
                  + (result.stopped ? qsTr(" (stopped)") + api.tr : "");
            root.message = "";
        }
    }

    function match(onlyNew) {
        if (running) {
            Internal.settings.raStop();
            message = qsTr("Stopping...") + api.tr;
            return;
        }
        var error = Internal.settings.raMatch(api.collections, onlyNew);
        if (error !== "") {
            message = error;
            return;
        }
        lastRun = "";
        message = qsTr("Working in the background: you can leave Settings and play.") + api.tr;
    }
    function refreshProgress() {
        if (running)
            return;
        var error = Internal.settings.raRefreshProgress(api.collections);
        message = error !== "" ? error : qsTr("Fetching your progress...") + api.tr;
    }
    function updateLists() {
        if (running)
            return;
        chooser.openFor(qsTr("Download the game lists again? They're kept a week; a new list picks up games added to RetroAchievements since.") + api.tr, [
            { "act": "yes", "text": qsTr("Download them again") + api.tr },
            { "act": "no", "text": qsTr("Keep them") + api.tr },
        ], function(act) {
            if (act !== "yes")
                return;
            Internal.settings.raForgetLists();
            root.refreshStatus();
            root.match(true);
        });
    }

    readonly property string matchSub: {
        if (run.running) {
            var stage = run.stage || "";
            return ((run.total || 0) > 0 && (run.done || 0) > 0
                    ? qsTr("Working: %1 · %2 of %3").arg(stage).arg(run.done).arg(run.total)
                    : qsTr("Working: %1").arg(stage)) + api.tr;
        }
        if (lastRun !== "")
            return lastRun;
        var st = status || {};
        if ((st.checked || 0) === 0)
            return qsTr("Every ROM is hashed the way RetroAchievements does and looked up, in the background") + api.tr;
        return qsTr("%1 of %2 games checked").arg(st.checked).arg(st.games) + api.tr;
    }

    readonly property var rows: {
        var out = [];
        var st = status || {};
        out.push({ "kind": "section", "text": qsTr("Your games") + api.tr });
        out.push({ "kind": "match", "text": running ? qsTr("Stop") + api.tr : qsTr("Match my games") + api.tr,
                   "sub": " ", "value": running ? "" : (st.checked > 0 && st.checked < st.games
                        ? qsTr("%1 new ›").arg(st.games - st.checked) + api.tr : qsTr("%1 games ›").arg(st.games || 0) + api.tr) });
        if (st.checked > 0 && st.checked < st.games && !running)
            out.push({ "kind": "matchall", "text": qsTr("Check every game again") + api.tr, "value": qsTr("%1 games").arg(st.games) + api.tr });
        out.push({ "kind": "info", "text": qsTr("Matched") + api.tr,
                   "value": (st.checked || 0) === 0 ? qsTr("nothing checked yet") + api.tr
                          : qsTr("%1 with achievements · %2 without · %3 not on RetroAchievements · %4 unknown")
                                .arg(st.withAchievements || 0).arg((st.matched || 0) - (st.withAchievements || 0)).arg(st.notFound || 0).arg(st.unsupported || 0) + api.tr,
                   "ok": (st.withAchievements || 0) > 0 });
        out.push({ "kind": "progress", "text": qsTr("Your progress") + api.tr,
                   "value": (st.total || 0) > 0 ? qsTr("%1 of %2 achievements earned · A fetches it again").arg(st.earned).arg(st.total) + api.tr
                                                : qsTr("fetched when your games are matched") + api.tr });
        out.push({ "kind": "section", "text": qsTr("Account") + api.tr });
        out.push({ "kind": "text", "key": "user", "text": qsTr("Username") + api.tr,
                   "value": s.user !== "" ? s.user : qsTr("not set") + api.tr, "muted": s.user === "" });
        out.push({ "kind": "text", "key": "key", "secret": true, "text": qsTr("Web API key") + api.tr,
                   "value": s.key !== "" ? "••••" + String(s.key).slice(-4) : qsTr("not set") + api.tr, "muted": s.key === "" });
        out.push({ "kind": "info", "text": qsTr("Status") + api.tr,
                   "value": checking ? qsTr("checking...") + api.tr : (login.text || ""), "ok": !!login.ok });
        out.push({ "kind": "help", "text": qsTr("How to get your key") + api.tr, "value": "" });
        out.push({ "kind": "section", "text": qsTr("Options") + api.tr });
        out.push({ "kind": "toggle", "key": "icons", "text": qsTr("Download the game icons") + api.tr,
                   "value": s.icons ? qsTr("On") + api.tr : qsTr("Off") + api.tr });
        out.push({ "kind": "toggle", "key": "autoRefresh", "text": qsTr("Refresh my progress after a game") + api.tr,
                   "value": s.autoRefresh ? qsTr("On") + api.tr : qsTr("Off") + api.tr });
        out.push({ "kind": "lists", "text": qsTr("Game lists") + api.tr,
                   "value": (st.lists || 0) > 0 ? qsTr("%1 consoles · from %2 · A downloads them again").arg(st.lists).arg(st.listsDate) + api.tr
                                                : qsTr("downloaded when you match") + api.tr });
        out.push({ "kind": "section", "text": qsTr("For themes") + api.tr });
        out.push({ "kind": "info", "text": qsTr("A matched game carries ra-id, ra-title, ra-achievements, ra-points, ra-earned, ra-icon and ra-hash in game.extra; a game that isn't on RetroAchievements has ra-id 0.") + api.tr, "value": "" });
        return out;
    }

    // the press (or tap) that opened this screen must not land on its first
    // row (that started a match on opening)
    property real openedAt: 0
    function activate(row) {
        if (Date.now() - openedAt < 600)
            return;
        if (row.kind === "match") match(status.checked > 0 && status.checked < status.games);
        else if (row.kind === "matchall") match(false);
        else if (row.kind === "progress") refreshProgress();
        else if (row.kind === "text") textDialog.openFor(row);
        else if (row.kind === "toggle") set(row.key, !s[row.key]);
        else if (row.kind === "help") helpDialog.focus = true;
        else if (row.kind === "lists") updateLists();
    }
    function back() {
        if (changed)
            Internal.settings.reloadLibrary();
        root.close();
    }

    Keys.onPressed: {
        if (api.keys.isCancel(event) && !event.isAutoRepeat) {
            event.accepted = true;
            back();
        }
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
        text: qsTr("Settings > RetroAchievements") + api.tr
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
        model: root.rows
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: height * 0.3
        preferredHighlightEnd: height * 0.7
        highlightMoveDuration: 150

        function selectable(i) {
            return i >= 0 && i < count && root.rows[i].kind !== "section" && root.rows[i].kind !== "info";
        }
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
        Keys.onPressed: {
            if (pageKeys(event))
                return;
            if (api.keys.isAccept(event) && !event.isAutoRepeat && selectable(currentIndex)) {
                event.accepted = true;
                root.activate(root.rows[currentIndex]);
            }
        }

        delegate: Item {
            readonly property bool isSection: modelData.kind === "section"
            readonly property bool isInfo: modelData.kind === "info"
            // the match row's second line is live (progress), read here, not in the rows
            readonly property string subText: modelData.kind === "match" ? root.matchSub : (modelData.sub || "")
            readonly property bool hasSub: !!modelData.sub
            readonly property bool highlighted: !isSection && !isInfo
                && ((ListView.isCurrentItem && ListView.view.activeFocus) || rowMouse.containsMouse)
            width: ListView.view.width
            height: isSection ? sectionText.height
                  : isInfo ? (modelData.value ? Math.max(vpx(44), infoValue.implicitHeight + vpx(16)) : rowLabel.implicitHeight + vpx(16))
                  : hasSub ? vpx(22) * 3.4 : vpx(22) * 2.4

            // a line between sections (above every section title but the first)
            Rectangle {
                visible: parent.isSection && index > 0
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: sectionText.font.pixelSize * 0.7
                height: Math.max(1, vpx(1))
                color: "#3a3a3a"
            }
            Text {
                id: sectionText
                visible: parent.isSection
                text: modelData.text
                color: "#bbb"
                font.family: globalFonts.sans
                font.pixelSize: vpx(19)
                topPadding: font.pixelSize * (index === 0 ? 0.75 : 1.5)
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
                color: parent.isInfo && !modelData.value ? "#999" : "#eee"
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(21)
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.right: modelData.value ? undefined : parent.right
                anchors.rightMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: parent.hasSub ? -vpx(11) : 0
                width: modelData.value ? parent.width * 0.4 : undefined
                elide: Text.ElideRight
                wrapMode: modelData.value ? Text.NoWrap : Text.WordWrap
                lineHeight: 1.2
            }
            Text {
                visible: parent.hasSub
                text: parent.subText
                color: "#999"
                font.family: globalFonts.sans
                font.pixelSize: vpx(15)
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.right: parent.right
                anchors.rightMargin: vpx(30)
                anchors.top: rowLabel.bottom
                anchors.topMargin: vpx(2)
                elide: Text.ElideRight
            }
            Text {
                id: infoValue
                visible: !parent.isSection && !!modelData.value
                text: modelData.value || ""
                color: modelData.ok ? "#9d9" : modelData.muted ? "#888" : "#c0c0c0"
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(16) : vpx(19)
                anchors.left: rowLabel.right
                anchors.leftMargin: vpx(16)
                anchors.right: parent.right
                anchors.rightMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: parent.hasSub ? -vpx(11) : 0
                horizontalAlignment: Text.AlignRight
                wrapMode: parent.isInfo ? Text.WordWrap : Text.NoWrap
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
                    root.activate(modelData);
                }
            }
        }
    }

    Text {
        id: messageBar
        text: root.message !== "" ? root.message : qsTr("A open · B back") + api.tr
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

    // a yes/no with a short list
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
        title: qsTr("RetroAchievements") + api.tr
        closeLabel: qsTr("Cancel") + api.tr
        onActivated: {
            var act = rows[index].act;
            focus = false;
            list.focus = true;
            if (callback) callback(act);
        }
        onClosed: { focus = false; list.focus = true; }
    }

    // -- How to get the key ---------------------------------------------------------

    PegasusDialog {
        id: helpDialog
        title: qsTr("Your RetroAchievements web API key") + api.tr
        buttons: [ qsTr("Ok") + api.tr ]
        onPicked: list.focus = true
        onCancelled: list.focus = true

        Repeater {
            model: [
                qsTr("Log in on retroachievements.org (a free account; the same one your emulator uses).") + api.tr,
                qsTr("Open your profile menu (top right) › Settings, and scroll to \"Keys\".") + api.tr,
                qsTr("Copy the Web API Key (not the Connect key).") + api.tr,
                qsTr("Enter your username and that key here. The Status row says \"Login works\" when RetroAchievements accepts them. The key stays on this device.") + api.tr,
            ]
            delegate: Row {
                spacing: vpx(12)
                width: parent.width
                Text {
                    width: vpx(28)
                    text: (index + 1) + "."
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(18)
                    font.bold: true
                }
                Text {
                    width: parent.width - vpx(40)
                    text: modelData
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(17)
                    wrapMode: Text.WordWrap
                    lineHeight: 1.1
                }
            }
        }
        Text {
            width: parent.width
            text: qsTr("Website: retroachievements.org/settings") + api.tr
            color: "#ddd"
            font.family: globalFonts.sans
            font.pixelSize: vpx(16)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WrapAnywhere
        }
    }

    // -- Typing the username or key ---------------------------------------------------

    FocusScope {
        id: textDialog

        property var row: null
        anchors.fill: parent
        enabled: focus
        visible: focus
        z: 6

        function openFor(r) {
            row = r;
            textInput.text = String(root.s[r.key] || "");
            textInput.cursorPosition = textInput.text.length;
            textKeyboard.row = 1;
            textKeyboard.col = 0;
            focus = true;
            textKeyboard.focus = true;
        }
        function finish() {
            var r = row;
            list.focus = true;
            if (r)
                root.set(r.key, textInput.text.trim());
        }

        Keys.onPressed: {
            if (api.keys.isCancel(event) && !event.isAutoRepeat) {
                event.accepted = true;
                list.focus = true;
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
            height: textColumn.height + vpx(40)
            anchors.horizontalCenter: parent.horizontalCenter
            y: vpx(30)
            color: "#555"
            Column {
                id: textColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(20)
                spacing: vpx(12)
                Text {
                    width: parent.width
                    text: textDialog.row ? textDialog.row.text : ""
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(19)
                    horizontalAlignment: Text.AlignHCenter
                }
                Rectangle {
                    width: parent.width
                    height: vpx(52)
                    color: "#333"
                    TextInput {
                        id: textInput
                        anchors.fill: parent
                        anchors.leftMargin: vpx(14)
                        anchors.rightMargin: vpx(14)
                        verticalAlignment: TextInput.AlignVCenter
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(20)
                        clip: true
                        cursorVisible: textDialog.visible
                        activeFocusOnPress: false
                        echoMode: textDialog.row && textDialog.row.secret ? TextInput.PasswordEchoOnEdit : TextInput.Normal
                    }
                }
            }
        }
        OnScreenKeyboard {
            id: textKeyboard
            anchors.bottom: parent.bottom
            anchors.bottomMargin: vpx(12)
            anchors.horizontalCenter: parent.horizontalCenter
            keyH: vpx(40)
            target: textInput
            onDone: textDialog.finish()
        }
    }
}
