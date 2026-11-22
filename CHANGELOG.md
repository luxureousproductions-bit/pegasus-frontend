# Pegasus X changelog

Newest first. Each entry is one commit on `pegasus-x`, so any of them can be
rolled back on its own with `git revert <commit>`.

## 2026-11-22

### Genres: a genre's spellings side by side

- **What:** in Library > Genres, a genre counted from more than one spelling
  ("51 games · 2 spellings") lists its spellings at the top of its page,
  above its games: "2 spellings, counted as one genre here:", then one row
  each, the most used first. A row shows the spelling exactly as written,
  between ‹ ›, with its game count (one game in amber). In every spelling
  after the first, the characters that differ from the first are amber and
  underlined (a different apostrophe, letter case, an extra space shown
  as ·); a space that isn't a plain one (non-breaking, tab, wide) is shown
  as ⍽ or ⇥ in any spelling. Under it, in words: "extra ’ U+2019 · missing '
  U+0027", "extra · U+0020 · missing · U+0020 ×2 · letter case" (the
  first: "the most used"). Each game row then says which spelling it has
  (" · spelling 2"). The spelling rows can be moved to with the d-pad (so
  a long list scrolls through them) and do nothing on A.
- **Why:** "2 spellings" said that the files differ, not how: the only way
  to see it was to open the games one by one.
