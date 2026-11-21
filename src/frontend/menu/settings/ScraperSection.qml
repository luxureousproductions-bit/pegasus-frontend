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


// Pegasus X: Settings > Gaming > Scraper. The scraper's own pages (sources,
// content, pictures, accounts, the running scrape) as they were inside
// Library, and, during an interactive scrape, the window to pick each
// game's match. Library's "Scraper options..." opens this too.
// Genres: every genre and its games; a game opens in Library, on that game.
FocusScope {
    id: root

    signal close

    anchors.fill: parent

    ScraperScreen {
        id: scraper
        focus: true
        visible: focus || !(genres.visible || library.visible)
        // back to Library when it was opened from there (a game's "Scraper options...")
        onClose: {
            if (library.active)
                library.focus = true;
            else
                root.close();
        }
        onOpenGenres: {
            genres.start();
            genres.focus = true;
        }
    }

    // -- Genres, and a game of one in Library ----------------------------------------

    GenresScreen {
        id: genres
        visible: focus
        z: 1
        onClose: scraper.focus = true
        onOpenGame: function(game) {
            library.wanted = game;
            if (library.active && library.item)
                library.item.openGameDirect(game);
            library.active = true;
            library.focus = true;
        }
    }
    Loader {
        id: library
        property var wanted: null
        anchors.fill: parent
        z: 2
        active: false
        enabled: focus
        visible: active && focus
        source: "MetadataEditor.qml"
        onLoaded: {
            item.openGameDirect(wanted);
            item.focus = true;
        }
        onFocusChanged: if (item && focus) item.focus = true
    }
    Connections {
        target: library.item
        ignoreUnknownSignals: true
        // Back from the game: the genre's games again, counted anew
        function onClose() {
            genres.focus = true;
            genres.refresh();
            // after the signal is done: the screen that sent it goes
            Qt.callLater(function() { library.active = false; });
        }
        // its "Scraper options...": the Scraper here (Back returns to the game)
        function onOpenScraper() { scraper.focus = true; }
    }

    // -- The matches to pick from (interactive mode) --------------------------------

    ScraperChoice {
        id: scraperChoice
        z: 9
        onFinished: scraper.focus = true
    }
    // one pick window at a time: this one while the Scraper is in front
    // (Library has its own for the games scraped from there)
    Connections {
        target: Internal.settings
        // (checked here: Connections.enabled needs QtQuick 2.7, this file is 2.6)
        function onScraperNeedsChoice(choice) {
            if (scraper.activeFocus || scraperChoice.activeFocus || genres.activeFocus)
                scraperChoice.openFor(choice);
        }
    }
    // waiting since before the Scraper opened (scraping went on after you left)
    Timer {
        running: true
        interval: 300
        onTriggered: {
            var pending = Internal.settings.scraperPendingChoice();
            if (pending.candidates !== undefined)
                scraperChoice.openFor(pending);
        }
    }
}
