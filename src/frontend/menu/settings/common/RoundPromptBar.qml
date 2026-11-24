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


import QtQuick 2.6


// Pegasus X: button prompts drawn Xbox-style, as in Android apps to import:
// A, B, X, Y round with the letter, LB / RB bumpers, LT / RT triggers. The
// same bar as PromptBar (place, height, colours). `items`: [{ keys: ["A"],
// text: "choose" }, ...]; `message`, when set, is shown instead (plain text).
Rectangle {
    id: bar

    property var items: []
    property string message: ""

    readonly property var faceColors: ({ "A": "#6fcf4f", "B": "#ee5a5a", "X": "#5aa9ee", "Y": "#f2d24d" })

    width: parent ? parent.width : implicitWidth
    height: vpx(17) * 1.2 + vpx(26)
    color: "#1a1d1f"

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: vpx(1)
        color: "#3a3f42"
    }

    Text {
        visible: bar.message !== ""
        anchors.centerIn: parent
        width: parent.width * 0.94
        text: bar.message
        color: "#ddd"
        font.family: globalFonts.sans
        font.pixelSize: vpx(17)
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
    }

    Row {
        visible: bar.message === ""
        anchors.centerIn: parent
        spacing: vpx(28)
        Repeater {
            model: bar.items
            delegate: Row {
                spacing: vpx(8)
                Repeater {
                    model: modelData.keys
                    delegate: Item {
                        readonly property bool face: bar.faceColors[modelData] !== undefined
                        readonly property bool trigger: modelData === "LT" || modelData === "RT"
                        width: face ? vpx(24) : vpx(36)
                        height: vpx(24)
                        anchors.verticalCenter: parent.verticalCenter
                        // A, B, X, Y: a round button
                        Rectangle {
                            visible: parent.face
                            anchors.fill: parent
                            radius: width / 2
                            color: bar.faceColors[modelData] || "#4a5054"
                        }
                        // LB / RB: a bumper
                        Rectangle {
                            visible: !parent.face && !parent.trigger
                            anchors.fill: parent
                            anchors.topMargin: vpx(3)
                            anchors.bottomMargin: vpx(3)
                            radius: height / 2
                            color: "#4a5054"
                        }
                        // LT / RT: a trigger (round top, flat bottom)
                        Rectangle {
                            visible: parent.trigger
                            anchors.fill: parent
                            radius: width * 0.4
                            color: "#4a5054"
                        }
                        Rectangle {
                            visible: parent.trigger
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: parent.height / 2
                            color: "#4a5054"
                        }
                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            color: parent.face ? "#161616" : "#e8ecee"
                            font.family: globalFonts.sans
                            font.pixelSize: parent.face ? vpx(15) : vpx(13)
                            font.bold: true
                        }
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.text
                    color: "#ddd"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(17)
                }
            }
        }
    }
}
