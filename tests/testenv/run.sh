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

# -- 1. first start: the ES-DE import and the ROM folder import, then the theme --
start 01-first-start "" 15 PEGASUSX_TEST_ESDE_IMPORT=1 "PEGASUSX_TEST_ROM_FOLDERS=$T/MyGames"
# the first-start setup: through its four steps (keeping the folder, skipping the rest)
shot 01-wizard-1
keys Return
sleep 1
shot 01-wizard-2
keys Down Return
sleep 1
shot 01-wizard-3
keys Down Return
sleep 3
shot 01-wizard-4
keys Down Return
sleep 2
shot 01-theme
stop 01-first-start
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
# Genres (the row under Needs your check). The fixture's genres, A to Z: Action (1 game),
# Platform (Platform, platform) and Shoot'em Up (Shoot'em Up, Shoot’em Up), under Search
# genres..., Only genres with 1 game and Sort (Search selected first). A genre's page:
# Change this genre... (selected), its two spellings side by side, then its games
keys Down Return
sleep 1
shot 03-genres
keys Down Down Down Down Return
shot 03-genre-games
keys Down Down Down Return
sleep 1
shot 03-genre-game
keys Escape
shot 03-genre-games-back
keys Escape Down Return
shot 03-genre-spellings
# Search genres...: "shoot" narrows the list as it's typed; then cleared
keys Escape q Return
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
# Add a ROM folder: the picker opens on the drives; LB (q) goes to the last folder browsed, ROMs
# (gba, snes in it); "Use this folder" is below its two entries
keys Return q Down Down Down Return
sleep 1
shot 08-gamedirs-picked
# untick one of the systems found
keys Down Return
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
# -- 8b. a system's ROM folders: the Folders view, adding the SD card's folder, removing it ------
SD="$T/sdcard"
sed -i "/^\[browse\]/,/^\[/ s|^last=.*|last=$SD/ROMs|" "$CFG/pegasusx.ini"
start 08b-folders "menu/settings/GameDirectory.qml" 5 "PEGASUSX_TEST_SD=$SD"
# the systems are A to Z (GameCube, My SNES picks, Nintendo 64, Nintendo Game Boy Advance,
# Nintendo SNES): LB to the top, down to SNES, A: its menu, A: Folders
keys q Down Down Down Down Down Down Down Return
shot 08b-system-menu
keys Return
sleep 1
shot 08b-folders
# Add a folder...: the picker (on the drives), LB to the last folder browsed (the SD card's
# ROMs); "Use this folder" there reads only its snes folder
keys Down Return
sleep 1
keys q
sleep 1
shot 08b-folders-picker
keys Down Return
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
start 08c-folder-checks "menu/settings/GameDirectory.qml" 3 "PEGASUSX_TEST_SD=$SD" "PEGASUSX_TEST_FOLDERS=$SD"
wait_log "Test: Folders: done" 60 "$R/logs/08c-folder-checks.stdout"
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
# a new install with apps (setup still to do: the test environment never runs setup, so
# pegasusx-root-asked isn't there): the picker opens with every app unticked; with none
# picked there's no Android collection, and Library > Options still has the picker
rm -f "$CFG/pegasusx-android-apps.json" "$CFG/pegasusx-root-asked" "$CFG/pegasusx-wizard"
find "$HOME" -name "gameindex-v1.json" -delete 2>/dev/null
start 14-apps-new "menu/settings/AndroidAppPicker.qml" 8 "PEGASUSX_TEST_APPS=$APPS"
shot 14-new-picker
stop 14-apps-new
cp "$CFG/pegasusx-android-apps.json" "$R/files/apps-new-install.json" 2>/dev/null
start 14-apps-new-library "menu/settings/MetadataEditor.qml" 6 "PEGASUSX_TEST_APPS=$APPS"
keys e
shot 14-new-library-options
stop 14-apps-new-library
# a library from an older build: setup done (as on a device after setup), no picks saved
rm -f "$CFG/pegasusx-android-apps.json"
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
keys q Return
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
keys Down Down Return
wait_log "Backup & reset: backup" 60 "$R/logs/15-backup.stdout"
sleep 1
shot 15-backup-done
# Run setup again: setup opens, nothing changes
find "$CFG" -type f -not -name "lastrun.log" -not -name "pegasusx-wizard" -exec md5sum {} + 2>/dev/null | sort > "$R/files/before-setup-again.txt"
keys Escape Down Down Return
sleep 3
shot 15-setup-again
find "$CFG" -type f -not -name "lastrun.log" -not -name "pegasusx-wizard" -exec md5sum {} + 2>/dev/null | sort > "$R/files/after-setup-again.txt"
stop 15-backup
ls "$T/storage/Download" > "$R/files/backups.txt" 2>/dev/null
BK="$(ls "$T/storage/Download"/PegasusX-backup-*.tar 2>/dev/null | head -n 1)"
[ -n "$BK" ] && tar -tf "$BK" > "$R/files/backup-contents.txt" 2>&1
[ -n "$BK" ] && cp "$BK" "$R/files/backup.tar"
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
sleep 8
shot 15-after-start-over
stop 15-start-over
find "$CFG" -type f -not -name "lastrun.log" | sed "s|$CFG/||" | sort > "$R/files/after-start-over-files.txt"
find "$T/MyGames" "$T/storage/ES-DE" "$T/storage/ROMs" "$CFG/themes" "$T/outside.txt" -type f -exec md5sum {} + 2>/dev/null | sort > "$R/files/kept-after.txt"
ls "$T/storage/Download" > "$R/files/backups-after-start-over.txt" 2>/dev/null
# the Pegasus X folder as it was (for the summary below)
rm -rf "$CFG"
mv "$T/cfg-saved" "$CFG"

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
    echo "## Scrape without a login (Hasheous + libretro)"
    grep -E "libretro|Hasheous|Saving|not found|no source" "$R/scraped-free/scraper.log" 2>/dev/null | head -n 12 | sed 's/^/    /'
    sed 's/^/    file: /' "$R/scraped-free/files.txt" 2>/dev/null | sort -u | head -n 8
    echo
    if [ -d "$R/scraped-launchbox" ]; then
        echo "## Scrape from the LaunchBox database (no login)"
        grep -E "LaunchBox|Downloaded|Saving|not found|no sure|Done:" "$R/scraped-launchbox/scraper.log" 2>/dev/null | head -n 16 | sed 's/^/    /'
        sed 's/^/    file: /' "$R/scraped-launchbox/files.txt" 2>/dev/null | head -n 10
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
    for gone in "pegasusx.ini" "settings.txt" "game_dirs.txt" "gamedirectory/" "downloaded_media/" "metadata/" "pegasusx-hidden.json"; do
        if grep -q "^$gone" "$R/files/after-start-over-files.txt"; then echo "    note: $gone is there after Start over (written again since?)"; else echo "    OK: $gone deleted by Start over"; fi
    done
    echo "    files in the Pegasus X folder after Start over (and the session after it):"
    sed 's/^/        /' "$R/files/after-start-over-files.txt" | head -n 30
    if [ -s "$R/files/backups-after-start-over.txt" ]; then echo "    OK: the backup in Download is still there"; else echo "    FAILED: the backup in Download is gone"; fi
    echo "    (screenshots: 15-backup-*, 15-setup-again, 15-wipe-*, 15-after-start-over)"
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
