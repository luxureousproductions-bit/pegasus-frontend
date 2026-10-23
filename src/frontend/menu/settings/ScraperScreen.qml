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


// Pegasus X: Settings > Library > Scraper. Laid out like ES-DE's scraper
// menu (where from, which games, which systems, accounts, content...), in
// Pegasus' style. Every change is saved right away.
FocusScope {
    id: root

    signal close

    anchors.fill: parent

    property string page: "main"   // main, systems, accounts, content, miximage, other
    property var s: Internal.settings.scraperSettings()
    property string message: ""
    property var accounts: ({})    // what the last check said
    property bool checking: false

    function set(key, value) {
        Internal.settings.setScraperSetting(key, value);
        var next = Object.assign({}, s);
        next[key] = value;
        keepPosition(function() { s = next; });
        if (key === "ssUser" || key === "ssPassword" || key === "sgdbKey" || key === "ssUse")
            checkAccounts();
    }
    function checkAccounts() {
        checking = true;
        Internal.settings.checkScraperAccounts();
    }
    Connections {
        target: Internal.settings
        function onScraperAccountsChecked(result) {
            root.checking = false;
            root.keepPosition(function() { root.accounts = result; });
        }
    }
    Component.onCompleted: checkAccounts()

    function keepPosition(change) {
        var index = list.currentIndex;
        var y = list.contentY;
        change();
        list.currentIndex = index;
        list.contentY = y;
    }

    // -- What each page lists ----------------------------------------------------

    readonly property var sourceChoices: [
        { "v": "screenscraper", "t": "ScreenScraper" },
        { "v": "steamgriddb", "t": qsTr("SteamGridDB (art only)") + api.tr },
    ]
    readonly property var gamesChoices: [
        { "v": "all", "t": qsTr("All games") + api.tr },
        { "v": "favorites", "t": qsTr("Favorite games") + api.tr },
        { "v": "nometadata", "t": qsTr("No metadata") + api.tr },
        { "v": "noimage", "t": qsTr("No game image") + api.tr },
        { "v": "novideo", "t": qsTr("No game video") + api.tr },
        { "v": "missing", "t": qsTr("Missing anything") + api.tr },
    ]
    readonly property var regionChoices: [
        { "v": "us", "t": "USA" }, { "v": "eu", "t": qsTr("Europe") + api.tr },
        { "v": "wor", "t": qsTr("World") + api.tr }, { "v": "jp", "t": qsTr("Japan") + api.tr },
        { "v": "uk", "t": "UK" }, { "v": "fr", "t": qsTr("France") + api.tr },
        { "v": "de", "t": qsTr("Germany") + api.tr }, { "v": "sp", "t": qsTr("Spain") + api.tr },
        { "v": "it", "t": qsTr("Italy") + api.tr }, { "v": "br", "t": qsTr("Brazil") + api.tr },
        { "v": "kr", "t": qsTr("Korea") + api.tr }, { "v": "au", "t": qsTr("Australia") + api.tr },
    ]
    readonly property var languageChoices: [
        { "v": "en", "t": "English" }, { "v": "es", "t": "Español" }, { "v": "fr", "t": "Français" },
        { "v": "de", "t": "Deutsch" }, { "v": "it", "t": "Italiano" }, { "v": "pt", "t": "Português" },
        { "v": "nl", "t": "Nederlands" }, { "v": "sv", "t": "Svenska" }, { "v": "pl", "t": "Polski" },
        { "v": "ru", "t": "Русский" }, { "v": "ja", "t": "日本語" }, { "v": "ko", "t": "한국어" },
        { "v": "zh", "t": "中文" },
    ]
    readonly property var miximageChoices: [
        { "v": "mixrbv2", "t": qsTr("ScreenScraper mix, style 2") + api.tr },
        { "v": "mixrbv1", "t": qsTr("ScreenScraper mix, style 1") + api.tr },
    ]
    readonly property var contentToggles: [
        ["getNames", qsTr("Game names") + api.tr],
        ["getRatings", qsTr("Ratings") + api.tr],
        ["getDetails", qsTr("Other metadata") + api.tr],
        ["getVideos", qsTr("Videos") + api.tr],
        ["getScreenshots", qsTr("Screenshot images") + api.tr],
        ["getTitlescreens", qsTr("Title screen images") + api.tr],
        ["getCovers", qsTr("Box cover images") + api.tr],
        ["getBackcovers", qsTr("Box back cover images") + api.tr],
        ["getBox3d", qsTr("3D box images") + api.tr],
        ["getPhysical", qsTr("Physical media images") + api.tr],
        ["getFanart", qsTr("Fan art images") + api.tr],
        ["getLogos", qsTr("Logos (marquees)") + api.tr],
        ["getMiximages", qsTr("Miximages") + api.tr],
        ["getManuals", qsTr("Game manuals") + api.tr],
    ]

    function labelOf(choices, value) {
        for (var i = 0; i < choices.length; i++) {
            if (choices[i].v === value)
                return choices[i].t;
        }
        return String(value);
    }

    // every collection, A to Z
    readonly property var collectionNames: {
        var out = [];
        for (var i = 0; i < api.collections.count; i++)
            out.push(api.collections.get(i).name);
        out.sort(function(a, b) { return a.toLowerCase() < b.toLowerCase() ? -1 : 1; });
        return out;
    }
    function isPicked(name) {
        return (s.systems || []).indexOf(name) >= 0;
    }
    function togglePicked(name) {
        var list_in = (s.systems || []).slice();
        var at = list_in.indexOf(name);
        if (at >= 0)
            list_in.splice(at, 1);
        else
            list_in.push(name);
        set("systems", list_in);
    }

    // which games the filter takes, in the systems picked
    function wanted(g) {
        var a = g.assets;
        var noDetails = !g.description && !g.summary && !g.developer;
        var noImage = a.boxFrontList.length === 0 && a.screenshotList.length === 0;
        var noVideo = a.videoList.length === 0;
        switch (s.games) {
            case "favorites": return g.favorite;
            case "nometadata": return noDetails;
            case "noimage": return noImage;
            case "novideo": return noVideo;
            case "missing": return noDetails || noImage || noVideo || a.logoList.length === 0;
        }
        return true;
    }
    readonly property int gameCount: {
        var n = 0;
        var picked = s.systems || [];
        for (var i = 0; i < api.collections.count; i++) {
            var c = api.collections.get(i);
            if (picked.indexOf(c.name) < 0)
                continue;
            for (var j = 0; j < c.games.count; j++) {
                if (wanted(c.games.get(j)))
                    n++;
            }
        }
        return n;
    }

    function startScraper() {
        if ((s.systems || []).length === 0) {
            message = qsTr("Pick the systems to scrape first.") + api.tr;
            return;
        }
        if (s.source === "screenscraper" && !s.hasScreenScraper) {
            message = qsTr("ScreenScraper isn't in this build yet: its developer login is missing.") + api.tr;
            return;
        }
        message = qsTr("%1 games to scrape. Scraping itself comes in the next build.").arg(gameCount) + api.tr;
    }

    readonly property var rows: {
        var out = [];
        var i;
        var onOff = function(b) { return b ? qsTr("On") + api.tr : qsTr("Off") + api.tr; };
        if (page === "main") {
            out.push({ "kind": "choice", "key": "source", "text": qsTr("Scrape from") + api.tr, "choices": sourceChoices });
            out.push({ "kind": "choice", "key": "games", "text": qsTr("Scrape these games") + api.tr, "choices": gamesChoices });
            out.push({ "kind": "page", "to": "systems", "text": qsTr("Scrape these systems") + api.tr,
                       "value": qsTr("%1 selected").arg((s.systems || []).length) + api.tr });
            out.push({ "kind": "page", "to": "accounts", "text": qsTr("Account settings") + api.tr,
                       "value": checking ? qsTr("checking...") + api.tr : "" });
            out.push({ "kind": "page", "to": "content", "text": qsTr("Content settings") + api.tr, "value": "" });
            out.push({ "kind": "page", "to": "miximage", "text": qsTr("Miximage settings") + api.tr, "value": "" });
            out.push({ "kind": "page", "to": "other", "text": qsTr("Other settings") + api.tr, "value": "" });
            out.push({ "kind": "start", "text": (s.systems || []).length > 0
                ? qsTr("Start · %1 games").arg(gameCount) + api.tr : qsTr("Start") + api.tr });
            return out;
        }
        if (page === "systems") {
            out.push({ "kind": "all", "text": qsTr("Select all") + api.tr, "value": "" });
            out.push({ "kind": "none", "text": qsTr("Select none") + api.tr, "value": "" });
            out.push({ "kind": "section", "text": qsTr("Systems") + api.tr });
            for (i = 0; i < collectionNames.length; i++)
                out.push({ "kind": "system", "text": collectionNames[i], "check": isPicked(collectionNames[i]) });
            return out;
        }
        if (page === "accounts") {
            out.push({ "kind": "section", "text": "ScreenScraper" });
            out.push({ "kind": "text", "key": "ssUser", "text": qsTr("Username") + api.tr,
                       "value": s.ssUser !== "" ? s.ssUser : qsTr("not set") + api.tr, "muted": s.ssUser === "" });
            out.push({ "kind": "text", "key": "ssPassword", "secret": true, "text": qsTr("Password") + api.tr,
                       "value": s.ssPassword !== "" ? "••••••••" : qsTr("not set") + api.tr, "muted": s.ssPassword === "" });
            out.push({ "kind": "toggle", "key": "ssUse", "text": qsTr("Use this account") + api.tr });
            out.push({ "kind": "info", "text": qsTr("Status") + api.tr,
                       "value": checking ? qsTr("checking...") + api.tr : (accounts.ss || ""), "ok": !!accounts.ssOk });
            out.push({ "kind": "section", "text": "SteamGridDB" });
            out.push({ "kind": "text", "key": "sgdbKey", "secret": true, "text": qsTr("API key") + api.tr,
                       "value": s.sgdbKey !== "" ? "••••" + String(s.sgdbKey).slice(-4) : qsTr("not set") + api.tr, "muted": s.sgdbKey === "" });
            out.push({ "kind": "info", "text": qsTr("Status") + api.tr,
                       "value": checking ? qsTr("checking...") + api.tr : (accounts.sgdb || ""), "ok": !!accounts.sgdbOk });
            out.push({ "kind": "info", "text": qsTr("Get a key") + api.tr, "value": "steamgriddb.com › Preferences › API" });
            return out;
        }
        if (page === "content") {
            for (i = 0; i < contentToggles.length; i++)
                out.push({ "kind": "toggle", "key": contentToggles[i][0], "text": contentToggles[i][1] });
            return out;
        }
        if (page === "miximage") {
            out.push({ "kind": "choice", "key": "miximageStyle", "text": qsTr("Miximage style") + api.tr, "choices": miximageChoices });
            out.push({ "kind": "info", "text": qsTr("Made by ScreenScraper from the game's art, ready to show.") + api.tr, "value": "" });
            return out;
        }
        if (page === "other") {
            out.push({ "kind": "choice", "key": "region", "text": qsTr("Region") + api.tr, "choices": regionChoices });
            out.push({ "kind": "choice", "key": "language", "text": qsTr("Preferred language") + api.tr, "choices": languageChoices });
            out.push({ "kind": "number", "key": "retries", "text": qsTr("Automatic retries on error") + api.tr, "min": 0, "max": 10, "step": 1, "suffix": "" });
            out.push({ "kind": "number", "key": "retryDelay", "text": qsTr("Retry attempt timer") + api.tr, "min": 1, "max": 30, "step": 1, "suffix": "s" });
            out.push({ "kind": "number", "key": "hashMaxMiB", "text": qsTr("Hash searches max file size") + api.tr, "min": 0, "max": 2000, "step": 50, "suffix": " MiB" });
            out.push({ "kind": "toggle", "key": "overwrite", "text": qsTr("Overwrite files and data") + api.tr });
            out.push({ "kind": "toggle", "key": "useHashes", "text": qsTr("Search using file hashes") + api.tr });
            out.push({ "kind": "toggle", "key": "useMetadataNames", "text": qsTr("Search using metadata names") + api.tr });
            out.push({ "kind": "toggle", "key": "interactive", "text": qsTr("Ask when the match isn't sure") + api.tr });
            return out;
        }
        return out;
    }

    function valueText(row) {
        if (row.kind === "choice") return labelOf(row.choices, s[row.key]);
        if (row.kind === "toggle") return s[row.key] ? qsTr("On") + api.tr : qsTr("Off") + api.tr;
        if (row.kind === "number") return String(s[row.key]) + row.suffix;
        if (row.kind === "system") return row.check ? "☑" : "☐";
        return row.value || "";
    }
    readonly property var pageTitles: ({
        "main": qsTr("Library > Scraper"), "systems": qsTr("Scraper > Systems"),
        "accounts": qsTr("Scraper > Accounts"), "content": qsTr("Scraper > Content"),
        "miximage": qsTr("Scraper > Miximages"), "other": qsTr("Scraper > Other settings"),
    })

    // -- Doing things ------------------------------------------------------------

    function openPage(to) {
        page = to;
        message = "";
        list.currentIndex = 0;
        list.positionViewAtBeginning();
        if (!list.selectable(0))
            list.currentIndex = 1;
    }
    function back() {
        if (page === "main") {
            root.close();
            return;
        }
        var from = page;
        page = "main";
        message = "";
        for (var i = 0; i < rows.length; i++) {
            if (rows[i].to === from) {
                list.currentIndex = i;
                return;
            }
        }
        list.currentIndex = 0;
    }
    // left / right on a row: the value before or after
    function step(row, dir) {
        if (row.kind === "choice") {
            var at = 0;
            for (var i = 0; i < row.choices.length; i++) {
                if (row.choices[i].v === s[row.key])
                    at = i;
            }
            at = (at + dir + row.choices.length) % row.choices.length;
            set(row.key, row.choices[at].v);
        }
        else if (row.kind === "toggle") {
            set(row.key, !s[row.key]);
        }
        else if (row.kind === "number") {
            set(row.key, Math.max(row.min, Math.min(row.max, s[row.key] + dir * row.step)));
        }
    }
    function activate(row) {
        if (row.kind === "page") openPage(row.to);
        else if (row.kind === "choice") picker.openFor(row);
        else if (row.kind === "toggle") step(row, 1);
        else if (row.kind === "system") togglePicked(row.text);
        else if (row.kind === "all") set("systems", collectionNames.slice());
        else if (row.kind === "none") set("systems", []);
        else if (row.kind === "text") textDialog.openFor(row);
        else if (row.kind === "start") startScraper();
    }

    Keys.onPressed: {
        if (event.isAutoRepeat)
            return;
        if (api.keys.isCancel(event)) {
            event.accepted = true;
            back();
        }
        else if (event.key === 1048587 || event.key === Qt.Key_F5) {
            // Start: start scraping, from any page
            event.accepted = true;
            startScraper();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#0c0d0e"
        opacity: 0.94
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: root.back()
        }
    }

    ScreenHeader {
        id: header
        text: (root.pageTitles[root.page] || "") + api.tr
        z: 2
    }

    ListView {
        id: list

        anchors.top: header.bottom
        anchors.bottom: messageBar.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.7
        clip: true
        focus: true
        model: root.rows

        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: height * 0.3
        preferredHighlightEnd: height * 0.7
        highlightMoveDuration: 150

        function selectable(i) {
            return i >= 0 && i < count && root.rows[i].kind !== "section" && root.rows[i].kind !== "info";
        }
        Keys.onUpPressed: {
            var i = currentIndex - 1;
            while (i >= 0 && !selectable(i))
                i--;
            if (i >= 0)
                currentIndex = i;
        }
        Keys.onDownPressed: {
            var i = currentIndex + 1;
            while (i < count && !selectable(i))
                i++;
            if (i < count)
                currentIndex = i;
        }
        Keys.onLeftPressed: if (selectable(currentIndex)) root.step(root.rows[currentIndex], -1)
        Keys.onRightPressed: if (selectable(currentIndex)) root.step(root.rows[currentIndex], 1)
        Keys.onPressed: {
            if (api.keys.isAccept(event) && !event.isAutoRepeat && selectable(currentIndex)) {
                event.accepted = true;
                root.activate(root.rows[currentIndex]);
            }
        }

        delegate: Item {
            readonly property bool isSection: modelData.kind === "section"
            readonly property bool isInfo: modelData.kind === "info"
            readonly property bool isStart: modelData.kind === "start"
            readonly property bool steps: modelData.kind === "choice" || modelData.kind === "number"
            readonly property bool highlighted: !isSection && !isInfo
                && ((ListView.isCurrentItem && ListView.view.activeFocus) || rowMouse.containsMouse)

            width: ListView.view.width
            height: isSection ? sectionText.height : isStart ? vpx(76) : isInfo ? vpx(42) : vpx(22) * 2.4

            Text {
                id: sectionText
                visible: parent.isSection
                text: modelData.text
                color: "#bbb"
                font.family: globalFonts.sans
                font.pixelSize: vpx(22)
                topPadding: font.pixelSize * (index === 0 ? 0.75 : 1.5)
                bottomPadding: font.pixelSize * 0.25
            }
            // Start
            Rectangle {
                visible: parent.isStart
                anchors.centerIn: parent
                width: Math.min(parent.width * 0.5, vpx(380))
                height: vpx(50)
                radius: vpx(6)
                color: parent.highlighted ? "#3aa" : "#2a5f5f"
                Text {
                    anchors.centerIn: parent
                    text: modelData.text
                    color: parent.parent.highlighted ? "#111" : "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(21)
                    font.bold: true
                }
            }
            Rectangle {
                width: parent.width
                height: vpx(3)
                anchors.bottom: parent.bottom
                color: "#3aa"
                visible: parent.highlighted && !parent.isStart
            }
            Text {
                id: rowLabel
                visible: !parent.isSection && !parent.isStart
                text: modelData.text
                color: parent.isInfo ? "#999" : "#eee"
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(21)
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                width: parent.isInfo && (modelData.value || "") === "" ? parent.width - vpx(60) : parent.width * 0.5
                elide: Text.ElideRight
            }
            Text {
                visible: !parent.isSection && !parent.isStart
                text: (parent.steps && parent.highlighted ? "◂  " : "") + root.valueText(modelData)
                      + (parent.steps && parent.highlighted ? "  ▸" : "")
                      + (modelData.kind === "page" ? "   ›" : "")
                color: modelData.kind === "info" && modelData.ok !== undefined ? (modelData.ok ? "#9d9" : "#e8b06a")
                     : modelData.muted ? "#888"
                     : modelData.kind === "system" && modelData.check ? "#5cc" : "#c0c0c0"
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(21)
                anchors.left: rowLabel.right
                anchors.leftMargin: vpx(16)
                anchors.right: parent.right
                anchors.rightMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
            }
            MouseArea {
                id: rowMouse
                anchors.fill: parent
                enabled: !parent.isSection && !parent.isInfo
                hoverEnabled: true
                onClicked: {
                    list.currentIndex = index;
                    list.focus = true;
                    root.activate(modelData);
                }
            }
        }
    }

    Text {
        id: messageBar
        text: root.message !== "" ? root.message
            : root.page === "main" ? qsTr("A select · ◂ ▸ change value · Start start scraping · B back") + api.tr
            : root.page === "systems" ? qsTr("A pick a system · B back") + api.tr
            : qsTr("A select · ◂ ▸ change value · B back") + api.tr
        color: "#ddd"
        font.family: globalFonts.sans
        font.pixelSize: vpx(17)
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.7
        topPadding: vpx(10)
        bottomPadding: vpx(16)
    }


    // -- A choice from a list (like ES-DE's popups) -----------------------------

    FocusScope {
        id: picker

        property var row: null
        anchors.fill: parent
        enabled: focus
        visible: focus
        z: 5

        function openFor(r) {
            row = r;
            var at = 0;
            for (var i = 0; i < r.choices.length; i++) {
                if (r.choices[i].v === root.s[r.key])
                    at = i;
            }
            pickerList.currentIndex = at;
            focus = true;
        }
        function done(index) {
            var r = row;
            list.focus = true;
            if (r)
                root.set(r.key, r.choices[index].v);
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
                done(pickerList.currentIndex);
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.5
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: list.focus = true
            }
        }
        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.5, vpx(560))
            Rectangle {
                width: parent.width
                height: pickerTitle.height
                color: "#444"
                Text {
                    id: pickerTitle
                    width: parent.width
                    text: picker.row ? picker.row.text : ""
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(19)
                    horizontalAlignment: Text.AlignHCenter
                    padding: font.pixelSize
                }
            }
            Rectangle {
                width: parent.width
                height: Math.min(pickerList.count * vpx(48), root.height * 0.6)
                color: "#333"
                ListView {
                    id: pickerList
                    anchors.fill: parent
                    clip: true
                    focus: true
                    keyNavigationWraps: true
                    model: picker.row ? picker.row.choices : []
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: vpx(48)
                        color: ListView.isCurrentItem ? "#585858" : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: modelData.t + (picker.row && root.s[picker.row.key] === modelData.v ? "   ✓" : "")
                            color: "#eee"
                            font.family: globalFonts.sans
                            font.pixelSize: vpx(18)
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: picker.done(index)
                        }
                    }
                }
            }
        }
    }


    // -- Typing a username, password or key ----------------------------------------

    FocusScope {
        id: textDialog

        property var row: null
        anchors.fill: parent
        enabled: focus
        visible: focus
        z: 6

        function openFor(r) {
            row = r;
            textInput.text = String(root.s[r.key] || "");
            textInput.cursorPosition = textInput.text.length;
            textKeyboard.row = 1;
            textKeyboard.col = 0;
            focus = true;
            textKeyboard.focus = true;
        }
        function finish() {
            var r = row;
            list.focus = true;
            if (r)
                root.set(r.key, textInput.text.trim());
        }

        Keys.onPressed: {
            if (api.keys.isCancel(event) && !event.isAutoRepeat) {
                event.accepted = true;
                list.focus = true;
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.5
            MouseArea { anchors.fill: parent }
        }
        Rectangle {
            width: Math.min(parent.width * 0.7, vpx(800))
            height: textColumn.height + vpx(40)
            anchors.horizontalCenter: parent.horizontalCenter
            y: vpx(30)
            color: "#444"
            radius: vpx(8)
            Column {
                id: textColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: vpx(20)
                spacing: vpx(12)
                Text {
                    width: parent.width
                    text: textDialog.row ? textDialog.row.text : ""
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
                        id: textInput
                        anchors.fill: parent
                        anchors.leftMargin: vpx(14)
                        anchors.rightMargin: vpx(14)
                        verticalAlignment: TextInput.AlignVCenter
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(20)
                        clip: true
                        cursorVisible: textDialog.visible
                        activeFocusOnPress: false  // Pegasus' own keyboard, not Android's
                        echoMode: textDialog.row && textDialog.row.secret ? TextInput.PasswordEchoOnEdit : TextInput.Normal
                    }
                }
            }
        }
        OnScreenKeyboard {
            id: textKeyboard
            anchors.bottom: parent.bottom
            anchors.bottomMargin: vpx(12)
            anchors.horizontalCenter: parent.horizontalCenter
            keyH: vpx(40)
            target: textInput
            onDone: textDialog.finish()
        }
    }
}
