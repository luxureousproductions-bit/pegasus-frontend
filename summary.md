# Test environment results

## Imports (first start)
    2026-11-28T05:31:38 [i] Test: ES-DE import: 2 systems, 4 games, error ``
    2026-11-28T05:31:38 [i] Test: ROM folder `/home/runner/work/_temp/px/MyGames`: systems n64=2, switch=1, switch2=1, psx=6
    2026-11-28T05:31:38 [i] Test: ROM folder import: 4 systems, 6 games, error ``

## Your own file wins over an import
    OK: Chrono Trigger has the title from your own file
    OK: it has ES-DE's screenshot too (import data on top of your file)

## Library > Genres
    OK: the old genres/hidden entry is gone after the first start
    [i] Genres: the hidden genres of an older build were removed (no genre is hidden now)
    [i] qml: Genres: "Platform" -> "Platformer": 2 of 2 games saved
    [i] qml: Genres: "Platform" counts 2 spellings as one: "Platform" (1): U+0050 U+006C U+0061 U+0074 U+0066 U+006F U+0072 U+006D | "platform" (1): U+0070 U+006C U+0061 U+0074 U+0066 U+006F U+0072 U+006D
    [i] qml: Genres: "Shoot'em Up" -> "Shoot'em Up": 1 of 1 games saved
    [i] qml: Genres: "Shoot'em Up" counts 2 spellings as one: "Shoot'em Up" (1): U+0053 U+0068 U+006F U+006F U+0074 U+0027 U+0065 U+006D U+0020 U+0055 U+0070 | "Shoot’em Up" (1): U+0053 U+0068 U+006F U+006F U+0074 U+2019 U+0065 U+006D U+0020 U+0055 U+0070
    [i] qml: Genres: 3 values: Action (1), Platform (2), Shoot'em Up (2)
    [i] qml: Genres: 3 values: Action (1), Platformer (2), Shoot'em Up (2)
    [i] qml: Genres: search "", only 1 game false, sort fewest: Action (1), Platform (2), Shoot'em Up (2)
    [i] qml: Genres: search "", only 1 game false, sort most: Platform (2), Shoot'em Up (2), Action (1)
    [i] qml: Genres: search "", only 1 game true, sort az: Action (1)
    [i] qml: Genres: search "s", only 1 game false, sort az: Shoot'em Up (2)
    [i] qml: Genres: search "sh", only 1 game false, sort az: Shoot'em Up (2)
    [i] qml: Genres: search "sho", only 1 game false, sort az: Shoot'em Up (2)
    [i] qml: Genres: search "shoo", only 1 game false, sort az: Shoot'em Up (2)
    [i] qml: Genres: search "shoot", only 1 game false, sort az: Shoot'em Up (2)
    [i] qml: Genres: suggested merges (0 ms): none

