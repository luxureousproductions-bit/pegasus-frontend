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


#pragma once

#include <QString>
#include <QStringList>
#include <atomic>
#include <functional>
#include <vector>


// Pegasus X: Settings > Backup & reset. A backup is one file (a tar archive,
// so it has no size or file-count limit): Pegasus X's own data, named in the
// lists below (BACKUP_FILES, BACKUP_DIRS, and ART_DIR when asked), never a
// password, key or token. Restore checks the whole file before it changes
// anything, saves the data as it is first (backup/), and keeps the logins on
// the device. Start over deletes only the files and folders named in
// WIPE_*, each one checked to be inside its folder (no link followed out of
// it); the themes, the ROM folders and everything else are never touched.
// Every function takes the folders it works in (Places), so the tests run
// them on throwaway folders.
namespace backup {

struct Places {
    QString data;      ///< the Pegasus X folder (paths::writableConfigDir)
    QString priv;      ///< app storage: the folder setting, setup markers (paths::privateConfigDir)
    QString app_data;  ///< app storage: pegasusx-private.ini, the GitHub token
    QString cache;     ///< caches (paths::writableCacheDir)
    QStringList never; ///< folders that are never a base (storage roots, home)

    static Places current();
};

/// What a backup holds, in the Pegasus X folder (files; folders and all in them)
const QStringList& backup_files();
const QStringList& backup_dirs();
const QString& art_dir();

/// Download (the default place for a backup)
QString default_folder();
/// PegasusX-backup-<date>-<time>-build<n>.tar
QString file_name(int build);
/// The size of the scraped art (downloaded_media), bytes
qint64 art_bytes(const Places&);

struct Progress {
    std::function<void(int)> percent;          ///< 0..100
    const std::atomic<bool>* cancel = nullptr;  ///< true: stop (nothing is left behind)
};

/// Writes a backup to `path` (as `path`.part, renamed when done; removed when
/// stopped or failed). "" when done, else why not ("Stopped" when stopped).
QString make(const Places&, const QString& path, bool art, int build, const Progress& = {});

struct Info {
    QString error;   ///< "": a backup this build can restore
    QString date;    ///< ISO date and time it was made
    int build = 0;
    bool art = false;
    int files = 0;
    qint64 bytes = 0;
};
/// The backup's details; its structure checked (not each file's checksum: restore does that)
Info read_info(const QString& path);

struct RestoreResult {
    QString error;        ///< "": restored
    QString saved_first;  ///< the backup of the data as it was (backup/)
    QStringList sign_in;  ///< logins the device doesn't have (ScreenScraper, RetroAchievements)
};
/// Checks the whole file first (nothing changes if it's not a good backup),
/// saves the data as it is, then puts the backup's in place of it. Logins on
/// the device are kept; the art in the backup is added to the art there.
RestoreResult restore(const Places&, const QString& path, int build, const Progress& = {});

/// pegasusx.ini without passwords, keys and tokens
QByteArray strip_secrets(const QByteArray& ini);
bool is_secret_key(const QString& key);

struct WipeItem {
    QString path;
    QString what;
    qint64 bytes = 0;
};
struct WipePlan {
    std::vector<WipeItem> remove;  ///< what's there now and goes
    QStringList keep;              ///< what stays, in words
};
WipePlan wipe_plan(const Places&);
/// Deletes what wipe_plan lists. "" when done, else what couldn't be deleted
QString wipe(const Places&, QStringList* removed = nullptr);

/// `path` (a name in `base`) can be deleted: `base` is a real folder, not a
/// storage root, and `path` is directly in it by name
bool safe_target(const Places&, const QString& base, const QString& path);

} // namespace backup
