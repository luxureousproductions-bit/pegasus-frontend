// Pegasus X
// Copyright (C) 2026  Esteban (MisterE88)
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


// Pegasus X: after a start that fixed damaged files (Recovery.h): which ones
// and how, once. Keep (B), Restore from a backup (X: Settings > Backup &
// reset, picking the backup file), Details (Y: what was moved to a damaged
// folder, and where). A or a tap on a button too.
FocusScope {
    id: root

    signal close
    signal restoreRequested

    readonly property var report: Internal.settings.recoveryReport()
    property bool details: false

    anchors.fill: parent
    enabled: focus
    visible: focus

    function line(f) {
        if (f.how === "good copy")
            return qsTr("%1: it was damaged; its last good copy is back.").arg(f.label) + api.tr;
        if (f.how === "put back")
            return qsTr("%1: a save was cut short (power off?); the saved file was put back.").arg(f.label) + api.tr;
        if (f.how === "left")
            return qsTr("%1: it's damaged, but it couldn't be moved aside, so it's left as it is.").arg(f.label) + api.tr;
        return qsTr("%1: it was damaged and there was no good copy; it starts over at its defaults.").arg(f.label) + api.tr;
    }
    function detailLines() {
        var out = [];
        for (var i = 0; i < report.length; i++) {
            var f = report[i];
            out.push(qsTr("%1 (%2): %3").arg(f.label).arg(f.path).arg(f.reason) + api.tr);
            if (f.moved.length === 0)
                out.push(qsTr("   nothing moved") + api.tr);
            for (var m = 0; m < f.moved.length; m++)
                out.push(qsTr("   moved to %1").arg(f.moved[m]) + api.tr);
        }
        return out.join("\n");
    }

    Keys.onPressed: {
        if (event.isAutoRepeat)
            return;
        if (api.keys.isDetails(event)) {
            event.accepted = true;
            root.restoreRequested();
        }
        else if (api.keys.isFilters(event)) {
            event.accepted = true;
            root.details = !root.details;
        }
    }

    PegasusDialog {
        id: dialog
        focus: true
        dialogWidth: Math.min(parent.width * 0.86, vpx(900))
        title: qsTr("Damaged files fixed") + api.tr
        buttons: [ qsTr("Keep") + api.tr, qsTr("Restore from a backup") + api.tr,
                   (root.details ? qsTr("Hide details") : qsTr("Details")) + api.tr ]
        text: {
            var out = [];
            for (var i = 0; i < root.report.length; i++)
                out.push(root.line(root.report[i]));
            return qsTr("Some of Pegasus X's files were damaged when it started, and were fixed:") + api.tr
                + "\n\n" + out.join("\n") + "\n\n"
                + qsTr("Nothing was deleted: the damaged files are kept in a \"damaged\" folder. Keep goes on as it is now; a backup puts everything back as it was then.") + api.tr;
        }
        onPicked: {
            if (index === 0)
                root.close();
            else if (index === 1)
                root.restoreRequested();
            else
                root.details = !root.details;
        }
        onCancelled: root.close()

        Text {
            visible: root.details
            width: parent.width
            text: root.details ? root.detailLines() : ""
            color: "#ccc"
            font.family: globalFonts.sans
            font.pixelSize: vpx(14)
            wrapMode: Text.WrapAnywhere
            lineHeight: 1.1
        }
        RoundPromptBar {
            width: parent.width
            items: [
                { "keys": ["A"], "text": qsTr("choose") + api.tr },
                { "keys": ["B"], "text": qsTr("keep") + api.tr },
                { "keys": ["X"], "text": qsTr("restore from a backup") + api.tr },
                { "keys": ["Y"], "text": (root.details ? qsTr("hide details") : qsTr("details")) + api.tr },
            ]
        }
    }
}
