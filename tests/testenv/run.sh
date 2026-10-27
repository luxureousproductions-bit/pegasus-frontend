#!/bin/bash
# Pegasus X test environment: runs Pegasus X on the fixture library (see
# fixture.sh) under a virtual screen, opens its screens, presses keys and
# takes screenshots; the logs and the files it wrote are kept too.
#   $1: pegasus-fe   $2: the fixture folder   $3: the results folder
set -u
BIN="$(readlink -f "$1")"
T="$(readlink -f "$2")"
R="$(mkdir -p "$3" && readlink -f "$3")"
export HOME="$T/home"
# GitHub's machines set their own XDG folders: the fixture's are used
export XDG_CONFIG_HOME="$HOME/.config" XDG_CACHE_HOME="$HOME/.cache" XDG_DATA_HOME="$HOME/.local/share"
export PEGASUSX_TEST_STORAGE="$T/storage"
export QT_QPA_PLATFORM=xcb
export QT_QUICK_BACKEND=software
CFG="$HOME/.config/pegasus-frontend"
mkdir -p "$R/logs" "$R/files"
PID=""

shot() { import -window root "$R/$1.png" 2>/dev/null; echo "  shot $1"; }
keys() { for k in "$@"; do xdotool key --clearmodifiers "$k"; sleep 0.7; done; }

start() {  # name, screen ("" for the theme), seconds to wait, extra env...
    local name="$1" screen="$2" wait="$3"; shift 3
    echo "== $name"
    if [ -n "$screen" ]; then
        env "$@" "$BIN" --pegasusx-open "$screen" > "$R/logs/$name.stdout" 2>&1 &
    else
        env "$@" "$BIN" > "$R/logs/$name.stdout" 2>&1 &
    fi
    PID=$!
    local wid=""
    for _ in $(seq 1 40); do
        wid="$(xdotool search --pid "$PID" 2>/dev/null | tail -n 1)"
        [ -n "$wid" ] && break
        sleep 0.5
    done
    if [ -n "$wid" ]; then
        xdotool windowsize "$wid" 1280 720 windowmove "$wid" 0 0 2>/dev/null
        xdotool windowfocus --sync "$wid" 2>/dev/null
    else
        echo "  no window!"
    fi
    sleep "$wait"
}
stop() {  # name
    kill "$PID" 2>/dev/null; sleep 1; kill -9 "$PID" 2>/dev/null; wait "$PID" 2>/dev/null
    cp "$CFG/lastrun.log" "$R/logs/$1.log" 2>/dev/null
}

