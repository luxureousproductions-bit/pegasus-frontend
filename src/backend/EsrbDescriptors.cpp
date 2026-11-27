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


#include "EsrbDescriptors.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>


namespace scraper {
namespace esrb {

QString default_server()
{
    return QStringLiteral("https://pegasusx-esrb.pegasusx.workers.dev/v1/esrb");
}

QUrl endpoint(const QString& setting)
{
    const QString text = setting.trimmed().isEmpty() ? default_server() : setting.trimmed();
    QUrl url(text, QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty())
        return {};
    const bool here = url.host() == QLatin1String("127.0.0.1") || url.host() == QLatin1String("localhost");
    if (url.scheme() != QLatin1String("https") && !(here && url.scheme() == QLatin1String("http")))
        return {};
    if (url.path().isEmpty() || url.path() == QLatin1String("/"))
        url.setPath(QStringLiteral("/v1/esrb"));
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

// ES-DE system -> IGDB platform id: server/esrb-worker/worker.js's PLATFORMS
// (the old IGDB scraper's, plus ps5, xboxseries, switch2), kept the same
const QHash<QString, int>& platforms()
{
    static const QHash<QString, int> table {
        { QStringLiteral("3do"), 50 }, { QStringLiteral("amiga"), 16 }, { QStringLiteral("amiga1200"), 16 }, { QStringLiteral("amiga600"), 16 },
        { QStringLiteral("amigacd32"), 114 }, { QStringLiteral("amstradcpc"), 25 }, { QStringLiteral("android"), 34 }, { QStringLiteral("androidapps"), 34 },
        { QStringLiteral("androidgames"), 34 }, { QStringLiteral("apple2"), 75 }, { QStringLiteral("apple2gs"), 115 }, { QStringLiteral("arcade"), 52 },
        { QStringLiteral("atomiswave"), 52 }, { QStringLiteral("cps"), 52 }, { QStringLiteral("cps1"), 52 }, { QStringLiteral("cps2"), 52 },
        { QStringLiteral("cps3"), 52 }, { QStringLiteral("fba"), 52 }, { QStringLiteral("fbneo"), 52 }, { QStringLiteral("mame"), 52 },
        { QStringLiteral("mame-advmame"), 52 }, { QStringLiteral("model2"), 52 }, { QStringLiteral("model3"), 52 }, { QStringLiteral("naomi"), 52 },
        { QStringLiteral("naomi2"), 52 }, { QStringLiteral("naomigd"), 52 }, { QStringLiteral("atari2600"), 59 }, { QStringLiteral("atari5200"), 66 },
        { QStringLiteral("atari7800"), 60 }, { QStringLiteral("atari800"), 65 }, { QStringLiteral("atarixe"), 65 }, { QStringLiteral("atarist"), 63 },
        { QStringLiteral("atarijaguar"), 62 }, { QStringLiteral("atarilynx"), 61 }, { QStringLiteral("c64"), 15 }, { QStringLiteral("colecovision"), 68 },
        { QStringLiteral("dos"), 13 }, { QStringLiteral("dreamcast"), 23 }, { QStringLiteral("famicom"), 99 }, { QStringLiteral("fds"), 51 },
        { QStringLiteral("gamegear"), 35 }, { QStringLiteral("gb"), 33 }, { QStringLiteral("gba"), 24 }, { QStringLiteral("gbc"), 22 },
        { QStringLiteral("gc"), 21 }, { QStringLiteral("genesis"), 29 }, { QStringLiteral("megadrive"), 29 }, { QStringLiteral("megadrivejp"), 29 },
        { QStringLiteral("intellivision"), 67 }, { QStringLiteral("macintosh"), 14 }, { QStringLiteral("mark3"), 64 }, { QStringLiteral("mastersystem"), 64 },
        { QStringLiteral("megacd"), 78 }, { QStringLiteral("megacdjp"), 78 }, { QStringLiteral("segacd"), 78 }, { QStringLiteral("msx"), 27 },
        { QStringLiteral("msx1"), 27 }, { QStringLiteral("msx2"), 53 }, { QStringLiteral("n3ds"), 37 }, { QStringLiteral("n64"), 4 },
        { QStringLiteral("n64dd"), 416 }, { QStringLiteral("nds"), 20 }, { QStringLiteral("neogeo"), 80 }, { QStringLiteral("neogeocd"), 136 },
        { QStringLiteral("neogeocdjp"), 136 }, { QStringLiteral("nes"), 18 }, { QStringLiteral("ngage"), 42 }, { QStringLiteral("ngp"), 119 },
        { QStringLiteral("ngpc"), 120 }, { QStringLiteral("odyssey2"), 133 }, { QStringLiteral("videopac"), 133 }, { QStringLiteral("pc"), 6 },
        { QStringLiteral("windows"), 6 }, { QStringLiteral("steam"), 6 }, { QStringLiteral("pc88"), 125 }, { QStringLiteral("pc98"), 149 },
        { QStringLiteral("pcengine"), 86 }, { QStringLiteral("tg16"), 86 }, { QStringLiteral("pcenginecd"), 150 }, { QStringLiteral("tg-cd"), 150 },
        { QStringLiteral("pcfx"), 274 }, { QStringLiteral("ps2"), 8 }, { QStringLiteral("ps3"), 9 }, { QStringLiteral("ps4"), 48 },
        { QStringLiteral("psp"), 38 }, { QStringLiteral("psvita"), 46 }, { QStringLiteral("psx"), 7 }, { QStringLiteral("saturn"), 32 },
        { QStringLiteral("saturnjp"), 32 }, { QStringLiteral("sega32x"), 30 }, { QStringLiteral("sega32xjp"), 30 }, { QStringLiteral("sega32xna"), 30 },
        { QStringLiteral("sfc"), 58 }, { QStringLiteral("sg-1000"), 84 }, { QStringLiteral("snes"), 19 }, { QStringLiteral("snesna"), 19 },
        { QStringLiteral("supergrafx"), 128 }, { QStringLiteral("switch"), 130 }, { QStringLiteral("vectrex"), 70 }, { QStringLiteral("vic20"), 71 },
        { QStringLiteral("virtualboy"), 87 }, { QStringLiteral("wii"), 5 }, { QStringLiteral("wiiu"), 41 }, { QStringLiteral("wonderswan"), 57 },
        { QStringLiteral("wonderswancolor"), 123 }, { QStringLiteral("x68000"), 121 }, { QStringLiteral("xbox"), 11 }, { QStringLiteral("xbox360"), 12 },
        { QStringLiteral("xboxone"), 49 }, { QStringLiteral("zxspectrum"), 26 }, { QStringLiteral("ps5"), 167 }, { QStringLiteral("xboxseries"), 169 },
        { QStringLiteral("switch2"), 508 },
    };
    return table;
}

int igdb_platform(const QString& system)
{
    return platforms().value(system.trimmed().toLower(), 0);
}

QString why_not(const Game& game)
{
    if (game.letter.isEmpty())
        return QStringLiteral("its x-esrb isn't one of EC, E, E10+, T, M, AO, RP");
    if (game.igdb_id > 0)
        return {};
    if (game.name.trimmed().isEmpty())
        return QStringLiteral("no name");
    // (the server takes no quotes or backslashes in a name, nor over 200 letters)
    if (game.name.contains(QLatin1Char('"')) || game.name.contains(QLatin1Char('\\')) || game.name.size() > 200)
        return QStringLiteral("its name has a quote or a backslash, or is too long");
    if (game.platform <= 0)
        return QStringLiteral("no IGDB platform for its system");
    if (game.year < 1950 || game.year > 2100)
        return QStringLiteral("no release year");
    return {};
}

QJsonObject request_entry(const Game& game)
{
    if (game.igdb_id > 0)
        return QJsonObject { { QStringLiteral("igdbId"), game.igdb_id }, { QStringLiteral("esrb"), game.letter } };
    return QJsonObject {
        { QStringLiteral("name"), game.name.simplified() },
        { QStringLiteral("platform"), game.platform },
        { QStringLiteral("year"), game.year },
        { QStringLiteral("esrb"), game.letter },
    };
}

namespace {
// a list of the server's as written: each one on one line, at most 30 of
// 100 letters (a file line each field; nothing that could break one)
QStringList clean_list(const QJsonValue& value)
{
    QStringList out;
    for (const QJsonValue& v : value.toArray()) {
        QString text = v.toString();
        for (QChar& c : text) {
            if (c.category() == QChar::Other_Control || c == QChar::LineSeparator || c == QChar::ParagraphSeparator)
                c = QLatin1Char(' ');
        }
        text = text.simplified().left(100);
        if (!text.isEmpty() && !out.contains(text))
            out.append(text);
        if (out.size() >= 30)
            break;
    }
    return out;
}
} // namespace

Outcome read_result(const QJsonObject& result, const Game& game, bool overwrite)
{
    Outcome out;
    const QString status = result.value(QStringLiteral("status")).toString();
    const QString id = result.value(QStringLiteral("igdbId")).toVariant().toString();
    const QString on_igdb = id.isEmpty() ? QString() : QStringLiteral(" (IGDB %1)").arg(id);
    if (status == QLatin1String("ok")) {
        const QStringList descriptors = clean_list(result.value(QStringLiteral("descriptors")));
        const QStringList interactive = clean_list(result.value(QStringLiteral("interactive")));
        QStringList said;
        if (!descriptors.isEmpty()) {
            if (overwrite || !game.has_descriptors) {
                out.changes.insert(QStringLiteral("x-esrb-descriptors"), descriptors.join(QStringLiteral(", ")));
                said.append(QStringLiteral("x-esrb-descriptors: ") + descriptors.join(QStringLiteral(", ")));
            }
            else {
                said.append(QStringLiteral("the x-esrb-descriptors line it has is kept"));
            }
        }
        if (!interactive.isEmpty()) {
            if (overwrite || !game.has_interactive) {
                out.changes.insert(QStringLiteral("x-esrb-interactive"), interactive.join(QStringLiteral(", ")));
                said.append(QStringLiteral("x-esrb-interactive: ") + interactive.join(QStringLiteral(", ")));
            }
            else {
                said.append(QStringLiteral("the x-esrb-interactive line it has is kept"));
            }
        }
        if (descriptors.isEmpty() && interactive.isEmpty()) {
            out.log = QStringLiteral("ok%1, %2, but no descriptors on IGDB: nothing written").arg(on_igdb, game.letter);
            out.result = QStringLiteral("ESRB descriptors: none on IGDB");
            return out;
        }
        out.kind = out.changes.isEmpty() ? Outcome::Nothing : Outcome::Write;
        out.log = QStringLiteral("ok%1, %2 -> %3").arg(on_igdb, game.letter, said.join(QStringLiteral("; ")));
        out.result = out.changes.isEmpty() ? QStringLiteral("ESRB descriptors: kept")
                   : QStringLiteral("ESRB descriptors: ") + descriptors.join(QStringLiteral(", "));
        return out;
    }
    if (status == QLatin1String("none")) {
        out.log = QStringLiteral("none%1: no ESRB rating on IGDB, or no single game with this name, platform and year: nothing written").arg(on_igdb);
        out.result = QStringLiteral("ESRB descriptors: not found");
        return out;
    }
    if (status == QLatin1String("mismatch")) {
        out.log = QStringLiteral("mismatch%1: IGDB's ESRB letter is %2, the game's %3: nothing written")
            .arg(on_igdb, result.value(QStringLiteral("igdbEsrb")).toString(), game.letter);
        out.result = QStringLiteral("ESRB descriptors: another letter on IGDB");
        return out;
    }
    if (status == QLatin1String("invalid")) {
        out.log = QStringLiteral("invalid: the server didn't take it (%1): nothing written").arg(result.value(QStringLiteral("reason")).toString());
        out.result = QStringLiteral("ESRB descriptors: not asked");
        return out;
    }
    if (status == QLatin1String("later")) {
        out.kind = Outcome::Later;
        out.retry_after = retry_after(QByteArray(), result, 5);
        out.log = QStringLiteral("later (%1): asked again in %2 s")
            .arg(result.value(QStringLiteral("reason")).toString().isEmpty() ? QStringLiteral("busy") : result.value(QStringLiteral("reason")).toString())
            .arg(out.retry_after);
        out.result = QStringLiteral("ESRB descriptors: later");
        return out;
    }
    out.log = QStringLiteral("an answer it doesn't know (\"%1\"): nothing written").arg(status.left(40));
    out.result = QStringLiteral("ESRB descriptors: not understood");
    return out;
}

int retry_after(const QByteArray& header, const QJsonObject& body, int fallback)
{
    bool ok = false;
    int seconds = QString::fromLatin1(header).trimmed().toInt(&ok);
    if (!ok || seconds <= 0) {
        const QJsonValue v = body.value(QStringLiteral("retryAfter"));
        seconds = v.isDouble() ? static_cast<int>(v.toDouble()) : 0;
    }
    if (seconds <= 0)
        seconds = fallback;
    return qBound(1, seconds, static_cast<int>(MAX_WAIT_S));
}

} // namespace esrb
} // namespace scraper
