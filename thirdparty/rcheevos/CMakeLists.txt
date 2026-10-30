# Pegasus X: RetroAchievements' hash module (rcheevos, MIT)
add_library(rcheevos-hash STATIC
    src/hash/rc_cdreader.c
    src/hash/rc_hash.c
    src/hash/rc_hash_disc.c
    src/hash/rc_hash_rom.c
    src/hash/rc_hash_zip.c
    src/util/md5.c
    src/util/rc_compat.c
    src/util/rc_util.c
    src/util/rc_version.c
)
target_include_directories(rcheevos-hash PUBLIC include PRIVATE src/util)
target_compile_definitions(rcheevos-hash PUBLIC RC_STATIC RC_HASH_NO_ENCRYPTED)
set_target_properties(rcheevos-hash PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
