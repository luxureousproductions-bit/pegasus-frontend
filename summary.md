# Test environment results

## Imports (first start)
    2026-10-31T17:28:46 [i] Test: ES-DE import: 2 systems, 4 games, error ``
    2026-10-31T17:28:46 [i] Test: ROM folder `/home/runner/work/_temp/px/MyGames`: systems n64=2, psx=6
    2026-10-31T17:28:46 [i] Test: ROM folder import: 2 systems, 4 games, error ``

## Your own file wins over an import
    OK: Chrono Trigger has the title from your own file
    OK: it has ES-DE's screenshot too (import data on top of your file)

## Scrape without a login (Hasheous + libretro)
    file: downloaded_media/snes/covers/Super Mario World (USA).png
    file: downloaded_media/snes/miximages/Super Mario World (USA).png
    file: downloaded_media/snes/screenshots/Super Mario World (USA).png
    file: downloaded_media/snes/titlescreens/Super Mario World (USA).png

## Scrape from the LaunchBox database (no login)
    [i] qml: [test] LaunchBox database: reading in
    [i] LaunchBox: Sony Playstation: 4651 games
    [i] LaunchBox: Nintendo Game Boy Advance: 2378 games
    [i] LaunchBox: Super Nintendo Entertainment System: 2964 games
    [i] LaunchBox: Nintendo GameCube: 814 games
    [i] LaunchBox: Nintendo 64: 818 games
    [i] LaunchBox: The database is ready
    [i] qml: [test] LaunchBox database: ready, 11625 games
    [i] Scraper: Scraping 1 games into `/home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media` (source: launchbox, interactive: no, overwrite: yes, ScreenScraper login: missing, LaunchBox database: yes)
    [i] Scraper: LaunchBox: using "Super Mario World" (Super Nintendo Entertainment System, id 136)
    [i] Scraper: LaunchBox: 10 pictures to download (of 73 it lists)
    [i] Scraper: Downloaded covers (12389 KB) -> https://images.launchbox-app.com/00fb4aca-c22f-4ca9-84d0-90cf8c22d4af.png
    [i] Scraper: Downloaded 3dboxes (1342 KB) -> https://images.launchbox-app.com/26563ff1-d24d-487b-b651-5a71fbe59a8c.png
    [i] Scraper: Downloaded backcovers (1418 KB) -> https://images.launchbox-app.com/1c158bd5-22be-49d8-a0ca-895e1a206f5a.jpg
    [i] Scraper: Downloaded physicalmedia (1032 KB) -> https://images.launchbox-app.com/a4906ecc-e7dd-4bda-ab8e-b57a461794d0.png
    [i] Scraper: Downloaded screenshots (72 KB) -> https://images.launchbox-app.com/20e828d7-c019-40d5-9165-3541da89304b.png
    file: downloaded_media/snes/3dboxes/Super Mario World (USA).png
    file: downloaded_media/snes/fanart/Super Mario World (USA).jpg
    file: downloaded_media/snes/physicalmedia/Super Mario World (USA).png
    file: downloaded_media/snes/titlescreens/Super Mario World (USA).png
    file: downloaded_media/snes/marquees/Super Mario World (USA).png
    file: downloaded_media/snes/banners/Super Mario World (USA).png
    file: downloaded_media/snes/screenshots/Super Mario World (USA).png
    file: downloaded_media/snes/covers/Super Mario World (USA).png
    file: downloaded_media/snes/spines/Super Mario World (USA).jpg
    file: downloaded_media/snes/miximages/Super Mario World (USA).png

