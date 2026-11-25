# Pegasus X changelog

Newest first. Each entry is one commit on `pegasus-x`, so any of them can be
rolled back on its own with `git revert <commit>`.

## 2026-11-24

### Damaged-file recovery: build fix (the list of copies to put back)

- **What:** `Recovery.cpp` filled its list of `.pxsave` / `.pxold` copies
  with `list = { ... }`, which Qt 5.15's QStringList can't tell apart from
  its other assignments; every build (Android, Linux, Windows, test env)
  stopped there. It's filled with `<<` now, the same names in the same
  order.
- **Why:** build 2272 (the commit below) didn't compile, so no APK came
  of it; this is the build to test.
- **Not changed:** anything the recovery does.
- **Tests:** the same as the commit below, now that it compiles.
- **Files:** `src/backend/Recovery.cpp`, `CHANGELOG.md`.
- **Commit:** "Damaged-file recovery: build fix (the list of copies to put back)".

### Damaged-file recovery, build 2: last good copies, startup check, notice

- **Why:** with a damaged settings.txt, favorites.txt, stats.db,
  pegasusx.ini or pegasusx-hidden.json, Pegasus X started at defaults or
  empty with no warning, and the next save wrote over the damaged file.
- **Files checked at each start, before anything reads them**
  (`src/backend/Recovery.cpp`): in the Pegasus X folder settings.txt,
  pegasusx.ini, game_dirs.txt, pegasusx-rom-folders.txt, favorites.txt,
  pegasusx-hidden.json, pegasusx-android-apps.json,
  pegasusx-android-kinds.json, stats.db, pegasusx-emulators.json,
  sdl_controllers.txt; in the private folder pegasusx-root.txt (first,
  before the Pegasus X folder is decided) and pegasusx-private.ini.
- **Damaged means:** JSON files: doesn't parse, or not an object (an
  array for pegasusx-emulators.json). stats.db: no SQLite header, or
  `PRAGMA integrity_check` isn't "ok". Text files: zero bytes in it, not
  UTF-8, or control characters; then settings.txt / the .ini files /
  sdl_controllers.txt / the ROM folder list are damaged when fewer than
  half their lines are lines of their kind ("key: value", "key=value" or
  "[section]", a mapping, "folder<TAB>system"); pegasusx-root.txt must be
  one absolute path. An empty file is damage only when its last good copy
  isn't empty (and for the ROM folder list, when it had folders);
  favorites.txt and game_dirs.txt may always be empty, so emptying
  favorites or hidden on purpose is never flagged (an emptied list empties
  its good copy too).
- **Last good copy:** `<name>.pxgood` beside the file, written with
  SafeFile, only from a file that passed, made again when they differ.
  pegasusx-private.ini's and pegasusx-root.txt's stay in the private
  folder; backups never take them.
- **Damaged file:** moved (never deleted or written over) to `damaged/`
  in the Pegasus X folder (`damaged/` in the private folder for the
  private files), named "yyyy-MM-dd_HH-mm-ss <name>" (stats.db's
  -journal/-wal/-shm with it). Its good copy takes its place if it passes,
  else that file starts at its defaults (no file); a good copy that fails
  goes to `damaged/` too. A file that can't be moved is left as it is.
- **Missing after a cut save:** with `.pxsave` and `.pxold` both there,
  `.pxsave` is tried first (written in full before the old file was
  moved), then `.pxold`; with one, that one. The first that passes the
  check is renamed back; if none passes they go to `damaged/` and the
  good copy (or defaults) is used. Missing with neither: left alone (first
  start, or removed on purpose). A fine file's leftover `.pxsave` /
  `.pxold` are removed.
- **Notice** (`RecoveryNotice.qml`): once after the start, only when
  something was fixed: which files and how, with Keep (B), Restore from a
  backup (X: Backup & reset's restore picker) and Details (Y: the reason
  and what went to `damaged/`, full paths). A on a button and touch too;
  the round A/B/X/Y prompts. Each action is a "Recovery:" log line.
- **Start over** (your answer) also deletes the `.pxgood` copies and the
  `damaged/` folders (Pegasus X folder and private), listed on its page;
  pegasusx-root.txt's good copy stays with the folder setting.
- **Not changed:** file and library formats, defaults, the theme, other
  screens, logging, backups' contents.
- **Tests:** new test_Recovery (junk / half-cut / wrong-type per kind,
  empty on purpose, ROM folders emptied, missing with .pxold, .pxsave,
  both, both with .pxsave cut, nothing whole, damaged good copy ->
  defaults, leftovers cleaned, other files byte-for-byte unchanged);
  test_BackupReset checks Start over removes them; test env step 16
  (settings.txt junk, hidden/favorites cut in half, game_dirs.txt only as
  .pxold, pegasusx.ini zeros; the notice, Details, Keep, Restore, and a
  clean start with no notice).
- **Files:** `src/backend/Recovery.h`, `Recovery.cpp` (new),
  `src/backend/backend.pro`, `CMakeLists.txt`, `src/app/main.cpp`,
  `src/backend/BackupReset.cpp`,
  `src/backend/providers/esde/EsdeProvider.cpp`,
  `src/backend/model/internal/settings/Settings.h`, `Settings.cpp`,
  `src/frontend/menu/settings/RecoveryNotice.qml` (new),
  `BackupResetScreen.qml`, `src/frontend/main.qml`, `MenuLayer.qml`,
  `frontend.qrc`, `tests/backend/recovery/` (new), `tests/backend/backend.pro`,
  `tests/CMakeLists.txt`, `tests/backend/backup/test_BackupReset.cpp`,
  `tests/testenv/run.sh`.
- **Commit:** "Damaged-file recovery: last good copies, startup check, notice".

### Settings: three rows back on the main list; Gamepad layout's controller and icons

- **Main list:** Reload all games (Games, below RetroAchievements), Gamepad
  layout and Mouse support (Controls, each its own row below Change
  controls) moved back from Advanced, with the same labels, descriptions
  and actions (Reload: the main list's own reload, as before the regroup).
  Main: Games (5 rows), Look (3), Controls (3), Pegasus X (3), Advanced...
  Advanced keeps Pegasus X folder, Accessible Android directories, Scan
  for games on launch, Missing games, Data sources, Fullscreen mode; its
  row's description says so. Gamepad layout from the main list opens as
  its own full screen with its bottom bar (as it did before the regroup),
  and B comes back to its row.
- **Gamepad layout's controller** (`assets/gamepad/preview.png`, source
  `preview.svg` beside it): an Xbox Series controller in thin white
  outlines, no fill, on a transparent background, 512 x 340: body and
  grips, outlined bumpers on the top edge with the triggers above them,
  sticks as double rings (left upper left, right lower right), the d-pad
  as an outlined cross lower left, A B X Y as outlined circles with white
  letters (Y top, X left, B right, A bottom), View and Menu as small
  circles. Guide: the XBSX-Experience theme's sphere
  (`assets/images/Xbox-logo2-tight.png`), embedded as it is, not redrawn.
- **The dots on it:** checked on a render of each of the 21 inputs at
  1280 x 720 and 1920 x 1080. The dots and the axis bars sit on their
  inputs. LB and RB moved into the middle of the new bumpers (fieldPoints).
  One more fix was needed in `GamepadPreview.qml`: the dots were placed on
  the item's box, but the picture keeps its shape inside it (the item gets a
  width; its height stays 340), so on a 1280 x 720 screen they sat about 12
  px low and on 1920 x 1080 about 15-20 px to the side. They're placed on
  the picture as drawn now (its painted size and offset).
