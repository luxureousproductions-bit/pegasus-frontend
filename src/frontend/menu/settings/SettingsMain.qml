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
import QtQuick 2.15
import QtQuick.Window 2.2
import Qt.labs.qmlmodels 1.0


FocusScope {
    id: root

    signal close
    signal openKeySettings
    signal openGamepadSettings
    signal openGameDirSettings
    signal openAndroidSafSettings
    signal openProviderSettings
    signal openLibraryHub
    signal reloadRequested

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
        text: qsTr("Settings") + api.tr
        z: 2
    }


    readonly property list<SettingsEntry> optionList: [
        SettingsEntry {
            label: QT_TR_NOOP("Language")
            type: SettingsEntry.Type.Select
            selectBox: localeBox
            selectValue: Internal.settings.locales.currentName
            section: "general"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Theme")
            type: SettingsEntry.Type.Select
            selectBox: themeBox
            selectValue: Internal.settings.themes.currentName
            section: "general"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Screen shape")
            desc: QT_TR_NOOP("Fit the screen to a shape, with black bars around it. Auto uses the whole screen.")
            type: SettingsEntry.Type.Select
            selectBox: aspectBox
            selectValue: Internal.settings.screenAspect === "auto" ? qsTr("Auto") + api.tr : Internal.settings.screenAspect
            section: "general"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Fullscreen mode")
            desc: QT_TR_NOOP("On some platforms this setting may have no effect.")
            type: SettingsEntry.Type.Bool
            boolValue: Internal.settings.fullscreen
            boolSetter: (val) => Internal.settings.fullscreen = val
            section: "general"
        },

        SettingsEntry {
            label: QT_TR_NOOP("Change controls...")
            type: SettingsEntry.Type.Button
            buttonAction: root.openKeySettings
            section: "controls"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Change gamepad layout...")
            type: SettingsEntry.Type.Button
            buttonAction: root.openGamepadSettings
            section: "controls"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Enable mouse support")
            desc: QT_TR_NOOP("By default the cursor is visible if there are any pointer devices connected.")
            type: SettingsEntry.Type.Bool
            boolValue: Internal.settings.mouseSupport
            boolSetter: (val) => Internal.settings.mouseSupport = val
            section: "controls"
        },

        SettingsEntry {
            label: QT_TR_NOOP("Set game directories...")
            type: SettingsEntry.Type.Button
            buttonAction: root.openGameDirSettings
            section: "gaming"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Accessible Android directories...")
            type: SettingsEntry.Type.Button
            buttonAction: root.openAndroidSafSettings
            section: "gaming"
            enabled: Qt.platform.os === "android"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Scan for games on launch")
            desc: QT_TR_NOOP("Scan for games every time Pegasus starts. You can disable this to improve loading times, but you will need to manually reload the game list to pick up changes.")
            type: SettingsEntry.Type.Bool
            boolValue: Internal.settings.scanOnLaunch
            boolSetter: (val) => Internal.settings.scanOnLaunch = val
            section: "gaming"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Validate game files")
            desc: QT_TR_NOOP("Check the game files and only show games that actually exist. You can disable this to improve loading times.")
            type: SettingsEntry.Type.Bool
            boolValue: Internal.settings.verifyFiles
            boolSetter: (val) => Internal.settings.verifyFiles = val
            section: "gaming"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Show missing games")
            desc: QT_TR_NOOP("Show all detected games, including those that may not exist.")
            type: SettingsEntry.Type.Bool
            boolValue: Internal.settings.showMissingGames
            boolSetter: (val) => Internal.settings.showMissingGames = val
            section: "gaming"
            enabled: Internal.settings.verifyFiles
        },
        SettingsEntry {
            label: QT_TR_NOOP("Enable/disable data sources...")
            type: SettingsEntry.Type.Button
            buttonAction: root.openProviderSettings
            section: "gaming"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Reload all games")
            type: SettingsEntry.Type.Button
            buttonAction: root.reloadRequested
            section: "gaming"
        },

        SettingsEntry {
            label: QT_TR_NOOP("Library...")
            desc: QT_TR_NOOP("ROM folders, media, ES-DE import, and each system's name, emulator and hidden items.")
            type: SettingsEntry.Type.Button
            buttonAction: root.openLibraryHub
            section: "pegasusx"
            enabled: Internal.settings.pegasusxLibrarySupported
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
                onActivate: model.buttonAction()
                enabled: model.enabled
            }
        }

        DelegateChoice {
            roleValue: SettingsEntry.Type.Select
            MultivalueOption {
                label: qsTr(model.label) + api.tr
                value: model.selectValue
                onActivate: model.selectBox.focus = true
            }
        }
    }

    Component {
        id: sectionTitle
        SectionTitle {
            required property string section

            readonly property string trText: switch (section) {
                case "general": return QT_TR_NOOP("General");
                case "controls": return QT_TR_NOOP("Controls");
                case "gaming": return QT_TR_NOOP("Gaming");
                case "pegasusx": return QT_TR_NOOP("Pegasus X library");
            }

            text: qsTr(trText) + api.tr
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
        highlightMoveDuration: 150
        preferredHighlightBegin: height * 0.3
        preferredHighlightEnd: height * 0.7

        section.property: "section"
        section.delegate: sectionTitle
    }


    MultivalueBox {
        id: localeBox
        z: 3

        model: Internal.settings.locales
        index: Internal.settings.locales.currentIndex

        onClose: options.focus = true
        onSelect: Internal.settings.locales.currentIndex = index
    }
    ListModel {
        id: aspectModel
        ListElement { name: "Auto"; value: "auto" }
        ListElement { name: "16:9"; value: "16:9" }
        ListElement { name: "16:10"; value: "16:10" }
        ListElement { name: "5:3"; value: "5:3" }
        ListElement { name: "3:2"; value: "3:2" }
        ListElement { name: "4:3"; value: "4:3" }
        ListElement { name: "1:1"; value: "1:1" }
    }
    MultivalueBox {
        id: aspectBox
        z: 3

        model: aspectModel
        index: {
            for (var i = 0; i < aspectModel.count; i++) {
                if (aspectModel.get(i).value === Internal.settings.screenAspect)
                    return i;
            }
            return 0;
        }

        onClose: options.focus = true
        onSelect: Internal.settings.screenAspect = aspectModel.get(index).value
    }
    MultivalueBox {
        id: themeBox
        z: 3

        model: Internal.settings.themes
        index: Internal.settings.themes.currentIndex

        onClose: options.focus = true
        onSelect: Internal.settings.themes.currentIndex = index
    }
}
