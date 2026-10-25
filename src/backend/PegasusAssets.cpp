// Pegasus Frontend
// Copyright (C) 2017  Mátyás Mustoha
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


#include "PegasusAssets.h"

#include "types/AssetType.h"
#include "utils/HashMap.h"

#include <QString>
#include <QStringList>


namespace pegasus_assets {

AssetType str_to_type(const QString& str)
{
    static const HashMap<QString, const AssetType> map {
        { QStringLiteral("boxfront"), AssetType::BOX_FRONT },
        { QStringLiteral("boxFront"), AssetType::BOX_FRONT },
        { QStringLiteral("box_front"), AssetType::BOX_FRONT },
        { QStringLiteral("boxart2D"), AssetType::BOX_FRONT },
        { QStringLiteral("boxart2d"), AssetType::BOX_FRONT },

        { QStringLiteral("boxback"), AssetType::BOX_BACK },
        { QStringLiteral("boxBack"), AssetType::BOX_BACK },
        { QStringLiteral("box_back"), AssetType::BOX_BACK },

        { QStringLiteral("boxspine"), AssetType::BOX_SPINE },
        { QStringLiteral("boxSpine"), AssetType::BOX_SPINE },
        { QStringLiteral("box_spine"), AssetType::BOX_SPINE },

        { QStringLiteral("boxside"), AssetType::BOX_SPINE },
        { QStringLiteral("boxSide"), AssetType::BOX_SPINE },
        { QStringLiteral("box_side"), AssetType::BOX_SPINE },

        { QStringLiteral("boxfull"), AssetType::BOX_FULL },
        { QStringLiteral("boxFull"), AssetType::BOX_FULL },
        { QStringLiteral("box_full"), AssetType::BOX_FULL },
        { QStringLiteral("box"), AssetType::BOX_FULL },

        { QStringLiteral("cartridge"), AssetType::CARTRIDGE },
        { QStringLiteral("disc"), AssetType::CARTRIDGE },
        { QStringLiteral("cart"), AssetType::CARTRIDGE },
        { QStringLiteral("logo"), AssetType::LOGO },
        { QStringLiteral("wheel"), AssetType::LOGO },
        { QStringLiteral("marquee"), AssetType::ARCADE_MARQUEE },
        { QStringLiteral("bezel"), AssetType::ARCADE_BEZEL },
        { QStringLiteral("screenmarquee"), AssetType::ARCADE_BEZEL },
        { QStringLiteral("border"), AssetType::ARCADE_BEZEL },
        { QStringLiteral("panel"), AssetType::ARCADE_PANEL },

        { QStringLiteral("cabinetleft"), AssetType::ARCADE_CABINET_L },
        { QStringLiteral("cabinetLeft"), AssetType::ARCADE_CABINET_L },
        { QStringLiteral("cabinet_left"), AssetType::ARCADE_CABINET_L },

        { QStringLiteral("cabinetright"), AssetType::ARCADE_CABINET_R },
        { QStringLiteral("cabinetRight"), AssetType::ARCADE_CABINET_R },
        { QStringLiteral("cabinet_right"), AssetType::ARCADE_CABINET_R },

        { QStringLiteral("tile"), AssetType::UI_TILE },
        { QStringLiteral("banner"), AssetType::UI_BANNER },
        { QStringLiteral("steam"), AssetType::UI_STEAMGRID },
        { QStringLiteral("steamgrid"), AssetType::UI_STEAMGRID },
        { QStringLiteral("grid"), AssetType::UI_STEAMGRID },
        { QStringLiteral("poster"), AssetType::POSTER },
        { QStringLiteral("flyer"), AssetType::POSTER },
        { QStringLiteral("background"), AssetType::BACKGROUND },
        { QStringLiteral("music"), AssetType::MUSIC },

        { QStringLiteral("screenshot"), AssetType::SCREENSHOT },
        { QStringLiteral("screenshots"), AssetType::SCREENSHOT },
        { QStringLiteral("video"), AssetType::VIDEO },
        { QStringLiteral("videos"), AssetType::VIDEO },
        { QStringLiteral("titlescreen"), AssetType::TITLESCREEN },

        // ES-DE style names
        { QStringLiteral("cover"), AssetType::BOX_FRONT },
        { QStringLiteral("3dbox"), AssetType::BOX_FRONT },
        { QStringLiteral("backcover"), AssetType::BOX_BACK },
        { QStringLiteral("physicalmedia"), AssetType::CARTRIDGE },
        { QStringLiteral("fanart"), AssetType::BACKGROUND },
        { QStringLiteral("miximage"), AssetType::UI_STEAMGRID },
        // their plurals and the other names Pegasus X knows (see SkraperAssetsProvider)
        { QStringLiteral("covers"), AssetType::BOX_FRONT },
        { QStringLiteral("box2dfront"), AssetType::BOX_FRONT },
        { QStringLiteral("box3d"), AssetType::BOX_FRONT },
        { QStringLiteral("3dboxes"), AssetType::BOX_FRONT },
        { QStringLiteral("backcovers"), AssetType::BOX_BACK },
        { QStringLiteral("box2dback"), AssetType::BOX_BACK },
        { QStringLiteral("spine"), AssetType::BOX_SPINE },
        { QStringLiteral("box2dside"), AssetType::BOX_SPINE },
        { QStringLiteral("support"), AssetType::CARTRIDGE },
        { QStringLiteral("fanarts"), AssetType::BACKGROUND },
        { QStringLiteral("backgrounds"), AssetType::BACKGROUND },
        { QStringLiteral("miximages"), AssetType::UI_STEAMGRID },
        { QStringLiteral("steamgrids"), AssetType::UI_STEAMGRID },
        { QStringLiteral("titlescreens"), AssetType::TITLESCREEN },
        { QStringLiteral("screenshottitle"), AssetType::TITLESCREEN },
        { QStringLiteral("snap"), AssetType::SCREENSHOT },
        { QStringLiteral("wheels"), AssetType::LOGO },
        { QStringLiteral("logos"), AssetType::LOGO },
        { QStringLiteral("banners"), AssetType::UI_BANNER },
        { QStringLiteral("tiles"), AssetType::UI_TILE },
        { QStringLiteral("posters"), AssetType::POSTER },
        { QStringLiteral("flyers"), AssetType::POSTER },
        { QStringLiteral("bezels"), AssetType::ARCADE_BEZEL },
    };

    const auto it = map.find(str);
    if (it != map.cend())
        return it->second;

    // Pegasus X: spelled any way ("3D Box", "box-front", "Screenshots")
    const QString plain = plain_name(str);
    for (const auto& entry : map) {
        if (same_plain_name(plain, plain_name(entry.first)))
            return entry.second;
    }

    // Pegasus X: any case ("BoxFront", "COVERS"), so file and folder names
    // spelled differently are found on Linux like on Android and Windows
    for (const auto& entry : map) {
        if (entry.first.compare(str, Qt::CaseInsensitive) == 0)
            return entry.second;
    }

    for (const auto& it : map) {
        if (str.startsWith(it.first))
            return it.second;
    }

    return AssetType::UNKNOWN;
}

QString plain_name(const QString& text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar ch : text) {
        if (ch.isLetterOrNumber())
            out += ch.toLower();
    }
    return out;
}

