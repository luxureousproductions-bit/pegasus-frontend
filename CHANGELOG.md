# Pegasus X changelog

Newest first. Each entry is one commit on `pegasus-x`, so any of them can be
rolled back on its own with `git revert <commit>`.

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
