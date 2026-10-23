// Pegasus X: the app's ScreenScraper developer login.
//
// Never put real values here. The Android build writes this file from the
// repository secrets SCREENSCRAPER_DEVID and SCREENSCRAPER_DEVPASSWORD (see
// .github/workflows/android.yml), XOR-ed with PEGASUSX_SS_MASK so they don't
// sit in the app as plain text. Without the secrets (eg. a fork), they stay
// empty and the scraper says ScreenScraper isn't available in this build.

#pragma once

#define PEGASUSX_SS_MASK 0x5A
static const unsigned char PEGASUSX_SS_DEVID[] = { 0 };
static const int PEGASUSX_SS_DEVID_LEN = 0;
static const unsigned char PEGASUSX_SS_DEVPASSWORD[] = { 0 };
static const int PEGASUSX_SS_DEVPASSWORD_LEN = 0;