- **Gamepad layout's icons** (`assets/gamepad/x360/`, the names as they
  were): Guide is the theme's sphere (`Xbox-logo2-tight.png`, copied as
  it is; the old picture is the fallback in `stock/guide.png`). Left Press
  is the theme's stick-press badge (`assets/images/kb_badge_l3.svg`, as it
  is, `lsb.svg`); the theme has no R one, so Right Press is a copy of it
  with its "L" drawn as an "R" (`rsb.svg`, same size, lines and colour).
  The old `lsb.png` / `rsb.png` (the theme's 6.png / 9.png press arrows) are
  gone; the stock ones stay the fallback. Only Gamepad layout uses these
  three files. The other icons are as they were.
- **Not changed:** any setting or saved value, the field list, its hint
  bar, header, gamepad picker, navigation, mapping, the recorder,
  sdl_controllers.txt, other screens, the theme.
- **Tests:** test env: the main list (top, bottom), Advanced, the
  updater's switch, Gamepad layout from the main list and back on its row.
- **Files:** `src/frontend/menu/settings/SettingsMain.qml`,
  `SettingsAdvanced.qml`, `SettingsScreen.qml` (a comment),
  `gamepad/GamepadPreview.qml`, `gamepad/GamepadField.qml`,
  `src/frontend/assets/gamepad/preview.png`, `preview.svg`,
  `x360/guide.png`, `x360/lsb.svg`, `x360/rsb.svg` (new),
  `x360/lsb.png`, `x360/rsb.png` (removed), `stock/guide.png` (the old
  Guide), `src/frontend/frontend.qrc`, `tests/testenv/run.sh`.
- **Commit:** "Settings: three rows back on the main list; Gamepad layout's controller and icons".

### Scraper part 2: the game, not its edition (LaunchBox, libretro)

- **Where the wrong picks came from:** LaunchBox's matching compares plain
  names: lowercase letters and digits only, with anything in brackets
  taken out (meant for "(USA)"). So on the database's SNES list
  "Chrono Trigger+" (a 2024 entry) is "chronotrigger", and on its Xbox list
  "Need for Speed: Most Wanted (Black Edition)" is "needforspeedmostwanted":
  both scored 100, the same as the game itself, and the order of the
  database decided (the edition came first). The system's own platform's
  best is used at once, so nothing else looked again. The Android entry
  that got the 2012 game: its 2005 original (PlayStation 2) and the 2012
  Android game both scored 100, and nothing told them apart.
  ScreenScraper matches on its own server (checksum, then the name sent),
  and Hasheous by checksum only: neither picks among names here. libretro
  takes the exact file name first, and only else the closest name (85 or
  more): that last step is where an edition could come in.
- **Now:** a name with an edition the wanted one doesn't have (a "+" or
  "Plus", "... Edition", Deluxe, HD, Remastered, GOTY / Game of the Year,
  Collector's, Director's Cut, Definitive, Anniversary; in brackets too)
  costs 10 points per edition (and so does one the wanted name has that
  the found one lacks): the game itself (100) beats its edition (90). An
  edition the file's name or the game's title names is the one wanted, so
  that file gets the edition. An edition with no plain game on the platform
  is still found (90 is sure enough). The same cost in libretro's closest
  name.
- **Year:** each game to scrape carries a year: the file name's "(2005)",
  else the game's release date. On the same score the game of that year
  comes first, and when two platforms tie (an Android port and its
  original), the only one of that year is used; with no year, or none or
  both of that year, it's still left for your check, as before. LaunchBox's
  data has no region; libretro's closest name already prefers the ROM's
  region.
- **Not changed:** a game you picked (Needs your check > Use this match,
  or Choose) is used by its id as before: the scoring isn't asked;
  ScreenScraper and Hasheous; the sources and their order; the speed
  changes of the last build; any setting or default.
- **Tests:** `test_LaunchBoxDb`: the editions of names (10 cases); SNES
  Chrono Trigger gets Chrono Trigger, not Chrono Trigger+ (listed first);
  Xbox Need for Speed Most Wanted gets the game, not the Black Edition
  (listed first); a file or title that names the edition gets it (Black
  Edition, Chrono Trigger+), and an edition alone is still found; a picked
  game by its id is that game; a year breaks the 2005 / 2012 tie (and no
  year, or another one, leaves it a tie).
- **Files:** `src/backend/LaunchBoxDb.cpp`, `src/backend/LaunchBoxDb.h`,
  `src/backend/Scraper.cpp`, `src/backend/Scraper.h`,
  `src/backend/model/internal/settings/Settings.cpp` (the year of a game to
  scrape), `tests/backend/launchboxdb/test_LaunchBoxDb.cpp`.
- **Commit:** "Scraper part 2: the game, not its edition (LaunchBox, libretro)".

### Scraper speed, part 1: shorter waits, downloads at once, smaller files, timings

- **Shorter waits** (`Scraper::get`): every request waited up to 120 s with
  no data, and a stalled one was tried again after that, so one stuck
  request could cost 2 to 8 minutes. Now a lookup (ScreenScraper, Hasheous,
  libretro's lists) gives up after 15 s with no data and is tried once more
  after 2 s; a 429 still waits Retry delay and is tried Retries times, as
  before. A picture, video or manual (and a match's picture in Choose)
  gives up after 20 s with no data, isn't tried again, is logged ("no data
  for 20 s, skipped") and the next file goes on. The wait is for no data at
  all (Qt's transfer timeout starts again with each piece that arrives): a
  slow download that keeps coming is never cut. The stop after 3 requests in
  a row with no answer is as it was.
- **Downloads at once:** a game's files were fetched one at a time. Now up
  to 4 at once from images.launchbox-app.com and thumbnails.libretro.com,
  and from ScreenScraper as many as your account's `ssuser.maxthreads`
  (read from each jeuInfos answer; 1 without it), any other server one at a
  time. They're stored in the order they were asked for, whatever order
  they arrive in, so the slots come out as before (the 2D box before the 3D
  one); the checks (a picture, a PDF, a video), the resizing off the main
  thread and storeMedia are as they were. The miximage is made after the
  last one, and Stop ends the ones on their way at once and saves what came.
- **Smaller files from ScreenScraper:** its picture links ask for the size
  Image size keeps (`maxwidth` / `maxheight`: 2560x1440, 1920x1080,
  1280x720, 640x480; nothing for Original), logos at most 1000x600; not
  videos or manuals. The final resize is still made here (process_image).
  The two items are added to the link as it came, so nothing else in it is
  written differently.
- **Timings in the Scraper log:** the checksum ("Checksum made (12.0 MB,
  0.3 s)"), each lookup (ScreenScraper, with how many downloads at once;
  Hasheous; LaunchBox), each download with its size ("Downloaded covers
  (175 KB, 0.4 s)"), all the downloads ("Downloads done: 7 of 7 in 1.9 s (at
  most 4 at once)"), the miximage, the save and the game ("Game done in
  12.3 s"). Links in the log still lose their query (no password or key).
- **Not changed:** the Scraper settings and their defaults, the settings
  screens, how matches are made, file names, folders, the metadata format,
  the theme.
- **Tests:** `test_Scraper` (new), against a server on the same machine: a
  lookup with no answer (given up at 15 s, once more after 2 s, done at
  about 32 s); a picture with no answer (skipped at about 20 s, asked once);
  a picture that arrives slowly over 28 s (all of it, not cut); nine
  pictures four at once (all saved, the same slots, files and order as one
  at a time, never more than four on their way, arriving out of order);
  Stop with four on their way (ends within a second, nothing new started).
- **Files:** `src/backend/Scraper.cpp`, `src/backend/Scraper.h`,
  `tests/backend/scraper/*` (new), `tests/backend/backend.pro`,
  `tests/CMakeLists.txt`.
- **Commit:** "Scraper speed, part 1: shorter waits, downloads at once, smaller files, timings".

### Test env builds again (fix to the own metadata test)

- **What:** the test environment's build stopped at Settings.cpp: its new
  own-metadata test reads a file with Pegasus' reader, whose result (a list
  of `FileFilter`) needs `PegasusFilter.h`, which wasn't included. That code
  is only built for the test environment, so the app and the compile check
  weren't affected. The header is included now.
- **Files:** `src/backend/model/internal/settings/Settings.cpp`.
- **Commit:** "Test env builds again (fix to the own metadata test)".

### Lists: LB / RB 10 rows, LT / RT top / bottom (the XBSX standard)

- **What:** Pegasus X's long lists had the bumpers and triggers the other
  way round from the XBSX-Experience theme. Now LB / RB move 10 rows up /
  down (stopping at the ends) and LT / RT go to the top / bottom, in
  Library, Game directory, Scraper, RetroAchievements, Genres, Android apps
  to import and Backup & reset (the file browser already does, previous
  entries). The hints say so: "LT / RT top / bottom" where they said
  "LB / RB top / bottom", and the two prompt bars that show both have them
  swapped.
- **Its own commit**, to be rolled back alone with `git revert`.
- **Not changed:** the Theme downloader (LB / RB change its tabs there, no
  list jump), the on-screen keyboard (LB / RB move the cursor, LT / RT
  change page), the button mapping itself.
- **Tests:** test env: the steps that went to the top with LB or the bottom
  with RB now use LT / RT.
- **Files:** `src/frontend/menu/settings/MetadataEditor.qml`,
  `GameDirectory.qml`, `ScraperScreen.qml`, `RetroAchievementsScreen.qml`,
  `GenresScreen.qml`, `AndroidAppPicker.qml`, `BackupResetScreen.qml`,
  `tests/testenv/run.sh`.
- **Commit:** "Lists: LB / RB 10 rows, LT / RT top / bottom".

### Test env: Add a ROM folder goes through the new browser

- **What:** the test step for Game directory > Add a ROM folder pressed LB
  for the last folder and Down to "Use this folder", as the old browser had
  them; the reworked browser (previous entry) has them in the quick jumps
  and on X. The step now goes Y > Suggested > X. Test only.
- **Files:** `tests/testenv/run.sh`.
- **Commit:** "Test env: Add a ROM folder goes through the new browser".

### Legacy cleanup, own metadata files at import, file browser rework

- **GameDirEditor.qml deleted** (and its line in `frontend.qrc`): nothing
  opened it since 80571ecb / 77fc6f60, and no test opens it by name
  (`--pegasusx-open` is used for other screens only). There were no
  translation files with its strings. `GameDirEditorEntry.qml` and
  `GameDirEditorButton.qml` stay (Advanced > Accessible Android directories
  uses both), and so does `FilePicker.qml`.
- **A ROM folder's own metadata file** (metadata.pegasus.txt, metadata.txt,
  *.metadata[.pegasus].txt), checked: a ROM folder added through Game
  directory or setup isn't a game directory (game_dirs.txt lists only
  `gamedirectory`); its own file is read once, when the folder is imported,
  into `gamedirectory/<system>/metadata.pegasus.txt`, and that copy is the
  one used and edited from then on (kept as it is, as decided). What was
  missing at import, now carried into the copy:
  - each game's `workdir:` (was dropped; a relative one is made a full path
    against the file's folder, as Pegasus reads it from there);
  - each game's `launch:` when it was your collection's and the system's
    collection has another one (a game used the system's then); a launch
    whose program is a relative path (`./run.sh ...`) is made a full path.
  Titles, fields, x- fields and assets carry over as before. Not supported
  (as before): your file's collection rules (`extensions:`, `files:`,
  `regex:`, `ignore-files:`), games whose files the system's extensions
  don't match, a game file's own `launch:`.
