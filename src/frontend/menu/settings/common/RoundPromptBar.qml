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
// A, B, X, Y are the stock pictures of Settings > Change gamepad layout
// (assets/gamepad/x360/a.png...; a round button with the letter drawn here
// if a picture can't be loaded), LB / RB bumpers, LT / RT triggers,
// Start / Select small pills, the d-pad drawn (no arrow characters: a font
// without them shows boxes). The same bar as PromptBar (place, height,
// colours). Give it either
//   `items`: [{ keys: ["A"], text: "choose" }, ...], or
//   `text`: PromptBar's words, "A edit · X undo · LB / RB top / bottom · B back":
//     a part that starts with a button's name gets the button drawn, the rest
//     as words; a part with no button is shown as words. Text with no button
//     at all (a message of the screen's) is shown plainly, wrapped, with no
//     bar behind it, as PromptBar does.
// `message`, when set, is shown instead (plain text, one line).
Rectangle {
    id: bar

    property var items: []
    property string message: ""
    property string text: ""

    readonly property var faceColors: ({ "A": "#6fcf4f", "B": "#ee5a5a", "X": "#5aa9ee", "Y": "#f2d24d" })
    // where Change gamepad layout (gamepad/GamepadField.qml) takes its button pictures
    readonly property string gamepadIcons: "qrc:/frontend/assets/gamepad/x360/"
    // the names PromptBar's words use, longest first, and the keys drawn for them
    readonly property var named: [
        ["LB / RB", ["LB", "RB"]], ["LT / RT", ["LT", "RT"]],
        ["◂ ▸", ["DPAD_LR"]], ["▴ ▾", ["DPAD_UD"]],
        ["Start", ["Start"]], ["Select", ["Select"]],
        ["LB", ["LB"]], ["RB", ["RB"]], ["LT", ["LT"]], ["RT", ["RT"]],
        ["▾", ["DPAD_D"]], ["▴", ["DPAD_U"]], ["◂", ["DPAD_L"]], ["▸", ["DPAD_R"]],
    ]

    // `text` as items; plain: no button in it
    readonly property var parsed: {
        var out = [];
        var found = false;
        var parts = String(text).split("·");
        for (var i = 0; i < parts.length; i++) {
            var part = parts[i].trim();
            if (part === "")
                continue;
            var keys = [];
            var rest = part;
            var sp = part.indexOf(" ");
            var head = sp < 0 ? part : part.substring(0, sp);
            if (faceColors[head] !== undefined) {
                keys = [head];
                rest = part.substring(head.length).trim();
            }
            else {
                for (var n = 0; n < named.length; n++) {
                    if (part === named[n][0] || part.indexOf(named[n][0] + " ") === 0) {
                        keys = named[n][1];
                        rest = part.substring(named[n][0].length).trim();
                        break;
                    }
                }
            }
            if (keys.length > 0)
                found = true;
            out.push({ "keys": keys, "text": rest });
        }
        return { "items": out, "plain": !found };
    }
    readonly property var shown: text !== "" ? parsed.items : items
    readonly property bool plainText: text !== "" && parsed.plain

    width: parent ? parent.width : implicitWidth
    height: plainText ? plainLabel.implicitHeight + vpx(26) : vpx(17) * 1.2 + vpx(26)
    visible: text !== "" || items.length > 0 || message !== ""
    color: plainText ? "transparent" : "#1a1d1f"

    Rectangle {
        visible: !bar.plainText
        anchors.top: parent.top
        width: parent.width
        height: vpx(1)
        color: "#3a3f42"
    }

    // a message of the screen's (no button in it), as PromptBar shows it
    Text {
        id: plainLabel
        visible: bar.plainText
        anchors.centerIn: parent
        width: parent.width * 0.7
        text: bar.plainText ? bar.text : ""
        color: "#ddd"
        font.family: globalFonts.sans
        font.pixelSize: vpx(17)
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
    }

    Text {
        visible: bar.message !== "" && !bar.plainText
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
        id: promptRow
        visible: bar.message === "" && !bar.plainText
        anchors.centerIn: parent
        spacing: vpx(28)
        // (a long line is made a little smaller rather than cut)
        scale: width > bar.width * 0.96 ? bar.width * 0.96 / width : 1
        Repeater {
            model: bar.shown
            delegate: Row {
                spacing: vpx(8)
                Repeater {
                    model: modelData.keys
                    delegate: Item {
                        readonly property bool face: bar.faceColors[modelData] !== undefined
                        readonly property bool trigger: modelData === "LT" || modelData === "RT"
                        readonly property bool pill: modelData === "Start" || modelData === "Select"
                        readonly property bool dpad: String(modelData).indexOf("DPAD_") === 0
                        readonly property bool drawnFace: face && faceIcon.status !== Image.Ready
                        width: face || dpad ? vpx(24) : pill ? pillLabel.implicitWidth + vpx(16) : vpx(36)
                        height: vpx(24)
                        anchors.verticalCenter: parent.verticalCenter
                        // A, B, X, Y: the gamepad layout menu's picture...
                        Image {
                            id: faceIcon
                            visible: parent.face && status === Image.Ready
                            anchors.fill: parent
                            source: parent.face ? bar.gamepadIcons + String(modelData).toLowerCase() + ".png" : ""
                            sourceSize.width: vpx(48)
                            sourceSize.height: vpx(48)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }
                        // ...or, without it, a round button with the letter
                        Rectangle {
                            visible: parent.drawnFace
                            anchors.fill: parent
                            radius: width / 2
                            color: bar.faceColors[modelData] || "#4a5054"
                        }
                        // LB / RB: a bumper; Start / Select: a small pill
                        Rectangle {
                            visible: !parent.face && !parent.trigger && !parent.dpad
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
                        // the d-pad: a cross, the arms meant lit
                        Item {
                            visible: parent.dpad
                            anchors.fill: parent
                            readonly property real arm: width / 3
                            readonly property string which: String(modelData).substring(5)
                            Rectangle { x: parent.arm; y: 0; width: parent.arm; height: parent.height; radius: vpx(2); color: "#4a5054" }
                            Rectangle { x: 0; y: parent.arm; width: parent.width; height: parent.arm; radius: vpx(2); color: "#4a5054" }
                            Rectangle { visible: parent.which === "LR" || parent.which === "L"; x: 0; y: parent.arm; width: parent.arm; height: parent.arm; radius: vpx(2); color: "#e8ecee" }
                            Rectangle { visible: parent.which === "LR" || parent.which === "R"; x: parent.arm * 2; y: parent.arm; width: parent.arm; height: parent.arm; radius: vpx(2); color: "#e8ecee" }
                            Rectangle { visible: parent.which === "UD" || parent.which === "U"; x: parent.arm; y: 0; width: parent.arm; height: parent.arm; radius: vpx(2); color: "#e8ecee" }
                            Rectangle { visible: parent.which === "UD" || parent.which === "D"; x: parent.arm; y: parent.arm * 2; width: parent.arm; height: parent.arm; radius: vpx(2); color: "#e8ecee" }
                        }
                        Text {
                            id: pillLabel
                            visible: !parent.dpad && (!parent.face || parent.drawnFace)
                            anchors.centerIn: parent
                            text: modelData
                            color: parent.face ? "#161616" : "#e8ecee"
                            font.family: globalFonts.sans
                            font.pixelSize: parent.face ? vpx(15) : parent.pill ? vpx(12) : vpx(13)
                            font.bold: true
                        }
                    }
                }
                Text {
                    visible: modelData.text !== ""
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
