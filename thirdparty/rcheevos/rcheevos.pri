# Pegasus X: RetroAchievements' hash module (rcheevos, MIT): the hash RA
# identifies a game by, with the per-console rules. Only the hashing
# sources are here; nothing talks to the network.
INCLUDEPATH += $$PWD/include $$PWD/src/util
DEFINES += RC_STATIC RC_HASH_NO_ENCRYPTED

SOURCES += \
    $$PWD/src/hash/rc_cdreader.c \
    $$PWD/src/hash/rc_hash.c \
    $$PWD/src/hash/rc_hash_disc.c \
    $$PWD/src/hash/rc_hash_rom.c \
    $$PWD/src/hash/rc_hash_zip.c \
    $$PWD/src/util/md5.c \
    $$PWD/src/util/rc_compat.c \
    $$PWD/src/util/rc_util.c \
    $$PWD/src/util/rc_version.c

HEADERS += \
    $$PWD/include/rc_hash.h \
    $$PWD/include/rc_consoles.h \
    $$PWD/include/rc_export.h \
    $$PWD/include/rc_error.h \
    $$PWD/include/rc_util.h \
    $$PWD/src/hash/rc_hash_internal.h \
    $$PWD/src/util/md5.h \
    $$PWD/src/util/rc_compat.h \
    $$PWD/src/util/rc_version.h
