#!/bin/bash
# Pegasus X test environment: runs Pegasus X on the fixture library (see
# fixture.sh) under a virtual screen, opens its screens, presses keys and
# takes screenshots; the logs and the files it wrote are kept too.
#   $1: pegasus-fe   $2: the fixture folder   $3: the results folder
set -u
BIN="$(readlink -f "$1")"
HERE="$(cd "$(dirname "$0")" && pwd)"
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
wait_log() {  # pattern, seconds, stdout file: until the running Pegasus X's output has it
    for _ in $(seq 1 "$2"); do
        grep -q "$1" "$CFG/lastrun.log" "$3" 2>/dev/null && return 0
        sleep 1
    done
    echo "  (no \"$1\" in the log after $2 s)"
}
stop() {  # name
    kill "$PID" 2>/dev/null; sleep 1; kill -9 "$PID" 2>/dev/null; wait "$PID" 2>/dev/null
    cp "$CFG/lastrun.log" "$R/logs/$1.log" 2>/dev/null
}

# an older build (2230-2238) hid Platform from Library > Genres: no genre is hidden now, and
# the first start removes the entry
mkdir -p "$CFG"
printf '[genres]\nhidden=platform\n' >> "$CFG/pegasusx.ini"

# -- 1. first start: the ES-DE import and the ROM folder import, then setup, then the theme --
# The fixture's game_dirs.txt is Pegasus data already in the folder: step 1 asks to use it or
# start fresh (Start fresh's confirm shown, cancelled; Use it). Step 2: Pick my ROMs folder
# (the picker's "Suggested" is the last folder browsed: MyGames), every system found ticked,
# Import, back in setup; Next; step 3 Skip; step 4 reads the library with setup still on
# screen (build 2274 and before: the theme came up here); then the theme, once.
printf '\n[browse]\nlast=%s\n' "$T/MyGames" >> "$CFG/pegasusx.ini"
start 01-first-start "" 15 PEGASUSX_TEST_ESDE_IMPORT=1 "PEGASUSX_TEST_ROM_FOLDERS=$T/MyGames"
shot 01-wizard-1
keys Down Return
sleep 1
shot 01-wizard-1-start-fresh
keys Escape Up Return
sleep 1
shot 01-wizard-2
keys Return
sleep 1
keys f Right Right Right Right Right Right Right Return i
sleep 2
shot 01-wizard-2-found
keys Page_Down Up Return
sleep 6
shot 01-wizard-2-imported
keys Return
sleep 1
shot 01-wizard-3
keys Down Return
sleep 6
shot 01-wizard-4
keys Down Return
sleep 5
shot 01-theme
stop 01-first-start
APPDATA="$(dirname "$(find "$HOME/.local/share" -name 'pegasusx-setup-done' 2>/dev/null | head -n 1)")"
{ [ -n "$APPDATA" ] && [ "$APPDATA" != "." ]; } || APPDATA="$XDG_DATA_HOME/pegasus-frontend/pegasus-frontend"
echo "$APPDATA" > "$R/files/appdata-dir.txt"
# three more starts: setup stays done (no setup, the theme each time)
for n in 1 2 3; do
    start "01-restart-$n" "" 10
    [ "$n" = 1 ] && shot 01-restart-1
    stop "01-restart-$n"
