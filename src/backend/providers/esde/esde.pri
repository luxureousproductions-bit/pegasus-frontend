ENABLED_COMPATS += "ES-DE"

DEFINES *= WITH_COMPAT_ESDE

HEADERS += $$PWD/EsdeProvider.h
SOURCES += $$PWD/EsdeProvider.cpp
RESOURCES += $$PWD/esde.qrc

# The gamelist.xml reader is shared with the EmulationStation data source
!contains(DEFINES, WITH_COMPAT_ES2) {
    HEADERS += $$PWD/../es2/Es2Metadata.h $$PWD/../es2/Es2Systems.h
    SOURCES += $$PWD/../es2/Es2Metadata.cpp
}