- **Restore from a ROM folder's own file** (new, manual): Settings > Game
  directory > a system > Folders > a folder > "Restore its games from its
  own metadata file...", only for a folder that has one, asked first. The
  copy's entries for the games your file lists are taken out (the system
  file backed up in `backup/<system>/` first, as "restore") and the folder
  read again, so they come back as the import makes them; other games stay
  as they are.
- **File browser** (`FilePicker.qml`: Game directory, setup, Backup &
  reset, Library art, ES-DE import, the Pegasus X folder): the folder's name
  and its whole path at the top; "Up a folder (<parent>)" as the first row;
  folders with their icon and a ›, files that can be picked bold, others
  grey; "Use this folder" (and "Create new folder...") over the list, always
  in view, X picks it; the row lights at once and the list scrolls to it
  smoothly; a scroll mark on long folders; going up lands on the folder you
  left. Buttons: A open, B up a folder (out on the drives), X use this
  folder, Y quick jumps (Drives and Last folder are there), LB / RB 10 rows,
  LT / RT top / bottom. One hint bar, the round one, with few prompts.
  Touch: tap a row, the up row, the buttons; drag to scroll; tap outside to
  close. What it returns (pick, pickFile, pickFolder, cancel) and how paths
  are kept are the same.
- **One hint bar:** Backup & reset hides its own while the browser is open
  (it showed two), and so does setup while a step's screen is open.