done
cp "$CFG/pegasusx.ini" "$R/files/pegasusx-after-first-start.ini" 2>/dev/null
cp "$CFG/pegasusx-android-apps.json" "$R/files/apps-after-setup.json" 2>/dev/null
cp "$CFG/game_dirs.txt" "$R/files/game_dirs.txt" 2>/dev/null
find "$T" -name "esde-import.metadata.pegasus.txt" | while read -r f; do
    cp "$f" "$R/files/OLD-PLACE_$(echo "${f#$T/}" | tr '/ ' '__')"
done
find "$CFG/metadata" -type f 2>/dev/null | while read -r f; do
    cp "$f" "$R/files/$(echo "${f#$CFG/}" | tr '/ ' '__')"
done
cp "$CFG/pegasusx-rom-folders.txt" "$R/files/" 2>/dev/null

# a game scraped before, then its scraped files deleted: the game directory
# still names a fan art that isn't there, and hides a scraped cover with the
# same name as the one in the ROM folder's media: the folder's art must show
N64="$CFG/gamedirectory/n64/metadata.pegasus.txt"
if [ -f "$N64" ]; then
    python3 - "$N64" "$T" "$CFG" <<'PY'
import sys
path, t, cfg = sys.argv[1:4]
lines = open(path, encoding="utf-8").read().split("\n")
out = []
for line in lines:
    out.append(line)
    if line.strip() == "file: %s/MyGames/Nintendo 64/Mario Kart 64 (USA).z64" % t:
        out.append("assets.background: %s/downloaded_media/n64/fanart/Mario Kart 64 (USA).png" % cfg)
        out.append("x-pegasusx-hidden-art: box_front|%s/downloaded_media/n64/covers/Mario Kart 64 (USA).png" % cfg)
open(path, "w", encoding="utf-8").write("\n".join(out))
PY
else
    echo "!! no game directory file for n64 after the first start"
fi

# a game an earlier run wasn't sure about (Library > Needs your check)
cat > "$CFG/metadata/review.json" <<JSON
{ "version": 1, "games": {
  "$T/MyGames/Nintendo 64/Star Fox 64 (USA).z64": { "title": "Star Fox 64 (USA)", "collection": "Nintendo 64",
    "system": "n64", "closest": "Star Fox 64 3D", "info": "Nintendo 3DS · 2011", "score": 55, "launchbox": 1 } } }
JSON

# a game added inside a subfolder of a watched ROM folder (psx/Extra/): only Extra's date
# changes, not psx's; the next start's refresh has to find it without a Rescan
head -c 2048 /dev/urandom > "$T/MyGames/psx/Extra/Ape Escape (USA).chd"

# -- 2. the settings ---------------------------------------------------------------
start 02-settings "menu/settings/SettingsScreen.qml" 6
shot 02-settings-top
keys Down Down Down Down Down Down Down Down Down Down Down Down Down Down Down Down
shot 02-settings-bottom
# Advanced... (the last row) and the updater's own switch (Check for updates: 3 rows up)
keys Return
sleep 1
shot 02-settings-advanced
keys Escape
sleep 1
keys Up Up Up Return
sleep 2
keys Up
shot 02-settings-updater
keys Escape
sleep 1
# Controls > Gamepad layout (2 rows above Check for updates), full screen with its
# footer (no pad here: its button pictures on the footer only); B: back on its row
keys Up Up Return
sleep 2
shot 02-gamepad-layout
keys Escape
sleep 1
shot 02-settings-gamepad-back
stop 02-settings
# Missing games: the two settings there were, shown and set as one choice. Each
# pair written into settings.txt, then what Advanced shows; then picked, then
# what settings.txt has
set_general() {
    python3 - "$CFG/settings.txt" "$1" "$2" <<'PY'
import sys, re
path, key, value = sys.argv[1:4]
try:
    text = open(path, encoding="utf-8").read()
except FileNotFoundError:
    text = ""
line = "general.%s: %s" % (key, value)
if re.search(r"^general\.%s:.*$" % re.escape(key), text, re.M):
    text = re.sub(r"^general\.%s:.*$" % re.escape(key), line, text, flags=re.M)
else:
    text = text.rstrip("\n") + "\n" + line + "\n"
open(path, "w", encoding="utf-8").write(text)
PY
}
missing_state() { grep -E "^general\.(verify-files|show-missing-games):" "$CFG/settings.txt" | sort | tr '\n' ' '; }
cp "$CFG/settings.txt" "$R/files/settings-before-missing.txt" 2>/dev/null
for pair in "true false hide" "true true show" "false false dontcheck" "false true dontcheck-show"; do
    set -- $pair
    set_general verify-files "$1"; set_general show-missing-games "$2"
    start "02-missing-$3" "menu/settings/SettingsAdvanced.qml" 5
    shot "02-missing-$3"
    stop "02-missing-$3"
    echo "$3: written $(missing_state)" >> "$R/files/missing-games.txt"
done
# picked: Hide -> Show, then Show -> Don't check (show-missing-games stays as it is)
set_general verify-files true; set_general show-missing-games false
start 02-missing-pick "menu/settings/SettingsAdvanced.qml" 5
keys Down Down Down Return
sleep 1
shot 02-missing-box
keys Down Return
sleep 1
echo "picked Show: $(missing_state)" >> "$R/files/missing-games.txt"
keys Return
sleep 1
keys Down Return
sleep 1
echo "picked Don't check: $(missing_state)" >> "$R/files/missing-games.txt"
shot 02-missing-picked
stop 02-missing-pick
cp "$R/files/settings-before-missing.txt" "$CFG/settings.txt" 2>/dev/null

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
# Metadata (the row under Needs your check), its Genres tab. The fixture's genres, A to Z:
# Action (1 game), Platform (Platform, platform) and Shoot'em Up (Shoot'em Up, Shoot’em Up),
# under Suggested merges, Merged, Search genres..., Only genres with 1 game and Sort
# (Suggested merges selected first). A genre's page: Change this genre... (selected), its
# two spellings side by side, then its games
keys Down Return
sleep 2
shot 03-genres
keys Down Down Down Down Down Down Return
shot 03-genre-games
keys Down Down Down Return
sleep 1
shot 03-genre-game
keys Escape
shot 03-genre-games-back
keys Escape Down Return
shot 03-genre-spellings
# Search genres...: "shoot" narrows the list as it's typed; then cleared
keys Escape Page_Up Down Down Return
sleep 1
xdotool type --clearmodifiers --delay 150 "shoot"
sleep 1
shot 03-genres-search-typing
keys Return
shot 03-genres-search
keys Return
sleep 1
keys BackSpace BackSpace BackSpace BackSpace BackSpace Return
shot 03-genres-search-cleared
# Only genres with 1 game (Action), then off; Sort: most games first, fewest, A to Z again
keys Down Return
shot 03-genres-only-ones
keys Return Down Return
shot 03-genres-sort-most
keys Return
shot 03-genres-sort-fewest
keys Return
# Change this genre...: Platform (2 games, two spellings) becomes Platformer, typed; then
# Shoot'em Up's first spelling (the straight ', A to Z on a tie) for both its games
keys Down Down Return Return
shot 03-genre-pick
keys Return
sleep 1
xdotool type --clearmodifiers --delay 150 "Platformer"
sleep 1
shot 03-genre-type
keys Return
shot 03-genre-confirm
keys Down Return
sleep 2
shot 03-genre-changed
keys Down Return Down Return
shot 03-genre-spelling-confirm
keys Down Return
sleep 2
shot 03-genre-spelling-used
keys Escape
shot 03-genres-back
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
# the game directory as Library > Genres left it (the scrapes below write genres too)
mkdir -p "$R/after-genres"
cp -r "$CFG/gamedirectory" "$R/after-genres/" 2>/dev/null

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

# -- 4b. Settings > Gaming > Scraper: the section that hosts the Scraper (loads at all)
start 04b-scraper-section "menu/settings/ScraperSection.qml" 6
shot 04b-scraper-section
stop 04b-scraper-section

start 05-scraper-accounts "menu/settings/ScraperScreen.qml" 6
keys Down Down Down Return
shot 05-accounts
keys Down Down Down Return
sleep 1
shot 05-accounts-help
# Extra systems for Android (its prompts: no ◂ ▸ there)
keys Escape Page_Up Down Down Return
sleep 1
shot 05-accounts-android
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
# the file picker opens at the last folder browsed: the ROMs folder (snes, gba in it)
printf '\n[browse]\nlast=%s\n' "$T/storage/ROMs" >> "$CFG/pegasusx.ini"
start 08-gamedirs "menu/settings/GameDirectory.qml" 5
shot 08-gamedirs
# Pick my ROMs folder: the picker opens on the drives; Y: the quick jumps, the last one is
# "Suggested" (the last folder browsed, ROMs: gba, snes in it); X "Use this folder"
keys Return f Right Right Right Right Right Right Right Return i
sleep 1
shot 08-gamedirs-picked
# untick one of the systems found (below Select all / Deselect all)
keys Down Down Return
shot 08-gamedirs-ticked
keys Escape
sleep 1
# Add systems: the list to tick
keys Down Return
sleep 1
shot 08-gamedirs-add
keys Escape
sleep 1
# A on a system: rescan or remove
keys Down Down Down Down Return
shot 08-gamedirs-system
stop 08-gamedirs
# -- 8d. Pick my ROMs folder: matched and unmatched folders, Deselect all / Select all, Cancel
# (nothing added: the game directory the same after), then a folder with no system in it --
FF="$T/FolderFirst"
mkdir -p "$FF/snes" "$FF/gba" "$FF/Random Stuff" "$FF/psx" "$T/NoSystems/notes" "$T/NoSystems/Pictures"
head -c 2048 /dev/urandom > "$FF/snes/Folder First Quest (USA).sfc"
head -c 2048 /dev/urandom > "$FF/gba/Folder First Advance (USA).gba"
echo "not a game" > "$FF/Random Stuff/readme.txt"
echo "not a game" > "$FF/psx/notes.txt"
echo "x" > "$T/NoSystems/notes/a.txt"
find "$CFG/gamedirectory" -type f -exec md5sum {} + 2>/dev/null | sort > "$R/files/folderfirst-before.txt"
sed -i "/^\[browse\]/,/^\[/ s|^last=.*|last=$FF|" "$CFG/pegasusx.ini"
start 08d-folder-first "menu/settings/GameDirectory.qml" 5
keys Return f Right Right Right Right Right Right Right Return i
sleep 2
shot 08d-folder-first-found
keys Down Return
shot 08d-folder-first-deselected
keys Up Return
shot 08d-folder-first-selected
keys Page_Down
shot 08d-folder-first-bottom
keys Escape
sleep 1
shot 08d-folder-first-cancelled
stop 08d-folder-first
find "$CFG/gamedirectory" -type f -exec md5sum {} + 2>/dev/null | sort > "$R/files/folderfirst-after.txt"
sed -i "/^\[browse\]/,/^\[/ s|^last=.*|last=$T/NoSystems|" "$CFG/pegasusx.ini"
start 08e-folder-first-empty "menu/settings/GameDirectory.qml" 5
keys Return f Right Right Right Right Right Right Right Return i
sleep 2
shot 08e-folder-first-nothing
# Tick my systems instead: the list to tick
keys Down Return
sleep 1
shot 08e-tick-instead
stop 08e-folder-first-empty

# -- 8b. a system's ROM folders: the Folders view, adding the SD card's folder, removing it ------
SD="$T/sdcard"
sed -i "/^\[browse\]/,/^\[/ s|^last=.*|last=$SD/ROMs|" "$CFG/pegasusx.ini"
start 08b-folders "menu/settings/GameDirectory.qml" 5 "PEGASUSX_TEST_SD=$SD"
# the systems are A to Z (GameCube, My SNES picks, Nintendo 64, Nintendo Game Boy Advance,
# Nintendo SNES): LT to the top, down to SNES, A: its menu, A: Folders
keys Page_Up Down Down Down Down Down Down Down Return
shot 08b-system-menu
keys Return
sleep 1
shot 08b-folders
# Add a folder...: the picker (on the drives); Y: the quick jumps, the last one is
# "Suggested" (the last folder browsed: the SD card's ROMs); X "Use this folder" there
# reads only its snes folder
keys Down Return
sleep 1
shot 08b-folders-picker-drives
keys f Right Right Right Right Right Right Right Return
sleep 1
shot 08b-folders-picker
# LB / RB 10 rows, LT / RT bottom / top (the list stays in the folder)
keys Page_Down
shot 08b-folders-picker-bottom
keys Page_Up
keys i
sleep 4
shot 08b-folders-added
# the list stays on the third row, now the SD card's folder: remove it and its games
keys Return
shot 08b-folders-remove
keys Down Return
sleep 1
shot 08b-folders-removed
stop 08b-folders
# the same steps, and the edge cases, through the same functions (checked in the summary)
# a ROM folder with a metadata file of its own (read once at import: titles,
# fields, art, each game's launch and workdir; then Restore from it)
OWN="$T/OwnMeta/snes"
mkdir -p "$OWN/art" "$OWN/tools"
head -c 4096 /dev/urandom > "$OWN/Alpha Quest (USA).sfc"
head -c 4096 /dev/urandom > "$OWN/Beta Racer (USA).sfc"
printf '\x89PNG\r\n\x1a\n' > "$OWN/art/alpha-box.png"
cat > "$OWN/metadata.pegasus.txt" <<'TXT'
collection: My SNES
shortname: snes
launch: /usr/bin/snes9x "{file.path}"

game: Alpha Quest Deluxe
file: Alpha Quest (USA).sfc
developer: Test Dev
publisher: Test Pub
genre: RPG
players: 2
release: 1994-05-01
summary: A short one.
x-test-field: kept
assets.box_front: art/alpha-box.png
launch: ./run-alpha.sh "{file.path}"
workdir: tools

game: Beta Racer
file: Beta Racer (USA).sfc
launch: /opt/emu/beta {file.path}
workdir: /tmp/beta
TXT
start 08c-folder-checks "menu/settings/GameDirectory.qml" 3 "PEGASUSX_TEST_SD=$SD" "PEGASUSX_TEST_FOLDERS=$SD" "PEGASUSX_TEST_OWNMETA=$OWN"
wait_log "Test: Own metadata: done" 90 "$R/logs/08c-folder-checks.stdout"
stop 08c-folder-checks

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

# -- a scrape without any login: Hasheous + libretro's thumbnails ----------------------------------
# (the fixture's ROMs are made up, so Hasheous won't know them: libretro finds the art by file name)
mkdir -p "$CFG"
python3 - "$CFG/pegasusx.ini" <<'PY'
import sys, configparser
p = sys.argv[1]
c = configparser.RawConfigParser(); c.optionxform = str
c.read(p)
if not c.has_section("scraper"): c.add_section("scraper")
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

# -- LaunchBox's database: read in for the fixture's systems, then a scrape from it ---------------
# (the zip is downloaded by the workflow into lbcache/ next to the sources)
LBZIP="$(dirname "$0")/../../lbcache/Metadata.zip"
if [ -f "$LBZIP" ]; then
    mkdir -p "$CFG/launchbox"
    cp "$LBZIP" "$CFG/launchbox/Metadata.zip"
    python3 - "$CFG/pegasusx.ini" <<'PY'
import sys, configparser
p = sys.argv[1]
c = configparser.RawConfigParser(); c.optionxform = str
c.read(p)
if not c.has_section("scraper"): c.add_section("scraper")
c.set("scraper", "source", "launchbox")
c.set("scraper", "overwrite", "true")
c.set("scraper", "useLibretro", "false")
with open(p, "w") as f: c.write(f, space_around_delimiters=False)
PY
    start 13-scrape-launchbox "" 8 "PEGASUSX_TEST_SCRAPE=Super Mario World" "PEGASUSX_TEST_LAUNCHBOX=1"
    wait_log "Scraper: Done\|LaunchBox: Reading the database:\|Download failed\|no game called" 900 "$R/logs/13-scrape-launchbox.stdout"
    sleep 2
    stop 13-scrape-launchbox
    mkdir -p "$R/scraped-launchbox"
    find "$CFG/downloaded_media/snes" -type f -iname "Super Mario World*" 2>/dev/null | sed "s|$CFG/||" > "$R/scraped-launchbox/files.txt" || true
    grep -h "Scraper\|LaunchBox\|\[test\]" "$R/logs/13-scrape-launchbox.stdout" | sed 's/\x1b\[[0-9;]*m//g' > "$R/scraped-launchbox/scraper.log" || true
    cp "$CFG/launchbox/status.json" "$R/scraped-launchbox/" 2>/dev/null || true
    python3 - "$CFG/launchbox/platforms.json" > "$R/scraped-launchbox/platforms.txt" 2>/dev/null <<'PY' || true
import json, sys
d = json.load(open(sys.argv[1]))
for k in sorted(d): print("%6d  %s" % (d[k], k))
PY
    ls "$CFG/launchbox/index" > "$R/scraped-launchbox/index.txt" 2>/dev/null || true
    # the x-esrb lines the scrape wrote (Super Mario World's), and an index's ESRB values
    grep -rn "^x-esrb:" "$CFG/gamedirectory" 2>/dev/null | sed "s|$CFG/||" > "$R/scraped-launchbox/esrb-lines.txt" || true
    python3 - "$CFG/launchbox/index" > "$R/scraped-launchbox/esrb-index.txt" 2>/dev/null <<'PY' || true
import json, os, sys, collections
d = sys.argv[1]
for f in sorted(os.listdir(d)):
    games = json.load(open(os.path.join(d, f)))
    c = collections.Counter(g.get("esrb", "<no key>") for g in games)
    print("%s: %s" % (f, ", ".join('"%s" %d' % (k, v) for k, v in c.most_common())))
PY
    # a Nintendo Switch 2 game (MyGames/switch2, a .nup) scraped from LaunchBox's database
    start 13b-scrape-switch2 "" 8 "PEGASUSX_TEST_SCRAPE=Donkey Kong Bananza" "PEGASUSX_TEST_LAUNCHBOX=1"
    wait_log "Scraper: Done\|no sure match\|no platform for\|no game called" 300 "$R/logs/13b-scrape-switch2.stdout"
    sleep 2
    stop 13b-scrape-switch2
    grep -h "Scraper\|LaunchBox\|\[test\]" "$R/logs/13b-scrape-switch2.stdout" | sed 's/\x1b\[[0-9;]*m//g' > "$R/scraped-launchbox/switch2.log" || true
    find "$CFG/downloaded_media/switch2" -type f 2>/dev/null | sed "s|$CFG/||" > "$R/scraped-launchbox/switch2-files.txt" || true
    # the same scrape with Library open on a collection: its preview panel shows the progress
    # while it runs, the picture again once it's done
    start 13c-scrape-progress "menu/settings/MetadataEditor.qml" 6 "PEGASUSX_TEST_SCRAPE=Donkey Kong Bananza" "PEGASUSX_TEST_LAUNCHBOX=1"
    keys Down Down Down Down Down Return
    sleep 1
    shot 13c-library-collection
    wait_log "Scraper: Scraping 1 games" 300 "$R/logs/13c-scrape-progress.stdout"
    sleep 1
    shot 13c-scrape-progress
    wait_log "Scraper: Done" 120 "$R/logs/13c-scrape-progress.stdout"
    sleep 2
    shot 13c-scrape-done
    stop 13c-scrape-progress
fi

# -- 14. Android apps to import (the app picker) and a hidden app's section --------------------
# Four "installed" apps (PEGASUSX_TEST_APPS); three of them are in an Android collection of the
# game directory (the test environment has no Pegasus app import of its own): Notes (an app),
# Drift Racer (Android says it's a game), Retro Emu (an emulator: "retroarch"), hidden. Camera
# isn't in the library. The first start with the picker keeps the three as picked.
APPS="Notes|com.test.notes|0;Drift Racer|com.test.drift|1;Retro Emu|com.test.retroarch|0;Camera|com.test.camera|0"
mkdir -p "$CFG/gamedirectory/android"
cat > "$CFG/gamedirectory/android/metadata.pegasus.txt" <<'TXT'
collection: Android
shortname: android

game: Notes
file: android:com.test.notes

game: Drift Racer
file: android:com.test.drift

game: Retro Emu
file: android:com.test.retroarch
TXT
python3 - "$CFG/pegasusx-hidden.json" <<'PY'
import json, os, sys
path = sys.argv[1]
data = json.load(open(path)) if os.path.exists(path) else {}
data.setdefault("collections", [])
data.setdefault("games", []).append({"path": "android:com.test.retroarch", "title": "Retro Emu", "collections": ["Android"]})
json.dump(data, open(path, "w"), indent=4)
PY
# a new install with apps (setup still to do: no setup-done marker, no older build's
# pegasusx-root-asked): the picker opens with every app unticked; with none
# picked there's no Android collection, and Library > Options still has the picker
rm -f "$CFG/pegasusx-android-apps.json" "$CFG/pegasusx-root-asked" "$CFG/pegasusx-wizard"
rm -f "$APPDATA/pegasusx-setup-done" "$APPDATA/pegasusx-setup-step"
find "$HOME" -name "gameindex-v1.json" -delete 2>/dev/null
start 14-apps-new "menu/settings/AndroidAppPicker.qml" 8 "PEGASUSX_TEST_APPS=$APPS"
shot 14-new-picker
stop 14-apps-new
cp "$CFG/pegasusx-android-apps.json" "$R/files/apps-new-install.json" 2>/dev/null
start 14-apps-new-library "menu/settings/MetadataEditor.qml" 6 "PEGASUSX_TEST_APPS=$APPS"
keys Page_Down
shot 14-new-library-options
stop 14-apps-new-library
# a library from an older build: setup done (as on a device after setup), no picks saved
# (no setup-done marker yet: an older build's pegasusx-root-asked and the app's cache say done)
rm -f "$CFG/pegasusx-android-apps.json"
rm -f "$APPDATA/pegasusx-setup-done" "$APPDATA/pegasusx-setup-step"
echo asked > "$CFG/pegasusx-root-asked"
find "$HOME" -name "gameindex-v1.json" -delete 2>/dev/null
start 14-apps-first "" 10 "PEGASUSX_TEST_APPS=$APPS"
wait_log "Android apps to import" 30 "$R/logs/14-apps-first.stdout"
stop 14-apps-first
cp "$CFG/pegasusx-android-apps.json" "$R/files/apps-after-first-start.json" 2>/dev/null
# the picker: Camera ticked and moved to Emulator (X), Notes unticked; Search apps... "dri":
# Deselect all shown, then Select all shown (Drift Racer off and on again); B clears the search
start 14-apps-picker "menu/settings/AndroidAppPicker.qml" 5 "PEGASUSX_TEST_APPS=$APPS"
shot 14-picker
keys Down Down Down Return i
keys Down Down Return
shot 14-picker-changed
keys Page_Up Return
sleep 1
xdotool type --clearmodifiers --delay 150 "dri"
sleep 1
keys Return
shot 14-picker-search
keys Down Down Return
shot 14-picker-deselect-shown
keys Up Return
shot 14-picker-select-shown
keys Escape
shot 14-picker-cleared
stop 14-apps-picker
cp "$CFG/pegasusx-android-apps.json" "$R/files/apps-after-picker.json" 2>/dev/null
cp "$CFG/pegasusx-android-kinds.json" "$R/files/android-kinds-after-picker.json" 2>/dev/null
# the library as read now: Drift Racer only (Notes not picked, Retro Emu hidden; Camera has
# no entry here: on Android, Pegasus' own app import has every app)
start 14-apps-theme "" 10 "PEGASUSX_TEST_APPS=$APPS" "PEGASUSX_TEST_DUMP=$R/apps-library.txt"
stop 14-apps-theme
# Library > Android: Retro Emu (hidden) under Emulators, not Games
start 14-apps-library "menu/settings/MetadataEditor.qml" 6 "PEGASUSX_TEST_APPS=$APPS"
shot 14-library
keys Down Down Down Return
sleep 2
shot 14-library-android
keys Down Down Down Down Down Down Down Down Down Down
shot 14-library-android-down
stop 14-apps-library

# -- 15. Settings > Backup & reset (all in the fixture's throwaway folders) ---------------------------
# planted secrets (a backup must leave them out), a theme in the Pegasus X folder, a file outside;
# the Pegasus X folder is copied first and put back after Start over (the summary reads it)
cp -a "$CFG" "$T/cfg-saved"
python3 - "$CFG/pegasusx.ini" <<'PY'
import sys
path = sys.argv[1]
text = open(path, encoding="utf-8").read() if __import__("os").path.exists(path) else ""
text += "\n[scraper]\nssUser=testuser\nssPassword=PLANTEDPASS\n\n[retroachievements]\nuser=rauser\nkey=PLANTEDKEY\n\n[github]\ntoken=PLANTEDTOKEN\n"
open(path, "w", encoding="utf-8").write(text)
PY
mkdir -p "$CFG/themes/FakeXBSX" "$CFG/downloaded_media/snes/covers"
echo "Item {}" > "$CFG/themes/FakeXBSX/theme.qml"
head -c 4096 /dev/urandom > "$CFG/downloaded_media/snes/covers/Fake.png"
echo "outside" > "$T/outside.txt"
find "$T/MyGames" "$T/storage/ES-DE" "$T/storage/ROMs" "$CFG/themes" "$T/outside.txt" -type f -exec md5sum {} + 2>/dev/null | sort > "$R/files/kept-before.txt"
# Back up (default folder: Download on the device's storage), without the art
start 15-backup "menu/settings/BackupResetScreen.qml" 5
shot 15-backup-main
keys Return
sleep 2
shot 15-backup-page
# Save to: the folder picker (one hint bar), closed again with B (on the drives)
keys Return
sleep 1
shot 15-backup-picker
keys Escape
sleep 1
keys Down Down Return
wait_log "Backup & reset: backup" 60 "$R/logs/15-backup.stdout"
sleep 1
shot 15-backup-done
# Run setup again: setup opens (the theme closes), nothing changes
find "$CFG" -type f -not -name "lastrun.log" -not -name "pegasusx-wizard" -exec md5sum {} + 2>/dev/null | sort > "$R/files/before-setup-again.txt"
keys Escape Down Down Return
sleep 3
shot 15-setup-again
find "$CFG" -type f -not -name "lastrun.log" -not -name "pegasusx-wizard" -exec md5sum {} + 2>/dev/null | sort > "$R/files/after-setup-again.txt"
# on to step 2, then Pegasus X closed mid-setup: the next start goes on at step 2 (no theme),
# B twice: back to step 1, then skip setup: the theme
keys Return
sleep 1
stop 15-backup
start 15-resume "" 10
shot 15-resume-step-2
keys Escape
sleep 1
keys Escape
sleep 5
shot 15-resume-skipped
stop 15-resume
ls "$T/storage/Download" > "$R/files/backups.txt" 2>/dev/null
BK="$(ls "$T/storage/Download"/PegasusX-backup-*.tar 2>/dev/null | head -n 1)"
[ -n "$BK" ] && tar -tf "$BK" > "$R/files/backup-contents.txt" 2>&1
[ -n "$BK" ] && cp "$BK" "$R/files/backup.tar"
# Start over with an App Data Folder picked: PXFolder holds Pegasus X's data (a copy of the
# folder's), the app's own folder still has the older copy (as a move leaves it); both go,
# the themes in both stay, and which folder is used stays. Setup's marker and step are kept
# aside and put back after (the app's own storage), with the Pegasus X folder.
PX="$T/PXFolder"
rm -rf "$PX" "$T/appdata-saved"
mkdir -p "$PX"
(cd "$CFG" && tar -cf - --exclude=./pegasusx-root.txt --exclude=./pegasusx-root-asked --exclude=./lastrun.log --exclude=./themes .) | (cd "$PX" && tar -xf -)
mkdir -p "$PX/themes/OtherTheme"
echo "Item {}" > "$PX/themes/OtherTheme/theme.qml"
echo "$PX" > "$CFG/pegasusx-root.txt"
cp -a "$APPDATA" "$T/appdata-saved"
find "$T/MyGames" "$T/storage/ES-DE" "$T/storage/ROMs" "$CFG/themes" "$PX/themes" "$T/outside.txt" -type f -exec md5sum {} + 2>/dev/null | sort > "$R/files/kept-before.txt"
# Start over: the Start over page, both confirms (Cancel focused: Right picks the other button)
rm -f "$CFG/pegasusx-wizard"
start 15-start-over "menu/settings/BackupResetScreen.qml" 5
keys Down Down Down Return
sleep 2
shot 15-wipe-page
keys Down Down Return
sleep 1
shot 15-wipe-confirm-1
keys Right Return
sleep 1
shot 15-wipe-confirm-2
keys Right Return
wait_log "Backup & reset: start over done" 60 "$R/logs/15-start-over.stdout"
sleep 1
shot 15-start-over-restarting
# it closes by itself to start again (the test environment starts it again here)
for _ in $(seq 1 30); do kill -0 "$PID" 2>/dev/null || break; sleep 0.5; done
if kill -0 "$PID" 2>/dev/null; then echo "still running" > "$R/files/start-over-quit.txt"; else echo "closed by itself" > "$R/files/start-over-quit.txt"; fi
stop 15-start-over
find "$CFG" -type f -not -name "lastrun.log" | sed "s|$CFG/||" | sort > "$R/files/after-start-over-files.txt"
find "$PX" -type f -not -name "lastrun.log" | sed "s|$PX/||" | sort > "$R/files/after-start-over-px-files.txt"
ls -a "$APPDATA" > "$R/files/after-start-over-appdata.txt" 2>&1
find "$T/MyGames" "$T/storage/ES-DE" "$T/storage/ROMs" "$CFG/themes" "$PX/themes" "$T/outside.txt" -type f -exec md5sum {} + 2>/dev/null | sort > "$R/files/kept-after.txt"
ls "$T/storage/Download" > "$R/files/backups-after-start-over.txt" 2>/dev/null
# the start after it: setup from step 1, no theme
start 15-after-start-over "" 10
shot 15-after-start-over
stop 15-after-start-over
# the Pegasus X folder and the app's storage as they were (for the summary below)
rm -rf "$CFG" "$PX" "$APPDATA"
mv "$T/cfg-saved" "$CFG"
cp -a "$T/appdata-saved" "$APPDATA"

# -- 16. damaged-file recovery (Recovery.h): files damaged or cut short before a start -------------
# settings.txt turned to junk, pegasusx-hidden.json (or favorites.txt) cut in half, game_dirs.txt
# left as only game_dirs.txt.pxold (a save cut short); each start's notice: Y details then B Keep,
# then X Restore from a backup; a start with nothing to fix shows nothing. Put back as it was after.
cp -a "$CFG" "$T/cfg-before-damage"
ls "$CFG" | grep -E '\.pxgood$' > "$R/files/good-copies.txt"
HALF="pegasusx-hidden.json"; [ -s "$CFG/$HALF" ] || HALF="favorites.txt"
for f in settings.txt "$HALF" game_dirs.txt pegasusx.ini; do
    [ -f "$CFG/$f" ] && md5sum < "$CFG/$f" | cut -d' ' -f1 > "$R/files/orig-$f.md5"
    [ -f "$CFG/$f.pxgood" ] && md5sum < "$CFG/$f.pxgood" | cut -d' ' -f1 > "$R/files/good-$f.md5"
done
head -c 3000 /dev/urandom > "$CFG/settings.txt"
[ -s "$CFG/$HALF" ] && python3 -c "import sys; p=sys.argv[1]; b=open(p,'rb').read(); open(p,'wb').write(b[:len(b)//2])" "$CFG/$HALF"
[ -f "$CFG/game_dirs.txt" ] && mv "$CFG/game_dirs.txt" "$CFG/game_dirs.txt.pxold"
echo "$HALF" > "$R/files/half-cut.txt"
start 16-recovery "" 15
shot 16-recovery-notice
keys f
sleep 1
shot 16-recovery-details
keys Escape
sleep 1
shot 16-recovery-after-keep
stop 16-recovery
for f in settings.txt "$HALF" game_dirs.txt; do
    [ -f "$CFG/$f" ] && md5sum < "$CFG/$f" | cut -d' ' -f1 > "$R/files/after-$f.md5"
done
ls -l "$CFG/damaged" > "$R/files/damaged-1.txt" 2>&1
ls "$CFG" | grep -E '\.(pxsave|pxold)$' > "$R/files/leftovers-1.txt"
# again: pegasusx.ini full of zero bytes; the notice's X opens Backup & reset's restore picker
head -c 2000 /dev/zero > "$CFG/pegasusx.ini"
start 16-recovery-restore "" 15
shot 16-recovery-notice-2
keys i
sleep 3
shot 16-recovery-restore-picker
stop 16-recovery-restore
[ -f "$CFG/pegasusx.ini" ] && md5sum < "$CFG/pegasusx.ini" | cut -d' ' -f1 > "$R/files/after-pegasusx.ini.md5"
ls -l "$CFG/damaged" > "$R/files/damaged-2.txt" 2>&1
# nothing to fix: no notice
start 16-recovery-none "" 15
shot 16-recovery-none
stop 16-recovery-none
rm -rf "$CFG"
mv "$T/cfg-before-damage" "$CFG"

# -- 17. Library > Metadata: suggested merges, merges, a restart and a rescrape, undo, recovery ------
# A folder of its own (MetaTest, in game_dirs.txt) with publishers and developers spelled several
# ways: Nintendo, Nintendo of America, Nintendo Co., Ltd.; Sega, SEGA Europe; developers Nintendo
# EAD, Sega AM2 (divisions: never suggested with Sega), Sega, Hudson Soft / HudsonSoft. Publishers:
# the first suggestion merged, then the next (both into Nintendo); Developers' suggestions; Merged.
# No game file may change (checksums before and after). Then a "rescrape" adds a game spelled
# "Nintendo Co., Ltd.": after a restart it shows as Nintendo. Undo; then aliases.json junked:
# recovery puts its last good copy back.
cp -a "$CFG" "$T/cfg-before-meta"
MT="$T/MetaTest"
mkdir -p "$MT"
for g in a b c d e f g h; do head -c 2048 /dev/urandom > "$MT/$g.bin"; done
cat > "$MT/metadata.pegasus.txt" <<'TXT'
collection: Meta Test
shortname: metatest
extensions: bin

game: Meta Alpha
file: a.bin
publisher: Nintendo
developer: Nintendo EAD

game: Meta Bravo
file: b.bin
publisher: Nintendo of America
developer: Nintendo EAD

game: Meta Charlie
file: c.bin
publisher: Nintendo Co., Ltd.
developer: Sega AM2

game: Meta Delta
file: d.bin
publisher: Sega
developer: Sega AM2

game: Meta Echo
file: e.bin
publisher: SEGA Europe
developer: Sega

game: Meta Foxtrot
file: f.bin
publisher: Nintendo of America
  Nintendo
developer: Hudson Soft

game: Meta Golf
file: g.bin
developer: HudsonSoft
TXT
echo "$MT" >> "$CFG/game_dirs.txt"
originals() {  # every metadata file, gamelist and game directory file: name and checksum
    find "$MT" "$T/Pegasus" "$T/storage/ROMs" "$T/storage/ES-DE/gamelists" "$T/MyGames" "$CFG/gamedirectory" \
        -type f \( -name '*.txt' -o -name '*.xml' -o -name '*.json' \) -exec md5sum {} + 2>/dev/null | sort
}
start 17-metadata "menu/settings/MetadataEditor.qml" 8
originals > "$R/files/meta-originals-before.txt"
keys Down Down Return
sleep 3
shot 17-genres
keys e
sleep 3
shot 17-publishers
keys Return
sleep 1
shot 17-publisher-suggestions
# the first suggestion (near-certain: Nintendo, Nintendo Co., Ltd.): Merge, both ticked
keys Return
sleep 1
shot 17-publisher-merge
keys Down Down Down Down Return
sleep 1
shot 17-publisher-merge-confirm
keys Right Return
sleep 2
shot 17-publisher-merged-1
# the next one (regional: Nintendo, Nintendo of America)
keys Page_Up Return
sleep 2
shot 17-publisher-suggestions-2
keys Return
sleep 1
shot 17-publisher-merge-2
keys Down Down Down Down Return
sleep 1
keys Right Return
sleep 2
shot 17-publisher-merged-2
# Developers: its suggestions (Sega AM2 never with Sega)
keys e
sleep 3
keys Return
sleep 1
shot 17-developer-suggestions
keys Escape q
sleep 2
# Publishers > Merged
keys Page_Up Down Return
sleep 1
shot 17-publisher-merged-list
originals > "$R/files/meta-originals-after-merge.txt"
cp "$CFG/metadata/aliases.json" "$R/files/aliases-after-merge.json" 2>/dev/null
stop 17-metadata
# a "rescrape" writes the old spelling into a game again; a restart reads it
cat >> "$MT/metadata.pegasus.txt" <<'TXT'

game: Meta Hotel
file: h.bin
publisher: Nintendo Co., Ltd.
TXT
originals > "$R/files/meta-originals-rescraped.txt"
start 17-metadata-restart "menu/settings/MetadataEditor.qml" 8
keys Down Down Return
sleep 3
keys e
sleep 3
shot 17-restart-publishers
# undo: Merged, its first (Nintendo), the confirm's Undo
keys Page_Up Down Return
sleep 1
keys Return
sleep 1
shot 17-undo-confirm
keys Right Return
sleep 2
shot 17-undone
keys Escape
sleep 2
originals > "$R/files/meta-originals-after-undo.txt"
cp "$CFG/metadata/aliases.json" "$R/files/aliases-after-undo.json" 2>/dev/null
stop 17-metadata-restart
# aliases.json junked: recovery puts its last good copy back
head -c 600 /dev/urandom > "$CFG/metadata/aliases.json"
start 17-metadata-recovery "menu/settings/MetadataEditor.qml" 5
stop 17-metadata-recovery
cp "$CFG/metadata/aliases.json" "$R/files/aliases-after-recovery.json" 2>/dev/null
ls "$CFG/damaged" > "$R/files/meta-damaged.txt" 2>&1
rm -rf "$CFG"
mv "$T/cfg-before-meta" "$CFG"

# -- 18. Settings > Gamepad layout's picture: every button's highlight, at the page's sizes -------
# (no pad here, so the page shows no picture: GamepadPreview.qml alone, in qmlscene, with
# gamepad_points.qml; each highlight's middle checked against where the button is)
mkdir -p "$R/gamepad"
if command -v qmlscene >/dev/null 2>&1; then
    (cd "$R/gamepad" && timeout 180 qmlscene "$HERE/gamepad_points.qml" > ../logs/18-gamepad-points.stdout 2>&1)
else
    echo "no qmlscene" > "$R/logs/18-gamepad-points.stdout"
fi

# -- summary --------------------------------------------------------------------------------------
# the game directory as it is at the end (written along the way)
find "$CFG/gamedirectory" "$CFG/backup" -type f 2>/dev/null | while read -r f; do
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
    echo "## Library > Genres"
    # an older build's hidden genre (genres/hidden=platform, put in before the first start)
    if grep -q "^hidden=" "$R/files/pegasusx-after-first-start.ini" 2>/dev/null; then
        echo "    FAILED: the old genres/hidden entry is still in pegasusx.ini after the first start"
    else
        echo "    OK: the old genres/hidden entry is gone after the first start"
    fi
    strip_ansi "$R/logs/01-first-start.stdout" | grep -h "hidden genres of an older build" | sed 's/^/    /'
    strip_ansi "$R/logs/03-library.stdout" | grep -h "Genres: " | sort -u | sed 's/^/    /'
    echo
    echo "### Change this genre... (Platform -> Platformer; Shoot’em Up -> Shoot'em Up)"
    python3 - "$R/after-genres/gamedirectory" "$T" <<'PY' | sed 's/^/    /'
import glob, os, sys
gd, t = sys.argv[1:3]
genres = {}   # game file name -> genres, in the game directory
for path in glob.glob(os.path.join(gd, "*", "metadata.pegasus.txt")):
    cur, files, gen = None, [], None
    def flush():
        for f in files:
            genres[os.path.basename(f)] = gen
    for line in open(path, encoding="utf-8").read().split("\n"):
        if line.startswith("game:") or line.startswith("collection:"):
            flush(); files, gen = [], None
        elif line.startswith("file:"):
            files.append(line[5:].strip())
        elif line.startswith("genre:"):
            gen = line[6:].strip()
    flush()
want = {
    "Super Mario World (USA).sfc": "Platformer",
    "Crash Bandicoot (USA).chd": "Platformer",
    "Wind Waker (USA).iso": "Shoot'em Up",
}
for f, g in want.items():
    have = genres.get(f)
    print(("OK: " if have == g else "FAILED: ") + "%s: genre %r in the game directory (wanted %r)" % (f, have, g))
# already spelled so: not written
have = genres.get("Metroid Prime (USA).iso")
print(("OK: " if have in (None, "Shoot'em Up") else "FAILED: ") + "Metroid Prime (USA).iso (already Shoot'em Up): %r in the game directory" % (have,))
# the files a change never writes to: the folder's own gamelist, the Pegasus metadata file
own = open(os.path.join(t, "MyGames/psx/gamelist.xml"), encoding="utf-8").read()
print(("OK: " if "<genre>platform</genre>" in own else "FAILED: ") + "psx's own gamelist.xml keeps <genre>platform</genre>")
peg = open(os.path.join(t, "Pegasus/gamecube/metadata.pegasus.txt"), encoding="utf-8").read()
print(("OK: " if "genre: Shoot’em Up" in peg and "genre: Shoot'em Up" in peg else "FAILED: ")
      + "Pegasus/gamecube/metadata.pegasus.txt keeps both its spellings")
PY
    n_edit="$(find "$CFG/backup" -name "*.edit.*" 2>/dev/null | wc -l)"
    echo "    backups made before the changes (reason edit): $n_edit"
    strip_ansi "$R/logs/03-library.stdout" | grep -h "Genres: \".*\" -> \|games saved in" | sed 's/^/    /'
    echo
    echo "## Nintendo Switch 2 (.nup)"
    S2="$CFG/gamedirectory/switch2/metadata.pegasus.txt"
    if grep -q "^collection: Nintendo Switch 2$" "$S2" 2>/dev/null && grep -q "Donkey Kong Bananza.nup" "$S2"; then
        echo "    OK: MyGames/switch2 is Nintendo Switch 2, with its .nup game"
    else
        echo "    FAILED: no Nintendo Switch 2 list with Donkey Kong Bananza.nup (gamedirectory/switch2)"
    fi
    if grep -q "^launch:" "$S2" 2>/dev/null; then
        echo "    FAILED: Nintendo Switch 2 has a launch line:"
        grep "^launch:" "$S2" | sed 's/^/        /'
    else
        echo "    OK: no launch command for Nintendo Switch 2"
    fi
    SW="$CFG/gamedirectory/switch/metadata.pegasus.txt"
    if grep -q "Super Mario Odyssey.nsp" "$SW" 2>/dev/null && ! grep -q "Stray Package.nup" "$SW"; then
        echo "    OK: Nintendo Switch takes its .nsp, not the .nup"
    else
        echo "    FAILED: Nintendo Switch's list (gamedirectory/switch) isn't .nsp only:"
        grep "^file:" "$SW" 2>/dev/null | sed 's/^/        /'
    fi
    strip_ansi "$R/logs/01-first-start.stdout" | grep -h "Test: ROM folder \`" | sed 's/^/    /'
    echo
    echo "## Art folders aren't read for games (psx/media/, psx/skraper/ have .zip files)"
    if grep -E "^file: .*/MyGames/psx/(media|skraper)/" "$CFG/gamedirectory/psx/metadata.pegasus.txt" >/dev/null 2>&1; then
        echo "    FAILED: a file in psx's art folders is listed as a game:"
        grep -E "^file: .*/MyGames/psx/(media|skraper)/" "$CFG/gamedirectory/psx/metadata.pegasus.txt" | sed 's/^/        /'
    else
        echo "    OK: nothing in psx/media/ or psx/skraper/ is listed as a game"
    fi
    if awk '/^Crash Bandicoot/{f=1;next} /^[^ ]/{f=0} f' "$R/art-per-game.txt" 2>/dev/null | grep -q "media/crash.png"; then
        echo "    OK: Crash Bandicoot still has its picture from psx/media/"
    else
        echo "    FAILED: Crash Bandicoot's psx/media/crash.png isn't in its art"
    fi
    echo
    echo "## A game added in a subfolder of a watched folder (psx/Extra/, after the first start)"
    if grep -q "Ape Escape (USA).chd" "$CFG/gamedirectory/psx/metadata.pegasus.txt" 2>/dev/null; then
        echo "    OK: Ape Escape is in psx's list without a Rescan"
    else
        echo "    FAILED: Ape Escape isn't in gamedirectory/psx/metadata.pegasus.txt"
    fi
    # (checked in the background once the library is up: each start's line)
    for f in "$R"/logs/*.stdout; do
        strip_ansi "$f" | grep -h "Background refresh\|ROM folders: \(new or removed\|Refresh stopped\)\|game lists made again" \
            | sed "s|^|    $(basename "$f" .stdout): |"
    done | head -n 20
    echo
    echo "## ROM folders per system (Settings > Game directory > a system > Folders)"
    strip_ansi "$R/logs/08c-folder-checks.log" 2>/dev/null | grep -h "Test: Folders:" | sed 's/^.*Test: Folders: /    /'
    echo
    echo "## A ROM folder's own metadata file (read at import, then Restore)"
    strip_ansi "$R/logs/08c-folder-checks.log" 2>/dev/null | grep -h "Test: Own metadata:" | sed 's/^.*Test: Own metadata: /    /'
    echo
    echo "## Scrape without a login (Hasheous + libretro)"
    grep -E "libretro|Hasheous|Saving|not found|no source" "$R/scraped-free/scraper.log" 2>/dev/null | head -n 12 | sed 's/^/    /'
    sed 's/^/    file: /' "$R/scraped-free/files.txt" 2>/dev/null | sort -u | head -n 8
    echo
    if [ -d "$R/scraped-launchbox" ]; then
        echo "## Scrape from the LaunchBox database (no login)"
        grep -E "LaunchBox|Downloaded|Saving|not found|no sure|Done:" "$R/scraped-launchbox/scraper.log" 2>/dev/null | head -n 16 | sed 's/^/    /'
        sed 's/^/    file: /' "$R/scraped-launchbox/files.txt" 2>/dev/null | head -n 10
        echo "    ESRB (x-esrb):"
        grep -E "ESRB" "$R/scraped-launchbox/scraper.log" 2>/dev/null | head -n 6 | cut -c1-600 | sed 's/^/        /'
        sed 's/^/        written: /' "$R/scraped-launchbox/esrb-lines.txt" 2>/dev/null
        if grep -q "Super Mario World\|snes" "$R/scraped-launchbox/esrb-lines.txt" 2>/dev/null; then echo "        OK: an x-esrb line written by the scrape"; else echo "        FAILED: no x-esrb line written"; fi
        sed 's/^/        index: /' "$R/scraped-launchbox/esrb-index.txt" 2>/dev/null | cut -c1-400
        echo
        echo "## Nintendo Switch 2 from the LaunchBox database"
        grep "Nintendo Switch 2$" "$R/scraped-launchbox/platforms.txt" 2>/dev/null | sed 's/^/    platform in the download: /'
        grep -i "switch_2\|switch 2" "$R/scraped-launchbox/index.txt" 2>/dev/null | sed 's/^/    index kept: /'
        grep -E "Collection|LaunchBox|no sure|Saving|Done:|no platform|\[test\]" "$R/scraped-launchbox/switch2.log" 2>/dev/null | head -n 12 | sed 's/^/    /'
        sed 's/^/    file: /' "$R/scraped-launchbox/switch2-files.txt" 2>/dev/null | head -n 10
        echo
    fi
    echo "## Android apps to import"
    strip_ansi "$R/logs/01-first-start.stdout" "$R/logs/14-apps-new.stdout" "$R/logs/14-apps-new-library.stdout" \
        "$R/logs/14-apps-first.stdout" "$R/logs/14-apps-picker.stdout" "$R/logs/14-apps-theme.stdout" \
        | grep -h "Android apps to import" | sed 's/^/    /'
    python3 - "$R/files" "$R/apps-library.txt" <<'PY' | sed 's/^/    /'
import json, os, sys
files, dump = sys.argv[1:3]
def load(name):
    try:
        return json.load(open(os.path.join(files, name)))
    except Exception:
        return None
setup = load("apps-after-setup.json")
print(("OK: " if setup == {"apps": []} else "FAILED: ")
      + "a new install (first start, setup to do): no app picked: %r" % (setup,))
new = load("apps-new-install.json")
print(("OK: " if new == {"apps": []} else "FAILED: ")
      + "a new install with apps (setup to do), the picker opened: none picked: %r" % (new,))
first = load("apps-after-first-start.json")
want = ["com.test.drift", "com.test.notes", "com.test.retroarch"]
print(("OK: " if first and first.get("apps") == want else "FAILED: ")
      + "first start with the picker: the apps in the library kept as picked (hidden Retro Emu too): %r" % (first,))
after = load("apps-after-picker.json")
want = ["com.test.camera", "com.test.drift", "com.test.retroarch"]
print(("OK: " if after and after.get("apps") == want else "FAILED: ")
      + "after the picker: Camera ticked, Notes unticked, Drift Racer off and on again: %r" % (after,))
kinds = load("android-kinds-after-picker.json") or {}
print(("OK: " if kinds.get("android:com.test.camera") == "Emulator" else "FAILED: ")
      + "Camera's section (X): %r" % (kinds.get("android:com.test.camera"),))
print(("OK: " if kinds.get("android:com.test.drift") == "Game" and kinds.get("android:com.test.retroarch") == "Emulator" else "FAILED: ")
      + "the picked apps keep their sections: Drift Racer %r, Retro Emu %r"
      % (kinds.get("android:com.test.drift"), kinds.get("android:com.test.retroarch")))
titles = []
try:
    titles = [l for l in open(dump, encoding="utf-8").read().split("\n") if l and not l.startswith(" ")]
except Exception:
    pass
for title, inside in (("Drift Racer", True), ("Notes", False), ("Retro Emu", False)):
    print(("OK: " if (title in titles) == inside else "FAILED: ")
          + "%s %s the library" % (title, "in" if inside else "not in"))
PY
    echo "    (Library > Android, the hidden Retro Emu under Emulators: screenshots 14-library-android*;"
    echo "     a new install: 14-new-picker, 14-new-library-options; the round prompts: 14-picker*)"
    echo
    echo "## Backup & reset (throwaway fixture folders)"
    strip_ansi "$R/logs/15-backup.stdout" "$R/logs/15-start-over.stdout" | grep -h "Backup & reset\|Backup: \|Start over: " | sed 's/^/    /' | head -n 60
    if [ -s "$R/files/backup.tar" ]; then
        echo "    OK: a backup file was saved in Download: $(cat "$R/files/backups.txt" | tr '\n' ' ')"
        if grep -a -q -E "PLANTEDPASS|PLANTEDKEY|PLANTEDTOKEN|ssPassword" "$R/files/backup.tar"; then
            echo "    FAILED: a planted password, key or token is in the backup"
        else
            echo "    OK: no planted password, key or token in the backup (ssUser kept: $(grep -a -c 'ssUser=testuser' "$R/files/backup.tar"))"
        fi
        for out in "downloaded_media" "themes/FakeXBSX" "lastrun.log" "launchbox/" "/backup/"; do
            if grep -q "$out" "$R/files/backup-contents.txt"; then echo "    FAILED: the backup has $out"; else echo "    OK: no $out in the backup"; fi
        done
        echo "    backup contents (tar -tf):"
        sed 's/^/        /' "$R/files/backup-contents.txt" | head -n 40
    else
        echo "    FAILED: no backup file in Download"
    fi
    if diff -q "$R/files/before-setup-again.txt" "$R/files/after-setup-again.txt" >/dev/null 2>&1; then
        echo "    OK: Run setup again changed no file (setup progress marker aside)"
    else
        echo "    FAILED: Run setup again changed files:"
        diff "$R/files/before-setup-again.txt" "$R/files/after-setup-again.txt" | head -n 10 | sed 's/^/        /'
    fi
    if diff -q "$R/files/kept-before.txt" "$R/files/kept-after.txt" >/dev/null 2>&1 && [ -s "$R/files/kept-after.txt" ]; then
        echo "    OK: Start over kept the ROM folders, ES-DE, the theme and the file outside ($(wc -l < "$R/files/kept-after.txt") files, same checksums)"
    else
        echo "    FAILED: Start over changed kept files:"
        diff "$R/files/kept-before.txt" "$R/files/kept-after.txt" | head -n 10 | sed 's/^/        /'
    fi
    for list in after-start-over-px-files after-start-over-files; do
        where="the App Data Folder (PXFolder)"; [ "$list" = after-start-over-files ] && where="the app's own folder (older copy)"
        for gone in "pegasusx.ini" "settings.txt" "game_dirs.txt" "gamedirectory/" "downloaded_media/" "metadata/" "pegasusx-hidden.json" \
                    "theme_settings/" "settings.txt.pxgood" "pegasusx.ini.pxgood" "game_dirs.txt.pxgood" "damaged/" "pegasusx-root-asked"; do
            if grep -q "^$gone" "$R/files/$list.txt"; then echo "    FAILED: $gone still in $where after Start over"; else echo "    OK: $gone deleted from $where"; fi
        done
    done
    if grep -q "^pegasusx-root.txt$" "$R/files/after-start-over-files.txt"; then echo "    OK: pegasusx-root.txt kept (the App Data Folder stays picked)"; else echo "    FAILED: pegasusx-root.txt gone"; fi
    if grep -q "pegasusx-setup-done" "$R/files/after-start-over-appdata.txt"; then echo "    FAILED: the setup-done marker is still there"; else echo "    OK: no setup-done marker after Start over"; fi
    echo "    after Start over Pegasus X $(cat "$R/files/start-over-quit.txt" 2>/dev/null) ($(strip_ansi "$R/logs/15-start-over.stdout" | grep -c "closing to start again") \"closing to start again\" lines)"
    echo "    files after Start over: PXFolder:"
    sed 's/^/        /' "$R/files/after-start-over-px-files.txt" | head -n 20
    echo "    the app's own folder:"
    sed 's/^/        /' "$R/files/after-start-over-files.txt" | head -n 20
    if [ -s "$R/files/backups-after-start-over.txt" ]; then echo "    OK: the backup in Download is still there"; else echo "    FAILED: the backup in Download is gone"; fi
    echo "    (screenshots: 15-backup-*, 15-setup-again, 15-resume-*, 15-wipe-*, 15-start-over-restarting, 15-after-start-over)"
    echo
    echo "## Setup (it stays up to step 4, never comes back once done, no theme while it's open)"
    for n in 01-first-start 01-restart-1 01-restart-2 01-restart-3 15-backup 15-resume 15-after-start-over; do
        echo "    $n:"
        strip_ansi "$R/logs/$n.stdout" | grep -E "Setup: |Theme: loaded|Pegasus X folder: |Game directory: Pick my ROMs" | sed 's/^\[i\] //; s/^qml: //' | head -n 16 | sed 's/^/        /'
    done
    first="$(strip_ansi "$R/logs/01-first-start.stdout" | grep -n -E "Setup: (finished|skipped)|Theme: loaded" | head -n 1)"
    case "$first" in
        *"Setup: finished"*|*"Setup: skipped"*) echo "    OK: first start: no theme before setup finished (first of the two: ${first#*:})" ;;
        *) echo "    FAILED: first start: the theme loaded before setup finished (${first#*:})" ;;
    esac
    seq="$(strip_ansi "$R/logs/01-first-start.stdout" | grep -o -E "Setup: (on screen|off screen|finished|skipped)|Theme: loaded" | tr '\n' ',')"
    echo "    first start, in order: $seq"
    case "$seq" in
        "Setup: on screen,Setup: finished,Setup: off screen,Theme: loaded"*) echo "    OK: first start: setup on screen until it finished, then the theme" ;;
        *) echo "    FAILED: first start: setup left the screen before it finished, or a theme came first" ;;
    esac
    for n in 1 2 3; do
        if strip_ansi "$R/logs/01-restart-$n.stdout" | grep -q "Setup: open"; then echo "    FAILED: restart $n opened setup"
        elif strip_ansi "$R/logs/01-restart-$n.stdout" | grep -q "Setup: done"; then echo "    OK: restart $n: setup done, not shown"
        else echo "    FAILED: restart $n: no \"Setup: done\" line"; fi
    done
    if strip_ansi "$R/logs/15-resume.stdout" | grep -q "goes on at step 2" && strip_ansi "$R/logs/15-resume.stdout" | grep -q "Setup: on screen"; then echo "    OK: closed mid-setup (step 2): the next start went on at step 2"; else echo "    FAILED: closed mid-setup: not at step 2"; fi
    if strip_ansi "$R/logs/15-after-start-over.stdout" | grep -q "Theme: loaded"; then echo "    FAILED: after Start over a theme loaded with setup open"; else echo "    OK: after Start over: setup, no theme"; fi
    echo "    (screenshots: 01-wizard-*, 01-theme, 01-restart-1, 15-setup-again, 15-resume-*, 15-after-start-over)"
    echo
    echo "## Pick my ROMs folder (08d, 08e; in setup: 01-wizard-2-*)"
    strip_ansi "$R/logs/08d-folder-first.stdout" "$R/logs/08e-folder-first-empty.stdout" "$R/logs/08-gamedirs.stdout" | grep -h "Pick my ROMs folder" | sed 's/^qml: //; s/^/    /'
    if diff -q "$R/files/folderfirst-before.txt" "$R/files/folderfirst-after.txt" >/dev/null 2>&1; then
        echo "    OK: Cancel added nothing (the game directory the same, $(wc -l < "$R/files/folderfirst-after.txt") files)"
    else
        echo "    FAILED: Cancel changed the game directory:"
        diff "$R/files/folderfirst-before.txt" "$R/files/folderfirst-after.txt" | head -n 10 | sed 's/^/        /'
    fi
    echo "    (screenshots: 08d-folder-first-*, 08e-folder-first-nothing, 08e-tick-instead)"
    echo
    echo "## Gamepad layout: each button's highlight on the picture (18, in qmlscene)"
    python3 - "$R" <<'PY' | sed 's/^/    /'
import re, sys, os
R = sys.argv[1]
text = open(R + "/logs/18-gamepad-points.stdout", errors="replace").read()
if "no qmlscene" in text:
    print("NOT RUN: no qmlscene in the test environment")
    sys.exit(0)
# where each button is on the 839 x 561 picture (pixels, measured on it), and how far
# a highlight may be from it (half the button, about)
want = { "a": (632.5, 227, 18), "b": (690.5, 170, 18), "x": (574.5, 170.5, 18), "y": (632.5, 114.5, 18),
         "dpup": (313, 265, 14), "dpdown": (313, 343, 14), "dpleft": (274, 304, 14), "dpright": (351, 304, 14),
         "lsx": (209, 170, 30), "lsy": (209, 170, 30), "lsb": (209, 170, 30),
         "rsx": (527, 297, 30), "rsy": (527, 297, 30), "rsb": (527, 297, 30),
         "select": (360, 170.5, 10), "start": (480, 170.5, 10), "guide": (420.5, 77, 20),
         "lb": (232, 22, 14), "rb": (607, 22, 14), "lt": (160, 42, 14), "rt": (679, 42, 14) }
try:
    from PIL import Image
except ImportError:
    Image = None
rows = re.findall(r"POINT (gamepad-(\d+)-(\w+)) ([-\d. ]+)", text)
if not rows:
    print("FAILED: no highlight positions written:")
    print(text[-800:])
    sys.exit(0)
bad = 0
for name, screen, field, nums in rows:
    x, y, w, h, pw, ph, cx, cy = [float(v) for v in nums.split()]
    # the picture's pixel under the highlight's middle
    left = (w - pw) / 2
    top = (h - ph) / 2
    px = (cx - left) / pw * 839
    py = (cy - top) / ph * 561
    wx, wy, tol = want[field]
    d = ((px - wx) ** 2 + (py - wy) ** 2) ** 0.5
    shape = abs(pw / ph - 839 / 561) < 0.01
    found = ""
    if Image and os.path.exists("%s/gamepad/%s.png" % (R, name)):
        im = Image.open("%s/gamepad/%s.png" % (R, name)).convert("RGB")
        xs = ys = n = 0
        # (around where it should be)
        bx = int(x + left + wx / 839 * pw)
        by = int(y + top + wy / 561 * ph)
        for yy in range(max(0, by - 40), min(im.height, by + 41)):
            for xx in range(max(0, bx - 40), min(im.width, bx + 41)):
                r, g, b = im.getpixel((xx, yy))
                if r < 90 and g > 180 and b > 180:   # the highlight, #3cc
                    xs += xx; ys += yy; n += 1
        if n:
            dx, dy = xs / n - x, ys / n - y   # in the picture's item
            found = " · drawn at %.0f, %.0f" % ((dx - left) / pw * 839, (dy - top) / ph * 561)
            d = max(d, (((dx - left) / pw * 839 - wx) ** 2 + ((dy - top) / ph * 561 - wy) ** 2) ** 0.5)
    ok = d <= tol and shape
    bad += 0 if ok else 1
    print("%s: %s screen %s: picture %.0f x %.0f, highlight on %.0f, %.0f (button at %g, %g; %.1f px off)%s"
          % ("OK" if ok else "FAILED", field, screen, pw, ph, px, py, wx, wy, d, found))
print("%s: %d highlights at %d sizes, %d off their button or the picture out of shape"
      % ("OK" if bad == 0 else "FAILED", len(rows), len(set(r[1] for r in rows)), bad))
PY
    echo "    (pictures: gamepad/gamepad-<screen width>-<button>.png)"
    echo
    echo "## Library > Metadata (17-metadata*)"
    for n in 17-metadata 17-metadata-restart 17-metadata-recovery; do
        echo "    $n:"
        strip_ansi "$R/logs/$n.stdout" | grep -E "(Genres|Publishers|Developers): (suggested merges|[0-9]+ values|merge |undo )|Metadata: |Recovery: .aliases" \
            | sed 's/^\[[iw]\] //; s/^qml: //' | cut -c1-400 | sed 's/^/        /'
    done
    if diff -q "$R/files/meta-originals-before.txt" "$R/files/meta-originals-after-merge.txt" >/dev/null 2>&1; then
        echo "    OK: merging changed no game file ($(wc -l < "$R/files/meta-originals-after-merge.txt") metadata files, gamelists and game directory files, same checksums)"
    else
        echo "    FAILED: a game file changed while merging:"
        diff "$R/files/meta-originals-before.txt" "$R/files/meta-originals-after-merge.txt" | head -n 10 | sed 's/^/        /'
    fi
    if diff -q "$R/files/meta-originals-rescraped.txt" "$R/files/meta-originals-after-undo.txt" >/dev/null 2>&1; then
        echo "    OK: the restart and the undo changed no game file"
    else
        echo "    FAILED: a game file changed after the restart or the undo:"
        diff "$R/files/meta-originals-rescraped.txt" "$R/files/meta-originals-after-undo.txt" | head -n 10 | sed 's/^/        /'
    fi
    python3 - "$R" <<'PY' | sed 's/^/    /'
import json, re, sys
R = sys.argv[1]
def log(name):
    try:
        return re.sub(r"\x1b\[[0-9;]*m", "", open("%s/logs/%s.stdout" % (R, name), encoding="utf-8", errors="replace").read())
    except OSError:
        return ""
def values(text, tab):
    found = re.findall(r"%s: \d+ values: (.*)" % tab, text)
    return found
first = log("17-metadata")
sug = re.findall(r"Developers: suggested merges \([0-9]+ ms\): (.*)", first)
bad = [p for line in sug for p in line.split(" | ") if "Sega AM2" in p and re.search(r"(^|: | \+ )Sega( \+|$)", p)]
print(("FAILED: Sega AM2 suggested with Sega: %s" % bad) if bad else "OK: Sega AM2 never suggested with Sega (%s)" % (sug[-1] if sug else "no developer suggestions logged"))
pub = re.findall(r"Publishers: suggested merges \([0-9]+ ms\): (.*)", first)
print(("OK: " if pub and "Nintendo" in pub[0] and "Nintendo of America" in pub[0] else "FAILED: ") + "Nintendo's spellings suggested: %s" % (pub[0] if pub else "none logged"))
merged = json.load(open(R + "/files/aliases-after-merge.json")) if __import__("os").path.exists(R + "/files/aliases-after-merge.json") else {}
p = merged.get("publisher", {})
ok = p.get("Nintendo of America") == "Nintendo" and p.get("Nintendo Co., Ltd.") == "Nintendo"
print(("OK: " if ok else "FAILED: ") + "aliases.json after the merges: %s" % json.dumps(p, ensure_ascii=False))
restart = log("17-metadata-restart")
vals = values(restart, "Publishers")
if vals:
    v = vals[0]
    ok = re.search(r"(^|, )Nintendo \(\d+\)", v) and "Nintendo of America (" not in v and "Nintendo Co., Ltd. (" not in v
    print(("OK: " if ok else "FAILED: ") + "after a restart and the rescrape, publishers: %s" % v)
    m = re.search(r"(^|, )Nintendo \((\d+)\)", v)
    print("    Nintendo's games after the rescrape: %s (Meta Hotel's \"Nintendo Co., Ltd.\" folded in)" % (m.group(2) if m else "?"))
if len(vals) > 1:
    v = vals[-1]
    print(("OK: " if "Nintendo of America (" in v and "Nintendo Co., Ltd. (" in v else "FAILED: ") + "after the undo, publishers: %s" % v)
try:
    u = json.load(open(R + "/files/aliases-after-undo.json"))
    print(("OK: " if not u.get("publisher") else "FAILED: ") + "aliases.json after the undo: %s" % json.dumps(u, ensure_ascii=False))
except Exception as e:
    print("FAILED: aliases.json after the undo: %s" % e)
try:
    r = json.load(open(R + "/files/aliases-after-recovery.json"))
    print("OK: aliases.json junked, then put back by recovery: %s" % json.dumps(r, ensure_ascii=False))
except Exception as e:
    print("FAILED: aliases.json after recovery: %s" % e)
PY
    echo "    the damaged folder: $(tr '\n' ' ' < "$R/files/meta-damaged.txt")"
    echo "    (screenshots: 17-*)"
    echo
    echo "## Damaged-file recovery (16-recovery*)"
    echo "    last good copies before the damage: $(tr '\n' ' ' < "$R/files/good-copies.txt")"
    echo "    Recovery: lines, first start (settings.txt junk, $(cat "$R/files/half-cut.txt") cut in half, game_dirs.txt only as .pxold):"
    strip_ansi "$R/logs/16-recovery.stdout" | grep -h "Recovery:" | sed 's/^/        /' | head -n 40
    HALF="$(cat "$R/files/half-cut.txt")"
    for f in settings.txt "$HALF"; do
        if [ -s "$R/files/good-$f.md5" ] && cmp -s "$R/files/good-$f.md5" "$R/files/after-$f.md5"; then
            echo "    OK: $f is its last good copy again (same checksum)"
        else
            echo "    FAILED: $f isn't its last good copy (good $(cat "$R/files/good-$f.md5" 2>/dev/null), now $(cat "$R/files/after-$f.md5" 2>/dev/null))"
        fi
    done
    if [ -s "$R/files/orig-game_dirs.txt.md5" ] && cmp -s "$R/files/orig-game_dirs.txt.md5" "$R/files/after-game_dirs.txt.md5"; then
        echo "    OK: game_dirs.txt put back from game_dirs.txt.pxold (same checksum as before)"
    else
        echo "    FAILED: game_dirs.txt wasn't put back as it was"
    fi
    if [ -s "$R/files/leftovers-1.txt" ]; then echo "    FAILED: leftovers after the start: $(tr '\n' ' ' < "$R/files/leftovers-1.txt")"; else echo "    OK: no .pxsave / .pxold left"; fi
    echo "    the damaged folder after it:"
    sed 's/^/        /' "$R/files/damaged-1.txt"
    echo "    Recovery: lines, second start (pegasusx.ini all zero bytes; X Restore from a backup):"
    strip_ansi "$R/logs/16-recovery-restore.stdout" | grep -h "Recovery:" | sed 's/^/        /' | head -n 20
    if [ -s "$R/files/good-pegasusx.ini.md5" ] && cmp -s "$R/files/good-pegasusx.ini.md5" "$R/files/after-pegasusx.ini.md5"; then
        echo "    OK: pegasusx.ini is its last good copy again"
    else
        echo "    note: pegasusx.ini differs from its good copy before the damage (the first start may have saved it, making a new one)"
    fi
    echo "    the damaged folder after it:"
    sed 's/^/        /' "$R/files/damaged-2.txt"
    n="$(strip_ansi "$R/logs/16-recovery-none.stdout" | grep -c "Recovery:.*\(damaged:\|put back\|moved to\|defaults\|missing, \)")"
    echo "    third start (nothing damaged): $n fixing Recovery: lines (expected 0, and no notice: 16-recovery-none)"
    echo "    (screenshots: 16-recovery-notice, -details, -after-keep, -notice-2, -restore-picker, -none)"
    echo
    echo "## Settings > Advanced: Missing games (verify-files / show-missing-games)"
    sed 's/^/    /' "$R/files/missing-games.txt" 2>/dev/null
    echo "    (what Advanced showed for each: screenshots 02-missing-*)"
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
    echo "## The game directory (and backups)"
    find "$CFG/gamedirectory" "$CFG/backup" "$CFG/imported" "$CFG/metadata" -type f 2>/dev/null | sed "s|$CFG/|    |"
    for f in "$CFG"/gamedirectory/*/metadata.pegasus.txt; do
        [ -f "$f" ] || continue
        echo
        echo "### $(dirname "$f" | xargs basename)/$(basename "$f")"
        sed 's/^/    /' "$f" | head -n 50
    done
} > "$R/summary.md"

echo "done"
