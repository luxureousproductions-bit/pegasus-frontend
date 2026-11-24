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
import QtQuick 2.15
import QtQuick.Window 2.2
import Qt.labs.qmlmodels 1.0


// Pegasus X: Settings > Advanced. The rows that left the main list, in the
// same look (the same row kinds as SettingsMain), and doing what they did
// there: the same settings, the same screens. Two of them read differently:
// "Missing games" is one choice over the two settings it was before
// (verifyFiles and showMissingGames, nothing new saved), and three rows have
// shorter names (Data sources, Gamepad layout, Mouse support).
FocusScope {
    id: root

    signal close
    signal openRootFolder
    signal openAndroidSafSettings
    signal openProviderSettings
    signal openGamepadSettings

    width: parent.width
    height: parent.height
    visible: 0 < (x + width) && x < Window.window.width

    enabled: focus

    Keys.onPressed: {
        if (api.keys.isCancel(event) && !event.isAutoRepeat) {
            event.accepted = true;
            root.close();
        }
    }


    PegasusUtils.HorizontalSwipeArea {
        anchors.fill: parent
        onSwipeRight: root.close()
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: root.close()
    }

    ScreenHeader {
        id: header
        text: qsTr("Settings > Advanced") + api.tr
        z: 2
    }


    // Missing games, over the two settings there were (no new one):
    //   Hide           verifyFiles on,  showMissingGames off
    //   Show           verifyFiles on,  showMissingGames on
    //   Don't check    verifyFiles off  (showMissingGames left as it is)
    readonly property int missingIndex: !Internal.settings.verifyFiles ? 2
                                      : (Internal.settings.showMissingGames ? 1 : 0)
    function setMissing(index) {
        if (index === 2) {
            Internal.settings.verifyFiles = false;
            return;
        }
        Internal.settings.verifyFiles = true;
        Internal.settings.showMissingGames = index === 1;
    }

    readonly property list<SettingsEntry> optionList: [
        SettingsEntry {
            label: QT_TR_NOOP("Pegasus X folder...")
            desc: QT_TR_NOOP("Where everything is kept: settings, themes, metadata, scraped art (downloaded_media/<system>/<kind>) and logs. Pick a folder of your own and move your data there.")
            type: SettingsEntry.Type.Button
            section: "advanced"
            buttonAction: root.openRootFolder
        },
        SettingsEntry {
            label: QT_TR_NOOP("Accessible Android directories...")
            type: SettingsEntry.Type.Button
            section: "advanced"
            buttonAction: root.openAndroidSafSettings
            enabled: Qt.platform.os === "android"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Scan for games on launch")
            desc: QT_TR_NOOP("Read every game folder each time Pegasus X starts. Off: the saved game list is used, and read again by itself when a game folder or metadata file changes (starts faster).")
            type: SettingsEntry.Type.Bool
            section: "advanced"
            boolValue: Internal.settings.scanOnLaunch
            boolSetter: (val) => Internal.settings.scanOnLaunch = val
        },
        SettingsEntry {
            label: QT_TR_NOOP("Missing games")
            type: SettingsEntry.Type.Select
            section: "advanced"
            selectBox: missingBox
            selectValue: missingModel.get(root.missingIndex).name
        },
        SettingsEntry {
            label: QT_TR_NOOP("Data sources...")
            type: SettingsEntry.Type.Button
            section: "advanced"
            buttonAction: root.openProviderSettings
        },
        SettingsEntry {
            label: QT_TR_NOOP("Reload all games")
            type: SettingsEntry.Type.Button
            section: "advanced"
            buttonAction: function() { Internal.settings.reloadAllGames(); }
        },
        SettingsEntry {
            label: QT_TR_NOOP("Gamepad layout...")
            type: SettingsEntry.Type.Button
            section: "advanced"
            buttonAction: root.openGamepadSettings
        },
        SettingsEntry {
            label: QT_TR_NOOP("Mouse support")
            desc: QT_TR_NOOP("By default the cursor is visible if there are any pointer devices connected.")
            type: SettingsEntry.Type.Bool
            section: "advanced"
            boolValue: Internal.settings.mouseSupport
            boolSetter: (val) => Internal.settings.mouseSupport = val
        },
        SettingsEntry {
            label: QT_TR_NOOP("Fullscreen mode")
            desc: QT_TR_NOOP("On some platforms this setting may have no effect.")
            type: SettingsEntry.Type.Bool
            section: "advanced"
            boolValue: Internal.settings.fullscreen
            boolSetter: (val) => Internal.settings.fullscreen = val
        }
    ]

    DelegateChooser {
        id: optionDelegates
        role: "type"

        DelegateChoice {
            roleValue: SettingsEntry.Type.Bool
            ToggleOption {
                label: qsTr(model.label) + api.tr
                desc: qsTr(model.desc) + api.tr
                checked: model.boolValue
                onCheckedChanged: model.boolSetter(checked)
                enabled: model.enabled
            }
        }

        DelegateChoice {
            roleValue: SettingsEntry.Type.Button
            SimpleButton {
                label: qsTr(model.label) + api.tr
                desc: model.desc ? qsTr(model.desc) + api.tr : ""
                onActivate: model.buttonAction()
                enabled: model.enabled
            }
        }

        DelegateChoice {
            roleValue: SettingsEntry.Type.Select
            MultivalueOption {
                label: qsTr(model.label) + api.tr
                value: qsTr(model.selectValue) + api.tr
                onActivate: model.selectBox.focus = true
            }
        }
    }

    ListView {
        id: options
        model: optionList
        delegate: optionDelegates

        width: root.width * 0.7
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.bottomMargin: header.height

        displayMarginBeginning: header.height
        displayMarginEnd: header.height

        focus: true
        highlightRangeMode: ListView.ApplyRange
        highlightMoveDuration: 0
        preferredHighlightBegin: height * 0.3
        preferredHighlightEnd: height * 0.7
    }


    ListModel {
        id: missingModel
        ListElement { name: QT_TR_NOOP("Hide") }
        ListElement { name: QT_TR_NOOP("Show") }
        ListElement { name: QT_TR_NOOP("Don't check (faster)") }
    }
    MultivalueBox {
        id: missingBox
        z: 3

        model: missingModel
        index: root.missingIndex

        onClose: options.focus = true
        onSelect: root.setMissing(index)
    }
}
