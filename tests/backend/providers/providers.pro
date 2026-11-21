TEMPLATE = subdirs

SUBDIRS += \
    pegasus \
    pegasus_media \
    emulationstation \
    favorites \
    gamedirectory \
    logiqx \
    playtime \

win32: SUBDIRS += \
    launchbox \
    playnite \
