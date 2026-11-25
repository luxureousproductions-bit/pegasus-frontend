// Pegasus X test environment: Settings > Gamepad layout's picture with each
// button's highlight on it, at the sizes the page shows it (0.3 of the
// screen's width: 854, 1280 and 1920 wide screens), saved as pictures for
// run.sh to check (the highlight's middle against where the button is on
// the picture). GamepadPreview.qml as the app has it; only vpx (main.qml's)
// and the picture's path (the app reads it from its resources) are given here.
import QtQuick 2.0
import "../../src/frontend/menu/settings/gamepad"

Item {
    id: root
    width: 620
    height: 440

    readonly property var fields: [ "a", "b", "x", "y", "dpup", "dpdown", "dpleft", "dpright",
        "lsx", "lsy", "lsb", "rsx", "rsy", "rsb", "select", "start", "guide", "lb", "rb", "lt", "rt" ]
    readonly property var screens: [ 854, 1280, 1920 ]
    property int si: 0
    property int fi: 0
    // as main.qml's: a size on a 1280 x 720 screen, scaled to this one
    function vpx(value) { return Math.round(value * screens[si] / 1280); }

    Rectangle { anchors.fill: parent; color: "#222" }

    GamepadPreview {
        id: preview
        x: 10
        y: 10
        width: Math.round(root.screens[root.si] * 0.3)
        source: Qt.resolvedUrl("../../src/frontend/assets/gamepad/preview.png")
        currentField: root.fields[root.fi]
    }

    // one picture at a time: grabbed, saved and logged, then the next button
    Timer {
        id: step
        interval: 250
        running: preview.status === Image.Ready
        onTriggered: {
            if (root.si >= root.screens.length) {
                Qt.quit();
                return;
            }
            var name = "gamepad-" + root.screens[root.si] + "-" + root.fields[root.fi];
            var info = [ name, preview.x, preview.y, preview.width, preview.height,
                         preview.paintedWidth, preview.paintedHeight,
                         preview.currentPointX, preview.currentPointY ].join(" ");
            root.grabToImage(function(result) {
                result.saveToFile(name + ".png");
                console.log("POINT " + info);
                root.fi++;
                if (root.fi >= root.fields.length) {
                    root.fi = 0;
                    root.si++;
                }
                step.restart();
            });
        }
    }
}
