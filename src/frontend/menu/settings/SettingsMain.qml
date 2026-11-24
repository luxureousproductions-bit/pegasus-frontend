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
    signal openEsdeImport
    signal openThemeDownloader
    signal openUpdater
    signal openRootFolder
    signal openBackupReset
    signal openMetadataEditor
    signal openScraper
    signal openRetroAchievements
    signal openAndroidSafSettings
    signal openProviderSettings
    signal reloadRequested
    signal openAdvanced

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


    // Pegasus X: 12 rows (Games, Look, Controls, Pegasus X, Advanced...); the
    // rest on the Advanced page (SettingsAdvanced.qml). The screens behind the
    // rows, and the settings themselves, are as they were.
    readonly property list<SettingsEntry> optionList: [
        SettingsEntry {
            label: QT_TR_NOOP("Game directory...")
            desc: QT_TR_NOOP("Your systems and where their games are: pick systems, then the ROM folder they're in, or import a gamelist. Pegasus X keeps one metadata file per system in its own folder; your ROM folders are only read.")
            type: SettingsEntry.Type.Button
            buttonAction: root.openGameDirSettings
            section: "games"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Library...")
            desc: QT_TR_NOOP("Your collections and games: edit their details, art and emulators, hide what you don't want to see, keep Android sorted. Saved in Pegasus metadata files, so every theme shows it.")
            type: SettingsEntry.Type.Button
            buttonAction: root.openMetadataEditor
            section: "games"
            enabled: Internal.settings.esdeImportSupported
        },
        SettingsEntry {
            label: QT_TR_NOOP("Scraper...")
            desc: QT_TR_NOOP("Your games' details and art from ScreenScraper or LaunchboxDB: sources, content, pictures, accounts, and scraping the whole library.")
            type: SettingsEntry.Type.Button
            buttonAction: root.openScraper
            section: "games"
            enabled: Internal.settings.esdeImportSupported
        },
        SettingsEntry {
            label: QT_TR_NOOP("RetroAchievements...")
            desc: QT_TR_NOOP("Your games found on RetroAchievements by their ROM's hash, and your progress on them. Themes read the result (game.extra[\"ra-id\"]...).")
            type: SettingsEntry.Type.Button
            buttonAction: root.openRetroAchievements
            section: "games"
            enabled: Internal.settings.esdeImportSupported
        },
        SettingsEntry {
            label: QT_TR_NOOP("Theme")
            type: SettingsEntry.Type.Select
            selectBox: themeBox
            selectValue: Internal.settings.themes.currentName
            section: "look"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Theme downloader...")
            desc: QT_TR_NOOP("Browse the Pegasus theme gallery with pictures, install and update themes, or add one from a GitHub link.")
            type: SettingsEntry.Type.Button
            buttonAction: root.openThemeDownloader
            section: "look"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Language")
            type: SettingsEntry.Type.Select
            selectBox: localeBox
            selectValue: Internal.settings.locales.currentName
            section: "look"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Change controls...")
            type: SettingsEntry.Type.Button
            buttonAction: root.openKeySettings
            section: "controls"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Check for updates...")
            desc: QT_TR_NOOP("Download and install the newest Pegasus X build, over this one: your settings, library and themes stay.")
            type: SettingsEntry.Type.Button
            buttonAction: root.openUpdater
            section: "pegasusx"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Backup & reset...")
            desc: QT_TR_NOOP("Back up Pegasus X's settings, library and metadata to one file (no passwords), restore it, run setup again, or start over.")
            type: SettingsEntry.Type.Button
            buttonAction: root.openBackupReset
            section: "pegasusx"
        },
        SettingsEntry {
            label: QT_TR_NOOP("Accurate play times...")
            desc: QT_TR_NOOP("Allow Usage access so play times count only the time the emulator was really in front.")
            type: SettingsEntry.Type.Button
            buttonAction: function() { Internal.settings.openUsageAccess(); }
            section: "pegasusx"
            enabled: Qt.platform.os === "android"
        },
        // Pegasus X: everything else (the folder, scanning, data sources,
        // gamepad layout, mouse, fullscreen) is on the Advanced page
        SettingsEntry {
            label: QT_TR_NOOP("Advanced...")
            desc: QT_TR_NOOP("Pegasus X folder, Android directories, scanning, missing games, data sources, reloading, gamepad layout, mouse and fullscreen.")
            type: SettingsEntry.Type.Button
            buttonAction: root.openAdvanced
            section: "advanced"
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
                case "games": return QT_TR_NOOP("Games");
                case "look": return QT_TR_NOOP("Look");
                case "controls": return QT_TR_NOOP("Controls");
                case "pegasusx": return QT_TR_NOOP("Pegasus X");
                default: return "";  // Advanced...: a gap, no title
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
        // Pegasus X: focus moves at once (no slide to catch up with a held D-pad)
        highlightMoveDuration: 0
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
    MultivalueBox {
        id: themeBox
        z: 3

        model: Internal.settings.themes
        index: Internal.settings.themes.currentIndex

        onClose: options.focus = true
        onSelect: Internal.settings.themes.currentIndex = index
    }
}
