# Pegasus X changelog

Newest first. Each entry is one commit on `pegasus-x`, so any of them can be
rolled back on its own with `git revert <commit>`.

## 2026-11-22

### Watched-folder refresh: no library re-read while the theme is in use

- **What:** when the background check of the watched ROM folders (build
  2236) made lists again, the library was read again as soon as the theme
  was in front, a few seconds after the start: the theme was closed, the
  loading splash came back and your place (the Guide too) was lost. Now
  it's only noted ("ROM folders: new or removed games, read when the theme
  is next left and returned to" in the log), and the re-read happens when
  Pegasus X comes back from the background (a game, or the home screen),
  once the theme has its focus again, or when the menu is closed, or at the
  next start (the lists are already written). Pulling down the
  notification shade doesn't count as leaving.
- **Why:** reported on the handheld: a surprise reload while using the
  theme.
- **Not changed:** the background check itself, what it writes, nothing
  at all when nothing changed, Rescan, Reload all games, the genre work.
- **Files:** `src/frontend/main.qml`.
- **Commit:** "Watched-folder refresh: no library re-read while the theme is in use".

### Fix: the test environment's one-game genre

- **What:** the fixture's one-game genre (Action) now comes from ES-DE's
  snes gamelist (A Link to the Past) instead of a second `genre:` line on
  Metroid Prime in the Pegasus metadata file.
- **Why:** build 2239's test run read that second line together with the
  first, as one genre "Shoot'em Up, Action", so the list had other rows than
  the steps expected and the steps that followed changed the wrong genre
  (fixture data only: the four game checks said FAILED). The app's own
  checks in that run passed (the old hidden= entry gone, search, filter
  and sort as logged).
- **Not changed:** the app.
- **Files:** `tests/testenv/fixture.sh`.
- **Commit:** "Fix: the test environment's one-game genre".

### Genres: search, one-game filter and sort instead of hiding

- **What:** Library > Genres no longer hides genres. "Hide genres...", its
  page with the Shown/Hidden switches, the "N hidden" text and the setting
  behind it (`hiddenGenres`/`setHiddenGenres`, `[genres] hidden=` in
  pegasusx.ini) are gone; a `hidden=` entry an earlier build saved is
  removed at the next start (logged), and nothing reads it any more. In
  their place, three rows above the list (only the view: no metadata is
  written, and they start fresh each time Genres is opened):
  - **Search genres...:** Pegasus' keyboard; the list narrows as you type
    (part of a name; letter case, apostrophes and spacing don't matter, and
    any of a genre's spellings counts). Start or Done keeps it, B puts the
    previous search back; an empty search shows the whole list. The row
    shows the search in quotes.
  - **Only genres with 1 game:** On/Off.
  - **Sort:** A to Z (the default, as before) → Most games first → Fewest
    games first → A to Z; the same count stays A to Z.
  The header still counts the whole library ("N genres · M with only one
  game") and adds " · K shown" while a search or the filter narrows it;
  "No genre matches." when nothing is left. A to Z stays the default
  because it's what the list did and what a search reads best with.
- **Why:** hiding wasn't what was wanted; filtering and sorting the list
  was.
- **Not changed:** the counting, a genre's page (spellings, Change this
  genre..., its games), the pick and confirm pages, the keyboard for
  "Type a genre...", Back, the keys (LB/RB top or bottom, LT/RT 10 rows).
  The rows A/tap opens or switches, like the other rows.
- **Tests:** `test_GenreKey` `view_search`, `view_only_ones`, `view_sort`.
  The test environment puts an old `hidden=platform` in pegasusx.ini before
  the first start and checks it's gone; the fixture's Metroid Prime also
  has Action (a one-game genre); new steps search ("shoot", as typed and
  kept), clear it, the 1-game filter and both sorts (screenshots, and a
  log line per view); the Hide genres steps are gone.
- **Files:** `src/frontend/menu/settings/GenresScreen.qml`,
  `src/frontend/menu/settings/GenreKey.js` (`view`),
  `src/backend/model/internal/settings/Settings.h/.cpp`,
  `tests/frontend/genrekey/test_GenreKey.cpp`, `tests/testenv/fixture.sh`,
  `tests/testenv/run.sh`.
- **Commit:** "Genres: search, one-game filter and sort instead of hiding".

### Android's earlier memory warnings are handled

- **What:** Pegasus X now listens to Android's onTrimMemory warnings
  (Android 13 sends them well before the last-ditch onLowMemory, which was
  the only one handled). What it does never unloads the theme:
  - in front, memory getting low (RUNNING_LOW): the QML engine's garbage is
    collected and unused components let go;
  - in front, critical (RUNNING_CRITICAL): the same, plus the window's
    unused graphics (`releaseMemory`, as onLowMemory and going to the
    background already do);
  - in the background list (BACKGROUND, MODERATE, COMPLETE): garbage
    collected and `releaseMemory`, but only while Pegasus X is still away
    when Qt gets to it. Qt doesn't run while Android has Pegasus X in the
    background, so a warning sent then is handled on the return; it's
    skipped then, because letting go at that moment would only slow the
    return (going to the background already released what it could).
  - RUNNING_MODERATE and UI_HIDDEN: nothing more.
  Each level is handled once while Pegasus X is away or until it's in
  front again; every warning is logged with its level.
- **Why:** with only onLowMemory, nothing was let go on Android's earlier
  warnings, and the theme's garbage stayed until Android was out of
  options.
- **Not changed:** onLowMemory, the release when going to the background,
  the theme kept loaded during a game (no rebuild, no splash).
- **Files:** `src/app/platform/android/src/org/pegasus_frontend/android/SystemStatus.java`
  (ComponentCallbacks2, `nativeTrimMemory`), `src/backend/FrontendLayer.h/.cpp`
  (`trimMemory`).
- **Commit:** "Android's earlier memory warnings are handled".

### Device listeners pause while Pegasus X is away

- **What:** while Pegasus X isn't in front (a game running, the screen off,
  the notification shade down), Android's frequent broadcasts aren't
  listened to: battery changes, Wi-Fi on/off and signal, airplane mode,
  volume and headphones, and the once-a-minute tick that read the
  controllers, the clock format and the Bluetooth devices' batteries. The
  30-second battery read stops too. Back in front, they're listened to
  again and everything is read at once (battery, Wi-Fi, sound,
  controllers, clock, Bluetooth batteries), so `api.device` is current
  within half a second, as the status check already restarts then. The
  6-hour update check no longer runs while Pegasus X is away: one that
  falls due then is made when it's back in front.
