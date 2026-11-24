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


#include "SafeFile.h"

#include "Log.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#ifdef Q_OS_WIN
#include <io.h>
#else
#include <unistd.h>
#endif


namespace {
const QString LOG_TAG = QStringLiteral("Save");

safefile::Step hook(const char* step)
{
    return safefile::test_hook ? safefile::test_hook(step) : safefile::Step::Go;
}

// the file's bytes on the storage itself, not only in the system's memory
bool sync_to_storage(QFile& file)
{
    if (!file.flush())
        return false;
#ifdef Q_OS_WIN
    return _commit(file.handle()) == 0;
#else
    return ::fsync(file.handle()) == 0;
#endif
}

// 1. the usual way: QSaveFile (temporary file, flushed to the storage, then
// renamed over the old one); false: not saved, the old file as it was
bool write_usual(const QString& path, const QByteArray& data, bool text, bool* died)
{
    QSaveFile file(path);
    // (never Qt's own "direct write" fallback: that writes into the old file)
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly | (text ? QIODevice::Text : QIODevice::NotOpen)))
        return false;
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }
    const safefile::Step written = hook("usual-written");
    if (written == safefile::Step::Die) {
        *died = true;
        return false;
    }
    const safefile::Step commit = hook("usual-commit");
    if (commit == safefile::Step::Die) {
        *died = true;
        return false;
    }
    if (commit == safefile::Step::Fail) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

// 2. the fallback (see SafeFile.h); false: not saved, the old file as it was
bool write_fallback(const QString& path, const QByteArray& data, bool text, bool* died)
{
    const QString new_path = safefile::fallback_new_path(path);
    const QString old_path = safefile::fallback_old_path(path);

    // a. the whole new text next to it, on the storage, its size checked
    {
        QFile out(new_path);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | (text ? QIODevice::Text : QIODevice::NotOpen))) {
            Log::warning(LOG_TAG, LOGMSG("`%1` can't be created either").arg(new_path));
            return false;
        }
        const int half = data.size() / 2;
        if (out.write(data.constData(), half) != half) {
            out.close();
            QFile::remove(new_path);
            return false;
        }
        if (hook("fallback-half") == safefile::Step::Die) {
            *died = true;
            return false;
        }
        const bool ok = out.write(data.constData() + half, data.size() - half) == data.size() - half
                     && sync_to_storage(out);
        out.close();
        // (Text mode can add a byte per line end on Windows: the size on disk
        // is at least the data's)
        if (!ok || QFileInfo(new_path).size() < data.size()) {
            QFile::remove(new_path);
            Log::warning(LOG_TAG, LOGMSG("`%1` couldn't be written in full").arg(new_path));
            return false;
        }
    }
    if (hook("fallback-written") == safefile::Step::Die) {
        *died = true;
        return false;
    }

    // b. the old file to a free name (a file that isn't there yet: nothing to move)
    const bool had_old = QFileInfo::exists(path);
    if (had_old) {
        QFile::remove(old_path);  // (one left by an earlier save cut short)
        if (!QFile::rename(path, old_path)) {
            QFile::remove(new_path);
            Log::warning(LOG_TAG, LOGMSG("`%1` can't be renamed").arg(path));
            return false;
        }
    }
    if (hook("fallback-old-moved") == safefile::Step::Die) {
        *died = true;
        return false;
    }

    // c. the new text to the file's own name
    if (!QFile::rename(new_path, path)) {
        if (had_old)
            QFile::rename(old_path, path);
        QFile::remove(new_path);
        Log::warning(LOG_TAG, LOGMSG("`%1` can't be renamed to `%2`").arg(new_path, path));
        return false;
    }
    if (hook("fallback-new-moved") == safefile::Step::Die) {
        *died = true;
        return false;
    }

    // d. the old file's copy goes
    if (had_old && !QFile::remove(old_path))
        Log::warning(LOG_TAG, LOGMSG("`%1` couldn't be deleted (harmless)").arg(old_path));
    return true;
}
} // namespace


namespace safefile {

std::function<Step(const char*)> test_hook;

QString fallback_new_path(const QString& path)
{
    return path + QStringLiteral(".pxsave");
}

QString fallback_old_path(const QString& path)
{
    return path + QStringLiteral(".pxold");
}

bool write(const QString& path, const QByteArray& data, bool text)
{
    if (path.isEmpty())
        return false;
    QDir().mkpath(QFileInfo(path).absolutePath());

    bool died = false;
    if (write_usual(path, data, text, &died))
        return true;
    if (died)
        return false;

    Log::info(LOG_TAG, LOGMSG("`%1`: the usual safe save didn't work here, trying the fallback").arg(path));
    if (write_fallback(path, data, text, &died)) {
        Log::info(LOG_TAG, LOGMSG("`%1` saved (fallback)").arg(path));
        return true;
    }
    if (!died)
        Log::warning(LOG_TAG, LOGMSG("`%1` couldn't be saved; the file as it was is kept").arg(path));
    return false;
}

} // namespace safefile
