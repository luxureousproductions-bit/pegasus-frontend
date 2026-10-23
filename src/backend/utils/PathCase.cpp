// Pegasus Frontend
// Copyright (C) 2017-2018  Mátyás Mustoha
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


#include "PathCase.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>


namespace {
#if defined(Q_OS_ANDROID) || defined(Q_OS_WINDOWS) || defined(Q_OS_MACOS)
constexpr bool CASE_SENSITIVE_FS = false;
#else
constexpr bool CASE_SENSITIVE_FS = true;
#endif

QMutex& cache_mutex()
{
    static QMutex mutex;
    return mutex;
}
// folder -> its entries, lower case -> as spelled
QHash<QString, QHash<QString, QString>>& listings()
{
    static QHash<QString, QHash<QString, QString>> cache;
    return cache;
}

const QHash<QString, QString>& listing_of(const QString& dir)
{
    auto& cache = listings();
    auto it = cache.find(dir);
    if (it != cache.end())
        return it.value();
    QHash<QString, QString> entries;
    const QStringList names = QDir(dir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QString& name : names)
        entries.insert(name.toLower(), name);
    return cache.insert(dir, entries).value();
}
} // namespace


namespace pathcase {
QString resolve(const QString& path)
{
    if (path.isEmpty())
        return {};
    if (QFileInfo::exists(path))
        return path;
    if (!CASE_SENSITIVE_FS || !path.startsWith(QLatin1Char('/')))
        return {};

    // each part as spelled on disk, from the root down
    const QStringList parts = QDir::cleanPath(path).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString current;
    QMutexLocker lock(&cache_mutex());
    for (const QString& part : parts) {
        const QString parent = current.isEmpty() ? QStringLiteral("/") : current;
        const QString exact = current + QLatin1Char('/') + part;
        if (QFileInfo::exists(exact)) {
            current = exact;
            continue;
        }
        const QHash<QString, QString>& entries = listing_of(parent);
        const auto found = entries.constFind(part.toLower());
        if (found == entries.cend())
            return {};
        current += QLatin1Char('/') + found.value();
    }
    return current;
}

bool file_exists(const QString& path)
{
    const QString real = resolve(path);
    return !real.isEmpty() && QFileInfo(real).isFile();
}

void clear_cache()
{
    QMutexLocker lock(&cache_mutex());
    listings().clear();
}
} // namespace pathcase