- **Why:** those broadcasts are delivered to Pegasus X's Java side even
  while a game runs, and each minute's tick read every controller and
  Bluetooth device (reflection, JSON). Qt's own timers already wait while
  Android has Pegasus X in the background (the app doesn't run Qt in the
  background), so the gain there is for the in-between states.
- **Not changed:** what `api.device` reports; Bluetooth connect/disconnect,
  the network, sound devices, controllers being plugged and the thermal
  status are still told as they happen (they're rare); the status check,
  the clock and free space were already paused; the start's update check.
- **Files:** `src/app/platform/android/src/org/pegasus_frontend/android/SystemStatus.java`
  (`pause`, `resume`), `src/backend/model/device/DeviceInfo.h/.cpp`,
  `src/frontend/main.qml`.
- **Commit:** "Device listeners pause while Pegasus X is away".

### Watched ROM folders are checked in the background, after the library is up

- **What:** the check of the watched ROM folders for added or removed games
  no longer runs at start before the library is read (the start waited for
  it, including the subfolder dates of the previous build); it runs in the
  background once the first library read is done. When it made lists
  again, the library is read again (as when leaving Library), but only
  while the theme is in front and Pegasus X active: not under the menu or a
  dialog, and not on a return from a game (no extra splash then). Else it
  waits for the menu to be closed; at the latest, the next start shows
  them (the lists are already written).
- **Safety:** one refresh or Rescan at a time (a lock: the second waits for
  the first, they write the same lists); Rescan and Reload all games ask a
  background refresh to stop after the folder it's on, and so does quitting.
  It starts once per run, and not while another library read is about to
  start. The lists are still written whole (a temporary file, then
  renamed), so a read never sees half a file.
- **Not changed:** what's looked at and written (build 2234's subfolder
  dates included), Rescan, Reload all games (still reads the folders before
  its reload), the test environment's imports.
- **Tests:** the test environment's summary shows each start's "Background
  refresh: N lists made again in M ms" line; the psx/Extra/ check (build
  2234) now relies on it.
- **Files:** `src/backend/providers/esde/EsdeProvider.h/.cpp` (lock,
  `cancel_rom_folder_refresh`), `src/backend/model/internal/settings/Settings.h/.cpp`
  (`startRomFolderRefresh`, `romFoldersRefreshed`), `src/backend/Backend.cpp`,
  `src/frontend/main.qml`, `tests/testenv/run.sh`.
- **Commit:** "Watched ROM folders are checked in the background, after the library is up".

### Game discovery skips the art folders

