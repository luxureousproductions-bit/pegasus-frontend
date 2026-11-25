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


#include "Recovery.h"

#include "Paths.h"
#include "SafeFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTextCodec>


namespace {
recovery::Report s_last;

QByteArray read_all(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString file_name(const QString& path)
{
    return QFileInfo(path).fileName();
}

// the lines that say something (not empty, not a comment)
QStringList meaningful_lines(const QString& text, const QStringList& comment_marks)
{
    QStringList out;
    for (const QString& raw : text.split(QLatin1Char('\n'))) {
        const QString line = QString(raw).remove(QLatin1Char('\r'));
        if (line.trimmed().isEmpty())
            continue;
        bool comment = false;
        for (const QString& mark : comment_marks)
            comment = comment || line.trimmed().startsWith(mark);
        if (!comment)
            out.append(line);
    }
    return out;
}

// more than half of the lines aren't what the file holds, or none is
QString mostly_junk(const QStringList& lines, const QRegularExpression& valid, const QString& what)
{
    if (lines.isEmpty())
        return {};
    int good = 0;
    for (const QString& line : lines)
        good += valid.match(line).hasMatch() ? 1 : 0;
    if (good == 0 || good * 2 < lines.size())
        return QStringLiteral("%1 of its %2 lines aren't %3").arg(lines.size() - good).arg(lines.size()).arg(what);
    return {};
}

QString sqlite_check(const QString& path)
{
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return QStringLiteral("can't be read");
        if (file.read(16) != QByteArray("SQLite format 3\0", 16))
            return QStringLiteral("not an SQLite database");
    }
    static int counter = 0;
    const QString connection = QStringLiteral("pegasusx-recovery-%1").arg(++counter);
    QString reason;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(path);
        if (!db.open()) {
            reason = QStringLiteral("can't be opened (%1)").arg(db.lastError().text());
        }
        else {
            QSqlQuery query(db);
            if (!query.exec(QStringLiteral("PRAGMA integrity_check")) || !query.next())
                reason = QStringLiteral("failed SQLite's integrity check (%1)").arg(query.lastError().text());
            else if (query.value(0).toString() != QLatin1String("ok"))
                reason = QStringLiteral("failed SQLite's integrity check (%1)").arg(query.value(0).toString().left(120));
            query.finish();
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return reason;
}

// `path` moved into `dir` as "<date>_<time> <name>"; its new path, or empty
// if it couldn't be moved (it's left as it is then)
QString move_to_damaged(const QString& path, const QString& dir, const QDateTime& now)
{
    QDir().mkpath(dir);
    const QString stamp = now.toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    QString target = dir + QLatin1Char('/') + stamp + QLatin1Char(' ') + file_name(path);
    for (int n = 2; QFileInfo::exists(target); n++)
        target = dir + QLatin1Char('/') + stamp + QStringLiteral(" (%1) ").arg(n) + file_name(path);
    if (QFile::rename(path, target))
        return target;
    // (another storage: a copy, then the file goes; the copy checked first)
    if (QFile::copy(path, target) && read_all(target) == read_all(path) && QFile::remove(path))
        return target;
    QFile::remove(target);
    return {};
}

bool good_bytes_differ(const QString& path, const QString& good)
{
    return !QFileInfo::exists(good) || read_all(good) != read_all(path);
}
} // namespace


namespace recovery {

QString good_path(const QString& path)
{
    return path + QStringLiteral(".pxgood");
}

QString check(Kind kind, const QByteArray& bytes, const QString& path, const QByteArray& good)
{
    if (bytes.isEmpty()) {
        // emptied on purpose (no favorites, no game folders), or a list that
        // had folders and has none any more: damage only then
        switch (kind) {
            case Kind::Lines:
                return {};
            case Kind::RomFolders:
                return QString::fromUtf8(good).contains(QLatin1Char('\t'))
                    ? QStringLiteral("empty, its last good copy has ROM folders") : QString();
            default:
                return good.isEmpty() ? QString() : QStringLiteral("empty, its last good copy isn't");
        }
    }
    if (kind == Kind::Sqlite)
        return sqlite_check(path);

    if (bytes.contains('\0'))
        return QStringLiteral("has zero bytes in it (a write cut short)");
    QTextCodec::ConverterState state;
    const QString text = QTextCodec::codecForName("UTF-8")->toUnicode(bytes.constData(), bytes.size(), &state);
    if (state.invalidChars > 0)
        return QStringLiteral("not readable text");
    for (const QChar c : text) {
        if (c.unicode() < 0x20 && c != QLatin1Char('\n') && c != QLatin1Char('\r') && c != QLatin1Char('\t'))
            return QStringLiteral("has unreadable characters in it");
    }

    switch (kind) {
        case Kind::JsonObject:
        case Kind::JsonArray: {
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(bytes, &error);
            if (error.error != QJsonParseError::NoError)
                return QStringLiteral("not JSON (%1 at %2)").arg(error.errorString()).arg(error.offset);
            if (kind == Kind::JsonObject ? !doc.isObject() : !doc.isArray())
                return QStringLiteral("JSON, but not the %1 it holds").arg(kind == Kind::JsonObject ? QStringLiteral("object") : QStringLiteral("list"));
            return {};
        }
        case Kind::Settings: {
            static const QRegularExpression rx(QStringLiteral("^(\\s+\\S|[A-Za-z0-9_.\\-]+\\s*:)"));
            return mostly_junk(meaningful_lines(text, { QStringLiteral("#") }), rx, QStringLiteral("settings"));
        }
        case Kind::Ini: {
            static const QRegularExpression rx(QStringLiteral("^\\s*(\\[[^\\]]+\\]\\s*$|[^=\\[\\s][^=]*=)"));
            return mostly_junk(meaningful_lines(text, { QStringLiteral(";"), QStringLiteral("#") }), rx, QStringLiteral("settings"));
        }
        case Kind::SdlMappings: {
            static const QRegularExpression rx(QStringLiteral("^[^,\\s]+,[^,]*,.*:"));
            return mostly_junk(meaningful_lines(text, { QStringLiteral("#") }), rx, QStringLiteral("gamepad mappings"));
        }
        case Kind::RomFolders: {
            static const QRegularExpression rx(QStringLiteral("^[^\\t]+\\t\\s*\\S"));
            const QStringList lines = meaningful_lines(text, {});
            if (lines.isEmpty() && QString::fromUtf8(good).contains(QLatin1Char('\t')))
                return QStringLiteral("no ROM folder in it, its last good copy has some");
            return mostly_junk(lines, rx, QStringLiteral("\"folder<TAB>system\" lines"));
        }
        case Kind::Root: {
            const QStringList lines = meaningful_lines(text, {});
            static const QRegularExpression rx(QStringLiteral("^(/|[A-Za-z]:[/\\\\])"));
            if (lines.size() != 1 || !rx.match(lines.first().trimmed()).hasMatch())
                return QStringLiteral("not one folder path");
            return {};
        }
        case Kind::Lines:
        case Kind::Sqlite:
            return {};
    }
    return {};
}

void recover(const Spec& spec, Report& report, const QDateTime& now)
{
    const QString path = spec.dir + QLatin1Char('/') + spec.name;
    const QString pxsave = safefile::fallback_new_path(path);
    const QString pxold = safefile::fallback_old_path(path);
    const QString good = good_path(path);
    const auto say = [&](const QString& line) {
        report.log.append(QStringLiteral("Recovery: `%1`: %2").arg(spec.name, line));
    };
    // (a file, checked: for SQLite by its path)
    const auto check_file = [&spec](const QString& file, const QByteArray& good_bytes) {
        return check(spec.kind, read_all(file), file, good_bytes);
    };
    const bool has_good = QFileInfo::exists(good);
    const QByteArray good_bytes = has_good ? read_all(good) : QByteArray();
    const QString good_reason = has_good ? check_file(good, QByteArray()) : QStringLiteral("none");
    const bool good_ok = has_good && good_reason.isEmpty();

    // the file and the ones SQLite keeps beside it
    const auto with_siblings = [&spec](const QString& file) {
        QStringList out { file };
        if (spec.kind == Kind::Sqlite) {
            for (const char* suffix : { "-journal", "-wal", "-shm" }) {
                if (QFileInfo::exists(file + QLatin1String(suffix)))
                    out.append(file + QLatin1String(suffix));
            }
        }
        return out;
    };
    // its last good copy in its place, else nothing (its defaults)
    const auto from_good_copy = [&](Fixed& fixed) {
        if (good_ok && safefile::write(path, good_bytes)) {
            fixed.how = QStringLiteral("good copy");
            say(QStringLiteral("its last good copy `%1` put in its place").arg(file_name(good)));
            return;
        }
        fixed.how = QStringLiteral("defaults");
        if (has_good && !good_ok) {
            const QString moved = move_to_damaged(good, spec.damaged_dir, now);
            if (!moved.isEmpty()) {
                fixed.moved.append(moved);
                say(QStringLiteral("its last good copy is damaged too (%1): moved to `%2`").arg(good_reason, moved));
            }
        }
        say(has_good ? QStringLiteral("starts at its defaults (no last good copy that passes)")
                     : QStringLiteral("starts at its defaults (no last good copy yet)"));
    };

    if (!QFileInfo::exists(path)) {
        // missing: put back from an interrupted save, if it left a whole copy
        QStringList candidates;
        if (QFileInfo::exists(pxsave) && QFileInfo::exists(pxold))
            candidates = { pxsave, pxold };  // (.pxsave was written in full before the old file moved)
        else if (QFileInfo::exists(pxsave))
            candidates = { pxsave };
        else if (QFileInfo::exists(pxold))
            candidates = { pxold };
        if (candidates.isEmpty())
            return;  // a first start, or taken out on purpose: nothing to do

        Fixed fixed;
        fixed.name = spec.name;
        fixed.label = spec.label;
        fixed.path = path;
        fixed.reason = QStringLiteral("missing after an interrupted save");
        say(QStringLiteral("missing, an interrupted save left %1 beside it").arg(
            candidates.size() == 2 ? QStringLiteral("`%1` and `%2`").arg(file_name(pxsave), file_name(pxold))
                                   : QStringLiteral("`%1`").arg(file_name(candidates.first()))));
        for (const QString& candidate : candidates) {
            const QString reason = check_file(candidate, good_bytes);
            if (reason.isEmpty() && QFile::rename(candidate, path)) {
                fixed.how = QStringLiteral("put back");
                fixed.reason += QStringLiteral(" (put back from %1)").arg(file_name(candidate));
                say(QStringLiteral("put back from `%1`").arg(file_name(candidate)));
                break;
            }
            say(QStringLiteral("`%1` not used: %2").arg(file_name(candidate), reason.isEmpty() ? QStringLiteral("can't be renamed") : reason));
        }
        if (fixed.how.isEmpty()) {
            // none whole: each one kept in `damaged`, its last good copy, else its defaults
            for (const QString& candidate : candidates) {
                const QString moved = move_to_damaged(candidate, spec.damaged_dir, now);
                if (!moved.isEmpty()) {
                    fixed.moved.append(moved);
                    say(QStringLiteral("`%1` moved to `%2`").arg(file_name(candidate), moved));
                }
            }
            from_good_copy(fixed);
            report.fixed.push_back(fixed);
            return;
        }
        report.fixed.push_back(fixed);
        // (on to "fine" below: the other one goes, its good copy is made)
    }
    else {
        const QString reason = check_file(path, good_bytes);
        if (!reason.isEmpty()) {
            Fixed fixed;
            fixed.name = spec.name;
            fixed.label = spec.label;
            fixed.path = path;
            fixed.reason = reason;
            say(QStringLiteral("damaged: %1").arg(reason));
            bool all_moved = true;
            for (const QString& file : with_siblings(path)) {
                const QString moved = move_to_damaged(file, spec.damaged_dir, now);
                if (moved.isEmpty()) {
                    all_moved = false;
                    break;
                }
                fixed.moved.append(moved);
                say(QStringLiteral("moved to `%1`").arg(moved));
            }
            if (!all_moved) {
                // never written over: left as it is (and said so)
                fixed.how = QStringLiteral("left");
                say(QStringLiteral("couldn't be moved to `%1`: left as it is").arg(spec.damaged_dir));
                report.fixed.push_back(fixed);
                return;
            }
            from_good_copy(fixed);
            report.fixed.push_back(fixed);
            return;
        }
    }

    // fine: what an interrupted save left goes, and its last good copy is made again
    for (const QString& leftover : { pxsave, pxold }) {
        if (QFileInfo::exists(leftover) && QFile::remove(leftover))
            say(QStringLiteral("`%1` left by an interrupted save removed (the file is fine)").arg(file_name(leftover)));
    }
    if (good_bytes_differ(path, good)) {
        if (safefile::write(good, read_all(path)))
            report.log.append(QStringLiteral("Recovery: `%1`: last good copy made (`%2`)").arg(spec.name, file_name(good)));
    }
}

Report run_root()
{
    Report report;
    const QString dir = paths::privateConfigDir();
    recover({ dir, QStringLiteral("pegasusx-root.txt"), Kind::Root, QStringLiteral("Pegasus X folder setting"),
              dir + QStringLiteral("/damaged") }, report);
    for (const Fixed& f : report.fixed)
        s_last.fixed.push_back(f);
    s_last.log += report.log;
    return report;
}

Report run_all()
{
    Report report;
    const QString data = paths::writableConfigDir();
    const QString damaged = data + QStringLiteral("/damaged");
    const std::vector<Spec> specs {
        { data, QStringLiteral("settings.txt"), Kind::Settings, QStringLiteral("Settings (theme, language, controls)"), damaged },
        { data, QStringLiteral("pegasusx.ini"), Kind::Ini, QStringLiteral("Pegasus X settings"), damaged },
        { data, QStringLiteral("game_dirs.txt"), Kind::Lines, QStringLiteral("Game folders list"), damaged },
        { data, QStringLiteral("pegasusx-rom-folders.txt"), Kind::RomFolders, QStringLiteral("ROM folder list"), damaged },
        { data, QStringLiteral("favorites.txt"), Kind::Lines, QStringLiteral("Favorites"), damaged },
        { data, QStringLiteral("pegasusx-hidden.json"), Kind::JsonObject, QStringLiteral("Hidden games and collections"), damaged },
        { data, QStringLiteral("pegasusx-android-apps.json"), Kind::JsonObject, QStringLiteral("Android apps to import"), damaged },
        { data, QStringLiteral("pegasusx-android-kinds.json"), Kind::JsonObject, QStringLiteral("Android sections"), damaged },
        { data, QStringLiteral("stats.db"), Kind::Sqlite, QStringLiteral("Play time"), damaged },
        { data, QStringLiteral("pegasusx-emulators.json"), Kind::JsonArray, QStringLiteral("Emulator choices"), damaged },
        { data, QStringLiteral("sdl_controllers.txt"), Kind::SdlMappings, QStringLiteral("Gamepad layouts"), damaged },
    };
    for (const Spec& spec : specs)
        recover(spec, report);
    // (the GitHub token: its copies stay in the app's private folder)
    const QString own = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    recover({ own, QStringLiteral("pegasusx-private.ini"), Kind::Ini, QStringLiteral("GitHub token (private)"),
              own + QStringLiteral("/damaged") }, report);

    for (const Fixed& f : report.fixed)
        s_last.fixed.push_back(f);
    s_last.log += report.log;
    return report;
}

const Report& last()
{
    return s_last;
}

void saved_empty(const QString& path)
{
    if (QFileInfo::exists(good_path(path)) && !read_all(good_path(path)).isEmpty())
        safefile::write(good_path(path), QByteArray());
}

} // namespace recovery
