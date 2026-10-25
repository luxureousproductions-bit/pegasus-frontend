#!/bin/bash
# Pegasus X test environment: a small library that has every kind of source
# Pegasus X reads, so they can be checked working together.
#   $1: the folder to make it in
# Makes:
#   storage/ES-DE ............ an ES-DE setup (settings, a gamelist, art)
#   storage/ROMs/snes, gba ... its ROMs folder (snes has a gamelist and art)
#   MyGames/Nintendo 64 ...... a ROM folder without metadata, named the usual way
#   MyGames/psx .............. a ROM folder with a gamelist.xml of its own and
#                              a two-disc game (.cue/.bin + .m3u)
#   Pegasus/gamecube ......... a folder with a Pegasus metadata file
#   home/.config/pegasus-frontend/game_dirs.txt (Pegasus/gamecube)
set -euo pipefail
T="$1"
S="$T/storage"
rm -rf "$T"
mkdir -p "$S/ES-DE/settings" "$S/ES-DE/gamelists/snes" "$S/ROMs/snes" "$S/ROMs/gba" \
         "$T/MyGames/Nintendo 64" "$T/MyGames/psx/media" "$T/Pegasus/gamecube" \
         "$T/home/.config/pegasus-frontend"

# a picture with a label (ImageMagick)
pic() {  # file size color label (the label left out if ImageMagick can't draw text)
    mkdir -p "$(dirname "$1")"
    convert -size "$2" "xc:$3" -fill white -gravity center -pointsize 34 -annotate 0 "$4" "$1" 2>/dev/null \
        || convert -size "$2" "xc:$3" "$1"
}
rom() { head -c 2048 /dev/urandom > "$1"; }

# -- ES-DE ------------------------------------------------------------------------
cat > "$S/ES-DE/settings/es_settings.xml" <<XML
<?xml version="1.0"?>
<string name="ROMDirectory" value="$S/ROMs" />
<string name="MediaDirectory" value="" />
<bool name="ShowHiddenGames" value="true" />
XML
for g in "Super Mario World (USA)" "The Legend of Zelda - A Link to the Past (USA)" "Chrono Trigger (USA)"; do
    rom "$S/ROMs/snes/$g.sfc"
done
cat > "$S/ES-DE/gamelists/snes/gamelist.xml" <<'XML'
<?xml version="1.0"?>
<gameList>
  <game>
    <path>./Super Mario World (USA).sfc</path>
    <name>Super Mario World</name>
    <desc>Mario and Yoshi in Dinosaur Land. (from the ES-DE gamelist)</desc>
    <developer>Nintendo EAD</developer>
    <publisher>Nintendo</publisher>
    <genre>Platform</genre>
    <releasedate>19901121T000000</releasedate>
    <players>2</players>
  </game>
  <game>
    <path>./Chrono Trigger (USA).sfc</path>
    <name>Chrono Trigger</name>
    <desc>A time-travel RPG. (from the ES-DE gamelist)</desc>
  </game>
</gameList>
XML
M="$S/ES-DE/downloaded_media/snes"
pic "$M/covers/Super Mario World (USA).png" 400x560 "#c33" "SMW cover"
pic "$M/screenshots/Super Mario World (USA).png" 512x448 "#36c" "SMW screenshot"
pic "$M/marquees/Super Mario World (USA).png" 600x200 "#fb0" "SMW logo"
pic "$M/3dboxes/Super Mario World (USA).png" 420x560 "#933" "SMW 3D box"
pic "$M/physicalmedia/Super Mario World (USA).png" 400x300 "#777" "SMW cart"
pic "$M/screenshots/Chrono Trigger (USA).png" 512x448 "#396" "CT screenshot"
rom "$S/ROMs/gba/Metroid Fusion (USA).gba"

# -- a ROM folder without metadata ---------------------------------------------------
rom "$T/MyGames/Nintendo 64/Mario Kart 64 (USA).z64"
rom "$T/MyGames/Nintendo 64/Star Fox 64 (USA).z64"

# -- a ROM folder with its own gamelist.xml and a two-disc game ----------------------------
for d in 1 2; do
    rom "$T/MyGames/psx/Final Fantasy VII (USA) (Disc $d).bin"
    printf 'FILE "Final Fantasy VII (USA) (Disc %s).bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n' "$d" \
        > "$T/MyGames/psx/Final Fantasy VII (USA) (Disc $d).cue"
done
printf 'Final Fantasy VII (USA) (Disc 1).cue\nFinal Fantasy VII (USA) (Disc 2).cue\n' > "$T/MyGames/psx/Final Fantasy VII (USA).m3u"
rom "$T/MyGames/psx/Crash Bandicoot (USA).chd"
pic "$T/MyGames/psx/media/crash.png" 400x400 "#e70" "Crash (gamelist image)"
cat > "$T/MyGames/psx/gamelist.xml" <<'XML'
<?xml version="1.0"?>
<gameList>
  <game>
    <path>./Crash Bandicoot (USA).chd</path>
    <name>Crash Bandicoot</name>
    <desc>From the folder's own gamelist.xml.</desc>
    <image>./media/crash.png</image>
  </game>
</gameList>
XML

# -- a folder with Pegasus metadata ---------------------------------------------------------
rom "$T/Pegasus/gamecube/Metroid Prime (USA).iso"
rom "$T/Pegasus/gamecube/Wind Waker (USA).iso"
cat > "$T/Pegasus/gamecube/metadata.pegasus.txt" <<'TXT'
collection: GameCube
shortname: gc
extensions: iso
launch: dolphin-emu -b -e "{file.path}"

game: Metroid Prime
file: Metroid Prime (USA).iso
description: Written in a Pegasus metadata file.
TXT

echo "$T/Pegasus/gamecube" > "$T/home/.config/pegasus-frontend/game_dirs.txt"
echo "Fixture ready in $T"