# -- 1. first start: the ES-DE import and the ROM folder import, then the theme --
start 01-first-start "" 15 PEGASUSX_TEST_ESDE_IMPORT=1 "PEGASUSX_TEST_ROM_FOLDERS=$T/MyGames"
keys Right Return
sleep 2
shot 01-theme
stop 01-first-start
cp "$CFG/game_dirs.txt" "$R/files/game_dirs.txt" 2>/dev/null
find "$T" -name "esde-import.metadata.pegasus.txt" | while read -r f; do
    cp "$f" "$R/files/OLD-PLACE_$(echo "${f#$T/}" | tr '/ ' '__')"
done
find "$CFG/metadata" -type f 2>/dev/null | while read -r f; do
    cp "$f" "$R/files/$(echo "${f#$CFG/}" | tr '/ ' '__')"
done
cp "$CFG/pegasusx-rom-folders.txt" "$R/files/" 2>/dev/null

# a game scraped before, then its scraped files deleted: the override still
# names a fan art that isn't there, and hides a scraped cover with the same
# name as the one in the ROM folder's media: the folder's art must show
mkdir -p "$CFG/metadata/overrides"
cat >> "$CFG/metadata/overrides/Nintendo_64.metadata.pegasus.txt" <<TXT

collection: Nintendo 64

game: Mario Kart 64 (USA)
file: $T/MyGames/Nintendo 64/Mario Kart 64 (USA).z64
assets.background: $CFG/downloaded_media/n64/fanart/Mario Kart 64 (USA).png
x-pegasusx-hidden-art: box_front|$CFG/downloaded_media/n64/covers/Mario Kart 64 (USA).png
x-pegasusx-edited: assets.background
TXT

# a game an earlier run wasn't sure about (Library > Needs your check)
cat > "$CFG/metadata/review.json" <<JSON
{ "version": 1, "games": {
  "$T/MyGames/Nintendo 64/Star Fox 64 (USA).z64": { "title": "Star Fox 64 (USA)", "collection": "Nintendo 64",
    "system": "n64", "closest": "Star Fox 64 3D", "info": "Nintendo 3DS · 2011", "score": 55, "igdb": 2183 } } }
JSON

# -- 2. the settings ---------------------------------------------------------------
start 02-settings "menu/settings/SettingsScreen.qml" 6
shot 02-settings-top
keys Down Down Down Down Down Down Down Down Down Down Down Down Down Down Down Down
shot 02-settings-bottom
stop 02-settings

# -- 3. Library: collections, a collection, a game -------------------------------------
start 03-library "menu/settings/MetadataEditor.qml" 6
shot 03-library
keys Return
sleep 1
shot 03-library-complete
keys Escape
keys Down Return
sleep 1
shot 03-library-review
keys Escape
keys Down Down Down Down Down
shot 03-library-down
keys Return
sleep 2
shot 03-library-collection
keys Down Down Down Down Down Down Down Down Down Down Down Down
shot 03-library-collection-games
keys Return
sleep 2
shot 03-library-game
stop 03-library

# -- 4. Scraper: its pages, then the offline miximage generator on every system --------
start 04-scraper "menu/settings/ScraperScreen.qml" 6
shot 04-scraper
keys Down Down Return
shot 04-scraper-systems
keys Return Escape
keys Down Down Down Return
shot 04-scraper-miximage
keys Down Down Return
sleep 2
shot 04-scraper-offline-running
keys Escape
sleep 1
stop 04-scraper
mkdir -p "$R/miximages"
find "$CFG" "$HOME" -path "*miximages*" -type f | head -n 10 | while read -r f; do cp "$f" "$R/miximages/"; done

start 05-scraper-accounts "menu/settings/ScraperScreen.qml" 6
keys Down Down Down Return
shot 05-accounts
keys Down Down Down Return
sleep 1
shot 05-accounts-help
stop 05-scraper-accounts

# -- 5. Theme downloader ------------------------------------------------------------------
start 06-themes "menu/settings/ThemeDownloader.qml" 30
shot 06-themes
keys Down Down Down
sleep 4
shot 06-themes-down
keys Right
sleep 3
shot 06-themes-picture2
stop 06-themes

# -- 6. Check for updates, game directories, ES-DE import ---------------------------------------
start 07-updater "menu/settings/UpdateDialog.qml" 8
shot 07-updater
stop 07-updater
start 08-gamedirs "menu/settings/GameDirEditor.qml" 5
shot 08-gamedirs
stop 08-gamedirs
start 09-esde-import "menu/settings/EsdeImport.qml" 6
shot 09-esde-import
keys Return; sleep 1; shot 09-esde-import-page
keys Return; shot 09-esde-import-options
keys Return; sleep 1; shot 09-folder-picker
keys Escape Escape Escape
keys Down Down Return; shot 09-esde-delete
keys Return; shot 09-esde-delete-ticked
stop 09-esde-import

# -- 7. the theme again (the library as read after the imports) --------------------------------
start 10-theme "" 12 "PEGASUSX_TEST_DUMP=$R/art-per-game.txt"
shot 10-theme
keys Right
sleep 1
shot 10-theme-next
stop 10-theme

# -- 8. the scraper, for real, when test logins are set (IGDB; SteamGridDB) ---------------------------
if [ -n "${IGDB_TEST_CLIENT_ID:-}" ] && [ -n "${IGDB_TEST_CLIENT_SECRET:-}" ]; then
    cat >> "$CFG/pegasusx.ini" <<INI

[scraper]
source=igdb
igdbClientId=$IGDB_TEST_CLIENT_ID
igdbClientSecret=$IGDB_TEST_CLIENT_SECRET
sgdbKey=${SGDB_TEST_KEY:-}
overwrite=true
INI
    start 11-scrape-igdb "" 60 "PEGASUSX_TEST_SCRAPE=Super Mario World"
    shot 11-scrape-igdb
    stop 11-scrape-igdb
    mkdir -p "$R/scraped"
    find "$CFG/downloaded_media" -type f 2>/dev/null | sed "s|$CFG/||" > "$R/scraped/files.txt"
    find "$CFG/downloaded_media" -type f 2>/dev/null | head -n 12 | while read -r f; do
        cp "$f" "$R/scraped/$(echo "${f#$CFG/downloaded_media/}" | tr '/' '_')"
    done
    cp "$CFG"/metadata/overrides/*.metadata.pegasus.txt "$R/scraped/" 2>/dev/null || true
    # the log, without the login
    grep -h "Scraper\|\[test\]" "$R/logs/11-scrape-igdb.log" | sed "s/$IGDB_TEST_CLIENT_ID/<client id>/g; s/$IGDB_TEST_CLIENT_SECRET/<secret>/g" > "$R/scraped/scraper.log" || true
    sed -i "s/$IGDB_TEST_CLIENT_ID/<client id>/g; s/$IGDB_TEST_CLIENT_SECRET/<secret>/g; s/${SGDB_TEST_KEY:-NOKEY}/<key>/g" "$R/logs/11-scrape-igdb.log" "$R/logs/11-scrape-igdb.stdout" 2>/dev/null || true
fi

# -- a scrape without any login: Hasheous + libretro's thumbnails ----------------------------------
# (the fixture's ROMs are made up, so Hasheous won't know them: libretro finds the art by file name)
mkdir -p "$CFG"
python3 - "$CFG/pegasusx.ini" <<'PY'
import sys, configparser
p = sys.argv[1]
c = configparser.RawConfigParser(); c.optionxform = str
c.read(p)
if not c.has_section("scraper"): c.add_section("scraper")
for k in ("igdbClientId", "igdbClientSecret", "sgdbKey"): c.remove_option("scraper", k)
c.set("scraper", "source", "screenscraper")
c.set("scraper", "overwrite", "true")
with open(p, "w") as f: c.write(f, space_around_delimiters=False)
PY
# (Hasheous is asked with a real game's checksum, Banjo-Kazooie (USA), to see its answer)
start 12-scrape-free "" 60 "PEGASUSX_TEST_SCRAPE=Super Mario World" "PEGASUSX_TEST_MD5=b29599651a13f681c9923d69354bf4a3"
stop 12-scrape-free
mkdir -p "$R/scraped-free"
find "$CFG/downloaded_media/snes" -type f -iname "Super Mario World*" 2>/dev/null | sed "s|$CFG/||" > "$R/scraped-free/files.txt" || true
grep -h "Scraper\|libretro\|Hasheous\|\[test\]" "$R/logs/12-scrape-free.log" > "$R/scraped-free/scraper.log" || true

# -- summary --------------------------------------------------------------------------------------
# the metadata files as they are at the end (overrides are written along the way)
find "$CFG/metadata" -type f 2>/dev/null | while read -r f; do
    cp "$f" "$R/files/$(echo "${f#$CFG/}" | tr '/ ' '__')"
done
{
    echo "# Test environment results"
    echo
    echo "## Imports (first start)"
    grep -h "Test:" "$R/logs/01-first-start.log" | sed 's/^/    /'
    echo
    strip_ansi() { cat "$@" | sed 's/\x1b\[[0-9;]*m//g'; }
    echo "## Your own file wins over an import"
    if grep -q "^Chrono Trigger (my own file)$" "$R/art-per-game.txt" 2>/dev/null; then
        echo "    OK: Chrono Trigger has the title from your own file"
    else
        echo "    FAILED: Chrono Trigger's title isn't the one from your own file:"
        grep "^Chrono" "$R/art-per-game.txt" 2>/dev/null | sed 's/^/        /'
    fi
    # ...and the import's art for it is used on top (the "Use" setting: fill)
    if awk '/^Chrono Trigger \(my own file\)$/{f=1;next} /^[^ ]/{f=0} f' "$R/art-per-game.txt" 2>/dev/null | grep -q "screenshots/Chrono Trigger"; then
        echo "    OK: it has ES-DE's screenshot too (import data on top of your file)"
    else
        echo "    FAILED: ES-DE's screenshot isn't on your own file's Chrono Trigger"
    fi
    strip_ansi "$R/logs/10-theme.stdout" | grep -E "already belongs to a different game|Imports onto your games" | head -n 5 | sed 's/^/    /'
    echo
    echo "## Scrape without a login (Hasheous + libretro)"
    grep -E "libretro|Hasheous|Saving|not found|no source" "$R/scraped-free/scraper.log" 2>/dev/null | head -n 12 | sed 's/^/    /'
    sed 's/^/    file: /' "$R/scraped-free/files.txt" 2>/dev/null | sort -u | head -n 8
    echo
    echo "## Library as read (last start)"
    strip_ansi "$R/logs/10-theme.stdout" | grep -E "Metafiles: Found|games found|Overrides:|Imports \(|Library:|Wrote|Kept|Set aside|Made the paths" | sed 's/^/    /' | head -n 40
    echo
    echo "## Warnings and errors"
    strip_ansi "$R"/logs/*.stdout | grep -iE "\[w\]|\[e\]|warning|error|qrc:|\.qml:[0-9]+" | grep -v "Test:" | sort | uniq -c | sort -rn | head -n 60 | sed 's/^/    /'
    echo
    echo "## Art each game has (last start)"
    sed 's/^/    /' "$R/art-per-game.txt" 2>/dev/null | sed "s|$T/||" | head -n 120
    echo
    echo "## Game directories"
    sed 's/^/    /' "$R/files/game_dirs.txt" 2>/dev/null
    echo
    echo "## Library read, per start (cache used? times)"
    for f in "$R"/logs/*.stdout; do
        n="$(basename "$f" .stdout)"
        line="$(strip_ansi "$f" | grep -E "Skipping full scan|Ignoring game index cache|cache restore took|post-processing took|games found$|Finished searching in [1-9]" | sed 's/^\[i\] //' | tr '\n' ';' | sed 's/;/ · /g')"
        echo "    $n: $line"
    done
    echo
    echo "## Screen open times"
    strip_ansi "$R"/logs/*.stdout | grep -h "\[timing\]" | sed 's/.*\[timing\] /    /' | sort | uniq
    echo
    echo "## Metadata written by Pegasus X"
    find "$CFG/metadata" -type f 2>/dev/null | sed "s|$CFG/|    |"
    for f in "$CFG"/metadata/overrides/*.metadata.pegasus.txt; do
        [ -f "$f" ] || continue
        echo
        echo "### $(basename "$f")"
        sed 's/^/    /' "$f" | head -n 60
    done
    if [ -d "$R/scraped" ]; then
        echo
        echo "## Scraper (IGDB test login)"
        sed 's/^/    /' "$R/scraped/scraper.log" | tail -n 40
        echo
        echo "    files:"
        sed 's/^/      /' "$R/scraped/files.txt"
    else
        echo
        echo "## Scraper: no test login (secrets IGDB_TEST_CLIENT_ID / IGDB_TEST_CLIENT_SECRET not set)"
    fi
} > "$R/summary.md"

# the image servers the scraper uses: do they answer without / with a User-Agent?
{
    for url in "https://images.igdb.com/igdb/image/upload/t_cover_big/co1wyy.jpg" \
               "https://images.igdb.com/igdb/image/upload/t_thumb/co1wyy.jpg"; do
        echo "$url"
        echo "  no UA:        $(curl -s -o /dev/null -w '%{http_code} %{content_type}' -A '' "$url")"
        echo "  Qt default:   $(curl -s -o /dev/null -w '%{http_code} %{content_type}' -A 'Mozilla/5.0' "$url")"
        echo "  PegasusX UA:  $(curl -s -o /dev/null -w '%{http_code} %{content_type}' -A 'PegasusX/1.0 (Pegasus frontend)' "$url")"
    done
} > "$R/image-servers.txt" 2>&1
echo "done"