- **Not changed:** still view-only: no file is written. How genres are
  counted (build 2228's key), the genre list, the log line, opening a game
  and Back. The metadata editor and its keyboard.
- **Tests:** `test_GenreKey`: the character-by-character comparison
  (apostrophe, letter case), the marks (apostrophe, a doubled space, a
  non-breaking space, letter case; plain spaces kept), the text escaped
  (a genre can't make markup), the note in words. The test environment's
  fixture now has two genres of two spellings each (Platform / platform,
  Shoot'em Up / Shoot’em Up, see `fixture.sh`); `03-genre-games` and the
  new `03-genre-spellings` show their pages, and `03-genre-game` opens the
  first game below the spellings.
- **Files:** `src/frontend/menu/settings/GenreKey.js` (`diff`, `markup`,
  `note`), `src/frontend/menu/settings/GenresScreen.qml`,
  `tests/frontend/genrekey/test_GenreKey.cpp`, `tests/testenv/fixture.sh`,
  `tests/testenv/run.sh`.
- **Commit:** "Genres: a genre's spellings side by side".

## 2026-11-21

### Genres: spellings that only look different count as one

- **What:** Library > Genres counts a genre under a matching key: letter
  case ignored, every apostrophe (' ’ ‘ ‛ ʼ ` ´ ′) as ', every run of spaces
  (non-breaking and other wide or thin spaces too) as one space, the ends
  trimmed, and the same spacing around "/". A genre with "/" stays one genre
  ("Beat'em Up / Fantasy" isn't split). A row shows the spelling most of its
  games have (ties: A to Z), and its games are every game with any of its
  spellings. When a row covers more than one spelling, its value says so
  ("51 games · 2 spellings"), since the files still differ. The one-off
  amber and the header use the merged counts. Each merge is logged once per
  read, with every spelling, its game count and its character codes
  (`Genres: "..." counts 2 spellings as one: "..." (50): U+0042 ... | ...`).
- **Why:** a game moved from "Beat'em / Fantasy" (1 game) to "Beat'em Up /
  Fantasy" (50 games) still showed two rows that looked alike (1 and 50
  games). The save reaches the game (`apply_scraped` sets its genre list)
  and Back reads the genres again, so the two rows were two different
  texts: Genres keyed them exactly as written (only trimmed), and the 50
  games' genre and the typed one differ in a character that looks the same
  (the Pegasus X keyboard types a straight '; the log line shows the real
  difference on the device).
- **Not changed:** nothing is rewritten: Genres stays view-only and the files
  keep their spellings. The metadata editor (its save, keyboard and
  suggestions), Back from a game, the LB/RB/LT/RT keys, the folder work,
  themes, the Scraper, the ES-DE import and the collection merge.
- **Tests:** `test_GenreKey` (new, runs `GenreKey.js` in a JS engine): the
  reported case (1 + 50 games: two texts, one row of 51 with 2 spellings),
  each apostrophe, space, "/" spacing and case variant, "/" not split,
  other genres apart, the spelling shown (most games, ties A to Z), the
  spellings count, a game with two spellings counted once, the log's codes.
- **Files:** `src/frontend/menu/settings/GenreKey.js` (new),
  `src/frontend/menu/settings/GenresScreen.qml`, `src/frontend/frontend.qrc`,
  `tests/frontend/` (new), `tests/tests.pro`, `tests/CMakeLists.txt`.
- **Commit:** "Genres: spellings that only look different count as one".

### Genres moves into Library

- **What:** Genres is a row in Library's "Your library · N games" section,
  under Needs your check (Complete my library, Needs your check, Genres),
  and no longer in Settings > Gaming > Scraper. The page is the same: every
  genre with its game count (one-offs in amber), a genre lists its games, a
  game opens on its page in Library, Back returns to the genre's games with
  the counts read again, and Back again to Library's first page on the
  Genres row. Same keys (LB/RB top or bottom, LT/RT 10 rows). Its title
  reads "Library > Genres" (was "Scraper > Genres"). The test environment
  walks through it in `03-library` (`03-genres...`), no longer in
  `04b-scraper-section`.
- **Why:** genres are part of the library, not the scraper.
- **Not changed:** the rest of the Scraper (its section is as in build 2222)
  and of Library, the build 2219 menu scrolling and the build 2224-2226
  folder work. A game opened from Genres is now Library's own game page
  (before, the Scraper opened a second Library on top).
- **Files:** `src/frontend/menu/settings/MetadataEditor.qml`,
  `GenresScreen.qml`, `ScraperScreen.qml`, `ScraperSection.qml`,
  `tests/testenv/run.sh`.
- **Commit:** "Library: Genres moves here from the Scraper".

### Test environment: the Folders steps, fixed

- **What:** `08b-folders` takes the remove screenshots on the SD card's
  folder (one key press too many reopened the picker), and `08c-folder-checks`
  starts from a clean SNES system (the SD card's folders taken out first if a
  step before left them in).
- **Why:** in build 2224's run the folder stayed in after `08b`, so three
  checks failed for that reason only (the add refused as "already listed",
  and the "as it was" comparison counted the SD games).
- **Not changed:** Pegasus X itself; only the test environment's steps (the
  checks run in its build only).
- **Tests:** the test environment's "ROM folders per system".
- **Files:** `tests/testenv/run.sh`, `src/backend/model/internal/settings/Settings.cpp`
  (the test environment's checks).
- **Commit:** "Test environment: Folders steps start clean".

### Test environment: the Folders picker step, fixed

- **What:** `08b-folders` presses LB in the picker again (it opens on the
  drives; LB goes to the last folder browsed, the SD card's ROMs).
- **Why:** without it the picker stayed on the drives, so the add and remove
  screenshots of build 2225's run show the picker, not the Folders view.
- **Not changed:** Pegasus X itself and the checks (`08c-folder-checks`).
- **Tests:** the test environment's `08b-folders` screenshots.
- **Files:** `tests/testenv/run.sh`.
- **Commit:** "Test environment: Folders picker step uses LB".

### Game directory: a system's ROM folders, one at a time

- **What:** a system's menu in Settings > Game directory has "Folders": each of
  its ROM folders with where it is (internal storage, SD card), whether it's
  there ("not found" in amber when not), and how many of the system's games
  come from it (counted from its file, so a missing SD card still shows them).
  "Add a folder..." picks another one with the folder picker (internal or SD
  card), also for systems that have folders already: its games are added to
  the system, and everything there stays (your details, art, emulators,
  launch commands, hidden games and favorites; your own metadata and ES-DE's
  entries aren't rewritten). A folder with other systems' folders in it
  (snes, gba...) adds only this system's one. A folder is refused, with a short
  reason, when it's listed already (any spelling: a trailing slash, /sdcard
  or /storage/self/primary for /storage/emulated/0, letter case on Android),
  inside or around a listed one, another system's (named), or has only other
  systems' folders. A on a folder: "Stop watching it, keep its games" or
  "Remove it and its games"; either way that folder only, that system only,
  and its "# folder: " (and "# own metadata: ") line goes. The main page's
  "ROM folders watched for new games" has one row per folder now (system,
  path), and removing one is that folder only.
- **Why:** the watched folders were grouped by their parent folder, so
  removing one row stopped watching every system's folder under it, and
  "remove games too" took the games under that parent out of every system,
  leaving the systems' "# folder: " lines behind. A system could only get a
  folder picked when it had none.
- **Not changed:** how folders are stored (`pegasusx-rom-folders.txt`, one
  "folder<TAB>system" line per system folder, and the system files' "# folder: "
  lines); game cache schema 3. Rescans and startup never delete games or
  forget a folder that isn't there (an SD card out); they skip it, as before.
  The same file name in two folders is two games (the duplicate log notes
  it); the same path is one game. Untouched libraries load the same.
  game_dirs.txt, migration, the game directory layout, ES-DE import, themes,
  launching and metadata precedence are unchanged.
- **Tests:** `test_GameDirectoryFolders` (new): taking one folder out leaves
  the folder next to it (same parent, same start of the name), a listed
  folder inside it and the other system alone, with no line left, your
  details and launch commands kept; keeping games keeps every entry; an
  unlisted folder changes no file; a missing folder keeps its games counted;
  listed, re-spelled, inside and around folders are refused; the same file
  name in two folders is two games and one path from two files is one game.
  Test environment: `08b-folders` (screenshots: system menu, Folders view,
  picker, added, remove, removed) and `08c-folder-checks` (the add, the
  refusals, a folder with two systems' folders, the SD card out with a rescan,
  stop watching, remove; the summary's "ROM folders per system").
- **Files:** `src/backend/providers/GameDirectory.h/.cpp`,
  `src/backend/providers/esde/EsdeProvider.h/.cpp`,
  `src/backend/model/internal/settings/Settings.h/.cpp`,
  `src/backend/platform/AndroidHelpers_testenv.cpp` (a second test storage),
  `src/frontend/menu/settings/GameDirectory.qml`,
  `tests/backend/providers/gamedirectory/` (new), `tests/backend/providers/providers.pro`,
  `tests/CMakeLists.txt`, `tests/testenv/fixture.sh`, `tests/testenv/run.sh`.
- **Commit:** "Game directory: a system's ROM folders, one at a time".

## 2026-11-20

### Scraper > Genres: every genre and its games

- **What:** the Scraper (Settings > Gaming > Scraper) has a "Genres" row
  after Log. It lists every genre in the library, A to Z, with how many
  games have it; one-offs show "1 game" in amber, and the top line says how
  many genres and how many with only one game. A genre lists its games
  (title, collection); a game opens in Library on that game's page, to fix
  its genre there. Back from the game returns to the genre's games, with
  the counts read again (a game whose genre was changed drops out of the
  list; a genre with no games left goes back to the genre list). Same keys
  as the Scraper's lists: LB/RB top or bottom, LT/RT 10 rows. The genres
  Android uses to sort apps ("Application", "Emulator") aren't listed, as
  in the keyboard's suggestions. The Scraper's title is now "Scraper"
  (was "Library > Scraper"). The test environment walks through Genres
  (`04b-genres...`).
- **Why:** one-off genres (typos, near duplicates) are easy to spot and fix
  one game at a time.
- **Not changed:** nothing is merged or rewritten automatically; genres
  change only in a game's Library page, as before. Library opened from
  Genres is the same Library (its "Scraper options..." brings the Scraper
  to the front; Back returns to the game).
- **Files:** `src/frontend/menu/settings/GenresScreen.qml` (new),
  `ScraperSection.qml`, `ScraperScreen.qml`, `MetadataEditor.qml`
  (`openGameDirect`), `src/frontend/frontend.qrc`, `tests/testenv/run.sh`.
- **Commit:** "Scraper: Genres, every genre with its game count".

### Fix: Library and Scraper open again (build 2221)

- **What:** in build 2221, Settings > Gaming > Library and > Scraper didn't
  open. Their "only the window in front answers" check for the interactive
  pick window used `Connections.enabled`, which QtQuick 2.6 (what these
  files import) doesn't have, so the whole screen failed to load ("Connections.enabled
  is not available in QtQuick 2.6"). The check is now made inside the
  handler instead, with the same result. The test environment now opens
  Settings > Gaming > Scraper too (`04b-scraper-section`), so a screen that
  doesn't load shows there.
- **Why:** the build and the test environment passed, but the test only
  showed it through a warning and missing Library lines.
- **Not changed:** what the Library and the Scraper do.
- **Files:** `src/frontend/menu/settings/MetadataEditor.qml`,
  `src/frontend/menu/settings/ScraperSection.qml`, `tests/testenv/run.sh`.
- **Commit:** "Fix: Library and Scraper load again (QtQuick 2.6)".

### Scraper gets its own row in Settings > Gaming

- **What:** Settings > Gaming now reads Game directory, Library, Scraper,
  RetroAchievements. "Scraper..." opens the scraper's pages (sources,
  content, pictures, accounts, the running scrape) as they were, in a small
  host screen (`ScraperSection.qml`) that also has its own window to pick
  each game's match in interactive mode. The Scraper row inside Library is
  gone. A game's "Scrape this game > Scraper options..." opens this same
  Scraper on top of the game; Back returns to the game.
- **Why:** the scraper is used for the whole library, not one collection,
  and lived three levels down inside Library.
- **Not changed:** the scraper's pages and settings, "Scrape this game" and
  "Scrape this collection..." in Library (with Library's own pick window).
  Only the window in front answers an interactive pick (the Scraper's or
  Library's), so two never open at once. No other settings, themes or the
  build 2219 menu scrolling.
- **Files:** `src/frontend/menu/settings/ScraperSection.qml` (new),
  `SettingsMain.qml`, `SettingsScreen.qml`, `MetadataEditor.qml`,
  `src/frontend/frontend.qrc`.
- **Commit:** "Settings: Scraper gets its own row in Gaming".

### Keyboard: a picked suggestion no longer adds a comma

- **What:** in Developer, Publisher and Genres (comma-separated lists),
  picking a suggestion from the row above the keys puts in just the name;
  the keyboard no longer adds ", " after it. Type a comma (on the letter
  page) and a space yourself when you want another item.
- **Why:** the automatic ", " looked like Space adding a comma: Space itself
  always typed a plain space (the space bar, Y, or a real keyboard), and
  nothing else in the menus changes text while typing. The comma came from
  `sugBar.apply` in `OnScreenKeyboard.qml`, which added ", " after a picked
  name in a comma list to make room for the next one.
- **Not changed:** suggestions themselves, word suggestions in other fields
  (they still add a space after the word), lists of lines (a new line after
  the item), and how lists are saved (split at commas).
- **Files:** `src/frontend/menu/settings/common/OnScreenKeyboard.qml`.
- **Commit:** "Keyboard: a picked suggestion adds no comma".

## 2026-11-19

### Built-in menus keep up with a held D-pad

- **What:** every list in Pegasus X's own menus moves focus at once
  (`highlightMoveDuration: 0`): Settings, Game directory, the Library /
  metadata editor (its rows, art strip, emulator and value chooser, dialog
  buttons), the Scraper pages and their chooser, RetroAchievements, the key
  and gamepad editors, the small value pop-ups (MultivalueBox), the folder
  picker's shortcuts and first-start setup. Lists that kept the current row
  in a band (ApplyRange) still do, now without a slide. On the Scraper's
  running page, the rows that change while scraping (progress, counts, the
  last game's picture, the last games) are read live by the rows themselves
  (`liveRow`), so a progress update no longer rebuilds the whole list; it's
  rebuilt only when Stop turns into Done, the first picture arrives, or a
  game is added to the last games (at most 8).
- **Why:** on Android the D-pad repeat comes from Android itself (Qt's
  gamepad module turns each key-down, repeats included, into a new Up/Down
  press, about 20 a second once it repeats). A 150 ms slide (300 ms in the
  gamepad editor; Qt's default speed where nothing was set) restarted on
  every press, so the list trailed behind focus and kept moving after the
  D-pad was let go. Rebuilding the Scraper's list on each progress update
  made that page jump and slowed every press.
- **Not changed:** the D-pad repeat (Android's delay and rate), touch
  flicking (Qt's own speeds), the option pop-ups (PegasusOptionsDialog), the
  other pages' rows, and anything in themes.
- **Files:** `src/frontend/menu/settings/` `SettingsMain.qml`,
  `GameDirectory.qml`, `MetadataEditor.qml`, `ScraperScreen.qml`,
  `RetroAchievementsScreen.qml`, `KeyEditor.qml`, `GamepadEditor.qml`,
  `FirstRunWizard.qml`, `common/MultivalueBox.qml`,
  `keyeditor/KeyEditorEntry.qml`, `gamedireditor/FilePicker.qml`.
- **Commit:** "Menus: focus moves at once, Scraper progress keeps its list".

## 2026-11-17

### Duplicate game files are noted in the log

- **What:** each time the library is read (a full scan or the game cache),
  game files with the same file name and extension under different paths
  are written to `lastrun.log` as warnings, across all collections and
  sources (Pegasus metadata files, ES-DE imports, ROM folder scans). For each
  name: "Library: duplicate game file `<name>`, <n> copies (only noted,
  nothing was changed):", then one line per copy with its path, its game's
  title, its collections, and where it came from ("a metadata file", "an ES-DE
  import", or "a ROM folder scan or another provider (no metadata entry)"),
  and at the end how many names have copies. Names are compared exactly,
  case included.
- **Why:** the same ROM in two places (eg. an imported copy and a folder
  copy) can show twice or launch the wrong copy; the log shows it before it
  causes trouble.
- **Not changed:** nothing is removed, merged, hidden or moved. The same
  path in two collections is one game (since build 2216), so not a
  duplicate. Apps and other URI entries aren't checked. Imports, migration,
  `gamedirectory/`, launching and the theme are untouched.
- **Tests:** `test_PegasusProvider::duplicate_files` (fixture
  `tests/backend/providers/pegasus/data/dupfiles`): one name under two paths
  (a metadata file and a ROM folder scan) is the only duplicate; the same
  path in two collections, the same name with another extension and two
  URIs aren't; the games and collections are the same after the check.
- **Files:** `src/backend/providers/LibraryRules.h`,
  `src/backend/providers/LibraryRules.cpp`,
  `tests/backend/providers/pegasus/test_PegasusProvider.cpp`,
  `tests/backend/providers/pegasus/data/data.qrc`,
  `tests/backend/providers/pegasus/data/dupfiles/`.
- **Commit:** "Library: note duplicate game files in the log".

### Event scripts run on Android

- **What:** port of original Pegasus' commit 3702598c ("Executes .sh for
  Android, like other platforms", #1195). On Android, the files in a settings
  folder's `scripts/<event>/` folders (`game-start`, `game-end`, `quit`,
  `config-changed`, `settings-changed`, `controls-changed`, ...) are run by
  `/system/bin/sh <file> <args>` through `MainActivity.runScript` (Java
  `ProcessBuilder`, the script's output goes to logcat as `Pegasus/script`).
  Any readable file counts there, since files on Android storage can't be
  marked executable. `lastrun.log` shows "Running `<event>` scripts..." and
  each script, as on desktop.
- **Why:** on Android only executable files were looked for, and shared
  storage has none, so no script ever ran.
- **Not changed:** desktop still runs executable files only, with
  `QProcess::execute`. Where scripts are looked for and when each event fires
  are unchanged. As in original Pegasus, on Android `game-end` only fires
  when a launch fails (a successful launch hands over to the emulator and
  isn't waited for), and the scripts run one after another, waited for.
- **Tests:** `test_ScriptRunner` (new, desktop): an executable `game-start`
  script runs with the game's path, a file that isn't executable doesn't,
  and an event with no folder runs nothing.
- **Files:** `src/backend/ScriptRunner.cpp`,
  `src/backend/platform/AndroidHelpers.h`,
  `src/backend/platform/AndroidHelpers.cpp`,
  `src/app/platform/android/src/org/pegasus_frontend/android/MainActivity.java`,
  `tests/backend/scriptrunner/`, `tests/backend/backend.pro`,
  `tests/CMakeLists.txt`.
- **Commit:** "Android: run event scripts (port of original Pegasus 3702598c)".

## 2026-11-16

### A game listed in two metadata files stays in both collections

- **What:** when a metadata file lists a game file (a ROM) that a block read
  earlier already has (eg. your own file, then a system's file), the game is
  now added to that file's collections too (`add_to_file_collections` in
  `Metadata::apply_game_entry`, file branch only). With no launch command of
  its own, it gets that collection's, as any game in it does. The rest of the
  block works as before: it only fills the game's gaps, or notes an import
  conflict. The game cache's schema goes to 3.
- **Why:** the second block only filled the first game's gaps, so the game
  was left out of the second file's collections. Eg. a ROM in a collection of
  yours (no launch line) and in the SNES file showed only in yours, and a game
  directory written from that library didn't list it in the SNES file.
- **Not changed:** your details and your own launch command still win (the
  second block only fills gaps). Android apps and other URIs are untouched
  (the URI branch stays as it was). Games are still matched by file, never by
  title: the SNES and DS Chrono Trigger stay two games. A library where no ROM
  is listed twice loads as before; with the cache schema at 3, the first start
  after the update does one full scan. The game directory, its migration and
  the same-title dedupe in LibraryRules are unchanged. One edge: a single
  file with two `collection:` headers that lists the same ROM under both now
  puts it in both.
- **Tests:** `test_PegasusProvider::game_in_two_files` (fixture
  `tests/backend/providers/pegasus/data/dupcoll`): one ROM in your file and
  in the SNES file is in both collections with SNES's launch command; your
  own launch command and details stay; the DS Chrono Trigger is a game of its
  own; an Android app in your file and in the import stays only in yours.
- **Files:** `src/backend/providers/pegasus_metadata/PegasusMetadata.cpp`,
  `src/backend/providers/GameDataCache.cpp`,
  `tests/backend/providers/pegasus/test_PegasusProvider.cpp`,
  `tests/backend/providers/pegasus/data/data.qrc`,
  `tests/backend/providers/pegasus/data/dupcoll/`.
- **Commit:** "Metafiles: a game listed in two files stays in both collections".

## 2026-11-15

### "Icon" in Library game info for Android apps

- **What:** in Settings > Library game info (the metadata editor), an
  imported Android app (first file `android:<package>`) has its logo slot
  labelled "Icon" instead of "Logo", also in "Add or replace art...". With no
  logo of yours, the slot shows the app's launcher icon
  (`image://androidicons/<package>`, the same image as `assets.tile`), in the
  row and in the preview. A picture you pick there replaces it and is saved
  as the logo, as logos are saved now. Removing it brings the icon back in
  the slot (X on the row then undoes the removal, as before).
- **Why:** an app's icon is what stands for its logo, so the slot shows it
  rather than nothing.
- **Not changed:** the icon is only shown: it's never written to the
  metadata file or `assets.logo`, so `pegasusx-user-art` lists `logo` only
  for a logo you picked. Every other game keeps "Logo" and its behaviour.
  Asset order, `keep_one_per_kind`, `assets.tile`, the Play Store lookup and
  launching are untouched.
- **Files:** `src/frontend/menu/settings/MetadataEditor.qml` only.
- **Commit:** "Library: show an Android app's icon as its Icon".

### Tell themes which art of an Android app is yours

- **What:** an imported Android app (first file `android:<package>`) gets the
  extra field `pegasusx-user-art`: the kinds of art whose first picture is a
  local file (a `file:` URL), named as in `pegasusx-hidden-art` (`box_front`,
  `logo`, `background`, `screenshot`, ...). Those are the pictures you picked
  in Settings > Library. No field when there's none; other games never get
  it. It's set in `apply_library_rules` (after `keep_one_per_kind` and the
  `box3d` step), which runs after both a GameDataCache restore and a full
  scan. When you save art in Settings > Library (or scrape the app), it's
  set again on the loaded game, also after art you removed is taken off, and
  themes are told (`extraChanged`) without a restart.
- **Why:** the Android import adds the launcher icon
  (`image://androidicons/<package>`) and Play Store web links after your art,
  so a theme could only guess from the URL which picture was yours. Play
  Store images are web links, so they're never listed.
- **Not changed:** asset order, `keep_one_per_kind`, the launcher icon in
  `assets.tile`, the Play Store lookup, launching. The kind names now come
  from one list shared with `remove_listed` (same names, same matching).
- **Files:** `src/backend/model/gaming/Assets.h`,
  `src/backend/model/gaming/Assets.cpp`, `src/backend/providers/LibraryRules.h`,
  `src/backend/providers/LibraryRules.cpp`,
  `src/backend/model/internal/settings/Settings.cpp`.
- **Commit:** "Android apps: mark the art you picked for themes".

### Free memory while a game runs

- **What:** when Pegasus X goes into the background (`applicationStateChanged`
  to `ApplicationHidden` or `ApplicationSuspended`, e.g. a game launched), the
  theme stays loaded as before, but its caches are let go:
  `QQuickWindow::releaseResources()` on the theme's window (the scene graph's
  unused graphics) and `trimComponentCache()` on the QML engine (components
  nothing uses). Nothing extra happens when Pegasus X is back
  (`ApplicationActive`); what's needed is made again as it's drawn. Android's
  low-memory signal (`SystemStatus.java`, `onLowMemory()`) triggers the same
  release. Each release is logged ("Frontend: released cached graphics and
  unused QML components (...)").
- **Why:** with the theme kept loaded during games (instant return), its
  cached graphics stayed in memory too, which leaves less for the emulator and
  makes it likelier that Android stops Pegasus X in the background.
- **Not changed:** the theme isn't torn down or reloaded, and session
  tracking (play time, `Backend::onProcessLaunched`) is untouched.
- **Files:** `src/backend/FrontendLayer.h`, `src/backend/FrontendLayer.cpp`,
  `src/app/platform/android/src/org/pegasus_frontend/android/SystemStatus.java`.
- **Commit:** "Free the theme's caches while a game runs".

### Clear stale settings locks at startup

- **What:** at startup, before the backend is created, any leftover
  `*.ini.lock` and `*.ini.lock.rmlock` files in the Pegasus X settings folder
  (`paths::writableConfigDir()`) are deleted, and each one is logged
  ("Startup: removed a stale settings lock: ..." in `lastrun.log`, and in
  logcat).
- **Why:** on Android, `pegasusx.ini` lives on external storage, where
  `flock()` isn't available (ENOSYS: "setNativeLocks failed: Function not
  implemented"). Qt 5.15 only treats a lock as stale when `flock()` works, and
  `QSettings::sync()` waits for the lock with no time limit. A lock file left
  behind by a crash or a kill could therefore block every later settings save.
  Pegasus X runs as a single instance, so a lock found at startup is always a
  leftover.
- **Files:** `src/app/main.cpp` only. Where settings are stored is unchanged.
- **Commit:** "Startup: clear stale settings locks".