- **What:** when Pegasus X looks for game files in a system folder (ROM
  folder import, watched-folder refresh, ES-DE import, the game counts on
  the Add systems/Folders pages), the folder's own media/, .media/ and
  skraper/ (any letter case) are no longer walked. Pegasus' own scan of a
  metadata file's folder (a collection with `extensions:`) already left out
  media/; it now also leaves out Media/ and skraper/.
- **Why:** those folders hold art (often thousands of files); walking them
  cost time, and a file in them with one of the system's extensions (a
  .zip, a .png for PICO-8) would have been listed as a game.
- **Only the system folder's own art folders:** a folder called media
  deeper down (switch/Zelda/media) is still read, as before, and so is
  every other folder, also a folder that is one game.
- **Not changed:** art is still found in those folders (that's a separate
  read: the gamelist's paths, ES-DE's media, the media/ kinds); nothing is
  deleted or moved; metadata files that name a file in them still work.
- **Tests:** the fixture puts a .zip in psx/media/ and in psx/skraper/box2d/;
  the summary checks that neither is listed as a game and that Crash
  Bandicoot still has its psx/media/crash.png.
- **Files:** `src/backend/providers/esde/EsdeProvider.cpp` (`is_art_folder`,
  `find_game_groups`, `quick_game_count`),
  `src/backend/providers/pegasus_metadata/PegasusFilter.cpp`,
  `tests/testenv/fixture.sh`, `tests/testenv/run.sh`.
- **Commit:** "Game discovery skips the art folders".

### Watched ROM folders: games added in a subfolder are found

- **What:** when Pegasus X checks whether a watched ROM folder changed
  (at start, and on Reload all games), it now also reads the dates of the
  folder's subfolders, up to 3 levels down (switch/A/B/C), at most 1000
  folders per watched folder; hidden folders and media/, .media/, skraper/
  are left out of that walk (their art dates count as before). Only folder
  dates are read, never the files. A folder changed since its list was made
  is read again, as before.
- **Why:** a game copied into an existing subfolder (switch/Zelda/) changes
  only that subfolder's date, not the watched folder's, so it stayed out
  until a manual Rescan.
- **Limits:** a game more than 3 levels down, or in a tree of more than
  1000 folders (the log says so), still needs a Rescan if its folder is
  past what's read. A Rescan reads everything, as before.
- **Not changed:** what a refresh reads and writes, the forced Rescan, the
  art-folder dates, the watched list's file.
- **Tests:** the test environment's fixture has an empty psx/Extra/; after
  the first start a game is put in it, and the summary checks that the next
  start's refresh lists it (Ape Escape) without a Rescan.
- **Files:** `src/backend/providers/esde/EsdeProvider.cpp` (`folder_changed`),
  `tests/testenv/fixture.sh`, `tests/testenv/run.sh`.
- **Commit:** "Watched ROM folders: games added in a subfolder are found".

### Fix: Genres' result line and the test environment's genre check

- **What:** the line after "Change N games" no longer passes an extra
  argument for one game (Qt logged `QString::arg: Argument missing`), and
  a change to a spelling of the same genre now reads ""Shoot'em Up" is now
  spelled "Shoot'em Up" in 1 more game." (was "...is now "Shoot'em Up"...",
  which read as if nothing changed). The test environment keeps a copy of
  the game directory right after its Library steps (`after-genres/`) and
  checks the genres there.