## Library as read (last start)
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/gba/metadata.pegasus.txt`
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/gc/metadata.pegasus.txt`
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/my_snes_picks/metadata.pegasus.txt`
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/n64/metadata.pegasus.txt`
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/psx/metadata.pegasus.txt`
    [i] Pegasus Metafiles: Found `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/snes/metadata.pegasus.txt`
    [i] GOG: 0 games found
    [i] Library: `Nintendo 64` launches with RetroArch · Mupen64Plus-Next (recommended; none set in its metadata)
    [i] Library: `Nintendo Game Boy Advance` launches with RetroArch · mGBA (recommended; none set in its metadata)
    [i] Library: `Nintendo SNES (Super Nintendo)` launches with RetroArch · Snes9x - Current (recommended; none set in its metadata)
    [i] Library: `Sony PlayStation` launches with RetroArch · Beetle PSX (recommended; none set in its metadata)
    [i] 10 games found

## Warnings and errors
         12 [w] qrc:/themes/pegasus-theme-grid/layer_platform/PlatformCard.qml:71:5: QML Image: Cannot open: qrc:/themes/pegasus-theme-grid/assets/logos/my snes picks.svg
         12 [w] The collection 'GOG' has no valid games, ignored
         11 [w] Pegasus Metafiles: `/home/runner/work/_temp/px/home/.config/pegasus-frontend/gamedirectory/n64/metadata.pegasus.txt`, line 11: Asset file `/home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/n64/fanart/Mario Kart 64 (USA).png` doesn't seem to exist
          7 [w] libpng warning: iCCP: known incorrect sRGB profile
          5 [w] qrc:/frontend/menu/settings/common/SimpleButton.qml:34: TypeError: Cannot read property 'width' of null
          2 [w] qrc:/frontend/menu/settings/common/ToggleOption.qml:31: TypeError: Cannot read property 'width' of null
          2 [w] qrc:/frontend/menu/settings/common/MultivalueOption.qml:33: TypeError: Cannot read property 'width' of null
          2 [w] libpng warning: profile matches sRGB but writing iCCP instead
          2 [w] libpng warning: known incorrect sRGB profile
          1 [w] ES-DE import: No gamelist file found for system `n64`
          1 [w] ES-DE import: No gamelist file found for system `gba`

## Art each game has (last start)
    Chrono Trigger (my own file)
      screenshot: storage/ES-DE/downloaded_media/snes/screenshots/Chrono Trigger (USA).png
      steam: home/.config/pegasus-frontend/downloaded_media/my snes picks/miximages/Chrono Trigger (USA).png
    Crash Bandicoot
      boxFront: MyGames/psx/media/crash.png
    Final Fantasy VII (USA)
    Mario Kart 64 (USA)
      boxFront: MyGames/Nintendo 64/media/covers/Mario Kart 64 (USA).png
      boxFront: MyGames/Nintendo 64/media/3D Boxes/Mario Kart 64 (USA).png
      logo: MyGames/Nintendo 64/media/wheel/Mario Kart 64 (USA).png
      screenshot: MyGames/Nintendo 64/media/screenshot/Mario Kart 64 (USA).png
      background: MyGames/Nintendo 64/media/fanart/Mario Kart 64 (USA).png
      steam: home/.config/pegasus-frontend/downloaded_media/n64/miximages/Mario Kart 64 (USA).png
      extra.box3d: file://MyGames/Nintendo 64/media/3D Boxes/Mario Kart 64 (USA).png
    Metroid Fusion (USA)
    Metroid Prime
    Star Fox 64 (USA)
      boxFront: MyGames/Nintendo 64/media/Star Fox 64 (USA)/boxFront.png
      screenshot: MyGames/Nintendo 64/media/Star Fox 64 (USA)/screenshot.png
      steam: home/.config/pegasus-frontend/downloaded_media/n64/miximages/Star Fox 64 (USA).png
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
    01-first-start: Pegasus Metafiles: Finished searching in 1ms · GOG: 0 games found · Game list post-processing took 0ms · 10 games found · 
    02-settings: Pegasus Metafiles: Finished searching in 1ms · GOG: 0 games found · Game list post-processing took 0ms · 10 games found · 
    03-library: Pegasus Metafiles: Finished searching in 1ms · GOG: 0 games found · Game list post-processing took 0ms · 10 games found · 
    04-scraper: Pegasus Metafiles: Finished searching in 1ms · GOG: 0 games found · Game list post-processing took 0ms · 10 games found · 
    05-scraper-accounts: Pegasus Metafiles: Finished searching in 1ms · GOG: 0 games found · Game list post-processing took 0ms · 10 games found · 
    06-themes: Pegasus Metafiles: Finished searching in 1ms · GOG: 0 games found · Game list post-processing took 0ms · 10 games found · 
    07-updater: Pegasus Metafiles: Finished searching in 1ms · GOG: 0 games found · Game list post-processing took 0ms · 10 games found · 
    08-gamedirs: Pegasus Metafiles: Finished searching in 1ms · GOG: 0 games found · Game list post-processing took 0ms · 10 games found · 
    09-esde-import: Pegasus Metafiles: Finished searching in 1ms · GOG: 0 games found · Game list post-processing took 0ms · 10 games found · 
    10-theme: Pegasus Metafiles: Finished searching in 1ms · GOG: 0 games found · Game list post-processing took 0ms · 10 games found · 
    12-scrape-free: Pegasus Metafiles: Finished searching in 1ms · GOG: 0 games found · Game list post-processing took 0ms · 10 games found · 
    13-scrape-launchbox: Pegasus Metafiles: Finished searching in 1ms · GOG: 0 games found · Game list post-processing took 0ms · 10 games found · 

## Screen open times
    EsdeImport.qml opened in 15 ms
    GameDirectory.qml opened in 21 ms
    Library: collection "Nintendo Game Boy Advance" (1 games) opened in 10 ms
    MetadataEditor.qml opened in 183 ms
    ScraperScreen.qml opened in 28 ms
    ScraperScreen.qml opened in 29 ms
    SettingsScreen.qml opened in 18 ms
    ThemeDownloader.qml opened in 24 ms
    UpdateDialog.qml opened in 9 ms

## The game directory (and backups)
    gamedirectory/.migrated
    gamedirectory/n64/metadata.pegasus.txt
    gamedirectory/psx/metadata.pegasus.txt
    gamedirectory/my_snes_picks/metadata.pegasus.txt
    gamedirectory/gba/metadata.pegasus.txt
    gamedirectory/gc/metadata.pegasus.txt
    gamedirectory/snes/metadata.pegasus.txt
    gamedirectory/imported-folders.json
    backup/old-metadata-20261031-172846/game_dirs.txt
    backup/snes/metadata.pegasus.txt.scrape.20261031-173229
    backup/snes/metadata.pegasus.txt.scrape.20261031-173345
    imported/snes/gamelist.xml
    metadata/library-index.json

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
    # Made from your library on 2026-10-31T17:28:46.
    # Pegasus X writes here (imports, Settings > Library, the scraper).
    
    collection: GameCube
    shortname: gc
    launch: dolphin-emu -b -e "{file.path}"
    
    game: Metroid Prime
    file: /home/runner/work/_temp/px/Pegasus/gamecube/Metroid Prime (USA).iso
    description: Written in a Pegasus metadata file.
    
    game: Wind Waker (USA)
    file: /home/runner/work/_temp/px/Pegasus/gamecube/Wind Waker (USA).iso
    

### my_snes_picks/metadata.pegasus.txt
    # Pegasus X game directory: My SNES picks
    # Made from your library on 2026-10-31T17:28:46.
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
    description: From the folder's own gamelist.xml.
    assets.box_front: /home/runner/work/_temp/px/MyGames/psx/media/crash.png
    
    game: Final Fantasy VII (USA)
    file: /home/runner/work/_temp/px/MyGames/psx/Final Fantasy VII (USA).m3u
    

### snes/metadata.pegasus.txt
    # Pegasus X game directory: Nintendo SNES (Super Nintendo)
    # Made by Pegasus X from ES-DE (/home/runner/work/_temp/px/storage/ES-DE).
    # Pegasus X writes here (imports, Settings > Library, the scraper).
    # folder: /home/runner/work/_temp/px/storage/ROMs/snes
    
    collection: Nintendo SNES (Super Nintendo)
    shortname: snes
    
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
    assets.titlescreen: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/titlescreens/Super Mario World (USA).png
    assets.background: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/fanart/Super Mario World (USA).jpg
    assets.banner: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/banners/Super Mario World (USA).png
    assets.box_back: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/backcovers/Super Mario World (USA).jpg
    assets.box_spine: /home/runner/work/_temp/px/home/.config/pegasus-frontend/downloaded_media/snes/spines/Super Mario World (USA).jpg
    
    game: The Legend of Zelda - A Link to the Past (USA)
    file: /home/runner/work/_temp/px/storage/ROMs/snes/The Legend of Zelda - A Link to the Past (USA).sfc
    
