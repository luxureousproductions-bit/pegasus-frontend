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
FocusScope {
    id: root

    signal close

    anchors.fill: parent

    ScraperScreen {
        id: scraper
        focus: true
        onClose: root.close()
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
        enabled: scraper.activeFocus || scraperChoice.activeFocus
        function onScraperNeedsChoice(choice) { scraperChoice.openFor(choice); }
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