bool same_plain_name(const QString& plain, const QString& wanted)
{
    if (plain.isEmpty() || wanted.isEmpty())
        return false;
    if (plain == wanted)
        return true;
    const auto plural_of = [](const QString& one, const QString& many) {
        return many.length() > one.length() && many.startsWith(one)
            && (many.mid(one.length()) == QLatin1String("s") || many.mid(one.length()) == QLatin1String("es"));
    };
    return plural_of(plain, wanted) || plural_of(wanted, plain);
}

bool is_3d_box(const QString& path)
{
    static const QStringList names {
        QStringLiteral("3dbox"), QStringLiteral("box3d"), QStringLiteral("3dboxart"),
        QStringLiteral("boxart3d"), QStringLiteral("3dcover"), QStringLiteral("cover3d"),
        QStringLiteral("boxes3d"), QStringLiteral("3dboxfront"), QStringLiteral("boxfront3d"),
    };
    // the folder it's in, then its own name ("<game>/3dbox.png")
    const QStringList parts = QString(path).replace(QLatin1Char('\\'), QLatin1Char('/')).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (int i = parts.size() - 1; i >= 0 && i >= parts.size() - 3; i--) {
        QString part = parts.at(i);
        if (i == parts.size() - 1) {
            const int dot = part.lastIndexOf(QLatin1Char('.'));
            if (dot > 0)
                part = part.left(dot);
        }
        const QString plain = plain_name(part);
        for (const QString& name : names) {
            if (same_plain_name(plain, name))
                return true;
        }
    }
    // a name that has it in it ("ps2-3dbox-front")
    const QString all = plain_name(path);
    return all.contains(QLatin1String("3dbox")) || all.contains(QLatin1String("box3d"));
}

} // namespace pegasus_assets