- **Tests:** test env: a ROM folder with its own metadata.pegasus.txt
  (titles, developer / publisher / genre / summary, an x- field, box art, a
  relative launch and workdir, an absolute one) added as Game directory adds
  one; the copy checked line by line and read back the way Pegasus reads the
  game directory (title, developer, players, release, art, x- field, each
  game's launch and workdir); an edit undone by Restore, other games kept, a
  backup made, a wrong folder refused; then taken out again. The browser:
  shots on the drives, in a folder, at the bottom (Game directory) and in
  Backup & reset; Add a folder now goes Y > Suggested > X.
- **Files:** `src/frontend/menu/settings/GameDirEditor.qml` (deleted),
  `src/frontend/frontend.qrc`, `src/backend/providers/esde/EsdeProvider.cpp/.h`,
  `src/backend/providers/GameDirectory.cpp/.h`,
  `src/backend/model/internal/settings/Settings.cpp/.h`,
  `src/frontend/menu/settings/GameDirectory.qml`,
  `src/frontend/menu/settings/gamedireditor/FilePicker.qml`,
  `src/frontend/menu/settings/BackupResetScreen.qml`,
  `src/frontend/menu/settings/FirstRunWizard.qml`, `tests/testenv/run.sh`.
- **Commit:** "Legacy cleanup, own metadata files at import, file browser rework".

### Two "Data folder" labels renamed

- **What:** "Data folder" named two different folders. Setup step 1 (the
  whole Pegasus X folder: settings, themes, game details, scraped art) is
  now **App Data Folder**; Library > Options' row for where scraped art and
  art you pick are kept (`Internal.settings.dataDir`) is now **Scraped Art
  Folder**. Neither name was shown anywhere else (the step list draws step
  1 from the same name; the row's folder picker has no title of its own).
- **Not changed:** "Pegasus X folder..." in Settings > Advanced, Library's
  "Clear backups", any path, setting key, saved value, folder move,
  import / scrape behaviour, code comments or variable names.
- **Tests:** test env screenshots (Library > Options); setup's step list
  isn't reachable there (setup never opens in the test env).
- **Files:** `src/frontend/menu/settings/FirstRunWizard.qml`,
  `src/frontend/menu/settings/MetadataEditor.qml`.
- **Commit:** "Two \"Data folder\" labels renamed".

### Gamepad layout from Advanced: a full screen again

- **What:** opened from Settings > Advanced, Gamepad layout was drawn on
  top of the Advanced list, both showing (test env shot). Root cause: from
  the main list it was a screen that slid in over the list; from Advanced
  it opens on top of Advanced (so B comes back to Advanced), and its screen
  has no background of its own. Advanced is now hidden while Gamepad
  layout is open, and shown again when it closes, so it looks as it did.
  The other three screens Advanced opens (Pegasus X folder, Android
  directories, Data sources) are dialogs and stay drawn over it, as they
  were over the main list.
- **Not changed:** Gamepad layout itself, what B does, the mapping.
- **Tests:** test env: Gamepad layout shot, and Advanced again after B.
- **Files:** `src/frontend/menu/settings/SettingsScreen.qml`.
- **Commit:** "Gamepad layout from Advanced: a full screen again".

### Gamepad layout: the footer loads again (fix to the button art)

- **What:** Settings > Advanced > Gamepad layout lost its footer (B back,
  and with it the screen's layout below the list). Root cause: the button
  art build added `mipmap: true` to the footer's B picture, and
  `GamepadFooter.qml` imports QtQuick 2.0, where Image has no `mipmap`
  (it came in 2.3), so QML refused the file ("Type GamepadFooter
  unavailable"). Removed there, and from `GamepadField.qml` too (it
  imports 2.15, so it worked, but it isn't needed: the pictures are drawn
  smooth anyway).
- **Not changed:** the pictures (still the theme's, the stock ones as the
  fallback), the screen, the mapping.
- **Tests:** test env: Gamepad layout shot; the log is checked for
  "unavailable" / "not available" lines.
- **Files:** `src/frontend/menu/settings/gamepad/GamepadFooter.qml`,
  `src/frontend/menu/settings/gamepad/GamepadField.qml`.
- **Commit:** "Gamepad layout: the footer loads again (fix to the button art)".

### Settings > Advanced: the rows show (fix to the regroup)

- **What:** Settings > Advanced opened empty, so its rows (and Missing
  games) couldn't be used. Root cause: `SettingsEntry` has a *required*
  `section`, and the Advanced rows had none, so QML refused to make the
  list ("Required property section was not initialized" in the log). Each
  row now has `section: "advanced"` (one section, no title drawn).
- **Also:** `SimpleButton` read `parent.width` while the list was letting
  go of a row scrolled out of view (parent null for that moment): four
  "Cannot read property 'width' of null" warnings when scrolling the main
  list. It's 0 then.
- **Not changed:** what the rows do, their order, any setting.
- **Tests:** test env: Advanced, Missing games (each saved pair, and picked
  both ways, `settings.txt` read back), Gamepad layout, the updater switch.
- **Files:** `src/frontend/menu/settings/SettingsAdvanced.qml`,
  `src/frontend/menu/settings/common/SimpleButton.qml`.
- **Commit:** "Settings > Advanced: the rows show (fix to the regroup)".

### Settings regroup, XBSX button art, new controller picture, themed keyboard

- **What, Settings:** the main list goes from 22 rows to 12 plus an
  Advanced page. Main: **Games** (Game directory, Library, Scraper,
  RetroAchievements) · **Look** (was General: Theme, Theme downloader,
  Language) · **Controls** (Change controls) · **Pegasus X** (Check for
  updates, Backup & reset, Accurate play times) · **Advanced...**.
  Advanced (`SettingsAdvanced.qml`, the same rows, focus and B / swipe /
  right-click as the other screens), in order: Pegasus X folder, Accessible
  Android directories, Scan for games on launch, Missing games, Data
  sources (was Enable/disable data sources), Reload all games, Gamepad
  layout (was Change gamepad layout), Mouse support (was Enable mouse
  support), Fullscreen mode (shown everywhere, as before).
- **Missing games** is one choice over the two settings there were, nothing
  new saved: Hide = `verifyFiles` on + `showMissingGames` off; Show = on +
  on; Don't check (faster) = `verifyFiles` off, `showMissingGames` left as
  it is. It shows what's saved now (`verifyFiles` off reads as Don't check
  whatever the other one says).
- **Check for updates** is one row; its automatic switch
  (`checkUpdatesOnStart`, same default) is now the top row of the updater
  screen (Up to it, A or a tap toggles it, Down back to the buttons).
- **What, button art:** every A B X Y, LB RB, LT RT, L3 R3, Start (Menu)
  and Select (View) picture in Pegasus X is now the XBSX-Experience theme's
  (`assets/images/controller/0-11.png` in luxureousproductions-bit/
  XBSX-Experience main, copied byte for byte into `assets/gamepad/x360/`
  under the names Pegasus X used). The old ones are kept in
  `assets/gamepad/stock/` as the fallback for Gamepad layout. Prompt bars:
  `RoundPromptBar` shows the pictures for all ten keys (it had them for
  A B X Y only; LB / RB / LT / RT / Start / Select were drawn), 30 px as
  A B X Y were, drawn shapes if a picture can't be loaded; the d-pad stays
  drawn (the theme has no d-pad art). `PromptBar` (setup, RetroAchievements
  messages) puts the same pictures in its text, the coloured keys if they
  can't be loaded.
- **What, controller picture:** `assets/gamepad/preview.png` (Gamepad
  layout) is a new flat drawing of an Xbox Series layout controller: left
  stick upper left, d-pad lower left, right stick lower right, A B X Y,
  View / Guide / Menu, bumpers and triggers, no logo. Same style (light
  grey body, dark parts, black outline), same 512 x 340, transparent; its
  source `preview.svg` beside it. Every `fieldPoints` entry in
  `GamepadPreview.qml` moved to the new spots.
- **What, keyboard:** Pegasus X's own on-screen keyboard
  (`OnScreenKeyboard.qml`) reskinned to the theme's keyboard look (its
  dark palette): the theme's panel, key colours, rounded keys, Xbox green
  focus frame, and its icons copied as they are (`assets/keyboard/kb_*.svg`:
  LT, LB, L3, RB, Start badges, chevrons, shift, space, backspace, enter;
  X / Y from the button art). Each icon falls back to the old words
  (`KbGlyph.qml`). Not a port: the theme's keyboard owns its own text, and
  the eleven screens that use Pegasus X's type into their own fields, with
  suggestions; the layout, keys, buttons, touch and typing are unchanged.
- **Not changed:** what any row opens, the setting keys, saved values,
  defaults, file formats, the theme API, the library, launch commands; the
  old signals of SettingsMain (kept); everything listed out of scope (Library
  Options names, Scraper extra systems, RetroAchievements beyond the
  pictures, setup wording, Help, button mapping, GameDirEditor).
- **Tests:** test env: the main list (top / bottom), Advanced, Gamepad
  layout, the updater's switch; Missing games shown for each pair of saved
  values and picked both ways, with `settings.txt` read back after each.
- **Files:** `src/frontend/menu/settings/SettingsMain.qml`,
  `SettingsAdvanced.qml` (new), `SettingsScreen.qml`, `UpdateDialog.qml`,
  `common/RoundPromptBar.qml`, `common/PromptBar.qml`,
  `common/OnScreenKeyboard.qml`, `common/KbGlyph.qml` (new),
  `gamepad/GamepadField.qml`, `gamepad/GamepadFooter.qml`,
  `gamepad/GamepadPreview.qml`, `src/frontend/assets/gamepad/x360/*`
  (12 theme files), `assets/gamepad/stock/*` (the old 12),
  `assets/gamepad/preview.png`, `preview.svg` (new),
  `assets/keyboard/*` (11 theme files), `src/frontend/frontend.qrc`,
  `tests/testenv/run.sh`.
- **Commit:** "Settings regroup, XBSX button art, new controller picture, themed keyboard".

### Damaged-file recovery, build 1: safe saves for nine files

- **What:** nine files were saved by writing straight into them (the old
  text emptied first), so a dead battery, a crash or a full storage during
  the save could leave one half-written or empty (an empty `game_dirs.txt`
  is an empty library). They're saved the safe way now, by one shared
  helper (`SafeFile`): `settings.txt` (`SettingsFile.cpp`),
  `game_dirs.txt` (`Settings.cpp`), `favorites.txt` (`Favorites.cpp`), the
  custom emulators file `pegasusx-emulators.json` (`EsdeProvider.cpp`), the
  downloaded ES-DE lists `esde-lists/es_systems.xml` and
  `es_find_rules.xml` (`Settings.cpp`), the setup step file
  `pegasusx-wizard` (`Settings.cpp`), the play session file (`Backend.cpp`),
  and two more found the same way: `pegasusx-root.txt`, which Pegasus X
  folder is used (`Paths.cpp`), and `sdl_controllers.txt`, the gamepad
  layouts (`GamepadManagerSDL2.cpp`).
- **How:** as the other Pegasus X files are saved (QSaveFile): the whole new
  text into a temporary file beside it, flushed to the storage, then renamed
  over the old one in one step. Qt's own "direct write" fallback is off.
- **Fallback** (a storage refusing that temporary file or that rename):
  a. the whole new text into `<name>.pxsave` beside it, flushed to the
  storage, its size checked; b. the old file renamed to `<name>.pxold`;
  c. `<name>.pxsave` renamed to the file's name; d. `<name>.pxold` deleted.
  (a) failing stops there; (b) failing deletes `.pxsave`; (c) failing puts
  `.pxold` back. The old file is never written into. A power cut: during
  (a), the old file is whole (a part of the new beside it); after (a), the
  old file whole, the new one whole beside it; between (b) and (c), for that
  instant, the file isn't there but both whole copies are beside it (to be
  put back by the startup check of a later build); after (c), the new file
  whole (the old copy beside it).
- **A save that can't be done** keeps the old file and logs a warning (as
  the old code's messages, and "Save:" lines); no message on screen.
