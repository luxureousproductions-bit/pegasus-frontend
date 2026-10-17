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


// Pegasus X: picks the emulator for each system that has games, from the same
// list ES-DE uses. The choice is used by the Pegasus X library and the ES-DE
// data source, and also overrides a system-wide choice made in ES-DE.
FocusScope {
    id: root

    property bool mSettingsChanged: false
    property var systems: []

    signal close

    anchors.fill: parent

    enabled: focus
    visible: opacity > 0.001
    opacity: focus ? 1.0 : 0.0
    Behavior on opacity { PropertyAnimation { duration: 150 } }

    Component.onCompleted: systems = Internal.settings.emulatorSystems()

    function closeMaybe() {
        if (mSettingsChanged)
            reloadDialog.focus = true;
        else
            root.close();
    }

    function currentText(sys) {
        if (sys.current !== "")
            return sys.current;
        return qsTr("Default (%1)").arg(sys.defaultLabel) + api.tr;
    }

    function openPicker(index) {
        if (index < 0 || index >= systems.length)
            return;
        picker.systemIndex = index;
        picker.focus = true;
    }

    function choose(index, label) {
        Internal.settings.setSystemEmulator(systems[index].name, label);
        systems = Internal.settings.emulatorSystems();
        list.currentIndex = index;  // the model was replaced
        list.focus = true;
        mSettingsChanged = true;
    }

    Keys.onPressed: {
        if (api.keys.isCancel(event) && !event.isAutoRepeat) {
            event.accepted = true;
            root.closeMaybe();
        }
    }


    Rectangle {
        anchors.fill: parent
        color: "#000"
        opacity: 0.3

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: root.closeMaybe()
        }
    }


    Rectangle {
        id: main

        height: parent.height * 0.8
        width: Math.min(height * 1.5, parent.width * 0.95)
        anchors.centerIn: parent

        color: "#444"
        radius: vpx(8)

        MouseArea {
            anchors.fill: parent
        }

        Text {
            id: info

            text: qsTr("Pegasus X launches each system with the emulator chosen here. "
                     + "Systems appear once they have a ROM folder.") + api.tr
            color: "#eee"
            font.family: globalFonts.sans
            font.pixelSize: vpx(18)
            lineHeight: 1.15

            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width * 0.9
            padding: font.pixelSize * lineHeight

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
        }

        Rectangle {
            anchors.top: info.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: vpx(15)
            anchors.topMargin: 0
            color: "#333"

            Text {
                text: qsTr("No systems found yet. Add ROM folders, or turn on the ES-DE data source, then come back here.") + api.tr
                font.family: globalFonts.sans
                font.pixelSize: vpx(18)
                color: "#ccc"

                width: parent.width * 0.88
                anchors.centerIn: parent
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter

                visible: root.systems.length === 0
            }

            ListView {
                id: list

                anchors.fill: parent
                clip: true

                model: root.systems
                delegate: systemEntry

                focus: true
                highlightRangeMode: ListView.ApplyRange
                preferredHighlightBegin: height * 0.5 - vpx(18) * 1.25
                preferredHighlightEnd: height * 0.5 + vpx(18) * 1.25
                highlightMoveDuration: 0

                Keys.onPressed: {
                    if (api.keys.isAccept(event) && !event.isAutoRepeat) {
                        event.accepted = true;
                        root.openPicker(currentIndex);
                    }
                }
            }
        }
    }

    Component {
        id: systemEntry

        Rectangle {
            readonly property bool highlighted: ListView.view.focus
                                                && (ListView.isCurrentItem || mouseArea.containsMouse)

            width: ListView.view.width
            height: vpx(18) * 2.5
            color: highlighted ? "#555" : "transparent"

            Text {
                id: nameLabel
                text: modelData.fullname
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(18)

                anchors.left: parent.left
                anchors.leftMargin: vpx(18)
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width * 0.5
                elide: Text.ElideRight
            }

            Text {
                text: root.currentText(modelData)
                color: modelData.current !== "" ? "#3cc" : "#bbb"
                font.family: globalFonts.sans
                font.pixelSize: vpx(16)

                anchors.right: parent.right
                anchors.rightMargin: vpx(18)
                anchors.left: nameLabel.right
                anchors.leftMargin: vpx(12)
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    list.currentIndex = index;
                    root.openPicker(index);
                }
            }
        }
    }


    // The list of emulators for one system
    FocusScope {
        id: picker

        property int systemIndex: -1
        readonly property var sys: (systemIndex >= 0 && systemIndex < root.systems.length)
                                   ? root.systems[systemIndex] : null
        // the first entry (empty label) is the default
        readonly property var entries: sys
            ? [{ "label": "", "installed": true }].concat(sys.options)
            : []

        anchors.fill: parent
        enabled: focus
        visible: focus

        onFocusChanged: {
            if (!focus || !sys)
                return;
            var idx = 0;
            for (var i = 1; i < entries.length; i++) {
                if (entries[i].label === sys.current)
                    idx = i;
            }
            pickerList.currentIndex = idx;
            pickerList.positionViewAtIndex(idx, ListView.Center);
        }

        function pick(index) {
            if (index < 0 || index >= entries.length)
                return;
            root.choose(systemIndex, entries[index].label);
        }

        Keys.onPressed: {
            if (event.isAutoRepeat)
                return;
            if (api.keys.isCancel(event)) {
                event.accepted = true;
                list.focus = true;
            }
            else if (api.keys.isAccept(event)) {
                event.accepted = true;
                picker.pick(pickerList.currentIndex);
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.4

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: list.focus = true
            }
        }

        Rectangle {
            id: pickerBox

            width: main.width * 0.75
            height: Math.min(pickerTitle.height + pickerList.contentHeight + vpx(30), main.height)
            anchors.centerIn: parent

            color: "#2c2d30"
            radius: vpx(8)

            MouseArea {
                anchors.fill: parent
            }

            Text {
                id: pickerTitle
                text: picker.sys ? qsTr("%1: launch with").arg(picker.sys.fullname) + api.tr : ""
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(20)

                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                padding: vpx(15)
                elide: Text.ElideRight
            }

            ListView {
                id: pickerList

                anchors.top: pickerTitle.bottom
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(15)
                anchors.topMargin: 0
                clip: true

                focus: true
                model: picker.entries
                highlightRangeMode: ListView.ApplyRange
                preferredHighlightBegin: height * 0.5 - vpx(18) * 1.25
                preferredHighlightEnd: height * 0.5 + vpx(18) * 1.25
                highlightMoveDuration: 0

                delegate: Rectangle {
                    readonly property bool highlighted: ListView.isCurrentItem || optionMouse.containsMouse
                    readonly property bool isDefault: modelData.label === ""

                    width: ListView.view.width
                    height: vpx(18) * 2.4
                    color: highlighted ? "#3aa" : "transparent"
                    radius: vpx(4)

                    Text {
                        id: optionLabel
                        text: parent.isDefault
                            ? qsTr("Default (%1)").arg(picker.sys ? picker.sys.defaultLabel : "") + api.tr
                            : modelData.label
                        color: parent.highlighted ? "#111" : "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(18)

                        anchors.left: parent.left
                        anchors.leftMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * 0.68
                        elide: Text.ElideRight
                    }

                    Text {
                        visible: !parent.isDefault
                        text: modelData.installed ? qsTr("installed") + api.tr : qsTr("not installed") + api.tr
                        color: parent.highlighted ? "#113" : (modelData.installed ? "#8d8" : "#d88")
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(14)

                        anchors.right: parent.right
                        anchors.rightMargin: vpx(12)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    MouseArea {
                        id: optionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: picker.pick(index)
                    }
                }
            }
        }
    }


    ReloadQuestion {
        id: reloadDialog
        onAccept: {
            list.focus = true;
            root.mSettingsChanged = false;
            Internal.settings.reloadProviders();
        }
        onCancel: {
            list.focus = true;
            root.mSettingsChanged = false;
            root.close();
        }
    }
}