- **Why:** build 2232's test run: the change was saved (`1 games saved in
  .../snes/metadata.pegasus.txt`, an "edit" backup before it), but the
  check read the files at the end of the run, after the scrape steps had
  written Super Mario World's genre again ("Platform" from LaunchBox,
  overwrite on), so it reported FAILED for that game.
- **Not changed:** what is saved and how; the pages; the other checks.
- **Files:** `src/frontend/menu/settings/GenresScreen.qml`,
  `tests/testenv/run.sh`.
- **Commit:** "Fix: Genres' result line and the test environment's genre check".

### Fix: test_GenreKey didn't compile in build 2231

- **What:** the `replace` and `same` tests held the JS function in a
  `const QJSValue`; `QJSValue::call` isn't const in Qt 5, so the test
  didn't compile and the test environment (which builds the tests too) and
  the compile check stopped at it. The two handles are no longer const.
- **Why:** build 2231's test environment and compile check failed on it
  (Android built: the app itself was fine).
- **Not changed:** the app; the tests' cases.
- **Files:** `tests/frontend/genrekey/test_GenreKey.cpp`.
- **Commit:** "Fix: test_GenreKey didn't compile in build 2231".

### Genres: change a genre in all its games

- **What:** a genre's page in Library > Genres starts with "Change this
  genre in all N games..." (one game: "...in its game..."). It opens a pick
  list: "Type a genre..." (Pegasus' own keyboard, your genres as
  suggestions; Start or Done takes it, B goes back) or any other genre you
  have, with its game count. A on one of the genre's spellings (build 2229)
  goes the same way with that spelling, to use it for the whole genre.
  Then a page shows what will happen before anything is written: "Replace
  "Platform" with "Platformer"" (or "Use the spelling ..."), "2 games
  change · 1 already as it would be · their other genres stay", "Keep as
  is" (selected first), "Change N games", and the games that change with
  their collection. Change writes them and returns to the genre list on the
  new genre, with a line saying what was done ("..." is now "..." in N
  games, or what couldn't be saved). Back on the confirm page returns to
  the pick list (or the genre's page, from a spelling).
- **How it changes a game:** every spelling of the chosen genre (its key)
  is replaced by the new one; the game's other genres stay as they are and
  in their order (Application and Emulator too); the new genre ends up
  once, where it first was (if the game had it already in another
  spelling, that one is kept, unless the change is to a spelling of the
  same genre). Games already as they'd be are left alone (not written).
- **How it's saved:** as Library saves a game's genre: into the
  collection's file in the game directory (the game's block added if it
  has none), never into a folder's own metadata or a gamelist. One write
  per collection, with one backup of the file first (reason "edit", as a
  hand edit; the last 3 are kept) instead of one per game. The loaded
  games show it at once (themes too), as after a game saved in Library; no
  read of the library is added. `save_games_metadata` (used by
  RetroAchievements) takes the backup's name as a new argument, "scrape"
  as before by default.
- **Not changed:** the counting, Hide genres... (build 2230), the metadata
  editor (its save, keyboard, suggestions), the RetroAchievements save
  (same backup name), artwork, search, launch commands, themes.
- **Tests:** `test_GenreKey` `replace` (12 cases: other genres kept in
  order, any spelling, two spellings once, the new one already there before
  or after, one spelling for all, unrelated duplicates and Application
  untouched, trimmed, a game without it, a "/" genre kept whole) and
  `same`. The test environment: `03-genre-pick`, `03-genre-type` ("Platformer"
  typed), `03-genre-confirm`, `03-genre-changed`, then Shoot'em Up's first
  spelling for the genre (`03-genre-spelling-confirm`, `-used`); the summary
  checks each game's genre in the game directory, that psx's own
  gamelist.xml and the Pegasus metadata file are as they were, and counts
  the "edit" backups. The step that opens a game moves down one row.
- **Files:** `src/backend/model/internal/settings/Settings.h/.cpp`
  (`saveGamesGenres`), `src/backend/providers/esde/EsdeProvider.h/.cpp`
  (`save_games_metadata`'s backup name), `src/frontend/menu/settings/GenreKey.js`
  (`replace`, `same`), `src/frontend/menu/settings/GenresScreen.qml`,
  `tests/frontend/genrekey/test_GenreKey.cpp`, `tests/testenv/run.sh`.
- **Commit:** "Genres: change a genre in all its games".

### Genres: hide genres from the list

- **What:** Library > Genres has a "Hide genres..." row under its header
  (its value: "none hidden" or "N hidden"). It opens a page listing every
  genre, each "Shown" or "Hidden" (amber); A or a tap switches it, saved at
  once. A hidden genre (all its spellings: it's kept by its key, as
  counted since build 2228) is left out of the Genres list; the header
  then adds " · N hidden". Back returns to the Hide genres... row.
- **Stored:** `pegasusx.ini` (the Pegasus X folder), `[genres]`, `hidden=`,
  the keys (lower case, apostrophes and spaces as the count makes them).
  Shown again, its key is removed; with none hidden the entry is gone.
- **Affects:** only the Genres list (and its header count). The games keep
  their genres: no metadata file is written. Themes, the metadata editor's
  genre field and its suggestions, the Scraper and anything else that
  reads genres see every genre as before.
- **Not changed:** the counting, the spelling rows (build 2229), opening a
  genre and a game, Back, the keys.
- **Tests:** the test environment, `03-genres-hide...`: Hide genres...,
  Platform hidden (`03-genres-hidden`: the list without it, "1 hidden"),
  then shown again; the summary checks `genres/hidden=platform` after
  hiding and none after showing again (`files/pegasusx-after-hide.ini`).
  The steps that open a genre move down one row (Hide genres... is first).
- **Files:** `src/backend/model/internal/settings/Settings.h/.cpp`
  (`hiddenGenres`, `setHiddenGenres`),
  `src/frontend/menu/settings/GenresScreen.qml`, `tests/testenv/run.sh`.
- **Commit:** "Genres: hide genres from the list".

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
