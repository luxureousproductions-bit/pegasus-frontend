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
import QtQuick 2.15
import QtQuick.Window 2.2


// Pegasus X: themes from the Pegasus theme gallery (and your own GitHub
// links), with their pictures, installed and updated in the app.
FocusScope {
    id: root

    signal close

    width: parent.width
    height: parent.height
    visible: 0 < (x + width) && x < Window.window.width
    enabled: focus

    readonly property var store: Internal.settings.themeStore
    property var all: []
    property string tab: "all"        // all, installed, updates, links
    property string message: ""
    property int pictureIndex: 0
    property var badPictures: ({})    // pictures that didn't load: skipped
    readonly property var pictures: {
        var out = [];
        if (!entry)
            return out;
        for (var i = 0; i < entry.pictures.length; i++) {
            if (!badPictures[entry.pictures[i]])
                out.push(entry.pictures[i]);
        }
        return out;
    }
    property int loadedCount: 0
    property int totalCount: 0

    readonly property var tabs: [
        { "v": "all", "t": qsTr("All") + api.tr },
        { "v": "installed", "t": qsTr("Installed") + api.tr },
        { "v": "updates", "t": qsTr("Updates") + api.tr },
        { "v": "links", "t": qsTr("My links") + api.tr },
    ]
    function countOf(t) {
        var n = 0;
        for (var i = 0; i < all.length; i++) {
            if (inTab(all[i], t))
                n++;
        }
        return n;
    }
    function inTab(e, t) {
        if (t === "installed") return e.installed;
        if (t === "updates") return e.update;
        if (t === "links") return e.custom;
        return true;
    }
    readonly property var rows: {
        var out = [];
        for (var i = 0; i < all.length; i++) {
            if (inTab(all[i], tab))
                out.push({ "kind": "theme", "entry": all[i] });
        }
        if (tab === "all" || tab === "links")
            out.push({ "kind": "add", "entry": null });
        out.push({ "kind": "refresh", "entry": null });
        return out;
    }
    readonly property var picked: (list.currentIndex >= 0 && list.currentIndex < rows.length) ? rows[list.currentIndex] : null
    readonly property var entry: picked && picked.kind === "theme" ? picked.entry : null

    function reload(keepId) {
        var id = keepId !== undefined ? keepId : (entry ? entry.id : "");
        all = store.themes();
        if (id === "")
            return;
        for (var i = 0; i < rows.length; i++) {
            if (rows[i].entry && rows[i].entry.id === id) {
                list.currentIndex = i;
                return;
            }
        }
    }
    function setTab(t) {
        tab = t;
        list.currentIndex = 0;
        pictureIndex = 0;
        message = "";
    }

    Component.onCompleted: {
        all = store.themes();
        // the list kept from last time right away, then the gallery online
        // for new themes and updates (refresh does nothing while one runs)
        store.refresh(false);
        store.refresh(true);
    }
    Connections {
        target: root.store
        function onListChanged() { root.reload(); }
        function onRefreshProgress(done, total) {
            root.loadedCount = done;
            root.totalCount = total;
        }
        function onInstallProgress(id, received, total, stage) { installDialog.progress(received, total, stage); }
        function onInstallFinished(id, ok, msg, dir) { installDialog.finished(ok, msg, dir); }
    }

    // -- What the buttons do ---------------------------------------------------------

    function useIt(e) {
        var index = Internal.settings.themes.indexOfDir(e.dir);
        if (index < 0) {
            message = qsTr("Pegasus doesn't see that theme yet.") + api.tr;
            return;
        }
        Internal.settings.themes.currentIndex = index;
        Internal.settings.closeMenu();
    }
    function installIt(e) {
        installDialog.openFor(e);
        store.install(e.id);
    }
    function act() {
        if (!picked)
            return;
        if (picked.kind === "add") { linkDialog.openFor(); return; }
        if (picked.kind === "refresh") { message = ""; store.refresh(true); return; }
        if (!entry.installed) installIt(entry);
        else if (entry.current) message = qsTr("It's the theme in use.") + api.tr;
        else useIt(entry);
    }
    function update() {
        if (entry && entry.installed)
            installIt(entry);
    }
    function removeIt() {
        if (!entry)
            return;
        if (entry.installed)
            confirm.openFor(qsTr("Remove \"%1\" from this device?").arg(entry.name) + api.tr, "remove");
        else if (entry.custom)
            confirm.openFor(qsTr("Take \"%1\" off your links?").arg(entry.name) + api.tr, "unlink");
    }
    function confirmed(what) {
        var e = entry;
        if (!e)
            return;
        if (what === "remove") {
            var error = store.remove(e.id);
            message = error !== "" ? error : qsTr("\"%1\" removed.").arg(e.name) + api.tr;
        }
        else if (what === "unlink") {
            store.removeLink(e.id);
        }
    }

    Keys.onPressed: {
        if (event.isAutoRepeat)
            return;
        if (api.keys.isCancel(event)) {
            event.accepted = true;
            root.close();
        }
        else if (api.keys.isAccept(event)) {
            event.accepted = true;
            act();
        }
        else if (api.keys.isFilters(event)) {
            event.accepted = true;
            update();
        }
        else if (api.keys.isDetails(event)) {
            event.accepted = true;
            removeIt();
        }
        else if (api.keys.isPrevPage(event) || api.keys.isNextPage(event)) {
            event.accepted = true;
            var at = 0;
            for (var i = 0; i < tabs.length; i++) {
                if (tabs[i].v === tab)
                    at = i;
            }
            at = (at + (api.keys.isNextPage(event) ? 1 : -1) + tabs.length) % tabs.length;
            setTab(tabs[at].v);
        }
    }
    Keys.onLeftPressed: if (pictures.length > 1) pictureIndex = (pictureIndex - 1 + pictures.length) % pictures.length
    Keys.onRightPressed: if (pictures.length > 1) pictureIndex = (pictureIndex + 1) % pictures.length

    // -- The page --------------------------------------------------------------------

    Rectangle {
        anchors.fill: parent
        color: "#222"
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: root.close()
        }
    }

    ScreenHeader {
        id: header
        text: qsTr("Settings > Theme downloader") + api.tr
        z: 2
    }
    Text {
        anchors.right: parent.right
        anchors.rightMargin: vpx(40)
        anchors.verticalCenter: header.verticalCenter
        z: 3
        text: root.store.loading
            ? (root.totalCount > 0 ? qsTr("Checking for new themes and updates... %1 of %2").arg(root.loadedCount).arg(root.totalCount) + api.tr
                                   : qsTr("Checking for new themes and updates...") + api.tr)
            : qsTr("%1 themes · Pegasus theme gallery · %2 updates").arg(root.all.length).arg(root.countOf("updates")) + api.tr
        color: "#888"
        font.family: globalFonts.sans
        font.pixelSize: vpx(15)
    }

    // the filters (LB / RB)
    Row {
        id: tabRow
        anchors.top: header.bottom
        anchors.left: listBox.left
        spacing: vpx(10)
        Repeater {
            model: root.tabs
            delegate: Item {
                readonly property bool on: root.tab === modelData.v
                width: tabText.width + vpx(30)
                height: vpx(44)
                Text {
                    id: tabText
                    anchors.centerIn: parent
                    text: modelData.v === "all" ? modelData.t : modelData.t + " (" + root.countOf(modelData.v) + ")"
                    color: parent.on ? "#eee" : "#999"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(19)
                    font.capitalization: Font.AllUppercase
                    font.italic: true
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: vpx(3)
                    color: "#3aa"
                    visible: parent.on
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.setTab(modelData.v)
                }
            }
        }
    }

    // the themes
    Rectangle {
        id: listBox
        anchors.top: tabRow.bottom
        anchors.topMargin: vpx(14)
        anchors.bottom: hints.top
        anchors.bottomMargin: vpx(10)
        anchors.left: parent.left
        anchors.leftMargin: vpx(40)
        width: parent.width * 0.36
        color: "transparent"

        ListView {
            id: list
            anchors.fill: parent
            clip: true
            focus: true
            model: root.rows
            highlightMoveDuration: 0
            highlightRangeMode: ListView.ApplyRange
            preferredHighlightBegin: height * 0.3
            preferredHighlightEnd: height * 0.7
            onCurrentIndexChanged: root.pictureIndex = 0

            // rows as in Pegasus' settings: the highlighted one underlined
            delegate: Item {
                readonly property bool isOn: ListView.isCurrentItem
                readonly property var e: modelData.entry
                width: ListView.view.width
                height: vpx(22) * 2.4

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: vpx(3)
                    color: "#3aa"
                    visible: parent.isOn
                }

                Text {
                    id: rowName
                    anchors.left: parent.left
                    anchors.leftMargin: vpx(16)
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, parent.width - badge.width - vpx(40))
                    text: modelData.kind === "add" ? qsTr("Add a theme from a GitHub link...") + api.tr
                        : modelData.kind === "refresh" ? qsTr("Check for new themes and updates") + api.tr
                        : parent.e.name
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(20)
                    elide: Text.ElideRight
                }
                Text {
                    anchors.left: rowName.right
                    anchors.leftMargin: vpx(8)
                    anchors.right: badge.left
                    anchors.rightMargin: vpx(8)
                    anchors.baseline: rowName.baseline
                    visible: modelData.kind === "theme"
                    text: parent.e ? parent.e.author : ""
                    color: "#999"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(15)
                    elide: Text.ElideRight
                }
                // what it is here, as Pegasus shows a value: on the right
                Text {
                    id: badge
                    readonly property string kind: !parent.e ? "" : parent.e.current ? "current"
                        : parent.e.update ? "update" : parent.e.installed ? "installed" : ""
                    visible: kind !== ""
                    anchors.right: parent.right
                    anchors.rightMargin: vpx(16)
                    anchors.verticalCenter: parent.verticalCenter
                    width: visible ? implicitWidth : 0
                    text: kind === "update" ? qsTr("Update") + api.tr
                        : kind === "current" ? qsTr("In use") + api.tr : qsTr("Installed") + api.tr
                    color: kind === "update" ? "#fc6" : "#c0c0c0"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(17)
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (list.currentIndex === index)
                            root.act();
                        list.currentIndex = index;
                    }
                }
            }
        }
        Text {
            anchors.centerIn: parent
            width: parent.width * 0.8
            visible: root.all.length === 0
            text: root.store.loading ? qsTr("Reading the theme list...") + api.tr
                                     : qsTr("No themes yet: check your connection, then pick \"Check for new themes\".") + api.tr
            color: "#999"
            font.family: globalFonts.sans
            font.pixelSize: vpx(17)
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // the highlighted theme: its pictures and details
    Item {
        id: details
        anchors.top: listBox.top
        anchors.bottom: listBox.bottom
        anchors.left: listBox.right
        anchors.leftMargin: vpx(26)
        anchors.right: parent.right
        anchors.rightMargin: vpx(40)

        Rectangle {
            id: pictureBox
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: Math.min(width * 9 / 16, parent.height * 0.6)
            color: "#333"
            visible: root.entry !== null

            Image {
                id: picture
                anchors.fill: parent
                anchors.margins: vpx(4)
                source: root.pictures.length > 0 ? root.pictures[Math.min(root.pictureIndex, root.pictures.length - 1)] : ""
                // a picture that doesn't load: the next one
                onStatusChanged: {
                    if (status === Image.Error && source != "") {
                        var bad = Object.assign({}, root.badPictures);
                        bad[source.toString()] = true;
                        root.badPictures = bad;
                    }
                }
                sourceSize.width: vpx(900)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                opacity: status === Image.Ready ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }
            Text {
                anchors.centerIn: parent
                visible: picture.status !== Image.Ready
                text: picture.status === Image.Loading ? qsTr("Loading the picture...") + api.tr
                    : qsTr("No picture") + api.tr
                color: "#666"
                font.family: globalFonts.sans
                font.pixelSize: vpx(16)
            }
        }
        // which picture (◂ ▸)
        Row {
            id: dots
            anchors.top: pictureBox.bottom
            anchors.topMargin: vpx(8)
            anchors.left: pictureBox.left
            spacing: vpx(6)
            visible: root.pictures.length > 1
            Repeater {
                model: root.pictures.length
                delegate: Rectangle {
                    width: vpx(18)
                    height: vpx(3)
                    color: index === root.pictureIndex ? "#3aa" : "#555"
                }
            }
        }
        Column {
            anchors.top: dots.bottom
            anchors.topMargin: vpx(10)
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            spacing: vpx(6)
            visible: root.entry !== null
            clip: true

            Text {
                width: parent.width
                text: root.entry ? root.entry.name : ""
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(26)
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: {
                    if (!root.entry)
                        return "";
                    var e = root.entry;
                    var parts = [e.author];
                    if (e.version !== "")
                        parts.push(qsTr("version %1").arg(e.version) + api.tr);
                    if (e.installed && e.installedVersion !== "" && e.installedVersion !== e.version)
                        parts.push(qsTr("you have %1").arg(e.installedVersion) + api.tr);
                    if (e.current)
                        parts.push(qsTr("in use") + api.tr);
                    else if (e.update)
                        parts.push(qsTr("update ready") + api.tr);
                    else if (e.installed)
                        parts.push(qsTr("installed") + api.tr);
                    return parts.join("  ·  ");
                }
                color: "#999"
                font.family: globalFonts.sans
                font.pixelSize: vpx(15)
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: {
                    if (!root.entry)
                        return "";
                    var s = root.entry.summary || "";
                    var d = root.entry.description || "";
                    return d !== "" && d !== s ? (s !== "" ? s + "\n\n" + d : d) : s;
                }
                color: "#bbb"
                font.family: globalFonts.sans
                font.pixelSize: vpx(16)
                wrapMode: Text.WordWrap
                maximumLineCount: 6
                elide: Text.ElideRight
                lineHeight: 1.15
            }
            Text {
                width: parent.width
                text: root.entry ? root.entry.url : ""
                color: "#999"
                font.family: globalFonts.sans
                font.pixelSize: vpx(14)
                elide: Text.ElideMiddle
            }
        }

        // the special rows: what they do
        Text {
            anchors.centerIn: parent
            width: parent.width * 0.8
            visible: root.picked !== null && root.picked.kind !== "theme"
            text: !root.picked ? ""
                : root.picked.kind === "add"
                    ? qsTr("Any Pegasus theme on GitHub, including one you're making: paste or type its link, eg. github.com/owner/theme, or github.com/owner/theme/tree/branch for a branch. It stays in My links, and updates show up like the others'.") + api.tr
                    : qsTr("The list is read again from the Pegasus theme gallery (it's kept for a week otherwise), and your installed themes are checked for updates.") + api.tr
            color: "#999"
            font.family: globalFonts.sans
            font.pixelSize: vpx(17)
            wrapMode: Text.WordWrap
            lineHeight: 1.2
        }
    }

    Text {
        id: hints
        anchors.bottom: parent.bottom
        anchors.bottomMargin: vpx(14)
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.9
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: {
            if (root.message !== "")
                return root.message;
            var parts = [];
            var e = root.entry;
            if (!e)
                parts.push(qsTr("A select") + api.tr);
            else if (!e.installed)
                parts.push(qsTr("A install") + api.tr);
            else if (!e.current)
                parts.push(qsTr("A use it") + api.tr);
            if (e && e.installed)
                parts.push((e.update ? qsTr("Y update") : qsTr("Y download again")) + api.tr);
            if (e && (e.installed || e.custom))
                parts.push((e.installed ? qsTr("X remove") : qsTr("X take off my links")) + api.tr);
            if (e && root.pictures.length > 1)
                parts.push(qsTr("◂ ▸ pictures") + api.tr);
            parts.push(qsTr("LB / RB filter") + api.tr);
            parts.push(qsTr("B back") + api.tr);
            return parts.join("  ·  ");
        }
        color: "#ddd"
        font.family: globalFonts.sans
        font.pixelSize: vpx(17)
    }


    // -- Installing ------------------------------------------------------------------

    PegasusDialog {
        id: installDialog

        property var e: null
        property real received: 0
        property real total: 0
        property string stage: "download"
        property bool done: false
        property bool ok: false
        property string result: ""
        property string dir: ""

        title: e ? (e.installed ? qsTr("Updating %1") : qsTr("Installing %1")).arg(e.name) + api.tr : ""
        busy: !done
        buttons: !done ? [] : ok ? [ qsTr("Use it now") + api.tr, qsTr("Stay here") + api.tr ] : [ qsTr("Ok") + api.tr ]
        text: {
            if (done)
                return ok ? qsTr("Done. \"Use it now\" switches to it and opens its home screen.") + api.tr : result;
            if (stage === "unpack")
                return qsTr("Unpacking...") + api.tr;
            var mb = function(b) { return (b / 1048576).toFixed(1); };
            return (total > 0 ? qsTr("Downloading... %1 of %2 MB").arg(mb(received)).arg(mb(total))
                              : qsTr("Downloading... %1 MB").arg(mb(received))) + api.tr;
        }

        function openFor(entry) {
            e = entry;
            received = 0;
            total = 0;
            stage = "download";
            done = false;
            ok = false;
            result = "";
            dir = "";
            choice = 0;
            focus = true;
        }
        function progress(r, t, s) {
            received = r;
            total = t;
            stage = s;
        }
        function finished(success, msg, d) {
            done = true;
            ok = success;
            result = msg;
            dir = d;
        }

        onPicked: {
            var d = dir;
            list.focus = true;
            if (ok && index === 0)
                root.useIt({ "dir": d });
        }
        onCancelled: {
            if (!done)
                root.store.cancelInstall();
            else
                list.focus = true;
        }

        Text {
            width: parent.width
            text: installDialog.e ? installDialog.e.url : ""
            color: "#c0c0c0"
            font.family: globalFonts.sans
            font.pixelSize: vpx(15)
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle
        }
        // how far: the download's size isn't always known (then it slides)
        Rectangle {
            width: parent.width
            height: vpx(10)
            color: "#333"
            clip: true
            visible: !installDialog.done
            readonly property bool known: installDialog.total > 0 && installDialog.stage === "download"
            Rectangle {
                visible: parent.known
                height: parent.height
                color: "#4ae"
                width: parent.width * Math.min(1, installDialog.received / Math.max(1, installDialog.total))
            }
            Rectangle {
                visible: !parent.known
                height: parent.height
                width: parent.width * 0.3
                color: "#4ae"
                NumberAnimation on x {
                    from: -installDialog.dialogWidth * 0.3
                    to: installDialog.dialogWidth
                    duration: 1200
                    loops: Animation.Infinite
                    running: installDialog.visible && !installDialog.done
                }
            }
        }
        Text {
            width: parent.width
            visible: !installDialog.done
            text: qsTr("B stop") + api.tr
            color: "#bbb"
            font.family: globalFonts.sans
            font.pixelSize: vpx(15)
            horizontalAlignment: Text.AlignHCenter
        }
    }


    // -- Removing: asked first ----------------------------------------------------

    PegasusDialog {
        id: confirm

        property string what: ""
        title: qsTr("Theme downloader") + api.tr
        buttons: [ qsTr("Yes") + api.tr, qsTr("No") + api.tr ]

        function openFor(q, w) {
            text = q;
            what = w;
            choice = 1;
            focus = true;
        }
        onPicked: {
            var w = what;
            list.focus = true;
            if (index === 0)
                root.confirmed(w);
        }
        onCancelled: list.focus = true
    }


    // -- Adding a GitHub link -----------------------------------------------------

    FocusScope {
        id: linkDialog

        anchors.fill: parent
        enabled: focus
        visible: focus
        z: 5

        function openFor() {
            linkInput.text = "github.com/";
            linkInput.cursorPosition = linkInput.text.length;
            linkKeyboard.row = 1;
            linkKeyboard.col = 0;
            focus = true;
            linkKeyboard.focus = true;
        }
        function finish() {
            var link = linkInput.text.trim();
            list.focus = true;
            if (link === "" || link === "github.com/")
                return;
            var error = root.store.addLink(link);
            root.message = error !== "" ? error : qsTr("Added: its details are being read.") + api.tr;
            if (error === "")
                root.setTab("links");
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
            opacity: 0.6
            MouseArea { anchors.fill: parent }
        }
        Rectangle {
            id: linkTitle
            width: Math.min(parent.width * 0.75, vpx(860))
            height: vpx(20) * 2.25
            anchors.horizontalCenter: parent.horizontalCenter
            y: vpx(30)
            color: "#333"
            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: vpx(15)
                text: qsTr("A theme's GitHub link (paste works too)") + api.tr
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(20)
                font.bold: true
            }
        }
        Rectangle {
            width: linkTitle.width
            height: linkColumn.height + vpx(36)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: linkTitle.bottom
            color: "#555"
            Column {
                id: linkColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(18)
                spacing: vpx(12)
                Rectangle {
                    width: parent.width
                    height: vpx(52)
                    color: "#333"
                    TextInput {
                        id: linkInput
                        anchors.fill: parent
                        anchors.leftMargin: vpx(14)
                        anchors.rightMargin: vpx(14)
                        verticalAlignment: TextInput.AlignVCenter
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(20)
                        clip: true
                        cursorVisible: linkDialog.visible
                        activeFocusOnPress: false
                        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    }
                }
                Text {
                    width: parent.width
                    text: qsTr("eg. github.com/owner/theme  ·  for a branch: github.com/owner/theme/tree/branch") + api.tr
                    color: "#aaa"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(14)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        }
        OnScreenKeyboard {
            id: linkKeyboard
            anchors.bottom: parent.bottom
            anchors.bottomMargin: vpx(12)
            anchors.horizontalCenter: parent.horizontalCenter
            keyH: vpx(40)
            target: linkInput
            onDone: linkDialog.finish()
        }
    }
}