- **Not changed:** the files' contents and formats, when they're saved,
  every other file (`pegasusx.ini` is saved by Qt's QSettings, already
  this way; caches, markers, downloads and one-time moves aren't changed).
- **Tests:** `test_SafeFile` on a throwaway folder: a save; a new file;
  text mode; a save cut before the swap (the old file whole); the rename
  refused (the fallback saves); the fallback cut at each step (old whole,
  new whole, or between its renames both whole copies beside it; never part
  of a file, never empty); a save after a cut one; a folder that refuses new
  files (the old file kept, not written into).
- **Files:** `src/backend/SafeFile.h/.cpp` (new), `src/backend/backend.pro`,
  `src/backend/CMakeLists.txt`, `src/backend/parsers/SettingsFile.cpp`,
  `src/backend/model/internal/settings/Settings.cpp`,
  `src/backend/providers/pegasus_favorites/Favorites.cpp`,
  `src/backend/providers/esde/EsdeProvider.cpp`, `src/backend/Backend.cpp`,
  `src/backend/Paths.cpp`, `src/backend/model/internal/GamepadManagerSDL2.cpp`,
  `tests/backend/safefile/*` (new), `tests/backend/backend.pro`,
  `tests/CMakeLists.txt`.
- **Commit:** "Damaged-file recovery, build 1: safe saves for nine files".

## 2026-11-23

### Prompts: every prompt on the bar's middle line

- **What:** with the A, B, X, Y pictures 30 px tall (2259), a prompt with a
  picture was taller than one with a pill (LB / RB, LT / RT, Start), and the
  bar lined the prompts up by their tops: the words after the pills sat
  about 3 px higher than the others. Each prompt is centred on the bar now.
- **Not changed:** anything else.
- **Files:** `src/frontend/menu/settings/common/RoundPromptBar.qml`.
- **Commit:** "Prompts: every prompt on the bar's middle line".

### Prompts: the A, B, X, Y pictures at the size of the drawn buttons

- **What:** the gamepad layout menu's pictures have a clear margin round the
  button (about a fifth of the picture), so in 2258 they showed smaller
  than the drawn buttons (about 19 px instead of 24) and the letters were
  hard to read. They're given 30 px now: the button itself is 24 px again.
- **Not changed:** words, keys, the bar's place and height, anything else.
- **Files:** `src/frontend/menu/settings/common/RoundPromptBar.qml`.
- **Commit:** "Prompts: the A, B, X, Y pictures at the size of the drawn buttons".

### Prompts: the gamepad layout menu's A, B, X, Y pictures; theme downloader too

- **What:** the round prompt bars show A, B, X and Y with the stock
  pictures of Settings > Change gamepad layout: the files that menu loads
  (`gamepad/GamepadField.qml`: `qrc:/frontend/assets/gamepad/x360/<name>.png`;
  one set, no other layout's pictures exist), `a.png`, `b.png`, `x.png`,
  `y.png`. If a picture can't be loaded, the drawn round button with its
  letter is shown, as before. LB / RB (pills), LT / RT (triggers), Start /
  Select and the drawn d-pad are as they were; the stock LB / RB pictures
  aren't used.
- **Where:** every round prompt bar: Android apps to import (its own copy of
  the bar is now the shared `RoundPromptBar`, same buttons and words),
  Backup & reset, Game directory and its folder picker, Library, every
  Scraper screen, and now the theme downloader (it had the square keys; its
  words, `◂ ▸ pictures` and `LB / RB filter` included, are as they were).
- **Not changed:** any word, key, layout or behavior; setup's own bar and
  RetroAchievements (still the square keys).
- **Files:** `src/frontend/menu/settings/common/RoundPromptBar.qml`,
  `src/frontend/menu/settings/AndroidAppPicker.qml`,
  `src/frontend/menu/settings/ThemeDownloader.qml`.
- **Commit:** "Prompts: the gamepad layout menu's A, B, X, Y pictures; theme downloader too".

### Round button prompts on Game directory, Library and Scraper screens

- **What:** the button prompts of Settings > Game directory (and its folder
  picker), Library (every page, and its RetroAchievements match dialog,
  Needs your check, a game's file picker) and every Scraper screen (the
  Scraper, Account settings, Extra systems for Android, the scraper's match
  choice) are drawn as in Android apps to import: A, B, X, Y round with the
  letter in Xbox colours, LB / RB as bumpers, LT / RT as triggers, Start /
  Select as small pills, and the d-pad drawn (◂ ▸, ▴ ▾, ▾ were characters a
  font may not have, shown as boxes). `RoundPromptBar` now also takes the
  old bar's words ("A edit · X undo · B back"), so each screen keeps its
  text; a message of a screen's still shows as plain text, as before.
- **The folder picker showed two hint lines** (its own, and the screen's
  behind it, eg. "A tick · B back"): the screen's bar is hidden while the
  picker is open (Game directory, Library), and the picker's own line is the
  round bar, same keys as before (A Open, B Up / Close, X Close, LB Last
  folder, RB Drives, Y Quick jumps, ▾ Use this folder).
- **Labels checked against each screen's keys:** Scraper's Extra systems for
  Android, Log and the running page said "◂ ▸ change", which changes
  nothing there: now "A select · LB / RB top / bottom · B back" and "B
  back". "LB / RB top / bottom" added on the long lists (Game directory's
  systems and Add systems, Library's collection and Needs your check,
  Scraper's systems and Extra systems for Android); the keys worked there
  already.
- **Also in setup:** setup's step 1 (Pegasus X folder), step 2 (Add
  systems: the Game directory screen) and step 3 (Import gamelist) use the
  same folder picker, and step 2 the same Game directory screen, so they
  show the round prompts too. The setup screen's own bar isn't changed.
- **Not changed:** any key, layout or behavior; the old `PromptBar` (theme
  downloader, RetroAchievements, setup); the Android apps picker; Backup &
  reset (already round).
- **Tests:** the test environment's screenshots of these screens (one more:
  Extra systems for Android).
- **Files:** `src/frontend/menu/settings/common/RoundPromptBar.qml`,
  `src/frontend/menu/settings/gamedireditor/FilePicker.qml`,
  `src/frontend/menu/settings/GameDirectory.qml`,
  `src/frontend/menu/settings/MetadataEditor.qml`,
  `src/frontend/menu/settings/ScraperScreen.qml`,
  `src/frontend/menu/settings/ScraperChoice.qml`, `tests/testenv/run.sh`.
- **Commit:** "Round button prompts on Game directory, Library and Scraper screens".

### Fix: Backup & reset's highlight jumped when the art size or progress came in

