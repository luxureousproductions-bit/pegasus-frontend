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


#include "FolderCopy.h"

#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>


namespace folder_copy {

Size size_of(const QString& from, const QStringList& skip)
{
    Size out;
    QDirIterator it(from, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (skip.contains(path.mid(from.length() + 1).section(QLatin1Char('/'), 0, 0)))
            continue;
        out.files++;
        out.bytes += it.fileInfo().size();
    }
    return out;
}

bool copy_file(const QString& from, const QString& to, const std::function<void(qint64)>& step)
{
    QFile in(from);
    QFile out(to);
    if (!in.open(QIODevice::ReadOnly))
        return false;
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    bool ok = true;
    while (!in.atEnd()) {
        const QByteArray part = in.read(1 << 20);
        if (part.isEmpty())
            break;  // (an error is checked below)
        if (out.write(part) != part.size()) {
            ok = false;
            break;
        }
        if (step)
            step(part.size());
    }
    ok = ok && in.error() == QFileDevice::NoError && out.flush();
    out.close();
    if (!ok)
        out.remove();
    return ok;
}

int copy(const QString& from, const QString& to, const QStringList& skip, QString& error, const Progress& progress)
{
    int copied = 0;
    qint64 bytes = 0;
    QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString rel = path.mid(from.length() + 1);
        const QString top = rel.section(QLatin1Char('/'), 0, 0);
        if (skip.contains(top))
            continue;
        const QFileInfo info = it.fileInfo();
        const QString target = to + QLatin1Char('/') + rel;
        if (info.isDir()) {
            QDir().mkpath(target);
            continue;
        }
        QDir().mkpath(QFileInfo(target).absolutePath());
        if (QFileInfo::exists(target))
            QFile::remove(target);
        const bool ok = copy_file(path, target, [&bytes, &copied, &rel, &progress](qint64 n) {
            bytes += n;
            if (progress)
                progress(bytes, copied, rel);
        });
        if (!ok) {
            error = QStringLiteral("Couldn't copy %1").arg(rel);
            return -1;
        }
        copied++;
        if (progress)
            progress(bytes, copied, rel);
    }
    return copied;
}

} // namespace folder_copy
