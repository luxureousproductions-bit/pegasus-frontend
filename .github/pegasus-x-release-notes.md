**Pegasus X** is an unofficial fork of [Pegasus Frontend](https://pegasus-frontend.org) for Android gaming handhelds, made for the [XboxOSv2](https://github.com/luxureousproductions-bit/XboxOSv2) theme but working with any theme. Not affiliated with the Pegasus project, please report issues [here](https://github.com/luxureousproductions-bit/pegasus-frontend/issues), not upstream.

### What's different from Pegasus
- **SD card ready at boot:** as the home app, Pegasus X waits for the SD card to finish mounting before loading, so collections on the card are no longer missing after a reboot
- **Instant return from games:** the theme stays loaded while a game runs, so coming back is instant and the theme keeps its place
- **Game manuals (PDF):** themes can show PDF manuals with Android's built-in PDF renderer (`image://pdf/...`, `api.pdfPageCount()`); XboxOSv2's `pdf-manuals` branch has a full reader
- Installs **next to** the official Pegasus (separate app, "Pegasus X")

### Install
- Download `pegasus-x_..._android64.apk` (almost every handheld) below, or `_android` for older 32-bit devices
- Pegasus X has its own settings folder: `Android/data/org.pegasus_frontend.android.xbox/files/pegasus-frontend/`. Copy your themes and `game_dirs.txt` there from your Pegasus folder, or add your game folders again in Settings
- Updates: add this repository to [Obtainium](https://github.com/ImranR98/Obtainium) and enable pre-releases

Pegasus X is GPLv3 like Pegasus; the full source is in this repository.