- **What:** on the Back up page, the scraped art's size (measured in the
  background), the tick and the progress were part of the rows, so the
  rows were made again when they changed, and the highlight went back to
  the top: a key pressed just then landed on the wrong row (the test
  environment's run ticked "Include scraped art" instead of pressing "Back
  up now", so no backup was made). They're live values of their rows now;
  the rows only change on what you do.
- **Not changed:** anything else.
- **Files:** `src/frontend/menu/settings/BackupResetScreen.qml`.
- **Commit:** "Fix: Backup & reset's highlight jumped when the art size or progress came in".

### Test fix: test_BackupReset's damaged-file case

- **What:** the test cut its own backup file short and then compared the
  folders with the snapshot taken before the cut, so the changed test file
  itself made it fail. It compares with a snapshot taken after the cut now.
- **Not changed:** the app.
- **Files:** `tests/backend/backup/test_BackupReset.cpp`.
- **Commit:** "Test fix: test_BackupReset's damaged-file case".

### Fix: Backup & reset's build (C++11)

- **What:** two lists in `BackupReset.cpp` were filled with `{ ... }` items
  of structs that have default values, which C++11 (this project's
  standard) can't build from braces: the compile check failed on Linux and
  Windows. They're made explicitly now.
- **Not changed:** what anything does.
- **Files:** `src/backend/BackupReset.cpp`.
- **Commit:** "Fix: Backup & reset's build (C++11)".

### Settings > Backup & reset: back up, restore, run setup again, start over

- **Where:** Settings, Pegasus X section, under "Pegasus X folder...":
  "Backup & reset...". The same list, controls (A, B, LB/RB top/bottom,
  LT/RT 10 rows, touch) and the round Xbox-style prompts of Android apps to
  import (now a shared `RoundPromptBar`; the picker itself isn't changed).
- **Back up:** one file, `PegasusX-backup-<date>-<time>-build<n>.tar`
  (a tar: no 4 GB or 65,535-file limit, unlike a plain zip), saved in
  Download by default or a folder you pick. It holds, from the Pegasus X
  folder: `pegasusx.ini` (passwords, keys and tokens taken out: any key
  named *password*, *token*, *secret*, *apikey*, `key` or `*_key`, so
  `scraper/ssPassword`, `retroachievements/key`, `github/token`; usernames
  stay), `settings.txt`, `game_dirs.txt`, `sdl_controllers.txt`,
  `favorites.txt`, `stats.db`, `pegasusx-hidden.json`,
  `pegasusx-android-kinds.json`, `pegasusx-android-apps.json`,
  `pegasusx-emulators.json`, `pegasusx-rom-folders.txt`,
  `pegasusx-themes/links.json`, and the folders `gamedirectory/`,
  `metadata/` (but not `metadata/library-index.json`, made again),
  `metafiles/`, `imported/`, `theme_settings/`; `downloaded_media/` only
  with "Include scraped art" (off; its size shown). Never: caches, logs,
  `backup/`, `launchbox/`, the theme store cache, `themes/`, the GitHub
  token file, links (not followed). A manifest (date, build, art) comes
  first, a list of every file's size and CRC-32 last. Progress and Stop;
  written as `.part` and renamed when done, so a stopped or failed backup
  leaves no file.
- **Restore:** pick the file; its date, build, art and size are shown;
  "Restore this backup..." asks first (Cancel focused). The whole file is
  checked before anything changes (each header, that every file is one a
  backup may hold, no `..` or absolute path, sizes, every CRC-32): a bad
  file changes nothing. Then the data as it is is saved to
  `backup/before-restore-<date>.tar` (no art), the backup's files replace
  the current ones (art in the backup is added), and the device's logins
  are kept; a login the device doesn't have is named ("Sign in again:
  ScreenScraper, RetroAchievements"). Settings are read again, and the
  library on closing the result.
- **Run setup again:** setup from step 1 in the same session; nothing is
  deleted. Step 1 says "Keep using this folder" when a Pegasus X folder is
  set, and picking the folder in use again just carries on (before, it
  waited forever: the same folder never finished).
- **Start over:** a page with Cancel focused, "Back up first...", and
  "Delete Pegasus X's data..." (its size), every item deleted with its size,
  and what's kept; then two confirms (Cancel focused in both). It deletes
  only these names: in the Pegasus X folder `pegasusx.ini`, `settings.txt`,
  `game_dirs.txt`, `sdl_controllers.txt`, `favorites.txt`, `stats.db`,
  `pegasusx-hidden.json`, `pegasusx-android-kinds.json`,
  `pegasusx-android-apps.json`, `pegasusx-emulators.json`,
  `pegasusx-rom-folders.txt`, `pegasusx-themes/`, `gamedirectory/`,
  `metadata/`, `metafiles/`, `imported/`, `theme_settings/`,
  `downloaded_media/`, `backup/`, `launchbox/`, `esde-lists/`; in app
  storage `pegasusx-wizard`, `pegasusx-root-asked`, `pegasusx-private.ini`
  (GitHub token); in the cache `gameindex-v1.json`, `netcache/`,
  `scraper-pictures/`, `import-conflicts.txt`, `update/`, `androidapps/`,
  `steam/`. Each must be directly in its folder (links resolved); a link is
  deleted, never what it points to; a folder that's a storage root, the
  home folder or `/` is refused. Kept: `themes/`, which Pegasus X folder is
  used (`pegasusx-root.txt`), `lastrun.log`, everything else. Then, in the
  same session, the library is read again and setup opens at step 1, like a
  new install (no Android app picked).
- **Not changed:** the Android apps picker, the scraper, imports, themes,
  every other screen; changing to another Pegasus X folder still closes
  Pegasus X (a folder is fixed at start).
- **Tests:** `test_BackupReset` (throwaway folders: a theme and a ROM folder
  inside the Pegasus X folder, ROMs with ES-DE files and a file outside,
  links pointing out): no secret in a backup, what's left out, art,
  a stopped backup leaves no file, backup -> restore round trip with the
  device's logins kept, sign in again, damaged / cut / not-a-backup /
  foreign-path files change nothing, Start over deletes only its list,
  refuses a storage root, `safe_target`. Test environment: Back up from the
  screen (no planted secret in the file), Run setup again (no file
  changed), Start over through both confirms (ROMs, ES-DE, the theme and an
  outside file unchanged).
- **Files:** `src/backend/BackupReset.h/.cpp` (new),
  `src/backend/model/internal/settings/Settings.h/.cpp`,
  `src/backend/backend.pro`, `src/backend/CMakeLists.txt`,
  `src/frontend/menu/settings/BackupResetScreen.qml` (new),
  `src/frontend/menu/settings/common/RoundPromptBar.qml` (new),
  `src/frontend/menu/settings/SettingsMain.qml`,
  `src/frontend/menu/settings/SettingsScreen.qml`,
  `src/frontend/menu/settings/FirstRunWizard.qml`,
  `src/frontend/menu/settings/RootFolderSetup.qml`, `src/frontend/main.qml`,
  `src/frontend/frontend.qrc`, `tests/backend/backup/*` (new),
  `tests/backend/backend.pro`, `tests/CMakeLists.txt`, `tests/testenv/run.sh`.
- **Commit:** "Settings > Backup & reset: back up, restore, run setup again, start over".

### Test environment: the new-install and older-library cases of Android apps to import

- **What:** the test environment never runs first-time setup, so to Pegasus
  X it's always a new install: build 2251's run found that, and its "older
  library" checks failed for that reason (not the app). The new-install
  case now opens the picker directly (setup to do); the older-library case
  marks setup as done first (`pegasusx-root-asked`), as on a device after
  setup.
- **Not changed:** the app; only `tests/testenv/run.sh`.
- **Files:** `tests/testenv/run.sh`.
- **Commit:** "Test environment: new-install and older-library cases of the app picker".

### Android apps to import: none picked on a new install; round button prompts

- **New installs start with no app picked.** When Pegasus X starts with
  first-time setup still to do (no Pegasus X folder asked yet, or setup in
  progress) and no picks are saved, an empty pick is saved before the first
  scan: every app is unticked, in setup's step 2 ("none yet") and in the
  picker, and no app is imported until you tick it. The library then has no
  Android collection; Library > Options keeps "Android apps to import"
  ("none").
- **Not changed:** a library from an older build (no setup to do, no picks
  saved) still keeps the apps it has as picked, with their sections, at its
  first start with the picker (build 2250's rule). Picks already saved are
  never replaced.
- **The picker's button prompts:** the same bar in the same place, with the
  buttons drawn Xbox-style: A, B, X round with the letter inside, LB / RB as
  bumpers and LT / RT as triggers with their label. Only the picker's bar;
  Settings' shared PromptBar (square keys, every other screen) is as it was.
  The keys are as they were: LB top, RB bottom, LT / RT 10 rows (as Genres),
  A tick, X section, B back.
- **Tests:** test environment: the first start (setup to do) saves no app
  picked; setup from step 2 with apps, the picker opened and closed: still
  none; Library's Options with no Android collection (screenshot); the 2250
  checks (an older library keeps its apps) unchanged.
- **Files:** `src/backend/providers/LibraryRules.h/.cpp`,
  `src/backend/model/internal/settings/Settings.h/.cpp`,
  `src/frontend/menu/settings/AndroidAppPicker.qml`,
  `src/frontend/menu/settings/MetadataEditor.qml`,
  `src/frontend/menu/settings/FirstRunWizard.qml`, `tests/testenv/run.sh`.
- **Commit:** "Android apps to import: none picked on a new install; round button prompts".

### Android apps to import (a picker); a hidden app keeps its section

- **Android apps to import:** a new screen listing every installed app that
  can be opened, with its icon and name, a tick, and its section
  (Application, Emulator or Games). Only the ticked apps are in the library,
  each in its section. Search apps... narrows the list (part of a name or
  package); Select all and Deselect all act on the apps shown (all of them
  without a search). A ticks or unticks, X (or a tap on the section) changes
  the section, B clears the search, then goes back; LB/RB top and bottom,
  LT/RT 10 rows, as in Genres. Touch works on every row. Each change is
  saved at once (`pegasusx-android-apps.json` in the config folder; the
  sections of the ticked apps go into the Android moves file, as a move in
  Library does, so an app update doesn't change them). The library takes it
  when it's read again: on leaving Library, or in setup.
- **Where:** Settings > Library > the Android collection ("Android apps to
  import", under "Android games"), and Library > Options (so it can still be
  reached when no app is picked and there's no Android collection); first-
  time setup, step 2 ("Android apps to import..."), on Android only.
- **Existing libraries:** the first start of this build keeps every app the
  library has now as picked (hidden ones too), with the section it has; no
  app goes. Until that's written (eg. before the first scan), every app is
  imported, as before.
- **New installs:** the first scan imports every app, as before, and keeps
  them as picked; setup's step 2 has the picker to untick what you don't
  want.
- **How it works:** Pegasus' own app import still reads every app into the
  game list cache (so ticking one later needs no rescan); the library rules
  leave out the installed apps (android:<package> entries) that aren't
  ticked. Your other Android entries (a game you added, a ROM launched with
  an app) aren't touched. An app that isn't installed isn't in the picker
  and isn't changed.
- **Fix: a hidden app showed in Games.** Hiding a game takes it out of the
  library, so Library > Android listed the hidden ones from the hidden list,
  which has no section: all of them went under Games. The library rules now
  remember each Android entry's section when they run (before the hidden
  ones are left out), and the hidden list gives it: a hidden app stays in its
  own section (your move first). Hiding and showing again are as before.
- **Not changed:** Hide and Show it again; Move to Games / Apps / Emulators
  (and Back to automatic); Android games' own collection; the scraper; ES-DE
  import; non-Android collections; themes.
- **Tests:** the test environment (`PEGASUSX_TEST_APPS`: apps that can be
  opened, on Linux): a first start keeps the library's apps as picked; the
  picker ticks, unticks, changes a section, searches, Deselect all / Select
  all on the search; the library then has only the picked ones; Library >
  Android shows the hidden emulator under Emulators (screenshots).
- **Files:** `src/backend/providers/LibraryRules.h/.cpp`,
  `src/backend/model/internal/settings/Settings.h/.cpp`,
  `src/backend/platform/AndroidHelpers_testenv.cpp`,
  `src/frontend/menu/settings/AndroidAppPicker.qml` (new),
  `src/frontend/menu/settings/MetadataEditor.qml`,
  `src/frontend/menu/settings/FirstRunWizard.qml`,
  `src/frontend/frontend.qrc`, `tests/testenv/run.sh`.
- **Commit:** "Android apps to import: a picker; a hidden app keeps its section".

### LaunchboxDB: search any system from Needs your check; extra systems for Android

- **Needs your check > Start on a game: search a system.** A list of every
  LaunchboxDB system (from the last Update), the ones already saved with
  their game count, the others with an estimate of what saving them
  costs ("about N MB to save"). A on a system: if its list is saved, its
  closest games for that title (up to 15, each with system, year and
  score); if not, its list is saved first (read from the LaunchboxDB
  download, 1 to 3 minutes; the download first if it isn't there), with
  its progress, then the results. B (or Stop) stops it at any point: the
  read stops between games and nothing is written; B on the results goes
  back to the systems, B on the systems to Needs your check. A on a
  result scrapes the game with it (LaunchboxDB, on the system it was
  picked on). The page's hint line has the new key. A saved system stays
  saved, so a second search on it is instant.
- **Scraper > Account settings > Extra systems for Android.** Every
  LaunchboxDB system with its game count and estimated size, ticked or
  not. Ticked systems are saved with the next Update LaunchboxDB, or now
  with "Save the ticked systems now" (Stop on the same row). Unticking a
  system removes its list, unless one of your collections uses that
  system (it says which). Saved systems are what Android collections are
  compared against (build 2248's tie rule, unchanged: within 5 points
  goes to Needs your check, choices with system and year).
- **Note:** a system saved from Needs your check is also a saved system,
  so it joins the Android comparisons too (and shows as ticked-or-saved
  in Extra systems for Android, where it can be removed).
- **Not changed:** ScreenScraper and its matching; matching for every
  collection that isn't Android (their own system's list first, the
  Switch 2 to Switch fallback); the tie margin; what Update LaunchboxDB
  saves for your collections.
- **Tests:** `test_LaunchBoxDb`: the saved systems list, a search on one
  system (and none on an unsaved one), a pick found on the system it was
  picked on, an extra system joining the Android comparisons and leaving
  them when removed (non-Android collections never see it, the other
  systems stay).
- **Files:** `src/backend/LaunchBoxDb.h/.cpp`, `src/backend/Scraper.h/.cpp`,
  `src/backend/model/internal/settings/Settings.h/.cpp`,
  `src/frontend/menu/settings/MetadataEditor.qml`,
  `src/frontend/menu/settings/ScraperScreen.qml`,
  `tests/backend/launchboxdb/test_LaunchBoxDb.cpp`.
- **Commit:** "LaunchboxDB: search any system from Needs your check; extra systems for Android".

### Fixes: Back from Scraper options, a game's "Nothing saved", Android namesakes

- **Back from Scraper options (a game in Library):** the Scraper opened on
  top of the game stayed loaded and drawn over everything after Back,
  while the keys went to the game's page under it: the screen looked
  frozen, and the presses that followed went on, unseen, to the pages
  under it and out of Library, whose library read on the way out (with its
  loading screen) looked like a restart. Now the Scraper is unloaded when
  Back closes it (just after its close signal, not from inside it), and
  the game's page is in front with its keys. Only the Scraper opened from
  Library; the other screens opened on top are as before.
- **"Nothing saved" after any scrape:** in Library, the check that a
  finished scrape was the one started from that game's page had a stray
  `if (state.saved > 0)` in front of it (a comment line between them), so
  a scrape that saved nothing skipped the check and the game's page said
  "Nothing saved: ..." for any scrape (Complete my library, a collection).
  The stray line is gone.
- **Android collections, a namesake on another platform:** the Android
  platform's sure match is no longer taken by itself: it's compared with
  your other saved platforms' best matches. If one of them is about as
  good (within 5 points), none is guessed: the game goes to Needs your
  check with each one listed with its platform and year (eg. "Need for
  Speed: Most Wanted" on Android, 2012, and on PlayStation 2, 2005), and
  Choose lists them. A clearly better match elsewhere is used; an Android
  match with no namesake elsewhere is used as before. Other collections:
  unchanged (their own platform's sure match first). ScreenScraper isn't
  changed: it's asked about one system per game (Android's, for these),
  so comparing would mean asking it again per other system, against its
  daily quota; not in this change.
- **Tests:** `test_LaunchBoxDb`: a new case (the Android 2012 entry and the
  PlayStation 2 2005 one: none used, both kept with platform and year;
  Switch 2 still takes its own platform's match), and the platform count
  for Android updated (a sixth saved platform in the test data).
- **Files:** `src/frontend/menu/settings/SettingsScreen.qml`,
  `src/frontend/menu/settings/MetadataEditor.qml`,
  `src/backend/LaunchBoxDb.h/.cpp` (`best`),
  `tests/backend/launchboxdb/test_LaunchBoxDb.cpp`.
- **Commit:** "Fixes: Back from Scraper options, a game's Nothing saved, Android namesakes".

### Fix: the scrape progress bar's binding loop

- **What:** the moving bar of the scrape progress panel (shown only when a
  scrape's total isn't known) now runs on the scrape's state alone; its
  own visibility in the condition made Qt log "Binding loop detected for
  property running" (build 2246's test run).
- **Not changed:** what the panel shows; everything else.
- **Files:** `src/frontend/menu/settings/MetadataEditor.qml`.
- **Commit:** "Fix: the scrape progress bar's binding loop".

### Library: a scrape's progress in the preview panel

- **What:** while a scrape runs (any route: a collection, a game, Complete
  my library, Needs your check; LaunchboxDB or ScreenScraper), the preview
  panel on the right of a collection's or a game's page in Library shows
  it instead of the picture: "Scraping", the game being scraped, its
  collection and system, the source it's looked up in (ScreenScraper,
  LaunchboxDB, or libretro thumbnails when neither has the system), a bar
  of the games done and "X of Y" (a moving bar and a running count if the
  total weren't known), and Saved / Not found / Failed so far. When the
  scrape ends (done, stopped, or failed) the panel shows the picture
  again, as before.
- **How:** the scraper's state (what the panel and the other scrape views
  already read) also tells the current game's collection, system and
  source; the source is worked out the way the scraper picks it. Nothing
  in how it scrapes changed.
- **Not changed:** the panel when nothing is scraping; the scrape itself,
  matching, the ScreenScraper list, the Switch 2 / Android fallbacks, the
  genre tools, the keys, touch, saving, the library refresh, themes, the
  Companion app.
- **Tests:** the test environment opens Library on a collection while a
  LaunchBox scrape runs (`13c-library-collection`, `13c-scrape-progress`,
  `13c-scrape-done`).
- **Files:** `src/backend/Scraper.cpp` (`state()`: collection, system,
  source), `src/frontend/menu/settings/MetadataEditor.qml` (the panel),
  `tests/testenv/run.sh`.
- **Commit:** "Library: a scrape's progress in the preview panel".

## 2026-11-22

### LaunchBox: Switch 2 falls back to Switch, Android to your other platforms

- **What:** when a game is matched against LaunchBox's database without
  asking (a scrape, Complete my library, LaunchBox filling in after
  ScreenScraper):
  - **Nintendo Switch 2:** the "Nintendo Switch 2" platform first; with no
    match there at the auto-match threshold, "Nintendo Switch" (a Switch
    game with a Switch 2 upgrade). On both: the Switch 2 one. A name with
    "Nintendo Switch 2 Edition" at the end ("- ...", "– ...", ": ...", or
    "Switch 2 Edition") counts as the name without it, on both sides, so
    "Breath of the Wild" matches its Switch 2 Edition by itself. (Only for
    Switch 2 games: other names are compared as before.)
  - **Android collections (Android, Android Games, Android Apps):** the
    "Android" platform first; else every other platform that's saved (the
    platforms of your collections), the best match used. When two or more
    platforms' best matches are within 5 points of each other, none is
    guessed: the game goes to Library > Needs your check, its line naming
    each one with its platform, and "Use" there becomes "Choose".
  "Choose" (interactive, and Needs your check) lists the matches of the
  same platforms, each with its platform, the system's own first on the
  same score; a match you picked is found on any of them and is used as
  it is. A game that still doesn't match goes to Needs your check, as
  before.
- **Update LaunchboxDB:** with a Switch 2 collection, the "Nintendo Switch"
  platform is saved too, also without a Switch collection. Android Games
  and Android Apps now count as LaunchBox's "Android" platform (before,
  only a collection called Android did), so it's saved for them too.
- **Why the Android fallback only searches the platforms you have:**
  saving every LaunchBox platform (190 platforms, ~190,000 games against
  ~18,600 for 7 now) would make the saved data about 10× bigger (roughly
  300 MB+), Update LaunchboxDB would hold several hundred MB in memory
  while writing it, and searching it all would load most of it; looking a
  platform up in the download only when needed would take tens of seconds
  or more per game. Only the platforms already saved are searched, and
  each is loaded the first time a fallback needs it in a run: no extra
  download, disk or Update time. A recomp whose original console has no
  collection of yours isn't found by itself (Needs your check, Choose).
- **Not changed:** the original Switch and every other collection (their
  own platform only, names compared as before); ScreenScraper and its
  system list; the threshold; saving; library refresh; XBSX; the genre
  tools; the controls.
- **Tests:** `test_LaunchBoxDb` (new): the platforms saved and searched,
  Switch 2 on its own platform first, the fall back to Switch, the Edition
  names (5 spellings, and on the LaunchBox side), Edition left alone for
  the original Switch, no fallback for the Switch, Android first, the
  Android fallback (an alternate name on Nintendo 64), the tie (Nintendo
  64 and Sega 32X: none used, both kept with their platform), a picked id
  found on a fallback platform, Choose's list with platforms.
- **Files:** `src/backend/LaunchBoxDb.h/.cpp` (`saved_platforms_for`,
  `search_platforms`, `best`, `platform_candidates`, Edition names, the
  Android names), `src/backend/Scraper.cpp` (`lbAuto`),
  `tests/backend/launchboxdb/` (new), `tests/backend/backend.pro`,
  `tests/CMakeLists.txt`.
- **Commit:** "LaunchBox: Switch 2 falls back to Switch, Android to your other platforms".

### Scraper: Nintendo Switch 2 is a LaunchBox system

- **What:** `Scraper::system_of()` now gives `switch2` for the Nintendo
  Switch 2 collection (build 2242). It only knew the systems on its
  ScreenScraper list and a few nicknames, so it gave nothing for Switch 2:
  Update LaunchboxDB didn't read the "Nintendo Switch 2" platform from the
  download, and every scrape route that uses `system_of()` had no system
  for its games ("LaunchBox has no platform for the system"). `switch2` is
  accepted as a LaunchBox-only system: LaunchBoxDb's `platform_for()`
  already maps it to "Nintendo Switch 2", and the scraper already skips
  ScreenScraper for a system that isn't on its list ("not a ScreenScraper
  system: LaunchBox and libretro only" in the log).
- **Not changed:** the ScreenScraper list and the other scraper tables (no
  Switch 2 there), the LaunchBox name matching, the original Switch, the
  genre work, themes, the Companion app.
- **Tests:** the test environment's LaunchBox step (with the real
  download) now has a Nintendo Switch 2 collection; a new step scrapes its
  game (Donkey Kong Bananza, a .nup) from LaunchBox, and the summary shows
  the platform's game count in the download, its index, the scrape's log
  and the files saved.
- **Files:** `src/backend/Scraper.cpp`, `tests/testenv/run.sh`.
- **Commit:** "Scraper: Nintendo Switch 2 is a LaunchBox system".

### Nintendo Switch 2 system for .nup files

- **What:** a new built-in system, "Nintendo Switch 2" (`switch2`), for
  `.nup` files (Peanuts Native's packages). It's in Settings > Game
  directory > Add systems like the others, a folder named `switch2` is
  recognized by its name, and it imports, rescans and is watched the same
  way. It has no launch command: none is documented for Peanuts Native, so
  none is made up; set one in Library (the system's emulator, or an
  emulator of yours) once you have one. The original Nintendo Switch is
  unchanged: it still takes .nca .nro .nso .nsp .xci, not .nup.
- **How:** ES-DE's bundled lists stay as ES-DE ships them; the system is in
  a small file of Pegasus X's own in the same format
  (`resources/pegasusx_systems.xml`), read after ES-DE's list (and after
  an ES-DE custom_systems file), so a `switch2` system there would be the
  one used. It's read even when a custom_systems file says loadExclusive.
- **Not changed:** the Switch system, ES-DE's files, the emulator lists,
  the genre work, builds 2234-2241, themes, the Companion app. The
  scrapers aren't told about Switch 2 (ScreenScraper has no id for it here;
  LaunchBox already knew the name).
- **Tests:** the fixture has MyGames/switch2/ (a .nup) and MyGames/switch/
  (a .nsp and a .nup); the summary checks that the .nup is a Nintendo
  Switch 2 game with no launch line, and that Nintendo Switch took only
  the .nsp.
- **Files:** `src/backend/providers/esde/resources/pegasusx_systems.xml`
  (new), `src/backend/providers/esde/esde.qrc`,
  `src/backend/providers/esde/EsdeProvider.cpp` (`load_import_setup`),
  `src/backend/providers/esde/resources/README.md`,
  `tests/testenv/fixture.sh`, `tests/testenv/run.sh`.
- **Commit:** "Nintendo Switch 2 system for .nup files".

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
