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


// Pegasus X: the button prompts at the foot of a screen. `text` is the
// prompts as words, "A edit · X undo · B back": a part that starts with a
// button's name gets the button drawn as a key (A green, B red, X blue,
// Y yellow; shoulders, triggers, Start, Select and the arrows as grey keys)
// and the words after it as they are. Text with no button in it (a message
// of the screen's) is shown plainly, wrapped, as before.
// One Text with rich text: nothing is created or laid out per prompt.
Rectangle {
    id: root

    property string text: ""
    property alias textWidth: label.width

    readonly property var keys: ({
        "A": "#6fcf4f", "B": "#ee5a5a", "X": "#5aa9ee", "Y": "#f2d24d"
    })
    readonly property var greyKeys: ["LB", "RB", "LT", "RT", "Start", "Select", "LB / RB", "LT / RT", "◂ ▸", "▴ ▾"]

    // text -> rich text, and whether any key was found
    readonly property var rendered: {
        var parts = String(text).split("·");
        var out = [];
        var found = false;
        for (var i = 0; i < parts.length; i++) {
            var part = parts[i].trim();
            if (part === "")
                continue;
            var key = "";
            var rest = part;
            var sp = part.indexOf(" ");
            var head = sp < 0 ? part : part.substring(0, sp);
            if (keys[head] !== undefined) {
                key = head;
            } else {
                for (var g = 0; g < greyKeys.length; g++) {
                    if (part === greyKeys[g] || part.indexOf(greyKeys[g] + " ") === 0) {
                        key = greyKeys[g];
                        break;
                    }
                }
            }
            if (key !== "") {
                found = true;
                rest = part.substring(key.length).trim();
                var labels = key.split(" / ");
                var html = "";
                for (var k = 0; k < labels.length; k++) {
                    var bg = keys[labels[k]] || "#4a5054";
                    var fg = keys[labels[k]] ? "#161616" : "#e8ecee";
                    html += "<span style=\"background-color:" + bg + ";color:" + fg + ";font-weight:bold\">&nbsp;"
                          + html(labels[k]) + "&nbsp;</span>";
                    if (k + 1 < labels.length)
                        html += "&nbsp;";
                }
                out.push(html + (rest !== "" ? "&nbsp;&nbsp;" + html(rest) : ""));
            } else {
                out.push(html(part));
            }
        }
        return { "html": out.join("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"), "keys": found };
    }

    function html(s) {
        return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

    width: parent ? parent.width : implicitWidth
    implicitHeight: label.implicitHeight + vpx(26)
    height: implicitHeight
    color: rendered.keys ? "#1a1d1f" : "transparent"

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: vpx(1)
        color: "#3a3f42"
        visible: root.rendered.keys
    }

    Text {
        id: label
        anchors.centerIn: parent
        width: parent.width * (root.rendered.keys ? 0.94 : 0.7)
        text: root.rendered.keys ? root.rendered.html : root.text
        textFormat: root.rendered.keys ? Text.RichText : Text.PlainText
        color: "#ddd"
        font.family: globalFonts.sans
        font.pixelSize: vpx(17)
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
    }
}
