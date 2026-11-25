// Pegasus X
// Copyright (C) 2026  Esteban (MisterE88)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.


#include "SetupState.h"

#include "Paths.h"
#include "SafeFile.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>


namespace {
const QString DONE_FILE = QStringLiteral("pegasusx-setup-done");
const QString STEP_FILE = QStringLiteral("pegasusx-setup-step");
const QString OLD_STEP_FILE = QStringLiteral("pegasusx-wizard");        // builds up to 2274, private folder
const QString OLD_ASKED_FILE = QStringLiteral("pegasusx-root-asked");   // (same)
const QString OLD_ROOT_FILE = QStringLiteral("pegasusx-root.txt");

int read_step(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    const int step = QString::fromLatin1(file.readAll()).trimmed().toInt();
    return (step >= 1 && step <= 4) ? step : 0;
}

bool file_not_empty(const QString& path)
{
    const QFileInfo fi(path);
    return fi.isFile() && fi.size() > 0;
}

bool dir_has_files(const QString& path)
{
    if (!QFileInfo(path).isDir())
        return false;
    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        // (the game directory's own marker isn't data)
        if (!it.fileName().startsWith(QLatin1Char('.')))
            return true;
    }
    return false;
}
} // namespace


namespace setupstate {

QString done_path(const Dirs& d) { return d.app_data + QLatin1Char('/') + DONE_FILE; }
QString step_path(const Dirs& d) { return d.app_data + QLatin1Char('/') + STEP_FILE; }

bool holds_data(const QString& dir)
{
    if (dir.isEmpty())
        return false;
    // what you made: not settings.txt (a start writes its defaults)
    for (const char* name : { "game_dirs.txt", "favorites.txt", "pegasusx.ini", "pegasusx-rom-folders.txt", "stats.db" }) {
        if (file_not_empty(dir + QLatin1Char('/') + QLatin1String(name)))
            return true;
    }
    for (const char* name : { "gamedirectory", "metadata", "theme_settings", "downloaded_media", "imported" }) {
        if (dir_has_files(dir + QLatin1Char('/') + QLatin1String(name)))
            return true;
    }
    return false;
}

int step(const Dirs& d)
{
    return read_step(step_path(d));
}

void set_step(const Dirs& d, int n)
{
    if (n <= 0) {
        QFile::remove(step_path(d));
        return;
    }
    QDir().mkpath(d.app_data);
    safefile::write(step_path(d), QByteArray::number(n) + '\n', true);
}

bool finish(const Dirs& d)
{
    QDir().mkpath(d.app_data);
    const bool ok = safefile::write(done_path(d), QByteArrayLiteral("done\n"), true);
    QFile::remove(step_path(d));
    QFile::remove(d.priv + QLatin1Char('/') + OLD_STEP_FILE);
    return ok;
}

void reopen(const Dirs& d, int n)
{
    QFile::remove(done_path(d));
    set_step(d, n);
}

State decide(const Dirs& d)
{
    State out;
    if (QFileInfo::exists(done_path(d))) {
        out.done = true;
        out.log = QStringLiteral("Setup: done (`%1`)").arg(done_path(d));
        return out;
    }
    int n = step(d);
    if (n == 0) {
        // an older build's step, in the private folder: moved to the app's storage
        const QString old = d.priv + QLatin1Char('/') + OLD_STEP_FILE;
        n = read_step(old);
        if (n > 0) {
            set_step(d, n);
            QFile::remove(old);
        }
    }
    if (n > 0) {
        out.step = n;
        out.log = QStringLiteral("Setup: not done, goes on at step %1").arg(n);
        return out;
    }
    const bool older_done = QFileInfo::exists(d.priv + QLatin1Char('/') + OLD_ASKED_FILE)
                         || file_not_empty(d.priv + QLatin1Char('/') + OLD_ROOT_FILE);
    // the app's cache isn't empty: its data wasn't cleared (Android's Clear
    // data empties it too; an update keeps it)
    const bool app_data_kept = !d.cache.isEmpty()
        && !QDir(d.cache).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).isEmpty();
    if (older_done && app_data_kept) {
        finish(d);
        out.done = true;
        out.log = QStringLiteral("Setup: done by an older build: marker written (`%1`)").arg(done_path(d));
        return out;
    }
    out.step = 1;
    set_step(d, 1);
    if (holds_data(d.data)) {
        out.found = d.data;
        out.log = QStringLiteral("Setup: not done%1; Pegasus data found in `%2`: step 1 asks to use it or start fresh")
            .arg(older_done ? QStringLiteral(" (the app's own data was cleared)") : QString(), d.data);
    }
    else {
        out.log = QStringLiteral("Setup: not done, starts at step 1");
    }
    return out;
}

State& session()
{
    static State state = []{ State s; s.done = true; return s; }();
    return state;
}

Dirs dirs()
{
    Dirs d;
    d.app_data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    d.priv = paths::privateConfigDir();
    d.data = paths::writableConfigDir();
    d.cache = paths::writableCacheDir();
    return d;
}

} // namespace setupstate
