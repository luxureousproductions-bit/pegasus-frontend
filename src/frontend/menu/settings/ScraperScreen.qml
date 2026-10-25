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


// Pegasus X: Settings > Library > Scraper. Laid out like a classic scraper
// menu (where from, which games, which systems, accounts, content...), in
// Pegasus' style. Every change is saved right away.
FocusScope {
    id: root

    signal close

    anchors.fill: parent

    property string page: "main"   // main, systems, accounts, content, miximage, other, running
    property var s: Internal.settings.scraperSettings()
    property string message: ""
    property var accounts: ({})    // what the last check said
    property bool checking: false

    function set(key, value) {
        Internal.settings.setScraperSetting(key, value);
        var next = Object.assign({}, s);
        next[key] = value;
        keepPosition(function() { s = next; });
        if (key === "ssUser" || key === "ssPassword" || key === "sgdbKey" || key === "ssUse"
                || key === "igdbClientId" || key === "igdbClientSecret")
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
    Component.onCompleted: {
        checkAccounts();
        // still scraping from before: its progress
        var st = Internal.settings.scraperState();
        if (st.running) {
            run = st;
            page = "running";
        }
    }

    // -- Scraping ------------------------------------------------------------------

    property var run: ({})         // the scraper's last state
    property var results: []       // the last games: { title, result, ok }
    Connections {
        target: Internal.settings
        function onScraperProgress(state) {
            // a game finished: on top of the list
            if (state.title && (state.done || 0) > (root.run.done || 0)) {
                var next = [ { "title": state.title, "result": state.result, "ok": state.ok } ].concat(root.results);
                root.results = next.slice(0, 8);
            }
            root.run = state;
        }
        function onScraperFinished(state) {
            root.run = state;
            root.message = state.stopReason
                ? state.stopReason + " · " + qsTr("%1 saved").arg(state.saved) + api.tr
                : qsTr("Done: %1 saved, %2 not found, %3 failed.").arg(state.saved).arg(state.notFound).arg(state.failed) + api.tr;
        }
    }

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
        { "v": "igdb", "t": qsTr("IGDB (best for modern, PC and Android games)") + api.tr },
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
    readonly property var sizeChoices: [
        { "v": "small", "t": qsTr("Small") + api.tr },
        { "v": "medium", "t": qsTr("Medium") + api.tr },
        { "v": "large", "t": qsTr("Large") + api.tr },
    ]
    readonly property var fitChoices: [
        { "v": "contain", "t": qsTr("Contain") + api.tr },
        { "v": "crop", "t": qsTr("Crop") + api.tr },
        { "v": "stretch", "t": qsTr("Stretch") + api.tr },
    ]

    // -- How to get each login (a popup from Account settings) -------------------
    readonly property var helpTexts: ({
        "ss": {
            "title": qsTr("ScreenScraper account") + api.tr,
            "steps": [
                qsTr("On screenscraper.fr, tap \"Inscription\" (sign up) at the top. Pick a username and password and enter your email.") + api.tr,
                qsTr("Open the email ScreenScraper sends and confirm your account.") + api.tr,
                qsTr("Enter the same username and password here, and leave \"Use this account\" on.") + api.tr,
                qsTr("Free accounts get a set number of games a day; supporting ScreenScraper raises it. The Status row shows how many are left today.") + api.tr,
            ],
            "site": "screenscraper.fr"
        },
        "igdb": {
            "title": qsTr("IGDB login (a Twitch app)") + api.tr,
            "steps": [
                qsTr("Log in to twitch.tv and turn on two-factor authentication (Settings › Security). Twitch asks for it before you can make an app.") + api.tr,
                qsTr("On dev.twitch.tv, open Console › Applications › Register Your Application.") + api.tr,
                qsTr("Name: anything no one else has used, eg. \"pegasusx-\" and your name. OAuth Redirect URL: https://localhost (tap Add). Category: Application Integration. Client type: Confidential.") + api.tr,
                qsTr("Open the app you made: copy the Client ID, then tap New Secret and copy the secret (it's shown only once).") + api.tr,
                qsTr("Enter both here. The Status row says \"Login works\" when Twitch accepts them.") + api.tr,
            ],
            "site": "dev.twitch.tv/console"
        },
        "sgdb": {
            "title": qsTr("SteamGridDB API key") + api.tr,
            "steps": [
                qsTr("On steamgriddb.com, log in with your Steam account (top right).") + api.tr,
                qsTr("Open your profile menu › Preferences › API.") + api.tr,
                qsTr("Tap Generate API Key and copy the key.") + api.tr,
                qsTr("Enter it here. The Status row says \"Key works\" when SteamGridDB accepts it.") + api.tr,
            ],
            "site": "steamgriddb.com/profile/preferences/api"
        },
    })
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
        var error = Internal.settings.startScraper(api.collections);
        if (error !== "") {
            message = error;
            return;
        }
        results = [];
        run = Internal.settings.scraperState();
        message = "";
        page = "running";
        list.currentIndex = 0;
    }

    function startGenerator() {
        if ((s.systems || []).length === 0) {
            message = qsTr("Pick the systems on the Scraper page first.") + api.tr;
            return;
        }
        var error = Internal.settings.startMiximageGenerator(api.collections);
        if (error !== "") {
            message = error;
            return;
        }
        results = [];
        run = Internal.settings.scraperState();
        message = "";
        page = "running";
        list.currentIndex = 0;
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
            out.push({ "kind": "help", "key": "ss", "text": qsTr("How to get an account") + api.tr, "value": "" });
            out.push({ "kind": "info", "text": qsTr("Status") + api.tr,
                       "value": checking ? qsTr("checking...") + api.tr : (accounts.ss || ""), "ok": !!accounts.ssOk });
            out.push({ "kind": "section", "text": "IGDB" });
            out.push({ "kind": "text", "key": "igdbClientId", "secret": true, "text": qsTr("Client ID") + api.tr,
                       "value": s.igdbClientId !== "" ? "••••" + String(s.igdbClientId).slice(-4) : qsTr("not set") + api.tr, "muted": s.igdbClientId === "" });
            out.push({ "kind": "text", "key": "igdbClientSecret", "secret": true, "text": qsTr("Client secret") + api.tr,
                       "value": s.igdbClientSecret !== "" ? "••••••••" : qsTr("not set") + api.tr, "muted": s.igdbClientSecret === "" });
            out.push({ "kind": "toggle", "key": "igdbFill", "text": qsTr("Fill in what ScreenScraper doesn't have") + api.tr });
            out.push({ "kind": "info", "text": qsTr("Status") + api.tr,
                       "value": checking ? qsTr("checking...") + api.tr : (accounts.igdb || ""), "ok": !!accounts.igdbOk });
            out.push({ "kind": "help", "key": "igdb", "text": qsTr("How to get a Client ID and secret") + api.tr, "value": "" });
            out.push({ "kind": "section", "text": "SteamGridDB" });
            out.push({ "kind": "text", "key": "sgdbKey", "secret": true, "text": qsTr("API key") + api.tr,
                       "value": s.sgdbKey !== "" ? "••••" + String(s.sgdbKey).slice(-4) : qsTr("not set") + api.tr, "muted": s.sgdbKey === "" });
            out.push({ "kind": "info", "text": qsTr("Status") + api.tr,
                       "value": checking ? qsTr("checking...") + api.tr : (accounts.sgdb || ""), "ok": !!accounts.sgdbOk });
            out.push({ "kind": "help", "key": "sgdb", "text": qsTr("How to get an API key") + api.tr, "value": "" });
            return out;
        }
        if (page === "content") {
            for (i = 0; i < contentToggles.length; i++)
                out.push({ "kind": "toggle", "key": contentToggles[i][0], "text": contentToggles[i][1] });
            return out;
        }
        if (page === "miximage") {
            out.push({ "kind": "toggle", "key": "getMiximages", "text": qsTr("Generate miximages when scraping") + api.tr });
            out.push({ "kind": "toggle", "key": "mixOverwrite", "text": qsTr("Overwrite miximages (scraper/offline generator)") + api.tr });
            out.push({ "kind": "offline", "text": qsTr("Offline generator") + api.tr,
                       "value": qsTr("%1 systems").arg((s.systems || []).length) + api.tr });
            out.push({ "kind": "section", "text": qsTr("Layout") + api.tr });
            out.push({ "kind": "choice", "key": "mixResolution", "text": qsTr("Miximage resolution") + api.tr, "choices": [
                { "v": "1280x960", "t": "1280×960" }, { "v": "1920x1440", "t": "1920×1440" }, { "v": "640x480", "t": "640×480" } ] });
            out.push({ "kind": "choice", "key": "mixFormat", "text": qsTr("Miximage file format") + api.tr, "choices": [
                { "v": "png", "t": "PNG" }, { "v": "webp", "t": qsTr("WEBP (PNG where not supported)") + api.tr } ] });
            out.push({ "kind": "choice", "key": "mixHorizontalFit", "text": qsTr("Horizontal screenshot fit") + api.tr, "choices": fitChoices });
            out.push({ "kind": "choice", "key": "mixVerticalFit", "text": qsTr("Vertical screenshot fit") + api.tr, "choices": fitChoices });
            out.push({ "kind": "choice", "key": "mixThreshold", "text": qsTr("Screenshot aspect ratio threshold") + api.tr, "choices": [
                { "v": "high", "t": qsTr("High") + api.tr }, { "v": "low", "t": qsTr("Low") + api.tr } ] });
            out.push({ "kind": "choice", "key": "mixBlankColor", "text": qsTr("Blank areas fill color") + api.tr, "choices": [
                { "v": "black", "t": qsTr("Black") + api.tr }, { "v": "frame", "t": qsTr("Frame") + api.tr } ] });
            out.push({ "kind": "choice", "key": "mixScaling", "text": qsTr("Screenshot scaling method") + api.tr, "choices": [
                { "v": "sharp", "t": qsTr("Sharp") + api.tr }, { "v": "smooth", "t": qsTr("Smooth") + api.tr } ] });
            out.push({ "kind": "choice", "key": "mixBoxSize", "text": qsTr("Box size") + api.tr, "choices": sizeChoices });
            out.push({ "kind": "choice", "key": "mixPhysicalSize", "text": qsTr("Physical media size") + api.tr, "choices": sizeChoices });
            out.push({ "kind": "toggle", "key": "mixRemoveLetterboxes", "text": qsTr("Remove letterboxes from screenshots") + api.tr });
            out.push({ "kind": "toggle", "key": "mixRemovePillarboxes", "text": qsTr("Remove pillarboxes from screenshots") + api.tr });
            out.push({ "kind": "toggle", "key": "mixRotateBoxes", "text": qsTr("Rotate horizontally oriented boxes") + api.tr });
            out.push({ "kind": "toggle", "key": "mixIncludeMarquee", "text": qsTr("Include marquee image") + api.tr });
            out.push({ "kind": "toggle", "key": "mixIncludeBox", "text": qsTr("Include box image") + api.tr });
            out.push({ "kind": "toggle", "key": "mixCoverFallback", "text": qsTr("Use cover image if 3D box is missing") + api.tr });
            out.push({ "kind": "toggle", "key": "mixIncludePhysical", "text": qsTr("Include physical media image") + api.tr });
            out.push({ "kind": "info", "text": qsTr("The offline generator makes miximages for the systems picked on the Scraper page from the art the games already have, without going online.") + api.tr, "value": "" });
            return out;
        }
        if (page === "running") {
            var total = run.total || 0;
            out.push({ "kind": "progress", "text": run.running
                ? qsTr("%1 of %2").arg(run.done || 0).arg(total) + api.tr
                : qsTr("Finished · %1 of %2").arg(run.done || 0).arg(total) + api.tr,
                "value": run.running && run.current ? run.current : "" });
            out.push({ "kind": "info", "text": qsTr("Saved · not found · failed") + api.tr,
                       "value": (run.saved || 0) + " · " + (run.notFound || 0) + " · " + (run.failed || 0) });
            // the last game: its picture and what was found
            if (run.preview && run.preview.title)
                out.push({ "kind": "preview", "text": "", "preview": run.preview, "result": run.result || "", "ok": run.ok });
            out.push({ "kind": "section", "text": qsTr("Last games") + api.tr });
            for (i = 0; i < results.length; i++)
                out.push({ "kind": "info", "text": results[i].title, "value": results[i].result, "ok": results[i].ok });
            if (results.length === 0)
                out.push({ "kind": "info", "text": qsTr("Starting...") + api.tr, "value": "" });
            out.push({ "kind": run.running ? "stop" : "done", "text": run.running ? qsTr("Stop") + api.tr : qsTr("Back to the scraper") + api.tr });
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
            out.push({ "kind": "toggle", "key": "interactive", "text": qsTr("Interactive mode (pick each match)") + api.tr });
            out.push({ "kind": "toggle", "key": "autoAcceptSingle", "text": qsTr("Auto-accept a single match") + api.tr });
            out.push({ "kind": "section", "text": qsTr("Pictures") + api.tr });
            out.push({ "kind": "choice", "key": "imageMaxSize", "text": qsTr("Biggest picture size") + api.tr, "choices": [
                { "v": "2560", "t": qsTr("2560×1440") + api.tr },
                { "v": "1920", "t": "1920×1080" },
                { "v": "1280", "t": "1280×720" },
                { "v": "640", "t": qsTr("640×480 (smallest files)") + api.tr },
                { "v": "original", "t": qsTr("Original (as downloaded)") + api.tr },
            ] });
            out.push({ "kind": "choice", "key": "trimBorders", "text": qsTr("Trim empty borders") + api.tr, "choices": [
                { "v": "off", "t": qsTr("Off") + api.tr },
                { "v": "logos", "t": qsTr("Logos only") + api.tr },
                { "v": "all", "t": qsTr("All pictures") + api.tr },
            ] });
            out.push({ "kind": "info", "text": qsTr("Logos are kept at 1000×600 at most. Smaller pictures load faster in themes.") + api.tr, "value": "" });
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
        "running": qsTr("Scraper > Scraping"),
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
        // scraping goes on in the background: back from its page leaves it running
        if (page === "main" || page === "running") {
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
        else if (row.kind === "help") helpDialog.openFor(row.key);
        else if (row.kind === "offline") startGenerator();
        else if (row.kind === "stop") Internal.settings.stopScraper();
        else if (row.kind === "done") { page = "main"; message = ""; list.currentIndex = 0; }
    }

    Keys.onPressed: {
        if (event.isAutoRepeat)
            return;
        if (api.keys.isCancel(event)) {
            event.accepted = true;
            back();
        }
        else if ((event.key === 1048587 || event.key === Qt.Key_F5) && page !== "running") {
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
            return i >= 0 && i < count && root.rows[i].kind !== "section" && root.rows[i].kind !== "info"
                && root.rows[i].kind !== "preview";
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
            readonly property bool isStart: modelData.kind === "start" || modelData.kind === "stop" || modelData.kind === "done"
            readonly property bool isProgress: modelData.kind === "progress"
            readonly property bool isMix: modelData.kind === "preview"
            readonly property bool steps: modelData.kind === "choice" || modelData.kind === "number"
            readonly property bool highlighted: !isSection && !isInfo && !isMix
                && ((ListView.isCurrentItem && ListView.view.activeFocus) || rowMouse.containsMouse)

            width: ListView.view.width
            height: isSection ? sectionText.height : isStart ? vpx(76) : isProgress ? vpx(84) : isMix ? vpx(300)
                  : isInfo ? (modelData.value ? vpx(42) : rowLabel.implicitHeight + vpx(16)) : vpx(22) * 2.4

            // the game just scraped: its picture and what was found
            Item {
                visible: parent.isMix
                anchors.fill: parent
                anchors.leftMargin: vpx(30)
                anchors.rightMargin: vpx(30)
                anchors.topMargin: vpx(10)
                anchors.bottomMargin: vpx(10)
                readonly property var p: modelData.preview || ({})

                Rectangle {
                    id: previewBox
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    width: height * 0.8
                    color: "#333"
                    Image {
                        id: previewImage
                        anchors.fill: parent
                        anchors.margins: vpx(6)
                        source: parent.parent.p.picture || ""
                        sourceSize.width: vpx(320)
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: false
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: previewImage.status !== Image.Ready
                        text: qsTr("No picture") + api.tr
                        color: "#777"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(16)
                    }
                }
                Column {
                    anchors.left: previewBox.right
                    anchors.leftMargin: vpx(20)
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    spacing: vpx(4)
                    clip: true
                    Text {
                        width: parent.width
                        text: parent.parent.p.title || ""
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(22)
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: modelData.result
                        color: modelData.ok ? "#9d9" : "#e8b06a"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(15)
                        elide: Text.ElideRight
                    }
                    Repeater {
                        model: [
                            [qsTr("Released") + api.tr, parent.parent.p.release],
                            [qsTr("Developer") + api.tr, parent.parent.p.developer],
                            [qsTr("Publisher") + api.tr, parent.parent.p.publisher],
                            [qsTr("Genre") + api.tr, parent.parent.p.genre],
                            [qsTr("Players") + api.tr, parent.parent.p.players],
                            [qsTr("Rating") + api.tr, parent.parent.p.rating],
                        ]
                        delegate: Row {
                            visible: !!modelData[1]
                            spacing: vpx(10)
                            Text {
                                width: vpx(110)
                                text: modelData[0]
                                color: "#999"
                                font.family: globalFonts.sans
                                font.pixelSize: vpx(16)
                            }
                            Text {
                                text: modelData[1] || ""
                                color: "#c0c0c0"
                                font.family: globalFonts.sans
                                font.pixelSize: vpx(16)
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        text: parent.parent.p.description || ""
                        color: "#aaa"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(15)
                        wrapMode: Text.WordWrap
                        maximumLineCount: 4
                        elide: Text.ElideRight
                    }
                }
            }

            // how far it is
            Rectangle {
                visible: parent.isProgress
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: vpx(30)
                anchors.rightMargin: vpx(30)
                anchors.bottom: parent.bottom
                anchors.bottomMargin: vpx(12)
                height: vpx(8)
                color: "#333"
                Rectangle {
                    height: parent.height
                    color: "#3aa"
                    width: parent.width * ((root.run.total || 0) > 0 ? (root.run.done || 0) / root.run.total : 0)
                    Behavior on width { NumberAnimation { duration: 250 } }
                }
            }

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
                color: parent.highlighted ? (modelData.kind === "stop" ? "#f43" : "#3c4") : "#666"
                Text {
                    anchors.centerIn: parent
                    text: modelData.text
                    color: "#eee"
                    font.family: globalFonts.sans
                    font.pixelSize: vpx(21)
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
                visible: !parent.isSection && !parent.isStart && !parent.isMix
                text: modelData.text
                color: parent.isInfo ? "#999" : "#eee"
                font.family: globalFonts.sans
                font.pixelSize: parent.isInfo ? vpx(17) : vpx(21)
                anchors.left: parent.left
                anchors.leftMargin: vpx(30)
                anchors.verticalCenter: parent.verticalCenter
                width: parent.isInfo && (modelData.value || "") === "" ? parent.width - vpx(60) : parent.width * 0.5
                // a note under the options: all of it, on several lines
                wrapMode: parent.isInfo && !modelData.value ? Text.WordWrap : Text.NoWrap
                elide: parent.isInfo && !modelData.value ? Text.ElideNone : Text.ElideRight
            }
            Text {
                visible: !parent.isSection && !parent.isStart && !parent.isMix
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
                enabled: !parent.isSection && !parent.isInfo && !parent.isMix
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


    // -- A choice from a list --------------------------------------------------

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


    // -- How to get a login --------------------------------------------------------

    FocusScope {
        id: helpDialog

        property var help: ({})
        anchors.fill: parent
        enabled: focus
        visible: focus
        z: 5

        function openFor(key) {
            help = root.helpTexts[key] || ({});
            helpFlick.contentY = 0;
            focus = true;
        }

        Keys.onPressed: {
            if (event.isAutoRepeat)
                return;
            if (api.keys.isCancel(event) || api.keys.isAccept(event)) {
                event.accepted = true;
                list.focus = true;
            }
        }
        Keys.onUpPressed: helpFlick.contentY = Math.max(0, helpFlick.contentY - vpx(60))
        Keys.onDownPressed: helpFlick.contentY = Math.max(0, Math.min(helpFlick.contentHeight - helpFlick.height, helpFlick.contentY + vpx(60)))

        Rectangle {
            anchors.fill: parent
            color: "#000"
            opacity: 0.6
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: list.focus = true
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.7, vpx(820))
            height: Math.min(parent.height * 0.85, helpColumn.height + vpx(40))
            color: "#2b2e30"
            radius: vpx(8)
            border.color: "#3aa"
            border.width: vpx(2)
            Flickable {
                id: helpFlick
                anchors.fill: parent
                anchors.margins: vpx(20)
                contentHeight: helpColumn.height
                clip: true
                Column {
                    id: helpColumn
                    width: parent.width
                    spacing: vpx(14)
                    Text {
                        width: parent.width
                        text: helpDialog.help.title || ""
                        color: "#eee"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(24)
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }
                    Repeater {
                        model: helpDialog.help.steps || []
                        delegate: Row {
                            spacing: vpx(12)
                            width: helpColumn.width
                            Rectangle {
                                width: vpx(30)
                                height: width
                                radius: width / 2
                                color: "#2a5f5f"
                                Text {
                                    anchors.centerIn: parent
                                    text: index + 1
                                    color: "#eee"
                                    font.family: globalFonts.sans
                                    font.pixelSize: vpx(16)
                                    font.bold: true
                                }
                            }
                            Text {
                                width: parent.width - vpx(42)
                                text: modelData
                                color: "#ddd"
                                font.family: globalFonts.sans
                                font.pixelSize: vpx(18)
                                wrapMode: Text.WordWrap
                                lineHeight: 1.15
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        text: qsTr("Website: %1").arg(helpDialog.help.site || "") + api.tr
                        color: "#5cc"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(17)
                        wrapMode: Text.WrapAnywhere
                    }
                    Text {
                        width: parent.width
                        text: qsTr("A / B close · ▴ ▾ scroll") + api.tr
                        color: "#999"
                        font.family: globalFonts.sans
                        font.pixelSize: vpx(15)
                        horizontalAlignment: Text.AlignHCenter
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
