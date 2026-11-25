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
import "gamedireditor"
import QtQuick 2.6


// Pegasus X: Settings > Backup & reset. Back up (one file: Pegasus X's own
// data, the scraped art if ticked, never a password, key or token), Restore
// (the file checked first, the data as it is saved first, logins on the
// device kept), Run setup again (nothing deleted), Start over (Pegasus X's
// own data deleted, after two confirms; the themes, ROMs and ES-DE stay).
// The work is in BackupReset.cpp; this is the screen.
FocusScope {
    id: root

    signal close

    anchors.fill: parent

    property string page: "main"       // main, backup, restore, wipe
    property string folder: ""         // where a backup goes
    property bool art: false           // Include scraped art
    property real artMb: -1            // its size (-1: being measured)
    property bool busy: false          // a backup or a restore running
    property string busyWhat: ""       // "backup", "restore", "wipe"
    property int percent: 0
    property string message: ""        // the last result, shown on the page
    property string lastFile: ""       // the last backup made
    property string restorePath: ""
    property var restoreInfo: ({})
    property var plan: ({ "remove": [], "keep": [], "mb": 0 })
    property string backTo: "main"     // where the backup page goes back to (Start over's "Back up first")

    readonly property var status: Internal.settings.backupStatus()

    // Pegasus X: opened from the damaged-files notice's "Restore from a
    // backup": straight to picking the backup file
    property bool openRestore: false

    Component.onCompleted: {
        folder = status.folder;
        list.jumpTo(false);
        if (openRestore) {
            selectKind("pickrestore");
            activate(rows[list.currentIndex]);
        }
    }

    Connections {
        target: Internal.settings
        function onBackupArtSize(mb) { root.artMb = mb; }
        function onBackupProgress(percent) { root.percent = percent; }
        function onBackupFinished(path, error) {
            root.busy = false;
            root.lastFile = error === "" ? path : "";
            root.message = error === "" ? qsTr("Saved: %1").arg(path) + api.tr
                         : error === "Stopped" ? qsTr("Stopped: no file was left behind.") + api.tr
                         : qsTr("Not saved: %1").arg(error) + api.tr;
            console.log("Backup & reset: backup " + (error === "" ? "saved: " + path : "not saved: " + error));
            root.selectKind(error === "" && root.backTo === "wipe" ? "backdone" : "go");
        }
        function onRestoreFinished(result) {
            root.busy = false;
            var text;
            if (result.error !== "" && !result.changed)
                text = result.error;
            else {
                text = (result.error === "" ? qsTr("Restored.") : qsTr("Restored, with a problem: %1").arg(result.error)) + api.tr
                     + "\n\n" + qsTr("Your data as it was is saved in %1, so this can be undone.").arg(result.savedFirst) + api.tr;
                if (result.signIn.length > 0)
                    text += "\n\n" + qsTr("Sign in again: %1 (Settings > Scraper > Account settings, Settings > RetroAchievements).")
                                     .arg(result.signIn.join(", ")) + api.tr;
                text += "\n\n" + qsTr("The library is read again when you close this.") + api.tr;
            }
            console.log("Backup & reset: restore " + (result.changed ? "done" : "not done") + (result.error !== "" ? ": " + result.error : "")
                        + (result.signIn.length > 0 ? ", sign in again: " + result.signIn.join(", ") : ""));
            dialog.show("restoredone", qsTr("Restore") + api.tr, text, [qsTr("OK") + api.tr], 0);
            dialog.changed = result.changed;
        }
        function onStartOverFinished(error) {
            console.log("Backup & reset: start over done" + (error !== "" ? ", " + error : ""));
            // Pegasus X starts again in a moment (nothing from before stays in
            // memory), with setup at step 1
            root.wipeDone = error !== "" ? error : " ";
        }
    }

    function mb(value) {
        if (value < 0)
            return qsTr("measuring...") + api.tr;
        return value >= 1024 ? qsTr("%1 GB").arg((value / 1024).toFixed(1)) + api.tr
                             : value >= 10 ? qsTr("%1 MB").arg(Math.round(value)) + api.tr
                             : qsTr("%1 MB").arg(value.toFixed(1)) + api.tr;
    }
    function dateText(iso) {
        var d = new Date(iso);
        return isNaN(d.getTime()) ? iso : d.toLocaleString(Qt.locale(), Locale.ShortFormat);
    }

    function openPage(p) {
        page = p;
        message = "";
        if (p === "backup" && artMb < 0)
            Internal.settings.requestBackupArtSize();
        if (p === "wipe")
            plan = Internal.settings.startOverPlan();
        list.jumpTo(false);
        list.positionViewAtBeginning();
    }

    readonly property var rows: {
        var out = [];
        var i;
        if (page === "main") {
            out.push({ "kind": "info", "text": qsTr("Pegasus X folder") + api.tr, "value": status.data });
            out.push({ "kind": "section", "text": qsTr("Back up") + api.tr });
            out.push({ "kind": "tobackup", "text": qsTr("Back up...") + api.tr,
                       "value": qsTr("settings, library, metadata; no passwords") + api.tr });
            out.push({ "kind": "section", "text": qsTr("Restore") + api.tr });
            out.push({ "kind": "pickrestore", "text": qsTr("Restore from a backup...") + api.tr, "value": qsTr("pick the file") + api.tr });
            out.push({ "kind": "section", "text": qsTr("Setup") + api.tr });
            out.push({ "kind": "setup", "text": qsTr("Run setup again") + api.tr, "value": qsTr("from step 1, nothing is deleted") + api.tr });
            out.push({ "kind": "section", "text": qsTr("Start over") + api.tr });
            out.push({ "kind": "towipe", "text": qsTr("Start over...") + api.tr,
                       "value": qsTr("delete Pegasus X's data, then setup") + api.tr, "warn": true });
            return out;
        }
        if (page === "backup") {
            out.push({ "kind": "info", "text": qsTr("A backup holds Pegasus X's settings, game folders list, game directory metadata, edits, hidden games, Android app picks and sections, emulators, favorites, play time and theme options. Never a password, key or token.") + api.tr, "value": "", "wrap": true });
            out.push({ "kind": "folder", "text": qsTr("Save to") + api.tr, "value": folder });
            // (the tick, the size and the progress are live values of the row:
            // they change without the rows being made again, which would move
            // the highlight)
            out.push({ "kind": "art", "text": qsTr("Include scraped art") + api.tr, "value": "", "live": "art" });
            if (busy && busyWhat === "backup") {
                out.push({ "kind": "info", "text": qsTr("Backing up...") + api.tr, "value": "", "live": "percent" });
                out.push({ "kind": "stop", "text": qsTr("Stop") + api.tr, "value": "" });
            }
            else {
                out.push({ "kind": "go", "text": qsTr("Back up now") + api.tr, "value": status.file });
                if (message !== "")
                    out.push({ "kind": "info", "text": message, "value": "", "wrap": true });
                if (backTo === "wipe" && lastFile !== "")
                    out.push({ "kind": "backdone", "text": qsTr("Back to Start over") + api.tr, "value": "" });
            }
            return out;
        }
        if (page === "restore") {
            var ri = restoreInfo || {};
            out.push({ "kind": "info", "text": qsTr("File") + api.tr, "value": restorePath });
            if (ri.error !== "") {
                out.push({ "kind": "info", "text": ri.error, "value": "", "wrap": true, "warn": true });
                out.push({ "kind": "pickrestore", "text": qsTr("Pick another file...") + api.tr, "value": "" });
                return out;
            }
            out.push({ "kind": "info", "text": qsTr("Made") + api.tr, "value": dateText(ri.date) });
            out.push({ "kind": "info", "text": qsTr("Build") + api.tr, "value": String(ri.build) });
            out.push({ "kind": "info", "text": qsTr("Scraped art") + api.tr, "value": (ri.art ? qsTr("included") : qsTr("not included")) + api.tr });
            out.push({ "kind": "info", "text": qsTr("Files") + api.tr, "value": ri.files + " · " + mb(ri.mb) });
            if (busy && busyWhat === "restore") {
                out.push({ "kind": "info", "text": qsTr("Restoring...") + api.tr, "value": "", "live": "percent" });
                return out;
            }
            out.push({ "kind": "dorestore", "text": qsTr("Restore this backup...") + api.tr, "value": "" });
            out.push({ "kind": "pickrestore", "text": qsTr("Pick another file...") + api.tr, "value": "" });
            return out;
        }
        if (page === "wipe") {
            out.push({ "kind": "info", "text": qsTr("Start over deletes Pegasus X's own data and opens setup, like a new install. It can't be undone: back up first if you may want it back.") + api.tr, "value": "", "wrap": true, "warn": true });
            out.push({ "kind": "cancel", "text": qsTr("Cancel") + api.tr, "value": "" });
            out.push({ "kind": "wipebackup", "text": qsTr("Back up first...") + api.tr, "value": lastFile !== "" ? qsTr("saved") + api.tr : "" });
            out.push({ "kind": "wipego", "text": qsTr("Delete Pegasus X's data...") + api.tr, "value": mb(plan.mb), "warn": true });
            out.push({ "kind": "section", "text": qsTr("Deleted") + api.tr });
            for (i = 0; i < plan.remove.length; i++)
                out.push({ "kind": "item", "text": plan.remove[i].what, "sub": plan.remove[i].path, "value": mb(plan.remove[i].mb) });
            if (plan.remove.length === 0)
                out.push({ "kind": "item", "text": qsTr("Nothing to delete") + api.tr, "value": "" });
            out.push({ "kind": "section", "text": qsTr("Kept") + api.tr });
            for (i = 0; i < plan.keep.length; i++)
                out.push({ "kind": "item", "text": plan.keep[i], "value": "", "done": true });
            return out;
        }
        return out;
    }

    function selectKind(kind) {
        for (var i = 0; i < rows.length; i++) {
            if (rows[i].kind === kind) {
                list.currentIndex = i;
                list.positionViewAtIndex(i, ListView.Contain);
                return;
            }
        }
    }

    function activate(row) {
        switch (row.kind) {
            case "tobackup":
                backTo = "main";
                openPage("backup");
                break;
            case "folder":
                filePicker.mode = "folder";
                filePicker.pickFolders = true;
                filePicker.fileNames = [];
                filePicker.fileExtensions = [];
                filePicker.openAt(folder);
                filePicker.focus = true;
                break;
            case "art":
                art = !art;
                break;
            case "go":
                message = "";
                busy = true;
                busyWhat = "backup";
                percent = 0;
                Internal.settings.startBackup(folder, art);
                selectKind("stop");
                break;
            case "stop":
                Internal.settings.cancelBackup();
                break;
            case "backdone":
                openPage("wipe");
                break;
            case "pickrestore":
                filePicker.mode = "file";
                filePicker.pickFolders = false;
                filePicker.fileNames = [];
                filePicker.fileExtensions = [".tar"];
                filePicker.openAt(folder);
                filePicker.focus = true;
                break;
            case "dorestore":
                dialog.show("restore", qsTr("Restore this backup?") + api.tr,
                            qsTr("Pegasus X's data is replaced by the backup's (made %1, build %2%3). Your data as it is now is saved first, so this can be undone. The logins on this device are kept.")
                                .arg(dateText(restoreInfo.date)).arg(restoreInfo.build)
                                .arg(restoreInfo.art ? qsTr(", with scraped art") : "") + api.tr,
                            [qsTr("Cancel") + api.tr, qsTr("Restore") + api.tr], 0);
                break;
            case "setup":
                console.log("Backup & reset: run setup again");
                Internal.settings.runSetupAgain();
                break;
            case "towipe":
                lastFile = "";
                openPage("wipe");
                break;
            case "cancel":
                openPage("main");
                selectKind("towipe");
                break;
            case "wipebackup":
                backTo = "wipe";
                openPage("backup");
                break;
            case "wipego":
                dialog.show("wipe1", qsTr("Start over?") + api.tr,
                            qsTr("This deletes %1 of Pegasus X's data: settings and logins, game directory and metadata, edits, hidden games, Android app picks, play time, favorites, theme options, scraped art, backups made before changes, downloads and caches.\n\nKept: your themes, your ROM folders (and ES-DE's files in them), the ES-DE folder, which Pegasus X folder is used, anything else.")
                                .arg(mb(plan.mb)) + api.tr,
                            [qsTr("Cancel") + api.tr, qsTr("Continue") + api.tr], 0);
                break;
        }
    }
    function back() {
        if (busy)
            return;  // (Stop first)
        if (page === "backup" && backTo === "wipe") {
            openPage("wipe");
            selectKind("wipebackup");
            return;
        }
        if (page !== "main") {
            var from = page;
            openPage("main");
            selectKind(from === "backup" ? "tobackup" : from === "restore" ? "pickrestore" : "towipe");
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
        text: root.page === "backup" ? qsTr("Backup & reset > Back up") + api.tr
            : root.page === "restore" ? qsTr("Backup & reset > Restore") + api.tr
            : root.page === "wipe" ? qsTr("Backup & reset > Start over") + api.tr
            : qsTr("Settings > Backup & reset") + api.tr
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
            return i >= 0 && i < count && root.rows[i].kind !== "info" && root.rows[i].kind !== "section";
        }
        function jumpTo(end) {
            var i = end ? count - 1 : 0;
            while (i >= 0 && i < count && !selectable(i))
                i += end ? -1 : 1;
            currentIndex = (i >= 0 && i < count) ? i : 0;
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
            else
                positionViewAtBeginning();
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
            readonly property bool isSection: modelData.kind === "section"
            readonly property bool wraps: !!modelData.wrap
            readonly property bool highlighted: ListView.isCurrentItem && list.activeFocus && !isInfo && !isSection

            width: ListView.view.width
            height: isSection ? vpx(46)
                  : wraps ? wrapText.implicitHeight + vpx(24)
                  : modelData.sub ? vpx(62)
                  : vpx(22) * 2.4

            Rectangle {
                visible: parent.isSection
                anchors.top: parent.top
                width: parent.width
                height: vpx(1)
                color: "#444"
            }
            Rectangle {
                width: parent.width
                height: vpx(3)
                anchors.bottom: parent.bottom
                color: "#3aa"
                visible: parent.highlighted
            }
            Text {
                id: wrapText
                visible: parent.wraps
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.right: parent.right
                anchors.rightMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                text: modelData.text
                color: modelData.warn ? "#e8b06a" : "#aaa"
                font.family: globalFonts.sans
                font.pixelSize: vpx(17)
                wrapMode: Text.WordWrap
            }
            Text {
                id: rowLabel
                visible: !parent.wraps
                text: modelData.text
                color: parent.isSection ? "#999" : parent.isInfo ? "#999" : modelData.warn ? "#e8b06a" : "#eee"
                font.family: globalFonts.sans
                font.pixelSize: parent.isSection || parent.isInfo ? vpx(17) : vpx(21)
                anchors.left: parent.left
                anchors.leftMargin: parent.isSection ? vpx(0) : vpx(30)
                anchors.verticalCenter: modelData.sub ? undefined : parent.verticalCenter
                anchors.top: modelData.sub ? parent.top : undefined
                anchors.topMargin: vpx(8)
                width: parent.width * 0.55
                elide: Text.ElideRight
            }
            Text {
                visible: !parent.wraps && !!modelData.sub
                text: modelData.sub || ""
                color: "#888"
                font.family: globalFonts.sans
                font.pixelSize: vpx(14)
                anchors.left: rowLabel.left
                anchors.top: rowLabel.bottom
                anchors.topMargin: vpx(4)
                width: parent.width - vpx(60)
                elide: Text.ElideMiddle
            }
            Text {
                visible: !parent.wraps && !parent.isSection
                text: modelData.live === "art" ? (root.art ? "☑" : "☐") + "  " + qsTr("about %1").arg(root.mb(root.artMb)) + api.tr
                    : modelData.live === "percent" ? root.percent + "%"
                    : (modelData.value || "")
                color: modelData.done ? "#8c8" : modelData.warn ? "#e8b06a" : "#c0c0c0"
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(19)
                anchors.left: rowLabel.right
                anchors.leftMargin: vpx(16)
                anchors.right: parent.right
                anchors.rightMargin: vpx(30)
                anchors.verticalCenter: modelData.sub ? undefined : parent.verticalCenter
                anchors.top: modelData.sub ? parent.top : undefined
                anchors.topMargin: vpx(8)
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
            }
            MouseArea {
                anchors.fill: parent
                enabled: !parent.isInfo && !parent.isSection
                onClicked: {
                    list.currentIndex = index;
                    list.focus = true;
                    root.activate(modelData);
                }
            }
        }
    }

    RoundPromptBar {
        id: prompts
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        // (the folder picker has its own: one bar at a time)
        visible: !filePicker.visible
        items: [
            { "keys": ["A"], "text": qsTr("choose") + api.tr },
            { "keys": ["LB", "RB"], "text": qsTr("10 rows") + api.tr },
            { "keys": ["LT", "RT"], "text": qsTr("top / bottom") + api.tr },
            { "keys": ["B"], "text": (root.busy ? qsTr("Stop first") : qsTr("back")) + api.tr },
        ]
    }

    // the confirm dialogs (Cancel focused) and the result of a restore
    PegasusDialog {
        id: dialog
        z: 5
        property string what: ""
        property bool changed: false
        function show(kind, t, body, labels, focused) {
            what = kind;
            title = t;
            text = body;
            buttons = labels;
            choice = focused;
            focus = true;
        }
        onPicked: {
            list.focus = true;
            if (what === "restore" && index === 1) {
                root.busy = true;
                root.busyWhat = "restore";
                root.percent = 0;
                console.log("Backup & reset: restore " + root.restorePath);
                Internal.settings.startRestore(root.restorePath);
            }
            else if (what === "restoredone") {
                if (changed)
                    Internal.settings.reloadLibrary();
            }
            else if (what === "wipe1" && index === 1) {
                // the second, deliberate confirm
                show("wipe2", qsTr("Delete it now?") + api.tr,
                     qsTr("Last check: Pegasus X's data (%1) is deleted now, then Pegasus X starts again with setup. This can't be undone.").arg(root.mb(root.plan.mb)) + api.tr,
                     [qsTr("Cancel") + api.tr, qsTr("Delete and start over") + api.tr], 0);
            }
            else if (what === "wipe2" && index === 1) {
                root.busy = true;
                root.busyWhat = "wipe";
                console.log("Backup & reset: start over");
                Internal.settings.startOver();
                busyDialog.focus = true;
            }
        }
        onCancelled: {
            list.focus = true;
            if (what === "restoredone" && changed)
                Internal.settings.reloadLibrary();
        }
    }
    property string wipeDone: ""   // Start over done (" ", or what couldn't be deleted)
    PegasusDialog {
        id: busyDialog
        z: 6
        title: qsTr("Start over") + api.tr
        text: root.wipeDone === "" ? qsTr("Deleting Pegasus X's data...") + api.tr
            : (root.wipeDone.trim() !== "" ? root.wipeDone.trim() + "\n\n" : "")
              + qsTr("Done. Pegasus X starts again now, with setup.") + api.tr
        busy: true
    }

    FilePicker {
        id: filePicker
        z: 4
        enabled: focus
        visible: focus
        property string mode: "folder"   // folder (Save to), file (a backup to restore)
        allowNewFolder: true
        infoText: mode === "file" ? qsTr("Select a Pegasus X backup (PegasusX-backup-....tar).") + api.tr
                                  : qsTr("Open the folder to save backups in, then \"Use this folder\".") + api.tr
        onCancel: list.focus = true
        onPickFolder: {
            if (mode !== "folder")
                return;
            root.folder = dir_path;
            list.focus = true;
            root.selectKind("folder");
        }
        onPickFile: {
            if (mode !== "file")
                return;
            root.restorePath = file_path;
            root.restoreInfo = Internal.settings.backupFileInfo(file_path);
            console.log("Backup & reset: picked " + file_path + ": " + (root.restoreInfo.error !== "" ? root.restoreInfo.error
                        : "made " + root.restoreInfo.date + ", build " + root.restoreInfo.build + ", art " + root.restoreInfo.art));
            list.focus = true;
            root.openPage("restore");
            root.selectKind(root.restoreInfo.error !== "" ? "pickrestore" : "dorestore");
        }
    }
}
