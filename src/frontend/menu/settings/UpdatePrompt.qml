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
import QtQuick 2.6


// Pegasus X: "Check for updates when Pegasus X starts" found a newer build.
// Update now downloads it and hands it to Android's installer; Pegasus X
// opens again by itself once the new build is in.
FocusScope {
    id: root

    signal close

    readonly property var updater: Internal.settings.updater
    readonly property string ustate: updater.state
    readonly property var info: updater.info
    property bool started: false

    anchors.fill: parent
    enabled: focus
    visible: focus

    PegasusDialog {
        id: dialog
        focus: true
        title: qsTr("Pegasus X update") + api.tr
        busy: root.started && root.ustate === "downloading"
        buttons: {
            if (!root.started)
                return [ qsTr("Update now") + api.tr, qsTr("Later") + api.tr ];
            if (root.ustate === "downloading")
                return [];
            if (root.ustate === "ready" || root.ustate === "error")
                return [ qsTr("Try again") + api.tr, qsTr("Close") + api.tr ];
            return [ qsTr("Close") + api.tr ];
        }
        text: {
            var i = root.info || {};
            if (!root.started)
                return qsTr("Build %1 is out (this is build %2).\n\nUpdate now downloads it and opens Android's installer; Pegasus X starts again by itself when it's in. Your settings, themes and metadata stay as they are.")
                    .arg(i.latest).arg(i.current) + api.tr;
            if (root.ustate === "downloading") {
                var mb = (i.received || 0) / 1048576;
                var total = (i.size || 0) / 1048576;
                return (total > 0
                    ? qsTr("Downloading build %1: %2 of %3 MB...").arg(i.latest).arg(mb.toFixed(1)).arg(total.toFixed(1))
                    : qsTr("Downloading build %1...").arg(i.latest)) + api.tr;
            }
            if (root.ustate === "ready")
                return (i.error ? i.error + "\n\n" : "")
                    + qsTr("Android's installer is open: pick Update there. Pegasus X starts again by itself afterwards.") + api.tr;
            if (root.ustate === "error")
                return qsTr("The update didn't work: %1").arg(i.error || "") + api.tr;
            return "";
        }
        onPicked: {
            if (!root.started) {
                if (index === 0) {
                    root.started = true;
                    root.updater.download();
                }
                else {
                    root.close();
                }
                return;
            }
            if ((root.ustate === "ready" || root.ustate === "error") && index === 0) {
                if (root.ustate === "ready")
                    root.updater.install();
                else
                    root.updater.download();
                return;
            }
            root.close();
        }
        onCancelled: {
            if (root.ustate === "downloading")
                root.updater.cancel();
            root.close();
        }
    }
}
