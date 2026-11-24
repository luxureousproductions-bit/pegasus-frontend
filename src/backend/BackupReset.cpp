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


#include "BackupReset.h"

#include "Log.h"
#include "Paths.h"
#if defined(Q_OS_ANDROID) || defined(PEGASUSX_TESTENV)
#include "platform/AndroidHelpers.h"
#endif

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QStringBuilder>
#include <algorithm>
#include <array>


namespace {
const QString LOG_TAG = QStringLiteral("Backup");
const QString MANIFEST = QStringLiteral("pegasusx-backup.json");
const QString FILE_LIST = QStringLiteral("pegasusx-backup-files.json");
const QString DATA_PREFIX = QStringLiteral("data/");
constexpr int FORMAT = 1;
constexpr qint64 CHUNK = 1 << 20;

// files inside the folders above that are only made again (not data)
const QStringList& backup_skip()
{
    static const QStringList list { QStringLiteral("metadata/library-index.json") };
    return list;
}

// -- CRC-32 (the zip/PNG one) ----------------------------------------------------

const std::array<quint32, 256>& crc_table()
{
    static const std::array<quint32, 256> table = []{
        std::array<quint32, 256> t {};
        for (quint32 i = 0; i < 256; i++) {
            quint32 c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    return table;
}
quint32 crc_update(quint32 crc, const char* data, qint64 len)
{
    const auto& t = crc_table();
    crc = ~crc;
    for (qint64 i = 0; i < len; i++)
        crc = t[(crc ^ static_cast<uchar>(data[i])) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

// -- tar (ustar, long names as pax headers) ----------------------------------------------

QByteArray octal(qint64 value, int width)
{
    // width bytes: the digits, then a NUL
    return QByteArray::number(value, 8).rightJustified(width - 1, '0') + '\0';
}

QByteArray tar_header(const QByteArray& name, qint64 size, char type)
{
    QByteArray h(512, '\0');
    const auto put = [&h](int at, const QByteArray& value) {
        for (int i = 0; i < value.size(); i++)
            h[at + i] = value[i];
    };
    put(0, name.left(99));
    put(100, octal(0644, 8));
    put(108, octal(0, 8));
    put(116, octal(0, 8));
    put(124, octal(size, 12));
    put(136, octal(QDateTime::currentSecsSinceEpoch(), 12));
    put(148, QByteArray(8, ' '));
    h[156] = type;
    put(257, QByteArray("ustar\0" "00", 8));
    quint32 sum = 0;
    for (const char c : h)
        sum += static_cast<uchar>(c);
    put(148, QByteArray::number(sum, 8).rightJustified(6, '0') + QByteArray("\0 ", 2));
    return h;
}

bool header_checksum_ok(const QByteArray& h)
{
    const QByteArray stored = h.mid(148, 8);
    bool ok = false;
    const quint32 want = stored.trimmed().replace('\0', "").trimmed().toUInt(&ok, 8);
    if (!ok)
        return false;
    quint32 sum = 0;
    for (int i = 0; i < 512; i++)
        sum += (i >= 148 && i < 156) ? static_cast<quint32>(' ') : static_cast<uchar>(h[i]);
    return sum == want;
}

class TarWriter {
public:
    explicit TarWriter(QFile& file) : m_file(file) {}

    bool entry(const QString& name, qint64 size)
    {
        const QByteArray utf8 = name.toUtf8();
        if (utf8.size() > 99) {
            // a pax header with the whole name ("<len> path=<name>\n")
            const QByteArray body = QByteArray(" path=") + utf8 + '\n';
            int len = body.size() + 1;
            while (QByteArray::number(len).size() + body.size() != len)
                len = QByteArray::number(len).size() + body.size();
            const QByteArray record = QByteArray::number(len) + body;
            if (!write(tar_header(QByteArrayLiteral("PaxHeader"), record.size(), 'x')) || !write(record) || !pad(record.size()))
                return false;
        }
        return write(tar_header(utf8, size, '0'));
    }
    bool write(const QByteArray& data) { return m_file.write(data) == data.size(); }
    bool write(const char* data, qint64 len) { return m_file.write(data, len) == len; }
    bool pad(qint64 size)
    {
        const qint64 rest = size % 512;
        return rest == 0 || write(QByteArray(static_cast<int>(512 - rest), '\0'));
    }
    bool finish() { return write(QByteArray(1024, '\0')); }

private:
    QFile& m_file;
};

struct TarEntry {
    QString name;
    qint64 size = 0;
    qint64 offset = 0;  ///< where its data starts
};

// every entry of a tar file, each header's checksum checked; "" or why not
QString scan_tar(QFile& file, std::vector<TarEntry>& out)
{
    QString pax_name;
    const qint64 total = file.size();
    while (true) {
        const QByteArray h = file.read(512);
        if (h.size() < 512)
            return QStringLiteral("the file ends too early");
        if (h.count('\0') == 512)
            return {};  // the end
        if (!header_checksum_ok(h))
            return QStringLiteral("a damaged header");
        bool ok = false;
        const qint64 size = QByteArray(h.mid(124, 12)).replace('\0', "").trimmed().toLongLong(&ok, 8);
        if (!ok || size < 0 || file.pos() + size > total)
            return QStringLiteral("a damaged header");
        const char type = h[156];
        const qint64 data_at = file.pos();
        const qint64 next = data_at + ((size + 511) / 512) * 512;
        if (type == 'x') {
            const QByteArray records = file.read(size);
            int at = 0;
            while (at < records.size()) {
                const int sp = records.indexOf(' ', at);
                bool len_ok = false;
                const int len = sp < 0 ? 0 : records.mid(at, sp - at).toInt(&len_ok);
                if (!len_ok || len <= 0 || at + len > records.size())
                    return QStringLiteral("a damaged header");
                const QByteArray record = records.mid(sp + 1, at + len - sp - 2);  // without the newline
                if (record.startsWith("path="))
                    pax_name = QString::fromUtf8(record.mid(5));
                at += len;
            }
        }
        else if (type == '0' || type == '\0') {
            TarEntry e;
            if (!pax_name.isEmpty())
                e.name = pax_name;
            else {
                const int end = h.indexOf('\0');
                e.name = QString::fromUtf8(h.left(qMin(end < 0 ? 100 : end, 100)));
            }
            pax_name.clear();
            e.size = size;
            e.offset = data_at;
            out.push_back(std::move(e));
        }
        else if (type != '5') {
            return QStringLiteral("an entry that isn't a plain file");
        }
        if (!file.seek(next))
            return QStringLiteral("the file ends too early");
    }
}

// -- what a backup holds ----------------------------------------------------------------

// a path inside the Pegasus X folder that a backup may hold: one of the
// files, or inside one of the folders; no "..", no absolute path
bool allowed_rel(const QString& rel)
{
    if (rel.isEmpty() || rel.startsWith(QLatin1Char('/')) || rel.contains(QLatin1Char('\\')) || rel.contains(QLatin1Char(':')))
        return false;
    for (const QString& part : rel.split(QLatin1Char('/'))) {
        if (part.isEmpty() || part == QLatin1String(".") || part == QLatin1String(".."))
            return false;
    }
    if (backup::backup_files().contains(rel))
        return true;
    for (const QString& dir : backup::backup_dirs() + QStringList { backup::art_dir() }) {
        if (rel.startsWith(dir % QLatin1Char('/')))
            return !backup_skip().contains(rel);
    }
    return false;
}

struct Source {
    QString rel;
    qint64 size = 0;
};

std::vector<Source> collect(const QString& data, bool art)
{
    std::vector<Source> out;
    const QDir base(data);
    for (const QString& rel : backup::backup_files()) {
        const QFileInfo fi(base.filePath(rel));
        if (fi.isFile() && !fi.isSymLink())
            out.push_back({ rel, fi.size() });
    }
    QStringList dirs = backup::backup_dirs();
    if (art)
        dirs.append(backup::art_dir());
    for (const QString& dir : dirs) {
        const QFileInfo top(base.filePath(dir));
        if (!top.isDir() || top.isSymLink())
            continue;
        // (links aren't followed nor kept)
        QDirIterator it(top.absoluteFilePath(), QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString rel = base.relativeFilePath(it.filePath());
            if (allowed_rel(rel))
                out.push_back({ rel, it.fileInfo().size() });
        }
    }
    std::sort(out.begin(), out.end(), [](const Source& a, const Source& b) { return a.rel < b.rel; });
    return out;
}

qint64 dir_bytes(const QString& path)
{
    const QFileInfo fi(path);
    if (fi.isSymLink() || !fi.exists())
        return 0;
    if (!fi.isDir())
        return fi.size();
    qint64 out = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        out += it.fileInfo().size();
    }
    return out;
}

// deletes a file, a link (only the link) or a folder and what's in it, never
// following a link out of it
bool remove_tree(const QString& path)
{
    const QFileInfo fi(path);
    if (fi.isSymLink() || fi.isFile())
        return QFile::remove(path);
    if (!fi.isDir())
        return !fi.exists();
    bool ok = true;
    const QFileInfoList entries = QDir(path).entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::NoSort);
    for (const QFileInfo& e : entries)
        ok = remove_tree(e.filePath()) && ok;
    return QDir().rmdir(path) && ok;
}

QString clean(const QString& path)
{
    return QDir::cleanPath(path);
}

QString canonical_or_clean(const QString& path)
{
    const QString c = QFileInfo(path).canonicalFilePath();
    return c.isEmpty() ? clean(path) : c;
}

// a folder no backup or wipe may work in: a storage root, the home folder...
bool is_forbidden_base(const backup::Places& places, const QString& base)
{
    if (base.isEmpty())
        return true;
    const QString b = clean(base);
    const QString bc = canonical_or_clean(base);
    if (b == QLatin1String("/") || bc == QLatin1String("/") || QDir(b).isRoot())
        return true;
    for (const QString& never : places.never) {
        if (never.isEmpty())
            continue;
        if (b == clean(never) || bc == canonical_or_clean(never))
            return true;
    }
    return false;
}

QString stamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd-HHmm"));
}
} // namespace


namespace backup {

Places Places::current()
{
    Places p;
    p.data = paths::writableConfigDir();
    p.priv = paths::privateConfigDir();
    p.app_data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    p.cache = paths::writableCacheDir();
    p.never = QStringList { QDir::rootPath(), QDir::homePath() };
#if defined(Q_OS_ANDROID) || defined(PEGASUSX_TESTENV)
    p.never += ::android::storage_paths();
#endif
#ifdef Q_OS_ANDROID
    p.never += QStringList { QStringLiteral("/storage"), QStringLiteral("/storage/emulated"),
                             QStringLiteral("/sdcard"), QStringLiteral("/data"), QStringLiteral("/mnt") };
#endif
    return p;
}

const QStringList& backup_files()
{
    static const QStringList list {
        QStringLiteral("pegasusx.ini"),                // Pegasus X's settings (no passwords, keys or tokens)
        QStringLiteral("settings.txt"),                // Pegasus' settings: theme, language, controls
        QStringLiteral("game_dirs.txt"),
        QStringLiteral("sdl_controllers.txt"),         // gamepad layouts
        QStringLiteral("favorites.txt"),
        QStringLiteral("stats.db"),                    // play time, last played
        QStringLiteral("pegasusx-hidden.json"),
        QStringLiteral("pegasusx-android-kinds.json"), // Android sections
        QStringLiteral("pegasusx-android-apps.json"),  // Android apps to import
        QStringLiteral("pegasusx-emulators.json"),     // your systems' emulators
        QStringLiteral("pegasusx-rom-folders.txt"),
        QStringLiteral("pegasusx-themes/links.json"),  // where your themes came from
    };
    return list;
}

const QStringList& backup_dirs()
{
    static const QStringList list {
        QStringLiteral("gamedirectory"),   // each system's metadata
        QStringLiteral("metadata"),        // edits, imports, Needs your check, RetroAchievements, now playing
        QStringLiteral("metafiles"),
        QStringLiteral("imported"),        // copies of ES-DE's gamelists
        QStringLiteral("theme_settings"),  // what themes saved through Pegasus
    };
    return list;
}

const QString& art_dir()
{
    static const QString dir = QStringLiteral("downloaded_media");
    return dir;
}

QString default_folder()
{
#if defined(Q_OS_ANDROID) || defined(PEGASUSX_TESTENV)
    return clean(::android::primary_storage_path() % QStringLiteral("/Download"));
#else
    const QString dl = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return dl.isEmpty() ? QDir::homePath() : dl;
#endif
}

QString file_name(int build)
{
    return QStringLiteral("PegasusX-backup-%1-build%2.tar").arg(stamp()).arg(build);
}

qint64 art_bytes(const Places& places)
{
    return dir_bytes(QDir(places.data).filePath(art_dir()));
}

QString make(const Places& places, const QString& path, bool art, int build, const Progress& progress)
{
    if (places.data.isEmpty() || !QFileInfo(places.data).isDir())
        return QStringLiteral("The Pegasus X folder isn't there");
    const std::vector<Source> sources = collect(places.data, art);
    qint64 total = 0;
    for (const Source& s : sources)
        total += s.size;

    QDir().mkpath(QFileInfo(path).absolutePath());
    const QString part = path % QStringLiteral(".part");
    QFile file(part);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QStringLiteral("Can't write in %1").arg(QFileInfo(path).absolutePath());
    TarWriter tar(file);
    const auto fail = [&file, &part](const QString& why) {
        file.close();
        QFile::remove(part);
        return why;
    };

    const QByteArray manifest = QJsonDocument(QJsonObject {
        { QStringLiteral("format"), FORMAT },
        { QStringLiteral("app"), QStringLiteral("Pegasus X") },
        { QStringLiteral("build"), build },
        { QStringLiteral("date"), QDateTime::currentDateTime().toString(Qt::ISODate) },
        { QStringLiteral("art"), art },
        { QStringLiteral("files"), static_cast<int>(sources.size()) },
        { QStringLiteral("bytes"), static_cast<double>(total) },
    }).toJson(QJsonDocument::Indented);
    if (!tar.entry(MANIFEST, manifest.size()) || !tar.write(manifest) || !tar.pad(manifest.size()))
        return fail(QStringLiteral("Couldn't write the backup (is the storage full?)"));

    QJsonObject listed;
    qint64 done = 0;
    int last_percent = -1;
    QByteArray buffer;
    for (const Source& s : sources) {
        if (progress.cancel && *progress.cancel)
            return fail(QStringLiteral("Stopped"));
        const QString from = QDir(places.data).filePath(s.rel);
        quint32 crc = 0;
        qint64 size = 0;
        if (s.rel == QLatin1String("pegasusx.ini")) {
            QFile in(from);
            if (!in.open(QIODevice::ReadOnly))
                return fail(QStringLiteral("Couldn't read %1").arg(s.rel));
            const QByteArray stripped = strip_secrets(in.readAll());
            size = stripped.size();
            crc = crc_update(0, stripped.constData(), size);
            if (!tar.entry(DATA_PREFIX % s.rel, size) || !tar.write(stripped) || !tar.pad(size))
                return fail(QStringLiteral("Couldn't write the backup (is the storage full?)"));
        }
        else {
            QFile in(from);
            if (!in.open(QIODevice::ReadOnly))
                return fail(QStringLiteral("Couldn't read %1").arg(s.rel));
            size = in.size();
            if (!tar.entry(DATA_PREFIX % s.rel, size))
                return fail(QStringLiteral("Couldn't write the backup (is the storage full?)"));
            qint64 left = size;
            while (left > 0) {
                if (progress.cancel && *progress.cancel)
                    return fail(QStringLiteral("Stopped"));
                buffer = in.read(qMin(CHUNK, left));
                if (buffer.isEmpty())
                    return fail(QStringLiteral("Couldn't read %1").arg(s.rel));
                crc = crc_update(crc, buffer.constData(), buffer.size());
                if (!tar.write(buffer))
                    return fail(QStringLiteral("Couldn't write the backup (is the storage full?)"));
                left -= buffer.size();
                done += buffer.size();
                const int percent = total > 0 ? static_cast<int>(done * 100 / total) : 100;
                if (progress.percent && percent != last_percent) {
                    last_percent = percent;
                    progress.percent(qMin(percent, 99));
                }
            }
            if (!tar.pad(size))
                return fail(QStringLiteral("Couldn't write the backup (is the storage full?)"));
        }
        listed.insert(s.rel, QJsonObject {
            { QStringLiteral("size"), static_cast<double>(size) },
            { QStringLiteral("crc"), static_cast<double>(crc) },
        });
    }

    const QByteArray list = QJsonDocument(QJsonObject { { QStringLiteral("files"), listed } }).toJson(QJsonDocument::Compact);
    if (!tar.entry(FILE_LIST, list.size()) || !tar.write(list) || !tar.pad(list.size()) || !tar.finish() || !file.flush())
        return fail(QStringLiteral("Couldn't write the backup (is the storage full?)"));
    file.close();
    QFile::remove(path);
    if (!QFile::rename(part, path)) {
        QFile::remove(part);
        return QStringLiteral("Couldn't name the backup %1").arg(QFileInfo(path).fileName());
    }
    if (progress.percent)
        progress.percent(100);
    Log::info(LOG_TAG, LOGMSG("Saved `%1`: %2 files%3").arg(path).arg(sources.size())
        .arg(art ? QStringLiteral(", with the scraped art") : QString()));
    return {};
}

namespace {
struct Checked {
    Info info;
    std::vector<TarEntry> entries;  ///< the data files (in data/)
    QJsonObject files;              ///< rel -> { size, crc }
};

Checked check_structure(QFile& file)
{
    Checked out;
    std::vector<TarEntry> all;
    const QString scanned = scan_tar(file, all);
    if (!scanned.isEmpty()) {
        out.info.error = QStringLiteral("Not a Pegasus X backup, or damaged (%1).").arg(scanned);
        return out;
    }
    if (all.size() < 2 || all.front().name != MANIFEST || all.back().name != FILE_LIST) {
        out.info.error = QStringLiteral("Not a Pegasus X backup.");
        return out;
    }
    const auto read_json = [&file](const TarEntry& e) {
        file.seek(e.offset);
        return QJsonDocument::fromJson(file.read(qMin<qint64>(e.size, 64 << 20))).object();
    };
    const QJsonObject manifest = read_json(all.front());
    if (manifest.value(QStringLiteral("app")).toString() != QLatin1String("Pegasus X")) {
        out.info.error = QStringLiteral("Not a Pegasus X backup.");
        return out;
    }
    if (manifest.value(QStringLiteral("format")).toInt() != FORMAT) {
        out.info.error = QStringLiteral("This backup was made by a newer Pegasus X: update first.");
        return out;
    }
    out.files = read_json(all.back()).value(QStringLiteral("files")).toObject();
    for (size_t i = 1; i + 1 < all.size(); i++) {
        const TarEntry& e = all[i];
        const QString rel = e.name.startsWith(DATA_PREFIX) ? e.name.mid(DATA_PREFIX.size()) : QString();
        if (!allowed_rel(rel)) {
            out.info.error = QStringLiteral("The backup has a file that isn't Pegasus X data (%1): not restored.").arg(e.name);
            return out;
        }
        const QJsonObject listed = out.files.value(rel).toObject();
        if (listed.isEmpty() || static_cast<qint64>(listed.value(QStringLiteral("size")).toDouble()) != e.size) {
            out.info.error = QStringLiteral("The backup is damaged (%1).").arg(rel);
            return out;
        }
        TarEntry kept = e;
        kept.name = rel;
        out.entries.push_back(std::move(kept));
        out.info.bytes += e.size;
    }
    if (static_cast<int>(out.entries.size()) != out.files.size()) {
        out.info.error = QStringLiteral("The backup is damaged (files missing).");
        return out;
    }
    out.info.date = manifest.value(QStringLiteral("date")).toString();
    out.info.build = manifest.value(QStringLiteral("build")).toInt();
    out.info.art = manifest.value(QStringLiteral("art")).toBool();
    out.info.files = static_cast<int>(out.entries.size());
    return out;
}
} // namespace

Info read_info(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        Info info;
        info.error = QStringLiteral("Can't open %1.").arg(QFileInfo(path).fileName());
        return info;
    }
    return check_structure(file).info;
}

RestoreResult restore(const Places& places, const QString& path, int build, const Progress& progress)
{
    RestoreResult out;
    if (places.data.isEmpty() || !QFileInfo(places.data).isDir() || is_forbidden_base(places, places.data)) {
        out.error = QStringLiteral("The Pegasus X folder can't be used.");
        return out;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        out.error = QStringLiteral("Can't open %1.").arg(QFileInfo(path).fileName());
        return out;
    }
    const Checked checked = check_structure(file);
    if (!checked.info.error.isEmpty()) {
        out.error = checked.info.error;
        return out;
    }
    const qint64 total = qMax<qint64>(1, checked.info.bytes);

    // 1. every file's checksum, before anything changes
    qint64 done = 0;
    for (const TarEntry& e : checked.entries) {
        if (progress.cancel && *progress.cancel) {
            out.error = QStringLiteral("Stopped: nothing was changed.");
            return out;
        }
        file.seek(e.offset);
        quint32 crc = 0;
        qint64 left = e.size;
        while (left > 0) {
            const QByteArray buf = file.read(qMin(CHUNK, left));
            if (buf.isEmpty())
                break;
            crc = crc_update(crc, buf.constData(), buf.size());
            left -= buf.size();
            done += buf.size();
            if (progress.percent)
                progress.percent(static_cast<int>(done * 40 / total));
        }
        const quint32 want = static_cast<quint32>(checked.files.value(e.name).toObject().value(QStringLiteral("crc")).toDouble());
        if (left != 0 || crc != want) {
            out.error = QStringLiteral("The backup is damaged (%1): nothing was changed.").arg(e.name);
            return out;
        }
    }

    // 2. the data as it is now, so this can be undone
    out.saved_first = QDir(places.data).filePath(QStringLiteral("backup/before-restore-%1.tar").arg(stamp()));
    const QString saved = make(places, out.saved_first, false, build, {});
    if (!saved.isEmpty()) {
        out.error = QStringLiteral("Couldn't save your data as it is first (%1): nothing was changed.").arg(saved);
        out.saved_first.clear();
        return out;
    }
    if (progress.percent)
        progress.percent(50);

    // the logins on this device, kept
    QString ss_user, ss_password, ra_user, ra_key;
    {
        const QSettings ini(QDir(places.data).filePath(QStringLiteral("pegasusx.ini")), QSettings::IniFormat);
        ss_user = ini.value(QStringLiteral("scraper/ssUser")).toString();
        ss_password = ini.value(QStringLiteral("scraper/ssPassword")).toString();
        ra_user = ini.value(QStringLiteral("retroachievements/user")).toString();
        ra_key = ini.value(QStringLiteral("retroachievements/key")).toString();
    }

    // 3. the data the backup has in place of what's there (the art: added)
    const QDir base(places.data);
    for (const QString& rel : backup_files()) {
        const QString target = base.filePath(rel);
        if (QFileInfo(target).exists() || QFileInfo(target).isSymLink())
            remove_tree(target);
    }
    for (const QString& rel : backup_dirs()) {
        const QString target = base.filePath(rel);
        if (QFileInfo(target).exists() || QFileInfo(target).isSymLink())
            remove_tree(target);
    }
    const QString data_canonical = canonical_or_clean(places.data);
    QStringList failed;
    for (const TarEntry& e : checked.entries) {
        const QString target = base.filePath(e.name);
        const QString parent = QFileInfo(target).absolutePath();
        QDir().mkpath(parent);
        // never written through a link out of the Pegasus X folder
        const QString parent_canonical = canonical_or_clean(parent);
        if (!parent_canonical.startsWith(data_canonical % QLatin1Char('/')) && parent_canonical != data_canonical) {
            failed.append(e.name);
            continue;
        }
        if (QFileInfo(target).isSymLink())
            QFile::remove(target);
        QFile to(target);
        if (!to.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            failed.append(e.name);
            continue;
        }
        file.seek(e.offset);
        qint64 left = e.size;
        bool ok = true;
        while (left > 0 && ok) {
            const QByteArray buf = file.read(qMin(CHUNK, left));
            ok = !buf.isEmpty() && to.write(buf) == buf.size();
            left -= buf.size();
            done += buf.size();
            if (progress.percent)
                progress.percent(50 + static_cast<int>((done - total) * 50 / total));
        }
        if (!ok)
            failed.append(e.name);
    }

    // 4. logins: this device's; the backup's names without theirs: sign in again
    {
        QSettings ini(QDir(places.data).filePath(QStringLiteral("pegasusx.ini")), QSettings::IniFormat);
        if (!ss_password.isEmpty()) {
            ini.setValue(QStringLiteral("scraper/ssUser"), ss_user);
            ini.setValue(QStringLiteral("scraper/ssPassword"), ss_password);
        }
        else if (!ini.value(QStringLiteral("scraper/ssUser")).toString().isEmpty()) {
            out.sign_in.append(QStringLiteral("ScreenScraper"));
        }
        if (!ra_key.isEmpty()) {
            ini.setValue(QStringLiteral("retroachievements/user"), ra_user);
            ini.setValue(QStringLiteral("retroachievements/key"), ra_key);
        }
        else if (!ini.value(QStringLiteral("retroachievements/user")).toString().isEmpty()) {
            out.sign_in.append(QStringLiteral("RetroAchievements"));
        }
        ini.sync();
    }
    if (progress.percent)
        progress.percent(100);
    if (!failed.isEmpty()) {
        out.error = QStringLiteral("%1 files couldn't be written (first: %2). Your data as it was is in %3.")
            .arg(failed.size()).arg(failed.first(), out.saved_first);
        Log::warning(LOG_TAG, out.error);
    }
    Log::info(LOG_TAG, LOGMSG("Restored `%1` (%2 files); the data as it was: `%3`")
        .arg(path).arg(checked.entries.size() - failed.size()).arg(out.saved_first));
    return out;
}

bool is_secret_key(const QString& key)
{
    const QString lower = key.toLower();
    const QString last = lower.mid(lower.lastIndexOf(QLatin1Char('/')) + 1);
    return last.contains(QLatin1String("password"))
        || last.contains(QLatin1String("token"))
        || last.contains(QLatin1String("secret"))
        || last.contains(QLatin1String("apikey"))
        || last == QLatin1String("key")
        || last.endsWith(QLatin1String("_key"));
}

QByteArray strip_secrets(const QByteArray& ini)
{
    QByteArray out;
    QString section;
    for (const QByteArray& line : ini.split('\n')) {
        const QByteArray t = line.trimmed();
        if (t.startsWith('[') && t.endsWith(']')) {
            section = QString::fromUtf8(t.mid(1, t.size() - 2));
        }
        else if (!t.startsWith(';') && !t.startsWith('#') && t.contains('=')) {
            const QString key = QString::fromUtf8(t.left(t.indexOf('='))).trimmed();
            const QString full = (section.isEmpty() || section == QLatin1String("General")) ? key : section % QLatin1Char('/') % key;
            if (is_secret_key(full))
                continue;
        }
        out += line;
        out += '\n';
    }
    if (out.endsWith("\n\n"))
        out.chop(1);
    return out;
}

bool safe_target(const Places& places, const QString& base, const QString& path)
{
    if (base.isEmpty() || path.isEmpty() || is_forbidden_base(places, base))
        return false;
    const QFileInfo base_info(base);
    if (!base_info.isDir())
        return false;
    const QString b = clean(base);
    const QString p = clean(path);
    if (!p.startsWith(b % QLatin1Char('/')))
        return false;
    const QString name = p.mid(b.size() + 1);
    if (name.isEmpty() || name.contains(QLatin1Char('/')) || name == QLatin1String(".") || name == QLatin1String(".."))
        return false;
    // the folder it's in, as it really is (links resolved), must be the base
    const QString parent_real = canonical_or_clean(QFileInfo(p).absolutePath());
    return parent_real == canonical_or_clean(b);
}

namespace {
struct Named {
    const QString* base;
    QString name;
    QString what;
};
std::vector<Named> wipe_names(const Places& places)
{
    std::vector<Named> out;
    const auto add = [&out](const QString& base, const QString& name, const QString& what) {
        out.push_back({ &base, name, what });
    };
    // the Pegasus X folder
    add(places.data, QStringLiteral("pegasusx.ini"), QStringLiteral("Pegasus X settings and logins"));
    add(places.data, QStringLiteral("settings.txt"), QStringLiteral("Settings (theme, language, controls)"));
    add(places.data, QStringLiteral("game_dirs.txt"), QStringLiteral("Game folders list"));
    add(places.data, QStringLiteral("sdl_controllers.txt"), QStringLiteral("Gamepad layouts"));
    add(places.data, QStringLiteral("favorites.txt"), QStringLiteral("Favorites"));
    add(places.data, QStringLiteral("stats.db"), QStringLiteral("Play time"));
    add(places.data, QStringLiteral("pegasusx-hidden.json"), QStringLiteral("Hidden games and collections"));
    add(places.data, QStringLiteral("pegasusx-android-kinds.json"), QStringLiteral("Android sections"));
    add(places.data, QStringLiteral("pegasusx-android-apps.json"), QStringLiteral("Android apps to import"));
    add(places.data, QStringLiteral("pegasusx-emulators.json"), QStringLiteral("Emulator choices"));
    add(places.data, QStringLiteral("pegasusx-rom-folders.txt"), QStringLiteral("ROM folder list (not the ROMs)"));
    add(places.data, QStringLiteral("pegasusx-themes"), QStringLiteral("Theme store records (not the themes)"));
    add(places.data, QStringLiteral("gamedirectory"), QStringLiteral("Game directory (each system's metadata)"));
    add(places.data, QStringLiteral("metadata"), QStringLiteral("Edits, imports, Needs your check, RetroAchievements matches"));
    add(places.data, QStringLiteral("metafiles"), QStringLiteral("Older edits"));
    add(places.data, QStringLiteral("imported"), QStringLiteral("Copies of ES-DE gamelists (not ES-DE's own)"));
    add(places.data, QStringLiteral("theme_settings"), QStringLiteral("Theme options saved through Pegasus"));
    add(places.data, QStringLiteral("downloaded_media"), QStringLiteral("Scraped art"));
    add(places.data, QStringLiteral("backup"), QStringLiteral("Backups made before changes"));
    add(places.data, QStringLiteral("launchbox"), QStringLiteral("LaunchboxDB download"));
    add(places.data, QStringLiteral("esde-lists"), QStringLiteral("Downloaded emulator list"));
    // app storage: setup, the GitHub token
    add(places.priv, QStringLiteral("pegasusx-wizard"), QStringLiteral("Setup progress"));
    add(places.priv, QStringLiteral("pegasusx-root-asked"), QStringLiteral("Setup done marker"));
    add(places.app_data, QStringLiteral("pegasusx-private.ini"), QStringLiteral("GitHub token"));
    // caches
    add(places.cache, QStringLiteral("gameindex-v1.json"), QStringLiteral("Game list cache"));
    add(places.cache, QStringLiteral("netcache"), QStringLiteral("Download cache"));
    add(places.cache, QStringLiteral("scraper-pictures"), QStringLiteral("Scraper downloads"));
    add(places.cache, QStringLiteral("import-conflicts.txt"), QStringLiteral("Import notes"));
    add(places.cache, QStringLiteral("update"), QStringLiteral("Downloaded updates"));
    add(places.cache, QStringLiteral("androidapps"), QStringLiteral("Android app details cache"));
    add(places.cache, QStringLiteral("steam"), QStringLiteral("Steam details cache"));
    return out;
}
} // namespace

WipePlan wipe_plan(const Places& places)
{
    WipePlan plan;
    QStringList seen;
    for (const Named& n : wipe_names(places)) {
        if (n.base->isEmpty())
            continue;
        const QString path = clean(*n.base % QLatin1Char('/') % n.name);
        const QFileInfo fi(path);
        if (seen.contains(path) || (!fi.exists() && !fi.isSymLink()))
            continue;
        seen.append(path);
        plan.remove.push_back({ path, n.what, dir_bytes(path) });
    }
    plan.keep = QStringList {
        QStringLiteral("Your themes (themes/ in the Pegasus X folder)"),
        QStringLiteral("Which Pegasus X folder is used"),
        QStringLiteral("Your ROM folders and everything in them (ES-DE gamelists and media too)"),
        QStringLiteral("The ES-DE folder"),
        QStringLiteral("Anything else in the Pegasus X folder, and everything outside it"),
        QStringLiteral("This session's log (lastrun.log)"),
    };
    return plan;
}

QString wipe(const Places& places, QStringList* removed)
{
    if (is_forbidden_base(places, places.data))
        return QStringLiteral("The Pegasus X folder is a storage root: nothing was deleted.");
    QStringList failed;
    for (const Named& n : wipe_names(places)) {
        if (n.base->isEmpty())
            continue;
        const QString path = clean(*n.base % QLatin1Char('/') % n.name);
        const QFileInfo fi(path);
        if (!fi.exists() && !fi.isSymLink())
            continue;
        if (!safe_target(places, *n.base, path)) {
            Log::warning(LOG_TAG, LOGMSG("Start over: `%1` left alone (not inside its folder)").arg(path));
            failed.append(path);
            continue;
        }
        if (remove_tree(path)) {
            if (removed)
                removed->append(path);
            Log::info(LOG_TAG, LOGMSG("Start over: removed `%1`").arg(path));
        }
        else {
            failed.append(path);
        }
    }
    return failed.isEmpty() ? QString() : QStringLiteral("Couldn't delete: %1").arg(failed.join(QStringLiteral(", ")));
}

} // namespace backup
