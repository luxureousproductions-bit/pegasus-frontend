TARGET = test_Scraper
SOURCES = $${TARGET}.cpp
QT += network
# Pegasus X: the ESRB server's platform table, compared with the app's
DEFINES += PEGASUSX_WORKER_JS=\\\"$$TOP_SRCDIR/server/esrb-worker/worker.js\\\"

include($${TOP_SRCDIR}/tests/cxxtest_common.pri)
