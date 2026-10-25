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


// Pegasus X: the scraper's interactive mode (like ES-DE's): the matches found
// for a game on the left, the highlighted one's art and details on the right.
// A uses it, Y searches another name, X skips the game, B stops scraping.
FocusScope {
    id: root

    signal finished   // the choice was made (or scraping stopped): focus goes back

    property var choice: ({})
    readonly property var candidates: choice.candidates || []
    readonly property var picked: (results.currentIndex >= 0 && results.currentIndex < candidates.length)
                                  ? candidates[results.currentIndex] : ({})

    anchors.fill: parent
    enabled: focus
    visible: focus

    function openFor(c) {
        choice = c;
        results.currentIndex = 0;
        results.positionViewAtBeginning();
        forceActiveFocus();
        results.focus = true;
    }
    function useIt() {
        if (candidates.length === 0)
            return;
        var index = results.currentIndex;
        root.finished();
        Internal.settings.scraperChoose(index);
    }
    function skipIt() {
        root.finished();
        Internal.settings.scraperSkip();
    }
    function stopAll() {
        root.finished();
        Internal.settings.stopScraper();
    }

    Keys.onPressed: {
        if (event.isAutoRepeat)
            return;
        if (api.keys.isAccept(event)) {
            event.accepted = true;
            useIt();
        }
        else if (api.keys.isFilters(event)) {
            event.accepted = true;
            searchDialog.openFor(choice.searchName || choice.title || "");
        }
        else if (api.keys.isDetails(event)) {
            event.accepted = true;
            skipIt();
        }
        else if (api.keys.isCancel(event)) {
            event.accepted = true;
            stopAll();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#0c0d0e"
        opacity: 0.96
        MouseArea { anchors.fill: parent }
    }

    // title: the game, its collection, where the matches come from
    Column {
        id: top
        anchors.top: parent.top
        anchors.topMargin: vpx(18)
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.9
        spacing: vpx(4)
        Text {
            width: parent.width
            text: root.choice.title || ""
            color: "#eee"
            font.family: globalFonts.sans
            font.pixelSize: vpx(28)
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: (root.choice.collection || "") + "   ·   "
                  + qsTr("%1 search: \"%2\"").arg(root.choice.source || "").arg(root.choice.searchName || "") + api.tr
            color: "#9aa"
            font.family: globalFonts.sans
            font.pixelSize: vpx(17)
            elide: Text.ElideRight
        }
    }

    // the matches
    Rectangle {
        id: listBox
        anchors.top: top.bottom
        anchors.topMargin: vpx(16)
        anchors.bottom: hints.top
        anchors.bottomMargin: vpx(10)
        anchors.left: parent.left
        anchors.leftMargin: parent.width * 0.05
        width: parent.width * 0.4
        color: "#16191b"
        radius: vpx(8)

        ListView {
            id: results
            anchors.fill: parent
            anchors.margins: vpx(6)
            clip: true
            focus: true
            model: root.candidates
            highlightMoveDuration: 0
            delegate: Rectangle {
                readonly property bool isOn: ListView.isCurrentItem
                width: ListView.view.width
                height: vpx(58)
                radius: vpx(6)
                color: isOn ? "#2a5f5f" : "transparent"
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: vpx(14)
                    anchors.rightMargin: vpx(14)
                    Text {
                        width: parent.width
                        text: modelData.name || ""
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(19)
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        visible: text !== ""
                        text: modelData.info || ""
                        color: parent.parent.isOn ? "#cdd" : "#889"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(15)
                        elide: Text.ElideRight
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (results.currentIndex === index)
                            root.useIt();
                        results.currentIndex = index;
                    }
                }
            }
        }
        Text {
            anchors.centerIn: parent
            width: parent.width * 0.85
            visible: root.candidates.length === 0
            text: qsTr("Nothing found. Y: search another name · X: skip this game") + api.tr
            color: "#e8b06a"
            font.family: globalFonts.sans
            font.pixelSize: vpx(18)
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // the highlighted match: art, details, description
    Item {
        anchors.top: listBox.top
        anchors.bottom: listBox.bottom
        anchors.left: listBox.right
        anchors.leftMargin: vpx(24)
        anchors.right: parent.right
        anchors.rightMargin: parent.width * 0.05
        visible: root.candidates.length > 0

        Rectangle {
            id: thumbBox
            anchors.top: parent.top
            anchors.left: parent.left
            width: parent.width * 0.42
            height: Math.min(parent.height * 0.62, width * 1.4)
            color: "#151718"
            radius: vpx(8)
            border.color: "#2c3134"
            border.width: vpx(2)
            Image {
                id: thumb
                anchors.fill: parent
                anchors.margins: vpx(8)
                source: root.picked.thumb || ""
                sourceSize.width: vpx(400)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                opacity: status === Image.Ready ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }
            Text {
                anchors.centerIn: parent
                visible: thumb.status !== Image.Ready
                text: thumb.status === Image.Loading ? qsTr("Loading...") + api.tr
                    : root.picked.thumbError !== undefined ? qsTr("The picture didn't download (%1)").arg(root.picked.thumbError || qsTr("no connection")) + api.tr
                    : qsTr("No picture") + api.tr
                color: "#666"
                font.family: globalFonts.sans
                font.pixelSize: vpx(16)
            }
        }

        Column {
            anchors.top: parent.top
            anchors.left: thumbBox.right
            anchors.leftMargin: vpx(18)
            anchors.right: parent.right
            spacing: vpx(6)
            Text {
                width: parent.width
                text: root.picked.name || ""
                color: "#eee"
                font.family: globalFonts.sans
                font.pixelSize: vpx(22)
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
            Repeater {
                model: [
                    [qsTr("Released") + api.tr, root.picked.release],
                    [qsTr("Developer") + api.tr, root.picked.developer],
                    [qsTr("Publisher") + api.tr, root.picked.publisher],
                    [qsTr("Genre") + api.tr, root.picked.genre],
                    [qsTr("Players") + api.tr, root.picked.players],
                    [qsTr("Rating") + api.tr, root.picked.rating],
                ]
                delegate: Row {
                    visible: !!modelData[1]
                    spacing: vpx(8)
                    Text {
                        text: modelData[0]
                        color: "#889"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(16)
                        width: vpx(100)
                    }
                    Text {
                        text: modelData[1] || ""
                        color: "#cdd"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(16)
                        width: vpx(260)
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Text {
            anchors.top: thumbBox.bottom
            anchors.topMargin: vpx(14)
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            text: root.picked.description || ""
            color: "#bbb"
            font.family: globalFonts.sans
            font.pixelSize: vpx(16)
            wrapMode: Text.WordWrap
            elide: Text.ElideRight
            clip: true
        }
    }

    Text {
        id: hints
        anchors.bottom: parent.bottom
        anchors.bottomMargin: vpx(14)
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("A use this match · Y search another name · X skip this game · B stop") + api.tr
        color: "#ddd"
        font.family: globalFonts.sans
        font.pixelSize: vpx(17)
    }


    // -- Another search name ------------------------------------------------------

    FocusScope {
        id: searchDialog

        anchors.fill: parent
        enabled: focus
        visible: focus
        z: 2

        function openFor(text) {
            searchInput.text = text;
            searchInput.cursorPosition = text.length;
            searchKeyboard.row = 1;
            searchKeyboard.col = 0;
            focus = true;
            searchKeyboard.focus = true;
        }
        function finish() {
            var name = searchInput.text.trim();
            results.focus = true;
            if (name === "")
                return;
            root.finished();
            Internal.settings.scraperRefine(name);
        }

        Keys.onPressed: {
            if (api.keys.isCancel(event) && !event.isAutoRepeat) {
                event.accepted = true;
                results.focus = true;
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.6
            MouseArea { anchors.fill: parent }
        }
        Rectangle {
            width: Math.min(parent.width * 0.7, vpx(800))
            height: searchColumn.height + vpx(40)
            anchors.horizontalCenter: parent.horizontalCenter
            y: vpx(30)
            color: "#444"
            radius: vpx(8)
            Column {
                id: searchColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(20)
                spacing: vpx(12)
                Text {
                    width: parent.width
                    text: qsTr("Search for") + api.tr
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(19)
                    horizontalAlignment: Text.AlignHCenter
                }
                Rectangle {
                    width: parent.width
                    height: vpx(52)
                    color: "#333"
                    border.color: "#3aa"
                    border.width: vpx(2)
                    TextInput {
                        id: searchInput
                        anchors.fill: parent
                        anchors.leftMargin: vpx(14)
                        anchors.rightMargin: vpx(14)
                        verticalAlignment: TextInput.AlignVCenter
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(20)
                        clip: true
                        cursorVisible: searchDialog.visible
                        activeFocusOnPress: false
                    }
                }
            }
        }
        OnScreenKeyboard {
            id: searchKeyboard
            anchors.bottom: parent.bottom
            anchors.bottomMargin: vpx(12)
            anchors.horizontalCenter: parent.horizontalCenter
            keyH: vpx(40)
            target: searchInput
            onDone: searchDialog.finish()
        }
    }
}
