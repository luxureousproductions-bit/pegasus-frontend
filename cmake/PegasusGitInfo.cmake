execute_process(COMMAND
    git
    --git-dir "${PROJECT_SOURCE_DIR}/.git"
    --work-tree "${PROJECT_SOURCE_DIR}"
    describe
    --always
    --dirty
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE PEGASUS_GIT_REVISION
)

execute_process(COMMAND
    git
    --git-dir "${PROJECT_SOURCE_DIR}/.git"
    --work-tree "${PROJECT_SOURCE_DIR}"
    log -1
    --format=%cd
    --date=short
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE PEGASUS_GIT_DATE
)

execute_process(COMMAND
    git
    --git-dir "${PROJECT_SOURCE_DIR}/.git"
    --work-tree "${PROJECT_SOURCE_DIR}"
    rev-list
    --count HEAD
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE PEGASUS_GIT_COMMIT_CNT
)

# Pegasus X: the build number (as in .qmake.conf): 2000 + the GitHub workflow
# run number for builds made by GitHub, else the commit count
if(DEFINED ENV{GITHUB_RUN_NUMBER} AND NOT "$ENV{GITHUB_RUN_NUMBER}" STREQUAL "")
    math(EXPR PEGASUSX_BUILD "2000 + $ENV{GITHUB_RUN_NUMBER}")
elseif(PEGASUS_GIT_COMMIT_CNT)
    set(PEGASUSX_BUILD "${PEGASUS_GIT_COMMIT_CNT}")
else()
    set(PEGASUSX_BUILD 0)
endif()
