// Pegasus Frontend
// Copyright (C) 2017-2019  Mátyás Mustoha
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


#include "Assets.h"
#include <QStringBuilder>
#include <QSet>
#include <QFileInfo>

#include <QUrl>


namespace model {

Assets::Assets(QObject* parent)
    : QObject(parent)
{}

const QStringList& Assets::get(AssetType key) const {
    static const QStringList empty;

    const auto it = m_asset_lists.find(key);
    if (it != m_asset_lists.cend())
        return it->second;

    return empty;
}

const QString& Assets::getFirst(AssetType key) const {
    static const QString empty;

    const QStringList& list = get(key);
    if (!list.isEmpty())
        return list.constFirst();

    return empty;
}

Assets& Assets::add_file(AssetType key, QString path)
{
    QString uri = QUrl::fromLocalFile(std::move(path)).toString();
    return add_uri(key, std::move(uri));
}

Assets& Assets::add_uri(AssetType key, QString url)
{
    QStringList& target = m_asset_lists[key];

    if (!url.isEmpty() && !target.contains(url))
        target.append(std::move(url));

    return *this;
}

void Assets::remove_duplicate_names()
{
    // only lists with more than one file are looked at, so this costs
    // next to nothing for most games
    for (auto& pair : m_asset_lists) {
        QStringList& list = pair.second;
        if (list.size() < 2)
            continue;
        QSet<QString> names;
        QSet<QString> sizes;  // "<bytes>.<ext>": the same file copied elsewhere
        QStringList kept;
        for (const QString& url : qAsConst(list)) {
            const QString name = url.mid(url.lastIndexOf(QLatin1Char('/')) + 1).toLower();
            if (!name.isEmpty() && names.contains(name))
                continue;

            QString local;
            if (url.startsWith(QLatin1String("file:")))
                local = QUrl(url).toLocalFile();
            else if (url.startsWith(QLatin1Char('/')))
                local = url;
            if (!local.isEmpty()) {
                const QFileInfo info(local);
                if (info.exists()) {
                    const QString size_key = QString::number(info.size()) % QLatin1Char('.') % info.suffix().toLower();
                    if (sizes.contains(size_key))
                        continue;
                    sizes.insert(size_key);
                }
            }
            names.insert(name);
            kept.append(url);
        }
        list = kept;
    }
}

void Assets::remove_listed(const QStringList& entries)
{
    static const HashMap<QString, AssetType> kinds {
        { QStringLiteral("box_front"), AssetType::BOX_FRONT },
        { QStringLiteral("box_back"), AssetType::BOX_BACK },
        { QStringLiteral("cartridge"), AssetType::CARTRIDGE },
        { QStringLiteral("logo"), AssetType::LOGO },
        { QStringLiteral("steam"), AssetType::UI_STEAMGRID },
        { QStringLiteral("background"), AssetType::BACKGROUND },
        { QStringLiteral("screenshot"), AssetType::SCREENSHOT },
        { QStringLiteral("titlescreen"), AssetType::TITLESCREEN },
        { QStringLiteral("video"), AssetType::VIDEO },
    };
    const auto name_of = [](const QString& path) {
        return path.mid(path.lastIndexOf(QLatin1Char('/')) + 1).toLower();
    };
    for (const QString& entry : entries) {
        const int bar = entry.indexOf(QLatin1Char('|'));
        if (bar <= 0)
            continue;
        const auto kind = kinds.find(entry.left(bar).trimmed());
        if (kind == kinds.cend())
            continue;
        const QString name = name_of(entry.mid(bar + 1).trimmed());
        if (name.isEmpty())
            continue;
        const auto list = m_asset_lists.find(kind->second);
        if (list == m_asset_lists.end())
            continue;
        QStringList kept;
        for (const QString& url : qAsConst(list->second)) {
            if (name_of(url) != name)
                kept.append(url);
        }
        list->second = kept;
    }
}

} // namespace model