### Change this genre... (Platform -> Platformer; Shoot’em Up -> Shoot'em Up)
    OK: Super Mario World (USA).sfc: genre 'Platformer' in the game directory (wanted 'Platformer')
    OK: Crash Bandicoot (USA).chd: genre 'Platformer' in the game directory (wanted 'Platformer')
    OK: Wind Waker (USA).iso: genre "Shoot'em Up" in the game directory (wanted "Shoot'em Up")
    OK: Metroid Prime (USA).iso (already Shoot'em Up): "Shoot'em Up" in the game directory
    OK: psx's own gamelist.xml keeps <genre>platform</genre>
    OK: Pegasus/gamecube/metadata.pegasus.txt keeps both its spellings
    backups made before the changes (reason edit): 3
    [i] Metadata: 1 games saved in `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/psx/metadata.pegasus.txt`
    [i] Metadata: 1 games saved in `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/snes/metadata.pegasus.txt`
    [i] qml: Genres: "Platform" -> "Platformer": 2 of 2 games saved
    [i] Metadata: 1 games saved in `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/gc/metadata.pegasus.txt`
    [i] qml: Genres: "Shoot'em Up" -> "Shoot'em Up": 1 of 1 games saved

## Nintendo Switch 2 (.nup)
    OK: MyGames/switch2 is Nintendo Switch 2, with its .nup game
    OK: no launch command for Nintendo Switch 2
    OK: Nintendo Switch takes its .nsp, not the .nup
    [i] Test: ROM folder `/home/runner/work/_temp/px/MyGames`: systems n64=2, switch=1, switch2=1, psx=6

## Art folders aren't read for games (psx/media/, psx/skraper/ have .zip files)
    OK: nothing in psx/media/ or psx/skraper/ is listed as a game
    OK: Crash Bandicoot still has its picture from psx/media/

## A game added in a subfolder of a watched folder (psx/Extra/, after the first start)
    OK: Ape Escape is in psx's list without a Rescan
    01-restart-1: [i] ROM folders: Background refresh: 0 lists made again in 0 ms
    01-restart-2: [i] ROM folders: Background refresh: 0 lists made again in 0 ms
    01-restart-3: [i] ROM folders: Background refresh: 0 lists made again in 0 ms
    02-missing-dontcheck-show: [i] ROM folders: Background refresh: 0 lists made again in 1 ms
    02-missing-dontcheck: [i] ROM folders: Background refresh: 0 lists made again in 0 ms
    02-missing-hide: [i] ROM folders: Background refresh: 0 lists made again in 1 ms
    02-missing-pick: [i] ROM folders: Background refresh: 0 lists made again in 1 ms
    02-missing-show: [i] ROM folders: Background refresh: 0 lists made again in 1 ms
    02-settings: [i] ROM folders: 1 game lists made again (new or removed games)
    02-settings: [i] ROM folders: Background refresh: 1 lists made again in 4 ms
    02-settings: [i] qml: ROM folders: new or removed games, read when the theme is next left and returned to
    03-library: [i] ROM folders: Background refresh: 0 lists made again in 0 ms
    04-scraper: [i] ROM folders: Background refresh: 0 lists made again in 0 ms
    04b-scraper-section: [i] ROM folders: Background refresh: 0 lists made again in 0 ms
    05-scraper-accounts: [i] ROM folders: Background refresh: 0 lists made again in 0 ms
    06-themes: [i] ROM folders: Background refresh: 0 lists made again in 1 ms
    07-updater: [i] ROM folders: Background refresh: 0 lists made again in 0 ms
    08-gamedirs: [i] ROM folders: Background refresh: 0 lists made again in 1 ms
    08b-folders: [i] ROM folders: Background refresh: 0 lists made again in 0 ms
    08c-folder-checks: [i] ROM folders: 6 game lists made again (new or removed games)

## ROM folders per system (Settings > Game directory > a system > Folders)
    OK: the SNES system (made from ES-DE) is there
    OK: refused the same folder with a trailing slash: "/home/runner/work/_temp/px/storage/ROMs/snes is already one of Nintendo SNES (Super Nintendo)'s folders."
    OK: refused the same folder written another way: "/home/runner/work/_temp/px/storage/ROMs/snes is already one of Nintendo SNES (Super Nintendo)'s folders."
    OK: refused a folder inside a listed one: "That folder is inside /home/runner/work/_temp/px/storage/ROMs/snes (a folder of Nintendo SNES (Super Nintendo)), which is read already."
    OK: refused a folder with a listed one inside: "That folder has /home/runner/work/_temp/px/storage/ROMs/gba (a folder of Nintendo Game Boy Advance) inside it. Pick the folder next to it, not around it."
    OK: refused another system's folder: "That's the Nintendo Game Boy Advance folder. Pick Nintendo SNES (Super Nintendo)'s own folder."
    OK: refused a folder with only other systems' folders: "That folder has other systems' folders in it (Nintendo Game Boy Advance, Sony PlayStation), none for Nintendo SNES (Super Nintendo). Pick Nintendo SNES (Super Nintendo)'s own folder."
    OK: nothing written by the refusals
    OK: the SD card's folder can be added as snes
    OK: added: 2 games read, error ``
    OK: the SD card's folder is listed
    OK: its game is in
    OK: every entry there before is as it was (details, art, emulators, launch)
    OK: the same file name in both folders: two entries
    OK: Folders view: [storage/ROMs/snes internal 3 games] [sdcard/ROMs/snes sd 2 games]
    OK: a folder with snes and gba folders in it: reads /home/runner/work/_temp/px/sdcard/Mixed/snes
    OK: only its SNES games came in
    OK: the GBA system is as it was
    OK: SD card out, rescan: its games and folder kept, still watched
    OK: SD card out: the Folders view says not found, with its games counted
    OK: stop watching a folder: no line left, its games stay
    OK: remove a folder and its games: 2 games out, no line left
    OK: the first folder and its games are as they were
    OK: removed folders are no longer watched
    OK: the GBA system is as it was
    done

## A ROM folder's own metadata file (read at import, then Restore)
    OK: the SNES system is there
    OK: the folder can be added ``
    OK: added: error ``
    OK: the games, with the titles from your file
    OK: its fields (developer, publisher, genre, summary, an x- field)
    OK: its box art, as a full path
    OK: a game's relative launch command, made a full path
    OK: a game's relative workdir, made a full path
    OK: a game's own launch and workdir, as they are
    OK: the system's other games are as they were
    OK: read: title, developer, players, release
    OK: read: box art
    OK: read: x-test-field
    OK: read: Alpha's launch `/home/runner/work/_temp/px/OwnMeta/snes/run-alpha.sh "{file.path}"`, workdir `/home/runner/work/_temp/px/OwnMeta/snes/tools`
    OK: read: Beta's launch `/opt/emu/beta {file.path}`, workdir `/tmp/beta`
    OK: the copy edited
    OK: restored 2 games, error ``
    OK: the edit is replaced by your file's developer
    OK: each game once
    OK: launch and workdir again
    OK: the system's other games are still as they were
    OK: a copy of the file in backup/snes/ first
    OK: a folder that isn't the system's: refused `That folder isn't one of this system's.`, nothing written
    OK: taken out again
    done

## Scrape without a login (Hasheous + libretro)
    file: downloaded_media/snes/miximages/Super Mario World (USA).png

## Scrape from the LaunchBox database (no login)
    [i] qml: [test] LaunchBox database: reading in
    [i] LaunchBox: Nintendo Switch 2: 229 games
    [i] LaunchBox: Nintendo Switch: 7950 games
    [i] LaunchBox: Nintendo 64: 822 games
    [i] LaunchBox: Nintendo GameCube: 815 games
    [i] LaunchBox: Super Nintendo Entertainment System: 2962 games
    [i] LaunchBox: Nintendo Game Boy Advance: 2389 games
    [i] LaunchBox: Sony Playstation: 4653 games
    [i] LaunchBox: ESRB values in these games: "(none)" 4136, "E - Everyone" 4765, "E10+ - Everyone 10+" 1573, "EC - Early Childhood" 46, "M - Mature" 949, "Not Rated" 5676, "RP - Rating Pending" 7, "T - Teen" 2668
    [i] LaunchBox: The database is ready
    [i] qml: [test] LaunchBox database: ready, 19820 games
    [i] Scraper: Scraping 1 games into `/home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media` (source: launchbox, interactive: no, overwrite: yes, ScreenScraper login: missing, LaunchBox database: yes)
    [i] Scraper: LaunchBox: looked up "Super Mario World" (0.1 s)
    [i] Scraper: LaunchBox: using "Super Mario World" (Super Nintendo Entertainment System, id 136)
    [i] Scraper: LaunchBox: ESRB "Not Rated": not one x-esrb takes
    [i] Scraper: LaunchBox: 10 pictures to download (of 73 it lists)
    file: downloaded_media/snes/titlescreens/Super Mario World (USA).png
    file: downloaded_media/snes/physicalmedia/Super Mario World (USA).png
    file: downloaded_media/snes/miximages/Super Mario World (USA).png
    file: downloaded_media/snes/backcovers/Super Mario World (USA).jpg
    file: downloaded_media/snes/marquees/Super Mario World (USA).png
    file: downloaded_media/snes/fanart/Super Mario World (USA).jpg
    file: downloaded_media/snes/3dboxes/Super Mario World (USA).png
    file: downloaded_media/snes/spines/Super Mario World (USA).jpg
    file: downloaded_media/snes/banners/Super Mario World (USA).png
    file: downloaded_media/snes/covers/Super Mario World (USA).png
    ESRB (x-esrb):
        [i] LaunchBox: ESRB values in these games: "(none)" 4136, "E - Everyone" 4765, "E10+ - Everyone 10+" 1573, "EC - Early Childhood" 46, "M - Mature" 949, "Not Rated" 5676, "RP - Rating Pending" 7, "T - Teen" 2668
        [i] Scraper: LaunchBox: ESRB "Not Rated": not one x-esrb takes
        FAILED: no x-esrb line written
        index: nintendo_64.json: "Not Rated" 452, "E - Everyone" 214, "" 72, "T - Teen" 54, "M - Mature" 28, "EC - Early Childhood" 2
        index: nintendo_game_boy_advance.json: "Not Rated" 1095, "E - Everyone" 800, "" 339, "T - Teen" 95, "E10+ - Everyone 10+" 49, "M - Mature" 8, "EC - Early Childhood" 3
        index: nintendo_gamecube.json: "E - Everyone" 288, "T - Teen" 197, "Not Rated" 154, "" 91, "M - Mature" 48, "E10+ - Everyone 10+" 35, "RP - Rating Pending" 1, "EC - Early Childhood" 1
        index: nintendo_switch.json: "E - Everyone" 2373, "T - Teen" 1879, "E10+ - Everyone 10+" 1424, "" 1413, "M - Mature" 732, "Not Rated" 129
        index: nintendo_switch_2.json: "T - Teen" 69, "E - Everyone" 60, "E10+ - Everyone 10+" 49, "M - Mature" 27, "" 13, "Not Rated" 7, "RP - Rating Pending" 4
        index: sony_playstation.json: "" 1842, "Not Rated" 1422, "E - Everyone" 869, "T - Teen" 360, "M - Mature" 102, "EC - Early Childhood" 40, "E10+ - Everyone 10+" 16, "RP - Rating Pending" 2
        index: super_nintendo_entertainment_system.json: "Not Rated" 2417, "" 366, "E - Everyone" 161, "T - Teen" 14, "M - Mature" 4

## Nintendo Switch 2 from the LaunchBox database
    platform in the download:    229  Nintendo Switch 2
    index kept: nintendo_switch_2.json
    [i] qml: [test] LaunchBox database: reading in
    [i] LaunchBox: Nintendo 64: 822 games
    [i] LaunchBox: Nintendo Game Boy Advance: 2389 games
    [i] LaunchBox: Sony Playstation: 4653 games
    [i] LaunchBox: Super Nintendo Entertainment System: 2962 games
    [i] LaunchBox: Nintendo GameCube: 815 games
    [i] LaunchBox: Nintendo Switch 2: 229 games
    [i] LaunchBox: Nintendo Switch: 7950 games
    [i] LaunchBox: ESRB values in these games: "(none)" 4136, "E - Everyone" 4765, "E10+ - Everyone 10+" 1573, "EC - Early Childhood" 46, "M - Mature" 949, "Not Rated" 5676, "RP - Rating Pending" 7, "T - Teen" 2668
    [i] LaunchBox: The database is ready
    [i] qml: [test] LaunchBox database: ready, 19820 games
    [i] Scraper: Scraping 1 games into `/home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media` (source: launchbox, interactive: no, overwrite: yes, ScreenScraper login: missing, LaunchBox database: yes)
    file: downloaded_media/switch2/miximages/Donkey Kong Bananza.png
    file: downloaded_media/switch2/backcovers/Donkey Kong Bananza.jpg
    file: downloaded_media/switch2/marquees/Donkey Kong Bananza.png
    file: downloaded_media/switch2/fanart/Donkey Kong Bananza.jpg
    file: downloaded_media/switch2/spines/Donkey Kong Bananza.jpg
    file: downloaded_media/switch2/covers/Donkey Kong Bananza.jpg
    file: downloaded_media/switch2/screenshots/Donkey Kong Bananza.png

## Android apps to import
    [i] Library: Android apps to import: a new install, no app picked yet
    [i] Library: Android apps to import: a new install, no app picked yet
    [i] Library: Android apps to import: 3 installed apps not picked, left out
    [i] qml: Android apps to import: 4 apps, 0 picked
    [i] Library: Android apps to import: 3 installed apps not picked, left out
    [i] Library: Android apps to import: the 3 apps in your library now (first start with the app picker)
    [i] qml: Android apps to import: 4 apps, 3 picked
    [i] qml: Android apps to import: "Camera" is imported (Application) (4 picked)
    [i] qml: Android apps to import: "Camera" goes into Emulator (4 picked)
    [i] qml: Android apps to import: "Notes" isn't imported (3 picked)
    [i] qml: Android apps to import: 1 apps unticked (2 picked)
    [i] qml: Android apps to import: 1 more apps ticked (3 picked)
    [i] Library: Android apps to import: 1 installed apps not picked, left out
    OK: a new install (first start, setup to do): no app picked: {'apps': []}
    OK: a new install with apps (setup to do), the picker opened: none picked: {'apps': []}
    OK: first start with the picker: the apps in the library kept as picked (hidden Retro Emu too): {'apps': ['com.test.drift', 'com.test.notes', 'com.test.retroarch']}
    OK: after the picker: Camera ticked, Notes unticked, Drift Racer off and on again: {'apps': ['com.test.camera', 'com.test.drift', 'com.test.retroarch']}
    OK: Camera's section (X): 'Emulator'
    OK: the picked apps keep their sections: Drift Racer 'Game', Retro Emu 'Emulator'
    OK: Drift Racer in the library
    OK: Notes not in the library
    OK: Retro Emu not in the library
    (Library > Android, the hidden Retro Emu under Emulators: screenshots 14-library-android*;
     a new install: 14-new-picker, 14-new-library-options; the round prompts: 14-picker*)

## Backup & reset (throwaway fixture folders)
    [i] Backup: Saved `/home/runner/work/_temp/px/storage/Download/PegasusX-backup-2026-11-28-0543-build2175.tar`: 19 files
    [i] qml: Backup & reset: backup saved: /home/runner/work/_temp/px/storage/Download/PegasusX-backup-2026-11-28-0543-build2175.tar
    [i] qml: Backup & reset: run setup again
    [i] qml: Backup & reset: start over
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/pegasusx.ini`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/settings.txt`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/game_dirs.txt`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/pegasusx-hidden.json`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/pegasusx-android-kinds.json`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/pegasusx-android-apps.json`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/pegasusx-rom-folders.txt`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/pegasusx-themes`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/gamedirectory`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/metadata`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/imported`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/downloaded_media`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/backup`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/launchbox`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/esde-lists`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/pegasusx.ini`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/settings.txt`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/game_dirs.txt`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/pegasusx-hidden.json`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/pegasusx-android-kinds.json`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/pegasusx-android-apps.json`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/pegasusx-rom-folders.txt`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/pegasusx-themes`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/metadata`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/imported`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/backup`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/launchbox`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/esde-lists`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/pegasusx-root-asked`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.local/share/pegasus-frontend/pegasus-frontend/pegasusx-setup-done`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.cache/pegasus-frontend/gameindex-v1.json`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.cache/pegasus-frontend/netcache`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.cache/pegasus-frontend/import-conflicts.txt`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/pegasusx.ini.pxgood`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/pegasusx.ini.pxgood`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/settings.txt.pxgood`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/settings.txt.pxgood`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/game_dirs.txt.pxgood`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/game_dirs.txt.pxgood`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/pegasusx-hidden.json.pxgood`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/pegasusx-hidden.json.pxgood`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/pegasusx-android-kinds.json.pxgood`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/pegasusx-android-kinds.json.pxgood`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/pegasusx-android-apps.json.pxgood`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/pegasusx-android-apps.json.pxgood`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/PXFolder/pegasusx-rom-folders.txt.pxgood`
    [i] Backup: Start over: removed `/home/runner/work/_temp/px/home/.config/pegasus-frontend/pegasusx-rom-folders.txt.pxgood`
    [i] Setup: Start over: setup from step 1 at the next start
    [i] qml: Backup & reset: start over done
    OK: a backup file was saved in Download: PegasusX-backup-2026-11-28-0543-build2175.tar 
    OK: no planted password, key or token in the backup (ssUser kept: 1)
    OK: no downloaded_media in the backup
    OK: no themes/FakeXBSX in the backup
    OK: no lastrun.log in the backup
    OK: no launchbox/ in the backup
    OK: no /backup/ in the backup
    backup contents (tar -tf):
        pegasusx-backup.json
        data/game_dirs.txt
        data/gamedirectory/.migrated
        data/gamedirectory/android/metadata.pegasus.txt
        data/gamedirectory/gba/metadata.pegasus.txt
        data/gamedirectory/gc/metadata.pegasus.txt
        data/gamedirectory/imported-folders.json
        data/gamedirectory/my_snes_picks/metadata.pegasus.txt
        data/gamedirectory/n64/metadata.pegasus.txt
        data/gamedirectory/psx/metadata.pegasus.txt
        data/gamedirectory/snes/metadata.pegasus.txt
        data/gamedirectory/switch/metadata.pegasus.txt
        data/gamedirectory/switch2/metadata.pegasus.txt
        data/imported/snes/gamelist.xml
        data/pegasusx-android-apps.json
        data/pegasusx-android-kinds.json
        data/pegasusx-hidden.json
        data/pegasusx-rom-folders.txt
        data/pegasusx.ini
        data/settings.txt
        pegasusx-backup-files.json
    OK: Run setup again changed no file (setup progress marker aside)
    OK: Start over kept the ROM folders, ES-DE, the theme and the file outside (39 files, same checksums)
    OK: pegasusx.ini deleted from the App Data Folder (PXFolder)
    OK: settings.txt deleted from the App Data Folder (PXFolder)
    OK: game_dirs.txt deleted from the App Data Folder (PXFolder)
    OK: gamedirectory/ deleted from the App Data Folder (PXFolder)
    OK: downloaded_media/ deleted from the App Data Folder (PXFolder)
    OK: metadata/ deleted from the App Data Folder (PXFolder)
    OK: pegasusx-hidden.json deleted from the App Data Folder (PXFolder)
    OK: theme_settings/ deleted from the App Data Folder (PXFolder)
    OK: settings.txt.pxgood deleted from the App Data Folder (PXFolder)
    OK: pegasusx.ini.pxgood deleted from the App Data Folder (PXFolder)
    OK: game_dirs.txt.pxgood deleted from the App Data Folder (PXFolder)
    OK: damaged/ deleted from the App Data Folder (PXFolder)
    OK: pegasusx-root-asked deleted from the App Data Folder (PXFolder)
    OK: pegasusx.ini deleted from the app's own folder (older copy)
    OK: settings.txt deleted from the app's own folder (older copy)
    OK: game_dirs.txt deleted from the app's own folder (older copy)
    OK: gamedirectory/ deleted from the app's own folder (older copy)
    OK: downloaded_media/ deleted from the app's own folder (older copy)
    OK: metadata/ deleted from the app's own folder (older copy)
    OK: pegasusx-hidden.json deleted from the app's own folder (older copy)
    OK: theme_settings/ deleted from the app's own folder (older copy)
    OK: settings.txt.pxgood deleted from the app's own folder (older copy)
    OK: pegasusx.ini.pxgood deleted from the app's own folder (older copy)
    OK: game_dirs.txt.pxgood deleted from the app's own folder (older copy)
    OK: damaged/ deleted from the app's own folder (older copy)
    OK: pegasusx-root-asked deleted from the app's own folder (older copy)
    OK: pegasusx-root.txt kept (the App Data Folder stays picked)
    OK: no setup-done marker after Start over
    after Start over Pegasus X closed by itself (1 "closing to start again" lines)
    files after Start over: PXFolder:
        pegasusx-android-apps.json
        themes/OtherTheme/theme.qml
    the app's own folder:
        pegasusx-root.txt
        pegasusx-root.txt.pxgood
        themes/FakeXBSX/theme.qml
    OK: the backup in Download is still there
    (screenshots: 15-backup-*, 15-setup-again, 15-resume-*, 15-wipe-*, 15-start-over-restarting, 15-after-start-over)

## Setup (it stays up to step 4, never comes back once done, no theme while it's open)
    01-first-start:
        Setup: not done; Pegasus data found in `/home/runner/work/_temp/px/home/.config/pegasus-frontend`: step 1 asks to use it or start fresh
        Setup: off screen
        Setup: open (no theme until it's finished or skipped)
        Setup: open: the library is read when setup asks for it
        Setup: on screen
        Setup: the Pegasus data found in /home/runner/work/_temp/px/home/.config/pegasus-frontend is used
        Game directory: Pick my ROMs folder /home/runner/work/_temp/px/MyGames: 4 systems found, 0 folders not matched
        Setup: finished at step 4: the theme loads
        Setup: off screen
        Theme: loaded qrc:///themes/pegasus-theme-grid/theme.qml
    01-restart-1:
        Setup: done (`/home/runner/work/_temp/px/home/.local/share/pegasus-frontend/pegasus-frontend/pegasusx-setup-done`)
        Setup: off screen
        Theme: loaded qrc:///themes/pegasus-theme-grid/theme.qml
    01-restart-2:
        Setup: done (`/home/runner/work/_temp/px/home/.local/share/pegasus-frontend/pegasus-frontend/pegasusx-setup-done`)
        Setup: off screen
        Theme: loaded qrc:///themes/pegasus-theme-grid/theme.qml
    01-restart-3:
        Setup: done (`/home/runner/work/_temp/px/home/.local/share/pegasus-frontend/pegasus-frontend/pegasusx-setup-done`)
        Setup: off screen
        Theme: loaded qrc:///themes/pegasus-theme-grid/theme.qml
    15-backup:
        Setup: done (`/home/runner/work/_temp/px/home/.local/share/pegasus-frontend/pegasus-frontend/pegasusx-setup-done`)
        Setup: off screen
        Theme: loaded qrc:///themes/pegasus-theme-grid/theme.qml
        Setup: run again from step 1
        Setup: on screen
    15-resume:
        Setup: not done, goes on at step 2
        Setup: off screen
        Setup: open (no theme until it's finished or skipped)
        Setup: open: the library is read when setup asks for it
        Setup: on screen
        Setup: skipped at step 1: the theme loads
        Setup: off screen
        Theme: loaded qrc:///themes/pegasus-theme-grid/theme.qml
    15-after-start-over:
        Setup: not done, goes on at step 1
        Setup: off screen
        Setup: open (no theme until it's finished or skipped)
        Setup: open: the library is read when setup asks for it
        Setup: on screen
    OK: first start: no theme before setup finished (first of the two: [i] Setup: finished at step 4: the theme loads)
    first start, in order: Setup: off screen,Setup: on screen,Setup: finished,Setup: off screen,Theme: loaded,
    FAILED: first start: setup left the screen before it finished, or a theme came first
    OK: restart 1: setup done, not shown
    OK: restart 2: setup done, not shown
    OK: restart 3: setup done, not shown
    OK: closed mid-setup (step 2): the next start went on at step 2
    OK: after Start over: setup, no theme
    (screenshots: 01-wizard-*, 01-theme, 01-restart-1, 15-setup-again, 15-resume-*, 15-after-start-over)

## Pick my ROMs folder (08d, 08e; in setup: 01-wizard-2-*)
    [i] qml: Game directory: Pick my ROMs folder /home/runner/work/_temp/px/FolderFirst: 2 systems found, 2 folders not matched
    [i] qml: Game directory: Pick my ROMs folder /home/runner/work/_temp/px/NoSystems: 0 systems found, 2 folders not matched
    [i] qml: Game directory: Pick my ROMs folder /home/runner/work/_temp/px/storage/ROMs: 2 systems found, 0 folders not matched
    OK: Cancel added nothing (the game directory the same, 10 files)
    (screenshots: 08d-folder-first-*, 08e-folder-first-nothing, 08e-tick-instead)

## Gamepad layout: each button's highlight on the picture (18, in qmlscene)
    OK: a screen 854: picture 256 x 171, highlight on 633, 227 (button at 632.5, 227; 0.0 px off)
    OK: b screen 854: picture 256 x 171, highlight on 690, 170 (button at 690.5, 170; 0.0 px off)
    OK: x screen 854: picture 256 x 171, highlight on 574, 170 (button at 574.5, 170.5; 0.0 px off)
    OK: y screen 854: picture 256 x 171, highlight on 633, 115 (button at 632.5, 114.5; 0.0 px off)
    OK: dpup screen 854: picture 256 x 171, highlight on 313, 265 (button at 313, 265; 0.0 px off)
    OK: dpdown screen 854: picture 256 x 171, highlight on 313, 343 (button at 313, 343; 0.0 px off)
    OK: dpleft screen 854: picture 256 x 171, highlight on 274, 304 (button at 274, 304; 0.0 px off)
    OK: dpright screen 854: picture 256 x 171, highlight on 351, 304 (button at 351, 304; 0.0 px off)
    OK: lsx screen 854: picture 256 x 171, highlight on 209, 170 (button at 209, 170; 0.0 px off)
    OK: lsy screen 854: picture 256 x 171, highlight on 209, 170 (button at 209, 170; 0.0 px off)
    OK: lsb screen 854: picture 256 x 171, highlight on 209, 170 (button at 209, 170; 0.0 px off)
    OK: rsx screen 854: picture 256 x 171, highlight on 527, 297 (button at 527, 297; 0.0 px off)
    OK: rsy screen 854: picture 256 x 171, highlight on 527, 297 (button at 527, 297; 0.0 px off)
    OK: rsb screen 854: picture 256 x 171, highlight on 527, 297 (button at 527, 297; 0.0 px off)
    OK: select screen 854: picture 256 x 171, highlight on 360, 170 (button at 360, 170.5; 0.0 px off)
    OK: start screen 854: picture 256 x 171, highlight on 480, 170 (button at 480, 170.5; 0.0 px off)
    OK: guide screen 854: picture 256 x 171, highlight on 421, 77 (button at 420.5, 77; 0.0 px off)
    OK: lb screen 854: picture 256 x 171, highlight on 232, 22 (button at 232, 22; 0.0 px off)
    OK: rb screen 854: picture 256 x 171, highlight on 607, 22 (button at 607, 22; 0.0 px off)
    OK: lt screen 854: picture 256 x 171, highlight on 160, 42 (button at 160, 42; 0.0 px off)
    OK: rt screen 854: picture 256 x 171, highlight on 679, 42 (button at 679, 42; 0.0 px off)
    OK: a screen 1280: picture 384 x 257, highlight on 633, 227 (button at 632.5, 227; 0.0 px off)
    OK: b screen 1280: picture 384 x 257, highlight on 690, 170 (button at 690.5, 170; 0.0 px off)
    OK: x screen 1280: picture 384 x 257, highlight on 574, 170 (button at 574.5, 170.5; 0.0 px off)
    OK: y screen 1280: picture 384 x 257, highlight on 633, 115 (button at 632.5, 114.5; 0.0 px off)
    OK: dpup screen 1280: picture 384 x 257, highlight on 313, 265 (button at 313, 265; 0.0 px off)
    OK: dpdown screen 1280: picture 384 x 257, highlight on 313, 343 (button at 313, 343; 0.0 px off)
    OK: dpleft screen 1280: picture 384 x 257, highlight on 274, 304 (button at 274, 304; 0.0 px off)
    OK: dpright screen 1280: picture 384 x 257, highlight on 351, 304 (button at 351, 304; 0.0 px off)
    OK: lsx screen 1280: picture 384 x 257, highlight on 209, 170 (button at 209, 170; 0.0 px off)
    OK: lsy screen 1280: picture 384 x 257, highlight on 209, 170 (button at 209, 170; 0.0 px off)
    OK: lsb screen 1280: picture 384 x 257, highlight on 209, 170 (button at 209, 170; 0.0 px off)
    OK: rsx screen 1280: picture 384 x 257, highlight on 527, 297 (button at 527, 297; 0.0 px off)
    OK: rsy screen 1280: picture 384 x 257, highlight on 527, 297 (button at 527, 297; 0.0 px off)
    OK: rsb screen 1280: picture 384 x 257, highlight on 527, 297 (button at 527, 297; 0.0 px off)
    OK: select screen 1280: picture 384 x 257, highlight on 360, 170 (button at 360, 170.5; 0.0 px off)
    OK: start screen 1280: picture 384 x 257, highlight on 480, 170 (button at 480, 170.5; 0.0 px off)
    OK: guide screen 1280: picture 384 x 257, highlight on 421, 77 (button at 420.5, 77; 0.0 px off)
    OK: lb screen 1280: picture 384 x 257, highlight on 232, 22 (button at 232, 22; 0.0 px off)
    OK: rb screen 1280: picture 384 x 257, highlight on 607, 22 (button at 607, 22; 0.0 px off)
    OK: lt screen 1280: picture 384 x 257, highlight on 160, 42 (button at 160, 42; 0.0 px off)
    OK: rt screen 1280: picture 384 x 257, highlight on 679, 42 (button at 679, 42; 0.0 px off)
    OK: a screen 1920: picture 576 x 385, highlight on 633, 227 (button at 632.5, 227; 0.0 px off)
    OK: b screen 1920: picture 576 x 385, highlight on 690, 170 (button at 690.5, 170; 0.0 px off)
    OK: x screen 1920: picture 576 x 385, highlight on 574, 170 (button at 574.5, 170.5; 0.0 px off)
    OK: y screen 1920: picture 576 x 385, highlight on 633, 115 (button at 632.5, 114.5; 0.0 px off)
    OK: dpup screen 1920: picture 576 x 385, highlight on 313, 265 (button at 313, 265; 0.0 px off)
    OK: dpdown screen 1920: picture 576 x 385, highlight on 313, 343 (button at 313, 343; 0.0 px off)
    OK: dpleft screen 1920: picture 576 x 385, highlight on 274, 304 (button at 274, 304; 0.0 px off)
    OK: dpright screen 1920: picture 576 x 385, highlight on 351, 304 (button at 351, 304; 0.0 px off)
    OK: lsx screen 1920: picture 576 x 385, highlight on 209, 170 (button at 209, 170; 0.0 px off)
    OK: lsy screen 1920: picture 576 x 385, highlight on 209, 170 (button at 209, 170; 0.0 px off)
    OK: lsb screen 1920: picture 576 x 385, highlight on 209, 170 (button at 209, 170; 0.0 px off)
    OK: rsx screen 1920: picture 576 x 385, highlight on 527, 297 (button at 527, 297; 0.0 px off)
    OK: rsy screen 1920: picture 576 x 385, highlight on 527, 297 (button at 527, 297; 0.0 px off)
    OK: rsb screen 1920: picture 576 x 385, highlight on 527, 297 (button at 527, 297; 0.0 px off)
    OK: select screen 1920: picture 576 x 385, highlight on 360, 170 (button at 360, 170.5; 0.0 px off)
    OK: start screen 1920: picture 576 x 385, highlight on 480, 170 (button at 480, 170.5; 0.0 px off)
    OK: guide screen 1920: picture 576 x 385, highlight on 421, 77 (button at 420.5, 77; 0.0 px off)
    OK: lb screen 1920: picture 576 x 385, highlight on 232, 22 (button at 232, 22; 0.0 px off)
    OK: rb screen 1920: picture 576 x 385, highlight on 607, 22 (button at 607, 22; 0.0 px off)
    OK: lt screen 1920: picture 576 x 385, highlight on 160, 42 (button at 160, 42; 0.0 px off)
    OK: rt screen 1920: picture 576 x 385, highlight on 679, 42 (button at 679, 42; 0.0 px off)
    OK: 63 highlights at 3 sizes, 0 off their button or the picture out of shape
    (pictures: gamepad/gamepad-<screen width>-<button>.png)

## Library > Metadata (17-metadata*)
    17-metadata:
        Metadata: 23 games read for genres in 0 ms: 5 values
        Genres: 5 values: Action (1), Adventure (1), Platform (2), Platformer (1), Shoot'em Up (2)
        Metadata: 1 suggested merges for genres (5 values) in 0 ms
        Genres: suggested merges (0 ms): maybe: Platform + Platformer
        Metadata: 23 games read for publishers in 0 ms: 5 values
        Publishers: 5 values: Nintendo (4), Nintendo Co., Ltd. (1), Nintendo of America (2), Sega (1), SEGA Europe (1)
        Metadata: tab Publishers
        Metadata: 3 suggested merges for publishers (5 values) in 0 ms
        Publishers: suggested merges (0 ms): near-certain: Nintendo + Nintendo Co., Ltd. | regional: Nintendo + Nintendo of America + Nintendo Co., Ltd. | regional: Sega + SEGA Europe
        Metadata: publishers merged: Nintendo Co., Ltd. shown as "Nintendo" (1 games show it now); no game file changed
        Publishers: merge Nintendo + Nintendo Co., Ltd. shown as "Nintendo": 1 spellings, 1 games
        Metadata: 23 games read for publishers in 0 ms: 4 values
        Publishers: 4 values: Nintendo (5), Nintendo of America (2), Sega (1), SEGA Europe (1)
        Metadata: 2 suggested merges for publishers (4 values) in 0 ms
        Publishers: suggested merges (0 ms): regional: Nintendo + Nintendo of America | regional: Sega + SEGA Europe
        Metadata: publishers merged: Nintendo Co., Ltd. | Nintendo of America shown as "Nintendo" (2 games show it now); no game file changed
        Publishers: merge Nintendo + Nintendo of America shown as "Nintendo": 2 spellings, 2 games
        Metadata: 23 games read for publishers in 0 ms: 3 values
        Publishers: 3 values: Nintendo (6), Sega (1), SEGA Europe (1)
        Metadata: 1 suggested merges for publishers (3 values) in 0 ms
        Publishers: suggested merges (0 ms): regional: Sega + SEGA Europe
        Metadata: 23 games read for developers in 0 ms: 6 values
        Developers: 6 values: Hudson Soft (1), HudsonSoft (1), Nintendo EAD (3), Nintendo EPD (1), Sega (1), Sega AM2 (2)
        Metadata: tab Developers
        Metadata: 2 suggested merges for developers (6 values) in 0 ms
        Developers: suggested merges (0 ms): maybe: Nintendo EAD + Nintendo EPD | maybe: Hudson Soft + HudsonSoft
        Metadata: 23 games read for publishers in 0 ms: 3 values
        Publishers: 3 values: Nintendo (6), Sega (1), SEGA Europe (1)
        Metadata: tab Publishers
        Metadata: 1 suggested merges for publishers (3 values) in 0 ms
        Publishers: suggested merges (0 ms): regional: Sega + SEGA Europe
    17-metadata-restart:
        Recovery: `aliases.json`: last good copy made (`aliases.json.pxgood`)
        Metadata: 23 games read for genres in 0 ms: 5 values
        Genres: 5 values: Action (1), Adventure (1), Platform (2), Platformer (1), Shoot'em Up (2)
        Metadata: 1 suggested merges for genres (5 values) in 0 ms
        Genres: suggested merges (0 ms): maybe: Platform + Platformer
        Metadata: 23 games read for publishers in 0 ms: 3 values
        Publishers: 3 values: Nintendo (7), Sega (1), SEGA Europe (1)
        Metadata: tab Publishers
        Metadata: 1 suggested merges for publishers (3 values) in 0 ms
        Publishers: suggested merges (0 ms): regional: Sega + SEGA Europe
        Metadata: publishers merge undone: Nintendo Co., Ltd. | Nintendo of America shown as they are again, not as "Nintendo" (4 games)
        Publishers: undo "Nintendo": 2 spellings, 4 games
        Metadata: 23 games read for publishers in 0 ms: 5 values
        Publishers: 5 values: Nintendo (4), Nintendo Co., Ltd. (2), Nintendo of America (2), Sega (1), SEGA Europe (1)
        Metadata: 3 suggested merges for publishers (5 values) in 0 ms
        Publishers: suggested merges (0 ms): near-certain: Nintendo + Nintendo Co., Ltd. | regional: Nintendo + Nintendo Co., Ltd. + Nintendo of America | regional: Sega + SEGA Europe
    17-metadata-recovery:
        Recovery: `aliases.json`: damaged: has zero bytes in it (a write cut short)
        Recovery: `aliases.json`: moved to `/home/runner/work/_temp/px/home/.config/pegasus-frontend/damaged/2026-11-28_05-46-28 aliases.json`
        Recovery: `aliases.json`: its last good copy `aliases.json.pxgood` put in its place
    OK: merging changed no game file (15 metadata files, gamelists and game directory files, same checksums)
    OK: the restart and the undo changed no game file
    OK: Sega AM2 never suggested with Sega (maybe: Nintendo EAD + Nintendo EPD | maybe: Hudson Soft + HudsonSoft)
    OK: Nintendo's spellings suggested: near-certain: Nintendo + Nintendo Co., Ltd. | regional: Nintendo + Nintendo of America + Nintendo Co., Ltd. | regional: Sega + SEGA Europe
    OK: aliases.json after the merges: {"Nintendo Co., Ltd.": "Nintendo", "Nintendo of America": "Nintendo"}
    OK: after a restart and the rescrape, publishers: Nintendo (7), Sega (1), SEGA Europe (1)
        Nintendo's games after the rescrape: 7 (Meta Hotel's "Nintendo Co., Ltd." folded in)
    OK: after the undo, publishers: Nintendo (4), Nintendo Co., Ltd. (2), Nintendo of America (2), Sega (1), SEGA Europe (1)
    OK: aliases.json after the undo: {"developer": {}, "genre": {}, "publisher": {}, "version": 1}
    OK: aliases.json junked, then put back by recovery: {"developer": {}, "genre": {}, "publisher": {"Nintendo Co., Ltd.": "Nintendo", "Nintendo of America": "Nintendo"}, "version": 1}
    the damaged folder: 2026-11-28_05-46-28 aliases.json 
    (screenshots: 17-*)

## Damaged-file recovery (16-recovery*)
    last good copies before the damage: game_dirs.txt.pxgood pegasusx-android-apps.json.pxgood pegasusx-android-kinds.json.pxgood pegasusx-hidden.json.pxgood pegasusx-rom-folders.txt.pxgood pegasusx.ini.pxgood settings.txt.pxgood 
    Recovery: lines, first start (settings.txt junk, pegasusx-hidden.json cut in half, game_dirs.txt only as .pxold):
        [w] Recovery: `settings.txt`: damaged: has zero bytes in it (a write cut short)
        [w] Recovery: `settings.txt`: moved to `/home/runner/work/_temp/px/home/.config/pegasus-frontend/damaged/2026-11-28_05-44-06 settings.txt`
        [w] Recovery: `settings.txt`: its last good copy `settings.txt.pxgood` put in its place
        [w] Recovery: `game_dirs.txt`: missing, an interrupted save left `game_dirs.txt.pxold` beside it
        [w] Recovery: `game_dirs.txt`: put back from `game_dirs.txt.pxold`
        [w] Recovery: `pegasusx-hidden.json`: damaged: not JSON (unterminated object at 110)
        [w] Recovery: `pegasusx-hidden.json`: moved to `/home/runner/work/_temp/px/home/.config/pegasus-frontend/damaged/2026-11-28_05-44-06 pegasusx-hidden.json`
        [w] Recovery: `pegasusx-hidden.json`: its last good copy `pegasusx-hidden.json.pxgood` put in its place
    OK: settings.txt is its last good copy again (same checksum)
    OK: pegasusx-hidden.json is its last good copy again (same checksum)
    OK: game_dirs.txt put back from game_dirs.txt.pxold (same checksum as before)
    OK: no .pxsave / .pxold left
    the damaged folder after it:
        total 8
        -rw-r--r-- 1 runner runner  110 Nov 28 05:44 2026-11-28_05-44-06 pegasusx-hidden.json
        -rw-r--r-- 1 runner runner 3000 Nov 28 05:44 2026-11-28_05-44-06 settings.txt
    Recovery: lines, second start (pegasusx.ini all zero bytes; X Restore from a backup):
        [w] Recovery: `pegasusx.ini`: damaged: has zero bytes in it (a write cut short)
        [w] Recovery: `pegasusx.ini`: moved to `/home/runner/work/_temp/px/home/.config/pegasus-frontend/damaged/2026-11-28_05-44-26 pegasusx.ini`
        [w] Recovery: `pegasusx.ini`: its last good copy `pegasusx.ini.pxgood` put in its place
    OK: pegasusx.ini is its last good copy again
    the damaged folder after it:
        total 12
        -rw-r--r-- 1 runner runner  110 Nov 28 05:44 2026-11-28_05-44-06 pegasusx-hidden.json
        -rw-r--r-- 1 runner runner 3000 Nov 28 05:44 2026-11-28_05-44-06 settings.txt
        -rw-r--r-- 1 runner runner 2000 Nov 28 05:44 2026-11-28_05-44-26 pegasusx.ini
    third start (nothing damaged): 0 fixing Recovery: lines (expected 0, and no notice: 16-recovery-none)
    (screenshots: 16-recovery-notice, -details, -after-keep, -notice-2, -restore-picker, -none)

## Settings > Advanced: Missing games (verify-files / show-missing-games)
    hide: written general.show-missing-games: false general.verify-files: true 
    show: written general.show-missing-games: true general.verify-files: true 
    dontcheck: written general.show-missing-games: false general.verify-files: false 
    dontcheck-show: written general.show-missing-games: true general.verify-files: false 
    picked Show: general.show-missing-games: true general.verify-files: true 
    picked Don't check: general.show-missing-games: true general.verify-files: false 
    (what Advanced showed for each: screenshots 02-missing-*)

## Library as read (last start)
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/gba/metadata.pegasus.txt`
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/gc/metadata.pegasus.txt`
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/my_snes_picks/metadata.pegasus.txt`
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/n64/metadata.pegasus.txt`
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/psx/metadata.pegasus.txt`
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/snes/metadata.pegasus.txt`
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/switch/metadata.pegasus.txt`
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/switch2/metadata.pegasus.txt`
    [i] GOG: 0 games found
    [i] Library: `Nintendo 64` launches with RetroArch · Mupen64Plus-Next (recommended; none set in its metadata)
    [i] Library: `Nintendo Game Boy Advance` launches with RetroArch · mGBA (recommended; none set in its metadata)
    [i] Library: `Nintendo SNES (Super Nintendo)` launches with RetroArch · Snes9x - Current (recommended; none set in its metadata)
    [i] Library: `Nintendo Switch` launches with Eden (Standalone) (recommended; none set in its metadata)
    [i] Library: `Sony PlayStation` launches with RetroArch · Beetle PSX (recommended; none set in its metadata)
    [i] 13 games found

## Warnings and errors
         42 [w] qrc:/themes/pegasus-theme-grid/layer_platform/PlatformCard.qml:71:5: QML Image: Cannot open: qrc:/themes/pegasus-theme-grid/assets/logos/switch2.svg
         42 [w] qrc:/themes/pegasus-theme-grid/layer_platform/PlatformCard.qml:71:5: QML Image: Cannot open: qrc:/themes/pegasus-theme-grid/assets/logos/my snes picks.svg
         42 [i] qml: Theme: loaded qrc:///themes/pegasus-theme-grid/theme.qml
         41 [w] The collection 'GOG' has no valid games, ignored
         32 [w] Requested theme `:/themes/pegasus-theme-grid` not found, falling back to default
         18 [w] qrc:/frontend/menu/settings/AndroidAppPicker.qml:352:17: QML Image: Invalid image provider: image://androidicons/com.test.drift
         10 [w] qrc:/frontend/menu/settings/AndroidAppPicker.qml:352:17: QML Image: Invalid image provider: image://androidicons/com.test.retroarch
         10 [w] qrc:/frontend/menu/settings/AndroidAppPicker.qml:352:17: QML Image: Invalid image provider: image://androidicons/com.test.notes
         10 [w] qrc:/frontend/menu/settings/AndroidAppPicker.qml:352:17: QML Image: Invalid image provider: image://androidicons/com.test.camera
          7 [w] qrc:/frontend/menu/settings/MetadataEditor.qml:3563:9: QML OnScreenKeyboard: Binding loop detected for property "suggestions"
          7 [w] libpng warning: iCCP: known incorrect sRGB profile
          4 [w] Pegasus Metafiles: `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/n64/metadata.pegasus.txt`, line 11: Asset file `/home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/n64/fanart/Mario Kart 64 (USA).png` doesn't seem to exist
          4 [w] No `theme.cfg` file found in `/home/runner/work/_temp/px/home/.config/pegasus-frontend/themes/FakeXBSX/`, theme skipped
          3 file:///home/runner/work/pegasus-frontend/pegasus-frontend/tests/testenv/gamepad_points.qml:31:9: Unable to assign [undefined] to QString
          3 [w] qrc:/themes/pegasus-theme-grid/layer_platform/PlatformCard.qml:71:5: QML Image: Cannot open: qrc:/themes/pegasus-theme-grid/assets/logos/metatest.svg
          3 [w] ES-DE import: No gamelist file found for system `switch`
          3 [w] ES-DE import: No gamelist file found for system `switch2`
          3 [w] ES-DE import: No gamelist file found for system `n64`
          2 [w] libpng warning: profile matches sRGB but writing iCCP instead
          2 [w] libpng warning: known incorrect sRGB profile
          2 [w] No `theme.cfg` file found in `/home/runner/work/_temp/px/PXFolder/themes/OtherTheme/`, theme skipped
          2 [w] ES-DE import: No gamelist file found for system `gba`
          1 [w] qrc:/frontend/menu/settings/common/MultivalueOption.qml:33: TypeError: Cannot read property 'width' of null
          1 [w] Recovery: `settings.txt`: moved to `/home/runner/work/_temp/px/home/.config/pegasus-frontend/damaged/2026-11-28_05-44-06 settings.txt`
          1 [w] Recovery: `settings.txt`: its last good copy `settings.txt.pxgood` put in its place
          1 [w] Recovery: `settings.txt`: damaged: has zero bytes in it (a write cut short)
          1 [w] Recovery: `pegasusx.ini`: moved to `/home/runner/work/_temp/px/home/.config/pegasus-frontend/damaged/2026-11-28_05-44-26 pegasusx.ini`
          1 [w] Recovery: `pegasusx.ini`: its last good copy `pegasusx.ini.pxgood` put in its place
          1 [w] Recovery: `pegasusx.ini`: damaged: has zero bytes in it (a write cut short)
          1 [w] Recovery: `pegasusx-hidden.json`: moved to `/home/runner/work/_temp/px/home/.config/pegasus-frontend/damaged/2026-11-28_05-44-06 pegasusx-hidden.json`
          1 [w] Recovery: `pegasusx-hidden.json`: its last good copy `pegasusx-hidden.json.pxgood` put in its place
          1 [w] Recovery: `pegasusx-hidden.json`: damaged: not JSON (unterminated object at 110)
          1 [w] Recovery: `game_dirs.txt`: put back from `game_dirs.txt.pxold`
          1 [w] Recovery: `game_dirs.txt`: missing, an interrupted save left `game_dirs.txt.pxold` beside it
          1 [w] Recovery: `aliases.json`: moved to `/home/runner/work/_temp/px/home/.config/pegasus-frontend/damaged/2026-11-28_05-46-28 aliases.json`
          1 [w] Recovery: `aliases.json`: its last good copy `aliases.json.pxgood` put in its place
          1 [w] Recovery: `aliases.json`: damaged: has zero bytes in it (a write cut short)

## Art each game has (last start)
    Ape Escape (USA)
    Chrono Trigger (my own file)
      screenshot: storage/ES-DE/downloaded_media/snes/screenshots/Chrono Trigger (USA).png
      steam: home/.config/pegasus-frontend/downloaded_media/my snes picks/miximages/Chrono Trigger (USA).png
    Crash Bandicoot
      boxFront: MyGames/psx/media/crash.png
    Donkey Kong Bananza
    Final Fantasy VII (USA)
    Mario Kart 64 (USA)
      boxFront: MyGames/Nintendo 64/media/covers/Mario Kart 64 (USA).png
      boxFront: MyGames/Nintendo 64/media/3D Boxes/Mario Kart 64 (USA).png
      logo: MyGames/Nintendo 64/media/wheel/Mario Kart 64 (USA).png
      screenshot: MyGames/Nintendo 64/media/screenshot/Mario Kart 64 (USA).png
      background: home/.config/pegasus-frontend/downloaded_media/n64/fanart/Mario Kart 64 (USA).png
      steam: home/.config/pegasus-frontend/downloaded_media/n64/miximages/Mario Kart 64 (USA).png
      extra.box3d: file://MyGames/Nintendo 64/media/3D Boxes/Mario Kart 64 (USA).png
    Metroid Fusion (USA)
    Metroid Prime
    Star Fox 64 (USA)
      boxFront: MyGames/Nintendo 64/media/Star Fox 64 (USA)/boxFront.png
      screenshot: MyGames/Nintendo 64/media/Star Fox 64 (USA)/screenshot.png
      steam: home/.config/pegasus-frontend/downloaded_media/n64/miximages/Star Fox 64 (USA).png
    Super Mario Odyssey
    Super Mario World
      boxFront: storage/ES-DE/downloaded_media/snes/covers/Super Mario World (USA).png
      boxFront: storage/ES-DE/downloaded_media/snes/3dboxes/Super Mario World (USA).png
      logo: storage/ES-DE/downloaded_media/snes/marquees/Super Mario World (USA).png
      screenshot: storage/ES-DE/downloaded_media/snes/screenshots/Super Mario World (USA).png
      cartridge: storage/ES-DE/downloaded_media/snes/physicalmedia/Super Mario World (USA).png
      steam: home/.config/pegasus-frontend/downloaded_media/snes/miximages/Super Mario World (USA).png
      extra.box3d: file://storage/ES-DE/downloaded_media/snes/3dboxes/Super Mario World (USA).png
    The Legend of Zelda - A Link to the Past (USA)
    Wind Waker (USA)
## Game directories
    /home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory

## Library read, per start (cache used? times)
    01-first-start: GOG: 0 games found · Game list post-processing took 0ms · 12 games found · 
    01-restart-1: GOG: 0 games found · Game list post-processing took 0ms · 12 games found · 
    01-restart-2: GOG: 0 games found · Game list post-processing took 0ms · 12 games found · 
    01-restart-3: GOG: 0 games found · Game list post-processing took 0ms · 12 games found · 
    02-missing-dontcheck-show: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    02-missing-dontcheck: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    02-missing-hide: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    02-missing-pick: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    02-missing-show: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    02-settings: GOG: 0 games found · Game list post-processing took 0ms · 12 games found · 
    03-library: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    04-scraper: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    04b-scraper-section: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    05-scraper-accounts: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    06-themes: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    07-updater: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    08-gamedirs: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    08b-folders: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    08c-folder-checks: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    08d-folder-first: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    08e-folder-first-empty: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    09-esde-import: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    10-theme: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    12-scrape-free: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    13-scrape-launchbox: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    13b-scrape-switch2: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    13c-scrape-progress: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    14-apps-first: GOG: 0 games found · Game list post-processing took 0ms · 15 games found · 
    14-apps-library: GOG: 0 games found · Game list post-processing took 0ms · 14 games found · 
    14-apps-new-library: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    14-apps-new: GOG: 0 games found · Game list post-processing took 0ms · 13 games found · 
    14-apps-picker: GOG: 0 games found · Game list post-processing took 0ms · 15 games found · 
    14-apps-theme: GOG: 0 games found · Game list post-processing took 0ms · 14 games found · 
    15-after-start-over: 
    15-backup: GOG: 0 games found · Game list post-processing took 0ms · 15 games found · 
    15-resume: Skipping full scan due to game index cache · Game list cache restore took 3ms · 15 games found · 
    15-start-over: GOG: 0 games found · Game list post-processing took 0ms · 15 games found · 
    16-recovery-none: GOG: 0 games found · Game list post-processing took 0ms · 15 games found · 
    16-recovery-restore: GOG: 0 games found · Game list post-processing took 0ms · 15 games found · 
    16-recovery: GOG: 0 games found · Game list post-processing took 0ms · 15 games found · 
    17-metadata-recovery: GOG: 0 games found · Game list post-processing took 0ms · 23 games found · 
    17-metadata-restart: GOG: 0 games found · Game list post-processing took 0ms · 23 games found · 
    17-metadata: GOG: 0 games found · Game list post-processing took 0ms · 23 games found · 
    18-gamepad-points: 

## Screen open times
    AndroidAppPicker.qml opened in 20 ms
    BackupResetScreen.qml opened in 22 ms
    BackupResetScreen.qml opened in 23 ms
    EsdeImport.qml opened in 22 ms
    GameDirectory.qml opened in 25 ms
    GameDirectory.qml opened in 26 ms
    GamepadEditor.qml opened in 17 ms
    Library: collection "Android" (1 games) opened in 4 ms
    Library: collection "Nintendo 64" (2 games) opened in 4 ms
    Library: collection "Nintendo SNES (Super Nintendo)" (3 games) opened in 7 ms
    Library: collection "Sony PlayStation" (3 games) opened in 6 ms
    MetadataEditor.qml opened in 107 ms
    MetadataEditor.qml opened in 108 ms
    MetadataEditor.qml opened in 109 ms
    MetadataEditor.qml opened in 122 ms
    MetadataEditor.qml opened in 124 ms
    ScraperScreen.qml opened in 22 ms
    ScraperScreen.qml opened in 23 ms
    ScraperSection.qml opened in 28 ms
    SettingsAdvanced.qml opened in 8 ms
    SettingsAdvanced.qml opened in 9 ms
    SettingsScreen.qml opened in 11 ms
    ThemeDownloader.qml opened in 23 ms
    UpdateDialog.qml opened in 7 ms

## The game directory (and backups)
    gamedirectory/my_snes_picks/metadata.pegasus.txt
    gamedirectory/n64/metadata.pegasus.txt
    gamedirectory/.migrated
    gamedirectory/imported-folders.json
    gamedirectory/switch/metadata.pegasus.txt
    gamedirectory/switch2/metadata.pegasus.txt
    gamedirectory/psx/metadata.pegasus.txt
    gamedirectory/gba/metadata.pegasus.txt
    gamedirectory/gc/metadata.pegasus.txt
    gamedirectory/snes/metadata.pegasus.txt
    gamedirectory/android/metadata.pegasus.txt
    backup/old-metadata-20261128-053221/game_dirs.txt
    backup/switch2/metadata.pegasus.txt.scrape.20261128-054111
    backup/switch2/metadata.pegasus.txt.scrape.20261128-054123
    backup/psx/metadata.pegasus.txt.edit.20261128-053522
    backup/gc/metadata.pegasus.txt.edit.20261128-053528
    backup/snes/metadata.pegasus.txt.restore.20261128-053910
    backup/snes/metadata.pegasus.txt.remove.20261128-053908
    backup/snes/metadata.pegasus.txt.scrape.20261128-054057
    backup/snes/metadata.pegasus.txt.edit.20261128-053522
    backup/snes/metadata.pegasus.txt.remove.20261128-053910
    imported/snes/gamelist.xml
    metadata/library-index.json

### android/metadata.pegasus.txt
    collection: Android
    shortname: android
    
    game: Notes
    file: android:com.test.notes
    
    game: Drift Racer
    file: android:com.test.drift
    
    game: Retro Emu
    file: android:com.test.retroarch

### gba/metadata.pegasus.txt
    # Pegasus X game directory: Nintendo Game Boy Advance
    # Made by Pegasus X from ES-DE (/home/runner/work/_temp/px/storage/ES-DE).
    # Pegasus X writes here (imports, Settings > Library, the scraper).
    # folder: /home/runner/work/_temp/px/storage/ROMs/gba
    
    collection: Nintendo Game Boy Advance
    shortname: gba
    
    game: Metroid Fusion (USA)
    file: /home/runner/work/_temp/px/storage/ROMs/gba/Metroid Fusion (USA).gba
    

### gc/metadata.pegasus.txt
    # Pegasus X game directory: GameCube
    # Made from your library on 2026-11-28T05:32:21.
    # Pegasus X writes here (imports, Settings > Library, the scraper).
    
    collection: GameCube
    shortname: gc
    launch: dolphin-emu -b -e "{file.path}"
    
    game: Metroid Prime
    file: /home/runner/work/_temp/px/Pegasus/gamecube/Metroid Prime (USA).iso
    genre: Shoot'em Up
    description: Written in a Pegasus metadata file.
    
    game: Wind Waker (USA)
    file: /home/runner/work/_temp/px/Pegasus/gamecube/Wind Waker (USA).iso
    genre: Shoot'em Up
    

### my_snes_picks/metadata.pegasus.txt
    # Pegasus X game directory: My SNES picks
    # Made from your library on 2026-11-28T05:32:21.
    # Pegasus X writes here (imports, Settings > Library, the scraper).
    
    collection: My SNES picks
    shortname: my snes picks
    
    game: Chrono Trigger (my own file)
    file: /home/runner/work/_temp/px/storage/ROMs/snes/Chrono Trigger (USA).sfc
    description: Written in my own metadata file: it wins over the ES-DE import.
    assets.screenshot: /home/runner/work/_temp/px/storage/ES-DE/downloaded_media/snes/screenshots/Chrono Trigger (USA).png
    assets.steam: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/my snes picks/miximages/Chrono Trigger (USA).png
    

### n64/metadata.pegasus.txt
    # Pegasus X game directory: Nintendo 64
    # Made by Pegasus X from the ROM folder /home/runner/work/_temp/px/MyGames.
    # Pegasus X writes here (imports, Settings > Library, the scraper).
    # folder: /home/runner/work/_temp/px/MyGames/Nintendo 64
    
    collection: Nintendo 64
    shortname: n64
    
    game: Mario Kart 64 (USA)
    file: /home/runner/work/_temp/px/MyGames/Nintendo 64/Mario Kart 64 (USA).z64
    assets.background: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/n64/fanart/Mario Kart 64 (USA).png
    x-pegasusx-hidden-art: box_front|/home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/n64/covers/Mario Kart 64 (USA).png
    assets.box_front: /home/runner/work/_temp/px/MyGames/Nintendo 64/media/covers/Mario Kart 64 (USA).png
      /home/runner/work/_temp/px/MyGames/Nintendo 64/media/3D Boxes/Mario Kart 64 (USA).png
    assets.logo: /home/runner/work/_temp/px/MyGames/Nintendo 64/media/wheel/Mario Kart 64 (USA).png
    assets.background: /home/runner/work/_temp/px/MyGames/Nintendo 64/media/fanart/Mario Kart 64 (USA).png
    assets.screenshot: /home/runner/work/_temp/px/MyGames/Nintendo 64/media/screenshot/Mario Kart 64 (USA).png
    assets.steam: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/n64/miximages/Mario Kart 64 (USA).png
    
    game: Star Fox 64 (USA)
    file: /home/runner/work/_temp/px/MyGames/Nintendo 64/Star Fox 64 (USA).z64
    assets.box_front: /home/runner/work/_temp/px/MyGames/Nintendo 64/media/Star Fox 64 (USA)/boxFront.png
    assets.screenshot: /home/runner/work/_temp/px/MyGames/Nintendo 64/media/Star Fox 64 (USA)/screenshot.png
    assets.steam: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/n64/miximages/Star Fox 64 (USA).png
    

### psx/metadata.pegasus.txt
    # Pegasus X game directory: Sony PlayStation
    # Made by Pegasus X from the ROM folder /home/runner/work/_temp/px/MyGames.
    # Pegasus X writes here (imports, Settings > Library, the scraper).
    # folder: /home/runner/work/_temp/px/MyGames/psx
    
    collection: Sony PlayStation
    shortname: psx
    
    game: Crash Bandicoot
    file: /home/runner/work/_temp/px/MyGames/psx/Crash Bandicoot (USA).chd
    sort-by: Crash Bandicoot (USA)
    genre: Platformer
    description: From the folder's own gamelist.xml.
    assets.box_front: /home/runner/work/_temp/px/MyGames/psx/media/crash.png
      /home/runner/work/_temp/px/MyGames/psx/skraper/box2d/Crash Bandicoot (USA).zip
    
    game: Final Fantasy VII (USA)
    file: /home/runner/work/_temp/px/MyGames/psx/Final Fantasy VII (USA).m3u
    
    game: Ape Escape (USA)
    file: /home/runner/work/_temp/px/MyGames/psx/Extra/Ape Escape (USA).chd
    

### snes/metadata.pegasus.txt
    # Pegasus X game directory: Nintendo SNES (Super Nintendo)
    # Made by Pegasus X from ES-DE (/home/runner/work/_temp/px/storage/ES-DE).
    # Pegasus X writes here (imports, Settings > Library, the scraper).
    # folder: /home/runner/work/_temp/px/storage/ROMs/snes
    
    collection: Nintendo SNES (Super Nintendo)
    shortname: snes
    
    game: Chrono Trigger (my own file)
    file: /home/runner/work/_temp/px/storage/ROMs/snes/Chrono Trigger (USA).sfc
    description: Written in my own metadata file: it wins over the ES-DE import.
    assets.screenshot: /home/runner/work/_temp/px/storage/ES-DE/downloaded_media/snes/screenshots/Chrono Trigger (USA).png
    
    game: Super Mario World
    file: /home/runner/work/_temp/px/storage/ROMs/snes/Super Mario World (USA).sfc
    sort-by: Super Mario World (USA)
    developer: Nintendo EAD
    publisher: Nintendo
    genre: Platform
    players: 2
    release: 1990-11-21
    description: Mario is having a vacation in Dinosaur Land when he learns that Princess Peach Toadstool has been kidnapped by the evil King Koopa Bowser. When Mario starts searching for her he finds a giant egg with a dinosaur named Yoshi hatching out of it. Yoshi tells Mario that his fellow dinosaurs have been imprisoned in eggs by Bowser's underlings. The intrepid plumber has to travel to their castles, rescue the dinosaurs, and eventually face King Koopa himself, forcing him to release the princess.
      .
      Super Mario World is a jump-and-run platformer and a follow-up to Super Mario Bros. 3. The gameplay is similar to previous installments and includes fast-paced platforming in (mostly) side-view stages populated by various enemies and traps, which must be completed within time limits. Mario has access to his traditional power-ups — the Super Mushroom that makes him bigger, the Fire Flower that allows him to throw little fireballs at enemies, and others. A new power-up is the Cape Feather: when wearing the cape, Mario can spin to hurt enemies, and fly as long as the player is able to sustain his balance in the air. Another new feature is the possibility to ride the dinosaur Yoshi, who can eat enemies and gain special attacks from some of them. In order to unlock certain areas, it is sometimes necessary to physically pick up and carry items (e.g. keys) and use them in appropriate places.
      .
      The game's world is divided into several large areas, each containing a number of stages. Outdoor scenarios prevail, but there are also many underground and underwater levels. Each large area has at least one ghost house, typically containing more elaborate traps and ghostly non-destructible enemies. In order to venture to the next area, the player usually has to visit a castle (a more difficult stage with harder enemies and dangerous traps) and defeat the boss enemy guarding it.
      .
      Many stages have secret exits, which lead to optional areas, such as fortresses with recurrent boss enemies, interconnected stages of the Star World, and Switch Palaces, which allow the player to materialize specifically colored bricks in all the stages, gaining access to power-ups and other secrets. Secret areas may also open access to other dinosaur types, with different abilities such as breathing fire or flying.
    rating: 95%
    assets.box_front: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/covers/Super Mario World (USA).png
      /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/3dboxes/Super Mario World (USA).png
    assets.cartridge: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/physicalmedia/Super Mario World (USA).png
    assets.logo: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/marquees/Super Mario World (USA).png
    assets.screenshot: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/screenshots/Super Mario World (USA).png
    assets.steam: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/miximages/Super Mario World (USA).png
    assets.background: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/fanart/Super Mario World (USA).jpg
    assets.banner: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/banners/Super Mario World (USA).png
    assets.box_back: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/backcovers/Super Mario World (USA).jpg
    assets.box_spine: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/spines/Super Mario World (USA).jpg
    assets.titlescreen: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/titlescreens/Super Mario World (USA).png
    
    game: The Legend of Zelda - A Link to the Past (USA)
    file: /home/runner/work/_temp/px/storage/ROMs/snes/The Legend of Zelda - A Link to the Past (USA).sfc
    genre: Action

### switch/metadata.pegasus.txt
    # Pegasus X game directory: Nintendo Switch
    # Made by Pegasus X from the ROM folder /home/runner/work/_temp/px/MyGames.
    # Pegasus X writes here (imports, Settings > Library, the scraper).
    # folder: /home/runner/work/_temp/px/MyGames/switch
    
    collection: Nintendo Switch
    shortname: switch
    
    game: Super Mario Odyssey
    file: /home/runner/work/_temp/px/MyGames/switch/Super Mario Odyssey.nsp
    

### switch2/metadata.pegasus.txt
    # Pegasus X game directory: Nintendo Switch 2
    # Made by Pegasus X from the ROM folder /home/runner/work/_temp/px/MyGames.
    # Pegasus X writes here (imports, Settings > Library, the scraper).
    # folder: /home/runner/work/_temp/px/MyGames/switch2
    
    collection: Nintendo Switch 2
    shortname: switch2
    
    game: Donkey Kong Bananza
    file: /home/runner/work/_temp/px/MyGames/switch2/Donkey Kong Bananza.nup
    description: Donkey Kong and His New Partner Pauline Smash, Bash, Climb and Sing Their Way Through a Brand-New 3D Platforming Action Adventure.
      .
      Bash through just about anything with the raw power of Donkey Kong! Crash through walls, carve tunnels with your fists, punch straight down into the ground, and even tear off chunks of terrain to swing around and throw – the more you smash, the more secrets you’ll open up to explore.
    developer: Nintendo EPD
    genre: Adventure
      Platform
    players: 2
    publisher: Nintendo
    rating: 88%
    release: 2025-07-17
    assets.background: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/switch2/fanart/Donkey Kong Bananza.jpg
    assets.box_back: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/switch2/backcovers/Donkey Kong Bananza.jpg
    assets.box_front: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/switch2/covers/Donkey Kong Bananza.jpg
    assets.box_spine: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/switch2/spines/Donkey Kong Bananza.jpg
    assets.logo: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/switch2/marquees/Donkey Kong Bananza.png
    assets.screenshot: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/switch2/screenshots/Donkey Kong Bananza.png
    assets.steam: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/switch2/miximages/Donkey Kong Bananza.png
    x-esrb: E10+
    
