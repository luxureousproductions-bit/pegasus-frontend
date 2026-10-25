HEADERS += \
    $$PWD/PowerCommands.h \
    $$PWD/TerminalKbd.h \

SOURCES += \
    $$PWD/TerminalKbd.cpp \


win32 {
    SOURCES += $$PWD/PowerCommands_win.cpp
}
else:unix:!android {
    macx: SOURCES += $$PWD/PowerCommands_mac.cpp
    else: SOURCES += $$PWD/PowerCommands_linux.cpp
}
else {
    SOURCES += $$PWD/PowerCommands_unimpl.cpp
}

# Pegasus X: the desktop test environment runs the Android-only parts with
# stand-ins for the Android calls
!android:!isEmpty(PEGASUSX_TESTENV) {
    DEFINES *= PEGASUSX_TESTENV
    HEADERS += $$PWD/AndroidHelpers.h
    SOURCES += $$PWD/AndroidHelpers_testenv.cpp
}

android {
    HEADERS += \
        $$PWD/AndroidAppIconProvider.h \
        $$PWD/AndroidHelpers.h \
        $$PWD/AndroidPdfProvider.h

    SOURCES += \
        $$PWD/AndroidAppIconProvider.cpp \
        $$PWD/AndroidHelpers.cpp \
        $$PWD/AndroidPdfProvider.cpp
}
