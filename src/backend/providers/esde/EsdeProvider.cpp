// Pegasus Frontend
// Copyright (C) 2017-2020  Mátyás Mustoha
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


#include "EsdeProvider.h"

#include "Log.h"
#include "model/gaming/Assets.h"
#include "model/gaming/Collection.h"
#include "model/gaming/Game.h"
#include "model/gaming/GameFile.h"
#include "platform/AndroidHelpers.h"
#include "providers/SearchContext.h"
#include "providers/es2/Es2Metadata.h"
#include "providers/es2/Es2Systems.h"
#include "providers/pegasus_metadata/PegasusFilter.h"
#include "providers/pegasus_metadata/PegasusMetadata.h"
#include "providers/skraper/SkraperAssetsProvider.h"
#include "Paths.h"
#include "utils/CommandTokenizer.h"
#include "utils/HashMap.h"
#include "utils/PathTools.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStringBuilder>
#include <QVariantMap>
#include <QTextStream>
#include <QUrl>
#include <QXmlStreamReader>
#include <algorithm>


// The bundled ES-DE configuration lives in a resource file of this static library
static void init_esde_resources() { Q_INIT_RESOURCE(esde); }


namespace {

struct EsdeCommand {
    QString label;
    QString raw;
};

struct EsdeSystem {
    QString name;
    QString fullname;
    QString path;
    QString extensions;
    QString platform;
    std::vector<EsdeCommand> commands;
};

struct GamelistInfo {
    QString system_alt_label;
    HashMap<QString, QString> game_alt_labels;  // absolute game path -> command label
    QSet<QString> hidden_paths;
};

struct Context {
    QString log_tag;
    QString primary_storage;
    QSet<QString> installed_packages;
    // emulator entry ("package/activity") -> what to launch on this device:
    // the entry itself, or the app's own game screen if that's named
    // differently in the installed build; used when filled
    HashMap<QString, QString> resolved_components;
    HashMap<QString, QStringList> emulators;  // emulator name -> "package/activity" entries
};


QString read_file(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

QString xml_unescape(QString str)
{
    return str
        .replace(QLatin1String("&quot;"), QLatin1String("\""))
        .replace(QLatin1String("&apos;"), QLatin1String("'"))
        .replace(QLatin1String("&lt;"), QLatin1String("<"))
        .replace(QLatin1String("&gt;"), QLatin1String(">"))
        .replace(QLatin1String("&amp;"), QLatin1String("&"));
}

// ES-DE writes some elements (<alternativeEmulator>, <loadExclusive>) next to
// the root element, which strict XML readers reject, so they're cut out first
QString cut_element(QString& text, const QString& name)
{
    const QRegularExpression rx(QStringLiteral("<%1\\s*/>|<%1\\b[^>]*>.*?</%1>").arg(name),
                                QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = rx.match(text);
    if (!match.hasMatch())
        return {};

    QString found = match.captured(0);
    text.remove(match.capturedStart(), match.capturedLength());
    return found;
}


// -- Locating the ES-DE setup -------------------------------------------------

QString find_esde_dir(const providers::Provider& provider)
{
    const auto option_it = provider.options().find(QStringLiteral("installdir"));
    if (option_it != provider.options().cend() && !option_it->second.empty())
        return QDir::cleanPath(option_it->second.front());

    for (const QString& root : ::android::storage_paths()) {
        if (root == QLatin1String("/"))
            continue;
        const QString dir = root % QStringLiteral("/ES-DE");
        if (QFileInfo::exists(dir % QStringLiteral("/settings/es_settings.xml"))
            || QFileInfo::exists(dir % QStringLiteral("/gamelists")))
            return dir;
    }
    return {};
}

HashMap<QString, QString> read_esde_settings(const QString& esde_dir)
{
    HashMap<QString, QString> out;
    const QString text = read_file(esde_dir % QStringLiteral("/settings/es_settings.xml"));
    static const QRegularExpression rx(QStringLiteral("<\\w+\\s+name=\"([^\"]+)\"\\s+value=\"([^\"]*)\""));
    auto it = rx.globalMatch(text);
    while (it.hasNext()) {
        const auto match = it.next();
        out[match.captured(1)] = xml_unescape(match.captured(2));
    }
    return out;
}

QStringList find_rom_roots(const providers::Provider& provider, const HashMap<QString, QString>& settings, const QString& primary_storage)
{
    QStringList out;
    const auto add = [&out](QString dir) {
        dir = QDir::cleanPath(dir);
        if (!dir.isEmpty() && QFileInfo(dir).isDir() && !out.contains(dir))
            out.append(dir);
    };

    const auto option_it = provider.options().find(QStringLiteral("romdir"));
    if (option_it != provider.options().cend()) {
        for (const QString& dir : option_it->second)
            add(dir);
        return out;
    }

    // NOTE: On Android, ES-DE keeps its ROMs directory in its own app storage, so
    //       usually it's not in es_settings.xml; look for the usual 'ROMs' folders
    const auto setting_it = settings.find(QStringLiteral("ROMDirectory"));
    if (setting_it != settings.cend() && !setting_it->second.isEmpty()) {
        QString dir = setting_it->second;
        if (dir.startsWith(QLatin1String("~/")))
            dir = primary_storage + dir.mid(1);
        add(dir);
    }
    for (const QString& root : ::android::storage_paths()) {
        if (root != QLatin1String("/"))
            add(root % QStringLiteral("/ROMs"));
    }
    return out;
}


// -- ES-DE configuration files ------------------------------------------------

void load_systems(const QString& log_tag, const QString& path,
                  HashMap<QString, EsdeSystem>& systems, std::vector<QString>& order, bool& load_exclusive)
{
    QString text = read_file(path);
    if (text.isEmpty())
        return;

    load_exclusive = !cut_element(text, QStringLiteral("loadExclusive")).isEmpty();

    QXmlStreamReader xml(text);
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("systemList")) {
        Log::warning(log_tag, LOGMSG("`%1` has no `<systemList>`, ignored").arg(path));
        return;
    }

    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("system")) {
            xml.skipCurrentElement();
            continue;
        }

        EsdeSystem sys;
        while (xml.readNextStartElement()) {
            const QStringRef tag = xml.name();
            if (tag == QLatin1String("command")) {
                QString label = xml.attributes().value(QLatin1String("label")).toString();
                sys.commands.push_back({ std::move(label), xml.readElementText().trimmed() });
            }
            else if (tag == QLatin1String("name"))      sys.name = xml.readElementText().trimmed();
            else if (tag == QLatin1String("fullname"))  sys.fullname = xml.readElementText().trimmed();
            else if (tag == QLatin1String("path"))      sys.path = xml.readElementText().trimmed();
            else if (tag == QLatin1String("extension")) sys.extensions = xml.readElementText().trimmed();
            else if (tag == QLatin1String("platform"))  sys.platform = xml.readElementText().trimmed();
            else xml.skipCurrentElement();
        }

        // the first file (the user's custom one) wins
        if (sys.name.isEmpty() || sys.path.isEmpty() || systems.count(sys.name))
            continue;
        order.push_back(sys.name);
        systems.emplace(sys.name, std::move(sys));
    }
    if (xml.hasError())
        Log::warning(log_tag, LOGMSG("`%1`: %2").arg(path, xml.errorString()));
}

void load_find_rules(const QString& log_tag, const QString& path, HashMap<QString, QStringList>& emulators)
{
    const QString text = read_file(path);
    if (text.isEmpty())
        return;

    QXmlStreamReader xml(text);
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("ruleList")) {
        Log::warning(log_tag, LOGMSG("`%1` has no `<ruleList>`, ignored").arg(path));
        return;
    }

    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("emulator")) {
            xml.skipCurrentElement();
            continue;
        }

        const QString name = xml.attributes().value(QLatin1String("name")).toString();
        QStringList entries;
        while (xml.readNextStartElement()) {
            const bool is_android = xml.name() == QLatin1String("rule")
                && xml.attributes().value(QLatin1String("type")) == QLatin1String("androidpackage");
            if (!is_android) {
                xml.skipCurrentElement();
                continue;
            }
            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("entry"))
                    entries.append(xml.readElementText().trimmed());
                else
                    xml.skipCurrentElement();
            }
        }

        // the first file (the user's custom one) wins
        if (!name.isEmpty() && !entries.isEmpty() && !emulators.count(name))
            emulators.emplace(name, std::move(entries));
    }
    if (xml.hasError())
        Log::warning(log_tag, LOGMSG("`%1`: %2").arg(path, xml.errorString()));
}

GamelistInfo read_gamelist(const QString& esde_dir, const QString& system_name, const QStringList& system_dirs)
{
    GamelistInfo info;

    QString text = read_file(esde_dir % QStringLiteral("/gamelists/") % system_name % QStringLiteral("/gamelist.xml"));
    if (text.isEmpty())
        return info;

    const QString alt_block = cut_element(text, QStringLiteral("alternativeEmulator"));
    if (!alt_block.isEmpty()) {
        static const QRegularExpression rx_label(QStringLiteral("<label>(.*?)</label>"),
                                                 QRegularExpression::DotMatchesEverythingOption);
        info.system_alt_label = xml_unescape(rx_label.match(alt_block).captured(1).trimmed());
    }

    QXmlStreamReader xml(text);
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("gameList"))
        return info;

    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("game")) {
            xml.skipCurrentElement();
            continue;
        }

        QString path;
        QString alt_label;
        bool hidden = false;
        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1String("path"))
                path = xml.readElementText().trimmed();
            else if (xml.name() == QLatin1String("altemulator"))
                alt_label = xml.readElementText().trimmed();
            else if (xml.name() == QLatin1String("hidden"))
                hidden = xml.readElementText().trimmed() == QLatin1String("true");
            else
                xml.skipCurrentElement();
        }
        if (path.isEmpty() || (alt_label.isEmpty() && !hidden))
            continue;

        for (const QString& dir : system_dirs) {
            const QString abs_path = ::clean_abs_path(QFileInfo(QDir(dir), path));
            if (!alt_label.isEmpty())
                info.game_alt_labels[abs_path] = alt_label;
            if (hidden)
                info.hidden_paths.insert(abs_path);
        }
    }
    return info;
}


// -- Launch commands ------------------------------------------------------------

// Splits at spaces, but keeps "quoted values" (and \" inside them) together
QStringList split_esde_command(const QString& cmd)
{
    QStringList out;
    QString current;
    bool in_quotes = false;
    for (int i = 0; i < cmd.length(); i++) {
        const QChar c = cmd.at(i);
        if (c == QLatin1Char('\\') && i + 1 < cmd.length() && cmd.at(i + 1) == QLatin1Char('"')) {
            current += QLatin1String("\\\"");
            i++;
            continue;
        }
        if (c == QLatin1Char('"'))
            in_quotes = !in_quotes;
        if (c.isSpace() && !in_quotes) {
            if (!current.isEmpty())
                out.append(current);
            current.clear();
            continue;
        }
        current += c;
    }
    if (!current.isEmpty())
        out.append(current);
    return out;
}

QString unquote(QString value)
{
    if (value.length() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
        value = value.mid(1, value.length() - 2);
    return value.replace(QLatin1String("\\\""), QLatin1String("\""));
}

// Turns ES-DE variables into Pegasus ones; false if something unsupported is left
bool expand_variables(QString& value, const QString& package, const QString& rom_root, const QString& primary_storage)
{
    value
        .replace(QLatin1String("%ROMSAF%"), QLatin1String("{file.documenturi}"))
        .replace(QLatin1String("%ROMPROVIDER%"), QLatin1String("{file.uri}"))
        .replace(QLatin1String("%ROMRAW%"), QLatin1String("{file.path}"))
        .replace(QLatin1String("%ROM%"), QLatin1String("{file.path}"))
        .replace(QLatin1String("%BASENAME%"), QLatin1String("{file.basename}"))
        .replace(QLatin1String("%FILENAME%"), QLatin1String("{file.name}"))
        .replace(QLatin1String("%GAMEDIRRAW%"), QLatin1String("{file.dir}"))
        .replace(QLatin1String("%GAMEDIR%"), QLatin1String("{file.dir}"))
        .replace(QLatin1String("%ROMPATHRAW%"), rom_root)
        .replace(QLatin1String("%ROMPATH%"), rom_root)
        .replace(QLatin1String("%ANDROIDPACKAGE%"), package)
        .replace(QLatin1String("%EXTERNALDATA%"), primary_storage)
        .replace(QLatin1String("%INTERNALDATA%"), QLatin1String("/data/user/0"));

    static const QRegularExpression rx_leftover(QStringLiteral("%[A-Z0-9_]+%"));
    return !rx_leftover.match(value).hasMatch();
}

QString quote_arg(const QString& arg)
{
    if (arg.isEmpty())
        return QStringLiteral("\"\"");

    const bool needs_quotes = arg.contains(QLatin1Char(' ')) || arg.contains(QLatin1Char('"')) || arg.contains(QLatin1Char('\''));
    if (!needs_quotes)
        return arg;
    if (!arg.contains(QLatin1Char('"')))
        return QLatin1Char('"') % arg % QLatin1Char('"');
    if (!arg.contains(QLatin1Char('\'')))
        return QLatin1Char('\'') % arg % QLatin1Char('\'');
    return arg;
}

/// Translates one ES-DE command into an 'am start' command, or returns an empty
/// string (with the reason in `error`) if it uses something Pegasus can't do
QString translate_command(const QString& raw, const Context& ctx, const QString& rom_root,
                          bool& emulator_installed, QString& error)
{
    const QStringList tokens = split_esde_command(raw);

    // ES-DE's Android apps: the game file holds the app's package name
    if (!tokens.isEmpty() && tokens.first().startsWith(QLatin1String("%ANDROIDAPP%="))) {
        emulator_installed = true;
        return QStringLiteral("am start -a android.intent.action.MAIN -c android.intent.category.LAUNCHER -p {file.contents}");
    }

    static const QRegularExpression rx_emulator(QStringLiteral("^%EMULATOR_([^%]+)%$"));
    const auto emu_match = tokens.isEmpty() ? QRegularExpressionMatch() : rx_emulator.match(tokens.first());
    if (!emu_match.hasMatch()) {
        error = QStringLiteral("not an emulator command");
        return {};
    }

    const QString emu_name = emu_match.captured(1);
    const auto emu_it = ctx.emulators.find(emu_name);
    if (emu_it == ctx.emulators.cend()) {
        error = QStringLiteral("unknown emulator `%1`").arg(emu_name);
        return {};
    }

    // prefer the first of the emulator's known packages that is installed
    // (checking the screen itself where known, as some builds of an
    // emulator name it differently)
    QString component = emu_it->second.first();
    emulator_installed = false;
    for (const QString& entry : emu_it->second) {
        if (!ctx.resolved_components.empty()) {
            const auto it = ctx.resolved_components.find(entry);
            if (it != ctx.resolved_components.cend() && !it->second.isEmpty()) {
                component = it->second;
                emulator_installed = true;
                break;
            }
        }
        else if (ctx.installed_packages.contains(entry.section(QLatin1Char('/'), 0, 0))) {
            component = entry;
            emulator_installed = true;
            break;
        }
    }
    const QString package = component.section(QLatin1Char('/'), 0, 0);

    QStringList out { QStringLiteral("am"), QStringLiteral("start"), QStringLiteral("-n"), component };
    for (int i = 1; i < tokens.size(); i++) {
        const QString& token = tokens.at(i);
        const int key_end = token.startsWith(QLatin1Char('%')) ? token.indexOf(QLatin1Char('%'), 1) : -1;
        if (key_end < 0) {
            error = QStringLiteral("unexpected `%1`").arg(token);
            return {};
        }
        const QString key = token.mid(1, key_end - 1);
        const bool has_value = key_end + 1 < token.length() && token.at(key_end + 1) == QLatin1Char('=');
        if (!has_value && key_end + 1 != token.length()) {
            error = QStringLiteral("unexpected `%1`").arg(token);
            return {};
        }

        if (!has_value) {
            if (key == QLatin1String("ACTIVITY_CLEAR_TASK"))       out << QStringLiteral("--activity-clear-task");
            else if (key == QLatin1String("ACTIVITY_CLEAR_TOP"))   out << QStringLiteral("--activity-clear-top");
            else if (key == QLatin1String("ACTIVITY_NO_HISTORY"))  out << QStringLiteral("--activity-no-history");
            else {
                error = QStringLiteral("unsupported `%1`").arg(token);
                return {};
            }
            continue;
        }

        QString value = unquote(token.mid(key_end + 2));
        // %INJECT%=%ROM%: the text inside the game file (eg. a Steam app ID)
        if (value == QLatin1String("%INJECT%=%ROM%"))
            value = QStringLiteral("{file.contents}");
        if (!expand_variables(value, package, rom_root, ctx.primary_storage)) {
            error = QStringLiteral("unsupported variable in `%1`").arg(token);
            return {};
        }

        if (key == QLatin1String("ACTION"))                       out << QStringLiteral("-a") << value;
        else if (key == QLatin1String("CATEGORY"))                out << QStringLiteral("-c") << value;
        else if (key == QLatin1String("DATA"))                    out << QStringLiteral("-d") << value;
        else if (key == QLatin1String("MIMETYPE"))                out << QStringLiteral("-t") << value;
        else if (key.startsWith(QLatin1String("EXTRAARRAY_")))    out << QStringLiteral("--esa") << key.mid(11) << value;
        else if (key.startsWith(QLatin1String("EXTRABOOL_")))     out << QStringLiteral("--ez") << key.mid(10) << value;
        else if (key.startsWith(QLatin1String("EXTRAINTEGER_")))  out << QStringLiteral("--ei") << key.mid(13) << value;
        else if (key.startsWith(QLatin1String("EXTRA_")))         out << QStringLiteral("-e") << key.mid(6) << value;
        else {
            error = QStringLiteral("unsupported `%1`").arg(token);
            return {};
        }
    }

    QStringList quoted;
    for (const QString& arg : out)
        quoted.append(quote_arg(arg));
    return quoted.join(QLatin1Char(' '));
}

/// The command for a label chosen in ES-DE (system-wide or per game), or else
/// the system's default command; if the default emulator isn't installed, the
/// first alternative whose emulator is installed is used instead
QString pick_command(const EsdeSystem& sys, const QString& label, const Context& ctx, const QString& rom_root)
{
    QString error;
    bool installed = false;

    if (!label.isEmpty()) {
        for (const EsdeCommand& cmd : sys.commands) {
            if (cmd.label != label)
                continue;
            const QString result = translate_command(cmd.raw, ctx, rom_root, installed, error);
            if (!result.isEmpty())
                return result;
            Log::warning(ctx.log_tag, LOGMSG("`%1`: the emulator `%2` can't be used (%3), using the default one")
                .arg(sys.name, label, error));
        }
    }

    QString first_usable;
    for (const EsdeCommand& cmd : sys.commands) {
        const QString result = translate_command(cmd.raw, ctx, rom_root, installed, error);
        if (result.isEmpty())
            continue;
        if (installed)
            return result;
        if (first_usable.isEmpty())
            first_usable = result;
    }
    return first_usable;
}


// -- Games and media --------------------------------------------------------------

QStringList parse_extensions(const QString& raw)
{
    QStringList out;
    static const QRegularExpression rx_space(QStringLiteral("\\s+"));
    for (const QString& ext : raw.toLower().split(rx_space, Qt::SkipEmptyParts)) {
        if (!out.contains(ext))
            out.append(ext);
    }
    return out;
}

bool has_extension(const QString& filename, const QStringList& extensions)
{
    const QString lower = filename.toLower();
    for (const QString& ext : extensions) {
        if (lower.endsWith(ext))
            return true;
    }
    return false;
}

void find_media(const QString& system_media_dir, const QString& system_dir,
                const HashMap<QString, model::Game*>& extless_path_to_game, size_t& found_assets)
{
    if (!QFileInfo(system_media_dir).isDir())
        return;

    HashMap<QString, QString> existing_dirs;  // lowercase -> actual folder name
    for (const QString& name : QDir(system_media_dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        existing_dirs.emplace(name.toLower(), name);

    constexpr auto DIR_FILTERS = QDir::Files | QDir::Readable | QDir::NoDotAndDotDot;
    constexpr auto DIR_FLAGS = QDirIterator::Subdirectories | QDirIterator::FollowSymlinks;

    // TODO: C++17
    for (const auto& asset_entry : providers::skraper::asset_dirs()) {
        for (const QString& dir_name_lower : asset_entry.second) {
            const auto dir_it = existing_dirs.find(dir_name_lower);
            if (dir_it == existing_dirs.cend())
                continue;

            const QDir type_dir(system_media_dir % QLatin1Char('/') % dir_it->second);
            QDirIterator file_it(type_dir.path(), DIR_FILTERS, DIR_FLAGS);
            while (file_it.hasNext()) {
                file_it.next();
                const QFileInfo finfo = file_it.fileInfo();

                // the media tree mirrors the ROM folder, eg. covers/Multidisk/Game.png
                const QString rel_dir = type_dir.relativeFilePath(finfo.absolutePath());
                const QString game_key = QDir::cleanPath(system_dir % QLatin1Char('/') % rel_dir
                                                         % QLatin1Char('/') % finfo.completeBaseName());
                const auto game_it = extless_path_to_game.find(game_key);
                if (game_it == extless_path_to_game.cend())
                    continue;

                game_it->second->assetsMut().add_file(asset_entry.first, file_it.filePath());
                found_assets++;
            }
        }
    }

    // Manuals (PDF): Pegasus has no asset type for them, so they are passed
    // to themes the same way as an 'x-manual' metadata field
    for (const QLatin1String dir_name_lower : { QLatin1String("manuals"), QLatin1String("manual") }) {
        const auto dir_it = existing_dirs.find(dir_name_lower);
        if (dir_it == existing_dirs.cend())
            continue;

        const QDir type_dir(system_media_dir % QLatin1Char('/') % dir_it->second);
        QDirIterator file_it(type_dir.path(), DIR_FILTERS, DIR_FLAGS);
        while (file_it.hasNext()) {
            file_it.next();
            const QFileInfo finfo = file_it.fileInfo();
            if (finfo.suffix().compare(QLatin1String("pdf"), Qt::CaseInsensitive) != 0)
                continue;

            const QString rel_dir = type_dir.relativeFilePath(finfo.absolutePath());
            const QString game_key = QDir::cleanPath(system_dir % QLatin1Char('/') % rel_dir
                                                     % QLatin1Char('/') % finfo.completeBaseName());
            const auto game_it = extless_path_to_game.find(game_key);
            if (game_it == extless_path_to_game.cend())
                continue;

            QVariantMap& extra = game_it->second->extraMapMut();
            if (!extra.contains(QStringLiteral("manual"))) {
                extra.insert(QStringLiteral("manual"), QStringList { finfo.absoluteFilePath() });
                found_assets++;
            }
        }
    }
}

/// The short name a system's collection gets. Themes pick system logos and
/// art by short name, using the common names (snes, megadrive, segacd...),
/// while ES-DE has its own names for regional and other variants. Those are
/// given the name of the system they are a variant of; ES-DE's own name is
/// still used for its gamelists and media folders.
QString pegasus_short_name(const QString& esde_name)
{
    static const HashMap<QString, QString> VARIANTS {
        { QStringLiteral("snesna"), QStringLiteral("snes") },
        { QStringLiteral("sfc"), QStringLiteral("snes") },
        { QStringLiteral("satellaview"), QStringLiteral("snes") },
        { QStringLiteral("sufami"), QStringLiteral("snes") },
        { QStringLiteral("famicom"), QStringLiteral("nes") },
        { QStringLiteral("sgb"), QStringLiteral("gb") },
        { QStringLiteral("n3ds"), QStringLiteral("3ds") },
        { QStringLiteral("n64dd"), QStringLiteral("n64") },
        { QStringLiteral("megadrivejp"), QStringLiteral("megadrive") },
        { QStringLiteral("megacd"), QStringLiteral("segacd") },
        { QStringLiteral("megacdjp"), QStringLiteral("segacd") },
        { QStringLiteral("sega32xjp"), QStringLiteral("sega32x") },
        { QStringLiteral("sega32xna"), QStringLiteral("sega32x") },
        { QStringLiteral("mark3"), QStringLiteral("mastersystem") },
        { QStringLiteral("saturnjp"), QStringLiteral("saturn") },
        { QStringLiteral("neogeocdjp"), QStringLiteral("neogeocd") },
        { QStringLiteral("amiga600"), QStringLiteral("amiga") },
        { QStringLiteral("amiga1200"), QStringLiteral("amiga") },
        { QStringLiteral("msx1"), QStringLiteral("msx") },
        { QStringLiteral("mame-advmame"), QStringLiteral("mame") },
        { QStringLiteral("mame-mame4all"), QStringLiteral("mame") },
    };
    const auto it = VARIANTS.find(esde_name);
    return it != VARIANTS.cend() ? it->second : esde_name;
}

/// The name without a disc tag: "Game (Disc 1)", "Game [CD2]", "Game (Disk 1 of 3)"
QString strip_disc_tag(const QString& name)
{
    static const QRegularExpression rx_disc(
        QStringLiteral("\\s*[\\(\\[]\\s*(disc|disk|cd|side)\\s*[0-9a-z]{1,2}(\\s*of\\s*[0-9]{1,2})?\\s*[\\)\\]]"),
        QRegularExpression::CaseInsensitiveOption);
    QString out = name;
    out.remove(rx_disc);
    return out.simplified();
}

bool has_disc_tag(const QString& name)
{
    return strip_disc_tag(name) != name.simplified();
}

/// The files a playlist or disc image file points to: the discs of an .m3u,
/// the tracks of a .cue (FILE lines) or a .gdi (5th column)
QStringList referenced_files(const QFileInfo& finfo)
{
    const QString suffix = finfo.suffix().toLower();
    if (suffix != QLatin1String("m3u") && suffix != QLatin1String("cue") && suffix != QLatin1String("gdi"))
        return {};

    QFile file(finfo.absoluteFilePath());
    if (file.size() > 1024 * 1024 || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    static const QRegularExpression rx_cue_file(QStringLiteral("^\\s*FILE\\s+(\"([^\"]+)\"|(\\S+))"),
                                                QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rx_gdi_line(QStringLiteral("^\\s*\\d+\\s+\\d+\\s+\\d+\\s+\\d+\\s+(\"([^\"]+)\"|(\\S+))"));

    const QDir dir = finfo.absoluteDir();
    QStringList out;
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        QString name;
        if (suffix == QLatin1String("m3u")) {
            name = line;
        }
        else {
            const auto match = (suffix == QLatin1String("cue") ? rx_cue_file : rx_gdi_line).match(line);
            if (match.hasMatch())
                name = match.captured(2).isEmpty() ? match.captured(3) : match.captured(2);
        }
        if (!name.isEmpty())
            out.append(::clean_abs_path(QFileInfo(dir, QDir::fromNativeSeparators(name))));
    }
    return out;
}

/// The game files of a system folder, one group per game:
/// - a folder named like a game file (eg. "Game.m3u/", "Game.ps3/") is one
///   game, as in ES-DE, and the files inside it are not listed on their own
/// - discs and tracks an .m3u, .cue or .gdi lists are not listed on their own
/// - files that only differ in a disc tag ("Game (Disc 1)", "Game (Disc 2)")
///   in the same folder are one game with several files; Pegasus then asks
///   which disc to start
/// Subfolders are searched too, so games in their own folder are found.
std::vector<std::vector<QFileInfo>> find_game_groups(const QString& system_dir, const QStringList& extensions,
                                                     const QSet<QString>& skipped_paths)
{
    std::vector<QFileInfo> candidates;
    constexpr auto entry_filters = QDir::Files | QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot;
    constexpr auto entry_flags = QDirIterator::FollowSymlinks | QDirIterator::Subdirectories;
    QDirIterator file_it(system_dir, entry_filters, entry_flags);
    while (file_it.hasNext()) {
        file_it.next();
        const QFileInfo finfo = file_it.fileInfo();
        if (!has_extension(finfo.fileName(), extensions))
            continue;
        if (skipped_paths.contains(::clean_abs_path(finfo)))
            continue;
        candidates.push_back(finfo);
    }
    std::sort(candidates.begin(), candidates.end(), [](const QFileInfo& a, const QFileInfo& b) {
        return a.absoluteFilePath().compare(b.absoluteFilePath(), Qt::CaseInsensitive) < 0;
    });

    // folders that are games themselves, and what playlists cover
    QStringList game_dirs;
    QSet<QString> covered;
    for (const QFileInfo& finfo : candidates) {
        if (finfo.isDir()) {
            game_dirs.append(::clean_abs_path(finfo) % QLatin1Char('/'));
        }
        else {
            for (const QString& path : referenced_files(finfo))
                covered.insert(path);
        }
    }
    const auto inside_game_dir = [&game_dirs](const QString& path) {
        for (const QString& dir : game_dirs) {
            if (path.startsWith(dir))
                return true;
        }
        return false;
    };

    std::vector<std::vector<QFileInfo>> groups;
    HashMap<QString, size_t> disc_group_index;  // dir/title without disc tag/ext -> group
    for (const QFileInfo& finfo : candidates) {
        const QString path = ::clean_abs_path(finfo);
        if (covered.contains(path) || inside_game_dir(path))
            continue;

        if (finfo.isFile() && has_disc_tag(finfo.completeBaseName())) {
            const QString key = ::clean_abs_dir(finfo) % QLatin1Char('/')
                % strip_disc_tag(finfo.completeBaseName()).toLower()
                % QLatin1Char('.') % finfo.suffix().toLower();
            const auto it = disc_group_index.find(key);
            if (it != disc_group_index.cend()) {
                groups[it->second].push_back(finfo);
                continue;
            }
            disc_group_index.emplace(key, groups.size());
        }
        groups.push_back({ finfo });
    }
    return groups;
}

} // namespace


namespace providers {
namespace esde {

EsdeProvider::EsdeProvider(QObject* parent)
    : Provider(QLatin1String("esde"), QStringLiteral("ES-DE"), PROVIDER_FLAG_CACHEABLE, parent)
{
    init_esde_resources();

    // Off until turned on in the settings, so it doesn't duplicate an existing setup
    setEnabled(false);
}

QStringList EsdeProvider::cacheInputFiles() const
{
    // Rescan when ES-DE's settings, custom systems or any gamelist change (eg.
    // after scraping), as those decide what this data source finds
    QStringList out;
    const QString esde_dir = find_esde_dir(*this);
    if (esde_dir.isEmpty())
        return out;

    out << esde_dir % QStringLiteral("/settings/es_settings.xml")
        << esde_dir % QStringLiteral("/custom_systems/es_systems.xml")
        << esde_dir % QStringLiteral("/custom_systems/es_find_rules.xml");

    const QDir gamelists_dir(esde_dir % QStringLiteral("/gamelists"));
    for (const QString& system : gamelists_dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        out << gamelists_dir.filePath(system % QStringLiteral("/gamelist.xml"));
    return out;
}

Provider& EsdeProvider::run(SearchContext& sctx)
{
    const QString esde_dir = find_esde_dir(*this);
    if (esde_dir.isEmpty()) {
        Log::info(display_name(), LOGMSG("No ES-DE folder found"));
        return *this;
    }
    Log::info(display_name(), LOGMSG("Using the ES-DE folder `%1`").arg(esde_dir));

    Context ctx;
    ctx.log_tag = display_name();
    ctx.primary_storage = ::android::primary_storage_path();
    for (const QString& package : ::android::installed_packages())
        ctx.installed_packages.insert(package);

    const HashMap<QString, QString> settings = read_esde_settings(esde_dir);
    const auto setting = [&settings](const char* key) {
        const auto it = settings.find(QLatin1String(key));
        return it != settings.cend() ? it->second : QString();
    };

    const QStringList rom_roots = find_rom_roots(*this, settings, ctx.primary_storage);
    if (rom_roots.isEmpty())
        Log::warning(display_name(), LOGMSG("No ROMs folder found, only systems with an absolute path will work"));
    for (const QString& root : rom_roots)
        Log::info(display_name(), LOGMSG("ROMs folder: `%1`").arg(root));

    QString media_root = setting("MediaDirectory");
    if (media_root.isEmpty())
        media_root = esde_dir % QStringLiteral("/downloaded_media");
    const bool show_hidden = setting("ShowHiddenGames") != QLatin1String("false");

    // System and emulator definitions: the user's custom files first, then ES-DE's own
    HashMap<QString, EsdeSystem> systems;
    std::vector<QString> system_order;
    bool load_exclusive = false;
    load_systems(display_name(), esde_dir % QStringLiteral("/custom_systems/es_systems.xml"), systems, system_order, load_exclusive);
    if (!load_exclusive) {
        bool unused = false;
        load_systems(display_name(), QStringLiteral(":/esde/es_systems.xml"), systems, system_order, unused);
    }
    load_find_rules(display_name(), esde_dir % QStringLiteral("/custom_systems/es_find_rules.xml"), ctx.emulators);
    load_find_rules(display_name(), QStringLiteral(":/esde/es_find_rules.xml"), ctx.emulators);
    Log::info(display_name(), LOGMSG("%1 systems and %2 emulators defined")
        .arg(QString::number(systems.size()), QString::number(ctx.emulators.size())));

    const es2::Metadata metahelper(display_name(), { esde_dir });

    size_t found_games_total = 0;
    size_t found_assets = 0;
    const float progress_step = system_order.empty() ? 1.f : 1.f / system_order.size();
    float progress = 0.f;

    for (const QString& system_name : system_order) {
        const EsdeSystem& sys = systems.at(system_name);

        progress += progress_step;
        emit progressChanged(progress);

        // %ROMPATH% systems can be in any of the ROMs folders (eg. internal and SD card)
        std::vector<std::pair<QString, QString>> dirs;  // (system dir, rom root)
        if (sys.path.contains(QLatin1String("%ROMPATH%"))) {
            for (const QString& root : rom_roots) {
                QString dir = sys.path;
                dir.replace(QLatin1String("%ROMPATH%"), root);
                dir = QDir::cleanPath(dir);
                if (QFileInfo(dir).isDir())
                    dirs.emplace_back(dir, root);
            }
        }
        else if (QFileInfo(sys.path).isDir()) {
            dirs.emplace_back(QDir::cleanPath(sys.path), rom_roots.isEmpty() ? QString() : rom_roots.first());
        }
        if (dirs.empty())
            continue;

        QStringList system_dirs;
        for (const auto& entry : dirs)
            system_dirs.append(entry.first);

        const GamelistInfo gamelist = read_gamelist(esde_dir, sys.name, system_dirs);
        const QStringList extensions = parse_extensions(sys.extensions);

        model::Collection& collection = *sctx.get_or_create_collection(sys.fullname.isEmpty() ? sys.name : sys.fullname);
        collection.setShortName(pegasus_short_name(sys.name));

        size_t found_games = 0;
        HashMap<QString, model::Game*> extless_path_to_game;
        std::vector<model::Game*> multi_disc_games;

        for (const auto& dir_entry : dirs) {
            const QString& system_dir = dir_entry.first;
            const QString& rom_root = dir_entry.second;

            const QString default_cmd = pick_command(sys, gamelist.system_alt_label, ctx, rom_root);
            if (default_cmd.isEmpty())
                Log::warning(display_name(), LOGMSG("`%1`: none of its emulators can be launched from Pegasus").arg(sys.name));
            if (collection.commonLaunchCmd().isEmpty())
                collection.setCommonLaunchCmd(default_cmd);

            const QSet<QString> skipped = show_hidden ? QSet<QString>() : gamelist.hidden_paths;
            for (const std::vector<QFileInfo>& group : find_game_groups(system_dir, extensions, skipped)) {
                const QFileInfo& first = group.front();
                const QString path = ::clean_abs_path(first);

                model::Game* game_ptr = sctx.game_by_filepath(path);
                const bool is_new = !game_ptr;
                if (is_new) {
                    game_ptr = sctx.create_game_for(collection);
                    sctx.game_add_filepath(*game_ptr, path);
                }
                sctx.game_add_to(*game_ptr, collection);

                // the other discs of a multi-disc game
                if (group.size() > 1) {
                    for (size_t i = 1; i < group.size(); i++) {
                        const QString disc_path = ::clean_abs_path(group[i]);
                        if (!sctx.game_by_filepath(disc_path))  // unless another source already has it
                            sctx.game_add_filepath(*game_ptr, disc_path);
                    }
                    if (is_new)
                        game_ptr->setTitle(strip_disc_tag(first.completeBaseName()));
                    multi_disc_games.push_back(game_ptr);
                }

                // a game-specific emulator picked in ES-DE, or this folder's default
                const auto alt_it = gamelist.game_alt_labels.find(path);
                if (alt_it != gamelist.game_alt_labels.cend() && (is_new || game_ptr->launchCmd().isEmpty())) {
                    const QString cmd = pick_command(sys, alt_it->second, ctx, rom_root);
                    if (!cmd.isEmpty())
                        game_ptr->setLaunchCmd(cmd);
                }
                else if (is_new && default_cmd != collection.commonLaunchCmd()) {
                    game_ptr->setLaunchCmd(default_cmd);
                }

                // media is matched by the file name of any disc, or of the playlist
                for (const QFileInfo& finfo : group) {
                    QString extless = ::clean_abs_path(finfo);
                    if (!finfo.isDir())
                        extless = ::clean_abs_dir(finfo) % QLatin1Char('/') % finfo.completeBaseName();
                    extless_path_to_game.emplace(std::move(extless), game_ptr);
                    if (group.size() > 1) {  // eg. covers/Game.png for "Game (Disc 1).chd"
                        extless_path_to_game.emplace(
                            ::clean_abs_dir(finfo) % QLatin1Char('/') % strip_disc_tag(finfo.completeBaseName()), game_ptr);
                    }
                }
                found_games++;
            }
        }

        if (found_games == 0)
            continue;
        found_games_total += found_games;
        Log::info(display_name(), LOGMSG("System `%1` provided %2 games").arg(sys.name, QString::number(found_games)));

        // metadata from ES-DE/gamelists/<system>/gamelist.xml
        for (const QString& system_dir : system_dirs) {
            const es2::SystemEntry sysentry {
                sys.fullname, sys.name, system_dir, sys.extensions, sys.platform, QString(),
            };
            metahelper.find_metadata_for(sysentry, sctx);
        }

        // names from the gamelist may still carry the first disc's tag
        for (model::Game* const game : multi_disc_games) {
            const QString clean = strip_disc_tag(game->title());
            if (clean != game->title() && !clean.isEmpty())
                game->setTitle(clean);
        }

        // media from ES-DE/downloaded_media/<system>/<type>/
        for (const QString& system_dir : system_dirs)
            find_media(media_root % QLatin1Char('/') % sys.name, system_dir, extless_path_to_game, found_assets);
    }

    Log::info(display_name(), LOGMSG("%1 games and %2 media files found")
        .arg(QString::number(found_games_total), QString::number(found_assets)));
    return *this;
}



// -- ES-DE import -----------------------------------------------------------------

namespace {
const QString IMPORT_FILE_NAME = QStringLiteral("esde-import.metadata.pegasus.txt");

/// What the import needs to know about an ES-DE setup
struct ImportSetup {
    QString esde_dir;
    QString media_root;
    bool show_hidden = true;
    HashMap<QString, EsdeSystem> systems;
    std::vector<QString> order;
    QStringList rom_roots;
};

ImportSetup load_import_setup(const QString& esde_dir)
{
    init_esde_resources();
    ImportSetup out;
    out.esde_dir = QDir::cleanPath(esde_dir);

    const HashMap<QString, QString> settings = read_esde_settings(out.esde_dir);
    const auto media_it = settings.find(QStringLiteral("MediaDirectory"));
    out.media_root = (media_it != settings.cend() && !media_it->second.isEmpty())
        ? media_it->second
        : out.esde_dir % QStringLiteral("/downloaded_media");
    const auto hidden_it = settings.find(QStringLiteral("ShowHiddenGames"));
    out.show_hidden = hidden_it == settings.cend() || hidden_it->second != QLatin1String("false");

    bool load_exclusive = false;
    const QString log_tag = QStringLiteral("ES-DE import");
    load_systems(log_tag, out.esde_dir % QStringLiteral("/custom_systems/es_systems.xml"), out.systems, out.order, load_exclusive);
    if (!load_exclusive) {
        bool unused = false;
        load_systems(log_tag, QStringLiteral(":/esde/es_systems.xml"), out.systems, out.order, unused);
    }

    // the ROMs folders: ES-DE's setting, the usual "ROMs" folders, and the
    // parent folders of the game directories set in Pegasus
    const auto add_root = [&out](QString dir) {
        dir = QDir::cleanPath(dir);
        if (!dir.isEmpty() && QFileInfo(dir).isDir() && !out.rom_roots.contains(dir))
            out.rom_roots.append(dir);
    };
    const auto rom_it = settings.find(QStringLiteral("ROMDirectory"));
    if (rom_it != settings.cend() && !rom_it->second.isEmpty()) {
        QString dir = rom_it->second;
        if (dir.startsWith(QLatin1String("~/")))
            dir = ::android::primary_storage_path() + dir.mid(1);
        add_root(dir);
    }
    for (const QString& root : ::android::storage_paths()) {
        if (root != QLatin1String("/"))
            add_root(root % QStringLiteral("/ROMs"));
    }
    {
        const providers::SearchContext probe;  // reads the game directory list
        for (const QString& dir : probe.root_game_dirs()) {
            add_root(dir);
            add_root(QFileInfo(dir).absolutePath());
        }
    }
    return out;
}

/// A system's folders: %ROMPATH%/<system> in every ROMs folder
QStringList system_folders(const EsdeSystem& sys, const QStringList& rom_roots)
{
    QStringList out;
    if (sys.path.contains(QLatin1String("%ROMPATH%"))) {
        for (const QString& root : rom_roots) {
            QString dir = sys.path;
            dir.replace(QLatin1String("%ROMPATH%"), root);
            dir = QDir::cleanPath(dir);
            if (QFileInfo(dir).isDir() && !out.contains(dir))
                out.append(dir);
        }
    }
    else if (QFileInfo(sys.path).isDir()) {
        out.append(QDir::cleanPath(sys.path));
    }
    return out;
}

/// The collection name used by the other metadata files of a folder, so the
/// imported games join that collection instead of making a second one
QString existing_collection_name(const QString& dir)
{
    static const QRegularExpression rx(QStringLiteral("^collection\\s*:\\s*(.+)$"),
                                       QRegularExpression::MultilineOption);
    for (const QFileInfo& finfo : QDir(dir).entryInfoList(QDir::Files | QDir::Readable, QDir::Name)) {
        const QString name = finfo.fileName();
        if (name == IMPORT_FILE_NAME)
            continue;
        if (name != QLatin1String("metadata.pegasus.txt") && name != QLatin1String("metadata.txt")
            && !name.endsWith(QLatin1String(".metadata.pegasus.txt")) && !name.endsWith(QLatin1String(".metadata.txt")))
            continue;
        const auto match = rx.match(read_file(finfo.absoluteFilePath()));
        if (match.hasMatch())
            return match.captured(1).trimmed();
    }
    return {};
}

/// A value for a metadata file: multiple lines are indented, empty lines are "."
QString metafile_value(const QString& value)
{
    QStringList lines = value.trimmed().split(QLatin1Char('\n'));
    for (int i = 1; i < lines.size(); i++) {
        const QString line = lines.at(i).trimmed();
        lines[i] = QStringLiteral("  ") % (line.isEmpty() ? QStringLiteral(".") : line);
    }
    return lines.join(QLatin1Char('\n'));
}

QString asset_key(AssetType type)
{
    switch (type) {
        case AssetType::BOX_FRONT: return QStringLiteral("box_front");
        case AssetType::BOX_BACK: return QStringLiteral("box_back");
        case AssetType::BOX_SPINE: return QStringLiteral("box_spine");
        case AssetType::BOX_FULL: return QStringLiteral("box_full");
        case AssetType::CARTRIDGE: return QStringLiteral("cartridge");
        case AssetType::LOGO: return QStringLiteral("logo");
        case AssetType::POSTER: return QStringLiteral("poster");
        case AssetType::ARCADE_MARQUEE: return QStringLiteral("marquee");
        case AssetType::ARCADE_BEZEL: return QStringLiteral("bezel");
        case AssetType::ARCADE_PANEL: return QStringLiteral("panel");
        case AssetType::ARCADE_CABINET_L: return QStringLiteral("cabinet_left");
        case AssetType::ARCADE_CABINET_R: return QStringLiteral("cabinet_right");
        case AssetType::UI_TILE: return QStringLiteral("tile");
        case AssetType::UI_BANNER: return QStringLiteral("banner");
        case AssetType::UI_STEAMGRID: return QStringLiteral("steam");
        case AssetType::BACKGROUND: return QStringLiteral("background");
        case AssetType::MUSIC: return QStringLiteral("music");
        case AssetType::SCREENSHOT: return QStringLiteral("screenshot");
        case AssetType::TITLESCREEN: return QStringLiteral("titlescreen");
        case AssetType::VIDEO: return QStringLiteral("video");
        default: return {};
    }
}

/// A local path for an asset or extra value (they may be file:// URLs)
QString local_path(const QString& value)
{
    const QUrl url(value);
    return url.isLocalFile() ? url.toLocalFile() : value;
}

/// The launch lines in an earlier import file, so importing again keeps the
/// emulators picked since: game file -> command, and the collection's in
/// `collection_launch`
HashMap<QString, QString> previous_launches(const QString& file_path, QString& collection_launch)
{
    HashMap<QString, QString> out;
    const QString text = read_file(file_path);
    if (text.isEmpty())
        return out;

    const QDir base(QFileInfo(file_path).absolutePath());
    QStringList files;
    QString launch;
    bool in_game = false;
    bool in_files = false;
    const auto flush = [&]{
        if (in_game && !launch.isEmpty()) {
            for (const QString& file : files)
                out.emplace(file, launch);
        }
        files.clear();
        launch.clear();
    };
    for (QString line : text.split(QLatin1Char('\n'))) {
        line.remove(QLatin1Char('\r'));
        if (line.trimmed().isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.at(0).isSpace()) {
            if (in_files)
                files.append(::clean_abs_path(QFileInfo(base, line.trimmed())));
            continue;
        }
        in_files = false;
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon <= 0)
            continue;
        const QString key = line.left(colon).trimmed().toLower();
        const QString value = line.mid(colon + 1).trimmed();
        if (key == QLatin1String("game") || key == QLatin1String("collection")) {
            flush();
            in_game = key == QLatin1String("game");
        }
        else if (key == QLatin1String("file") && !value.isEmpty()) {
            files.append(::clean_abs_path(QFileInfo(base, value)));
        }
        else if (key == QLatin1String("files")) {
            in_files = true;
            if (!value.isEmpty())
                files.append(::clean_abs_path(QFileInfo(base, value)));
        }
        else if (key == QLatin1String("launch") || key == QLatin1String("command")) {
            if (in_game)
                launch = value;
            else
                collection_launch = value;
        }
    }
    flush();
    return out;
}

/// One game's entry in a metadata file. If the user's own metadata already
/// has the game (`own`), its title is kept and only what it lacks is written,
/// so the user's details win whichever file Pegasus reads first.
QString game_entry(const model::Game& game, const QStringList& files, const QDir& base_dir,
                   const model::Game* own, const QString& launch)
{
    QString out;
    QTextStream stream(&out);
    stream << "game: " << metafile_value(own ? own->title() : game.title()) << '\n';
    if (files.size() == 1) {
        stream << "file: " << base_dir.relativeFilePath(files.first()) << '\n';
    }
    else {
        stream << "files:\n";
        for (const QString& file : files)
            stream << "  " << base_dir.relativeFilePath(file) << '\n';
    }
    if (!own && !game.sortBy().isEmpty() && game.sortBy() != game.title())
        stream << "sort-by: " << game.sortBy() << '\n';
    if (!game.developerListConst().isEmpty() && !(own && !own->developerListConst().isEmpty()))
        stream << "developer: " << game.developerListConst().join(QStringLiteral(", ")) << '\n';
    if (!game.publisherListConst().isEmpty() && !(own && !own->publisherListConst().isEmpty()))
        stream << "publisher: " << game.publisherListConst().join(QStringLiteral(", ")) << '\n';
    if (!game.genreListConst().isEmpty() && !(own && !own->genreListConst().isEmpty()))
        stream << "genre: " << game.genreListConst().join(QStringLiteral(", ")) << '\n';
    if (game.playerCount() > 1 && !(own && own->playerCount() > 1))
        stream << "players: " << game.playerCount() << '\n';
    if (game.releaseDate().isValid() && !(own && own->releaseDate().isValid()))
        stream << "release: " << game.releaseDate().toString(Qt::ISODate) << '\n';
    if (!game.summary().isEmpty() && !(own && !own->summary().isEmpty()))
        stream << "summary: " << metafile_value(game.summary()) << '\n';
    if (!game.description().isEmpty() && !(own && !own->description().isEmpty()))
        stream << "description: " << metafile_value(game.description()) << '\n';
    // a game's own emulator: after the description, before the rating
    if (!launch.isEmpty())
        stream << "launch: " << launch << '\n';
    if (game.rating() > 0.f && !(own && own->rating() > 0.f))
        stream << "rating: " << qRound(game.rating() * 100.f) << "%\n";

    // NOTE: Assets only has a public getter per type
    const auto asset_list = [](const model::Assets& a) {
        return std::vector<std::pair<AssetType, const QStringList*>> {
            { AssetType::BOX_FRONT, &a.boxFrontList() },
            { AssetType::BOX_BACK, &a.boxBackList() },
            { AssetType::BOX_SPINE, &a.boxSpineList() },
            { AssetType::BOX_FULL, &a.boxFullList() },
            { AssetType::CARTRIDGE, &a.cartridgeList() },
            { AssetType::LOGO, &a.logoList() },
            { AssetType::POSTER, &a.posterList() },
            { AssetType::ARCADE_MARQUEE, &a.marqueeList() },
            { AssetType::ARCADE_BEZEL, &a.bezelList() },
            { AssetType::ARCADE_PANEL, &a.panelList() },
            { AssetType::ARCADE_CABINET_L, &a.cabinetLeftList() },
            { AssetType::ARCADE_CABINET_R, &a.cabinetRightList() },
            { AssetType::UI_TILE, &a.tileList() },
            { AssetType::UI_BANNER, &a.bannerList() },
            { AssetType::UI_STEAMGRID, &a.steamList() },
            { AssetType::BACKGROUND, &a.backgroundList() },
            { AssetType::MUSIC, &a.musicList() },
            { AssetType::SCREENSHOT, &a.screenshotList() },
            { AssetType::TITLESCREEN, &a.titlescreenList() },
            { AssetType::VIDEO, &a.videoList() },
        };
    };
    const auto lists = asset_list(game.assets());
    const auto own_lists = own ? asset_list(own->assets()) : decltype(lists)();
    for (size_t i = 0; i < lists.size(); i++) {
        const QString key = asset_key(lists[i].first);
        if (key.isEmpty() || lists[i].second->isEmpty())
            continue;
        if (own && !own_lists[i].second->isEmpty())
            continue;
        stream << "assets." << key << ": " << local_path(lists[i].second->first()) << '\n';
    }

    // x- fields, eg. x-manual
    for (auto it = game.extraMap().cbegin(); it != game.extraMap().cend(); ++it) {
        if (own && own->extraMap().contains(it.key()))
            continue;
        QStringList values = it.value().toStringList();
        if (values.isEmpty())
            values.append(it.value().toString());
        for (QString& value : values)
            value = local_path(value);
        if (!values.isEmpty() && !values.first().isEmpty())
            stream << "x-" << it.key() << ": " << values.first() << '\n';
    }

    stream.flush();
    return out;
}
} // namespace


QString esde_default_dir()
{
    for (const QString& root : ::android::storage_paths()) {
        if (root == QLatin1String("/"))
            continue;
        const QString dir = root % QStringLiteral("/ES-DE");
        if (QFileInfo::exists(dir % QStringLiteral("/settings/es_settings.xml"))
            || QFileInfo::exists(dir % QStringLiteral("/gamelists")))
            return dir;
    }
    return {};
}


std::vector<EsdeImportSystem> esde_import_systems(const QString& esde_dir)
{
    std::vector<EsdeImportSystem> out;
    if (esde_dir.isEmpty() || !QFileInfo(esde_dir).isDir())
        return out;

    const ImportSetup setup = load_import_setup(esde_dir);
    for (const QString& name : setup.order) {
        const EsdeSystem& sys = setup.systems.at(name);
        const QStringList folders = system_folders(sys, setup.rom_roots);
        if (folders.isEmpty())
            continue;

        EsdeImportSystem entry;
        entry.name = name;
        entry.fullname = sys.fullname.isEmpty() ? name : sys.fullname;
        entry.short_name = pegasus_short_name(name);
        entry.folders = folders;
        entry.has_gamelist = QFileInfo::exists(setup.esde_dir % QStringLiteral("/gamelists/") % name % QStringLiteral("/gamelist.xml"));
        entry.has_media = QFileInfo(setup.media_root % QLatin1Char('/') % name).isDir();

        const QStringList extensions = parse_extensions(sys.extensions);
        for (const QString& folder : folders)
            entry.games += static_cast<int>(find_game_groups(folder, extensions, {}).size());
        if (entry.games > 0)
            out.push_back(std::move(entry));
    }
    return out;
}


EsdeImportResult esde_import(const QString& esde_dir, const QStringList& system_names, bool combine_android,
                             const std::function<void(int, int, const QString&)>& progress)
{
    EsdeImportResult result;
    if (esde_dir.isEmpty() || !QFileInfo(esde_dir).isDir()) {
        result.error = QStringLiteral("ES-DE folder not found");
        return result;
    }

    const ImportSetup setup = load_import_setup(esde_dir);
    const QString log_tag = QStringLiteral("ES-DE import");
    const es2::Metadata metahelper(log_tag, { setup.esde_dir });

    QStringList todo;
    for (const QString& name : setup.order) {
        if (system_names.isEmpty() || system_names.contains(name))
            todo.append(name);
    }

    int done = 0;
    for (const QString& name : todo) {
        const EsdeSystem& sys = setup.systems.at(name);
        if (progress)
            progress(done, todo.size(), sys.fullname.isEmpty() ? name : sys.fullname);
        done++;

        const QStringList folders = system_folders(sys, setup.rom_roots);
        if (folders.isEmpty())
            continue;

        // Android Apps and Android Games can share one "Android" collection
        const bool as_android = combine_android
            && (name == QLatin1String("androidapps") || name == QLatin1String("androidgames"));
        const QString short_name = as_android ? QStringLiteral("android") : pegasus_short_name(sys.name);

        const QStringList extensions = parse_extensions(sys.extensions);
        const GamelistInfo gamelist = read_gamelist(setup.esde_dir, sys.name, folders);
        const QSet<QString> skipped = setup.show_hidden ? QSet<QString>() : gamelist.hidden_paths;
        bool system_written = false;

        for (const QString& folder : folders) {
            // the games, read into a scratch list the same way the old ES-DE
            // data source did, then written out as a metadata file
            providers::SearchContext tmp_sctx(QStringList {});
            model::Collection& collection = *tmp_sctx.get_or_create_collection(sys.name);

            HashMap<QString, model::Game*> extless_path_to_game;
            std::vector<std::pair<model::Game*, QStringList>> games;  // game, its files
            for (const std::vector<QFileInfo>& group : find_game_groups(folder, extensions, skipped)) {
                model::Game* game = tmp_sctx.create_game_for(collection);
                QStringList files;
                for (const QFileInfo& finfo : group) {
                    const QString path = ::clean_abs_path(finfo);
                    tmp_sctx.game_add_filepath(*game, path);
                    files.append(path);

                    QString extless = path;
                    if (!finfo.isDir())
                        extless = ::clean_abs_dir(finfo) % QLatin1Char('/') % finfo.completeBaseName();
                    extless_path_to_game.emplace(extless, game);
                    if (group.size() > 1) {
                        extless_path_to_game.emplace(
                            ::clean_abs_dir(finfo) % QLatin1Char('/') % strip_disc_tag(finfo.completeBaseName()), game);
                    }
                }
                tmp_sctx.game_add_to(*game, collection);
                if (group.size() > 1)
                    game->setTitle(strip_disc_tag(group.front().completeBaseName()));
                games.emplace_back(game, files);
            }
            if (games.empty()) {
                tmp_sctx.discard_all();
                continue;
            }

            const es2::SystemEntry sysentry {
                sys.fullname, sys.name, folder, sys.extensions, sys.platform, QString(),
            };
            metahelper.find_metadata_for(sysentry, tmp_sctx);

            // the games already in the folder's own metadata files
            providers::SearchContext own_sctx(QStringList {});
            {
                const providers::pegasus::Metadata own_reader(log_tag);
                for (const QFileInfo& finfo : QDir(folder).entryInfoList(QDir::Files | QDir::Readable, QDir::Name)) {
                    const QString fname = finfo.fileName();
                    if (fname == IMPORT_FILE_NAME)
                        continue;
                    if (fname == QLatin1String("metadata.pegasus.txt") || fname == QLatin1String("metadata.txt")
                        || fname.endsWith(QLatin1String(".metadata.pegasus.txt")) || fname.endsWith(QLatin1String(".metadata.txt")))
                        own_reader.apply_metafile(::clean_abs_path(finfo), own_sctx);
                }
            }
            size_t found_assets = 0;
            find_media(setup.media_root % QLatin1Char('/') % sys.name, folder, extless_path_to_game, found_assets);

            // the file
            QString collection_name = existing_collection_name(folder);
            if (collection_name.isEmpty() && as_android)
                collection_name = QStringLiteral("Android");
            if (collection_name.isEmpty())
                collection_name = sys.fullname.isEmpty() ? sys.name : sys.fullname;

            // emulators picked since the last import are kept
            QString collection_launch;
            const HashMap<QString, QString> old_launches =
                previous_launches(folder % QLatin1Char('/') % IMPORT_FILE_NAME, collection_launch);

            QString text;
            QTextStream stream(&text);
            stream << "# Made by Pegasus X from ES-DE (" << setup.esde_dir << ").\n"
                   << "# Settings > ES-DE import replaces this file when run again,\n"
                   << "# keeping the emulators picked in Settings > Emulators.\n\n"
                   << "collection: " << collection_name << '\n'
                   << "shortname: " << short_name << '\n';
            if (!collection_launch.isEmpty())
                stream << "launch: " << collection_launch << '\n';
            stream << '\n';
            const QDir base_dir(folder);
            for (const auto& entry : games) {
                model::Game* const game = entry.first;
                const QString clean = strip_disc_tag(game->title());
                if (entry.second.size() > 1 && !clean.isEmpty())
                    game->setTitle(clean);
                const model::Game* own = nullptr;
                for (const QString& file : entry.second) {
                    own = own_sctx.game_by_filepath(file);
                    if (own)
                        break;
                }
                QString launch;
                for (const QString& file : entry.second) {
                    const auto launch_it = old_launches.find(file);
                    if (launch_it != old_launches.cend()) {
                        launch = launch_it->second;
                        break;
                    }
                }
                stream << game_entry(*game, entry.second, base_dir, own, launch) << '\n';
            }
            stream.flush();
            tmp_sctx.discard_all();
            own_sctx.discard_all();

            const QString file_path = folder % QLatin1Char('/') % IMPORT_FILE_NAME;
            QFile file(file_path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                Log::warning(log_tag, LOGMSG("Could not write `%1`").arg(file_path));
                if (result.error.isEmpty())
                    result.error = QStringLiteral("Could not write to %1 (no write access?)").arg(folder);
                continue;
            }
            file.write(text.toUtf8());
            Log::info(log_tag, LOGMSG("Wrote %1 games to `%2`").arg(QString::number(games.size()), file_path));

            result.games += static_cast<int>(games.size());
            result.written_files.append(file_path);
            result.folders.append(folder);
            system_written = true;
        }
        if (system_written)
            result.systems++;
    }
    if (progress)
        progress(todo.size(), todo.size(), QString());
    return result;
}



// -- Emulators: reading and writing launch lines in metadata files ----------------

namespace {

/// One "key: value" entry of a metadata file, with its continuation lines
struct MetaEntry {
    QString key;     ///< lowercase
    int first = 0;   ///< line index
    int last = 0;    ///< line index of its last continuation line
    QStringList values;
};

/// A "collection:" or "game:" block and its entries
struct MetaBlock {
    bool is_collection = false;
    QString name;
    int first = 0;
    int last = 0;  ///< last line of its last entry
    std::vector<MetaEntry> entries;
    int collection_index = -1;  ///< for a game, the collection block above it

    const MetaEntry* entry(const QString& key) const {
        for (const MetaEntry& e : entries) {
            if (e.key == key)
                return &e;
        }
        return nullptr;
    }
};

/// A metadata file, kept as lines so it can be changed without touching
/// anything else in it
struct MetaFile {
    QString path;
    QString dir;
    QStringList lines;
    QString newline = QStringLiteral("\n");
    std::vector<MetaBlock> blocks;

    bool load(const QString& file_path) {
        QFile file(file_path);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        QString text = QString::fromUtf8(file.readAll());
        path = file_path;
        dir = QFileInfo(file_path).absolutePath();
        if (text.contains(QLatin1String("\r\n")))
            newline = QStringLiteral("\r\n");
        text.remove(QLatin1Char('\r'));
        if (text.endsWith(QLatin1Char('\n')))
            text.chop(1);
        lines = text.split(QLatin1Char('\n'));
        parse();
        return true;
    }

    void parse() {
        blocks.clear();
        MetaBlock* block = nullptr;
        MetaEntry* entry = nullptr;
        int current_collection = -1;
        for (int i = 0; i < lines.size(); i++) {
            const QString& line = lines.at(i);
            if (line.trimmed().isEmpty() || line.startsWith(QLatin1Char('#'))) {
                continue;
            }
            const bool continuation = line.at(0).isSpace();
            if (continuation) {
                if (entry) {
                    entry->last = i;
                    entry->values.append(line.trimmed());
                    block->last = i;
                }
                continue;
            }
            const int colon = line.indexOf(QLatin1Char(':'));
            if (colon <= 0) {
                entry = nullptr;
                continue;
            }
            const QString key = line.left(colon).trimmed().toLower();
            const QString value = line.mid(colon + 1).trimmed();

            if (key == QLatin1String("collection") || key == QLatin1String("game")) {
                blocks.emplace_back();
                block = &blocks.back();
                block->is_collection = key == QLatin1String("collection");
                block->name = value;
                block->first = i;
                if (block->is_collection)
                    current_collection = static_cast<int>(blocks.size()) - 1;
                else
                    block->collection_index = current_collection;
            }
            if (!block) {
                entry = nullptr;
                continue;
            }
            block->entries.emplace_back();
            entry = &block->entries.back();
            entry->key = key;
            entry->first = i;
            entry->last = i;
            if (!value.isEmpty())
                entry->values.append(value);
            block->last = i;
        }
    }

    /// The files of a game block, as absolute paths
    QStringList game_files(const MetaBlock& block) const {
        QStringList out;
        const QDir base(dir);
        for (const MetaEntry& e : block.entries) {
            if (e.key != QLatin1String("file") && e.key != QLatin1String("files"))
                continue;
            for (const QString& value : e.values)
                out.append(::clean_abs_path(QFileInfo(base, value)));
        }
        return out;
    }

    /// Sets (or removes, if `command` is empty) the launch entry of a block
    void set_launch(int block_index, const QString& command) {
        const MetaBlock& block = blocks.at(static_cast<size_t>(block_index));
        const MetaEntry* existing = block.entry(QStringLiteral("launch"));
        if (!existing)
            existing = block.entry(QStringLiteral("command"));

        const QString new_line = QStringLiteral("launch: ") % command.simplified();
        if (existing) {
            for (int i = existing->last; i > existing->first; i--)
                lines.removeAt(i);
            if (command.isEmpty())
                lines.removeAt(existing->first);
            else
                lines[existing->first] = new_line;
            parse();
            return;
        }
        if (command.isEmpty())
            return;

        // where it goes: a collection's after its short name; a game's after
        // its description, else before its rating, else at its end
        int insert_at = block.last + 1;
        if (block.is_collection) {
            const MetaEntry* after = block.entry(QStringLiteral("shortname"));
            if (!after)
                after = &block.entries.front();
            insert_at = after->last + 1;
        }
        else if (const MetaEntry* description = block.entry(QStringLiteral("description"))) {
            insert_at = description->last + 1;
        }
        else if (const MetaEntry* rating = block.entry(QStringLiteral("rating"))) {
            insert_at = rating->first;
        }
        lines.insert(insert_at, new_line);
        parse();
    }

    /// Adds a game block with a launch entry to the end of a collection's part of the file
    void add_game(int collection_index, const QString& title, const QString& game_path, const QString& command) {
        int insert_at = lines.size();
        for (size_t i = static_cast<size_t>(collection_index) + 1; i < blocks.size(); i++) {
            if (blocks[i].is_collection) {
                insert_at = blocks[i].first;
                break;
            }
        }
        // after the last block of the collection, before any blank lines
        while (insert_at > 0 && lines.at(insert_at - 1).trimmed().isEmpty())
            insert_at--;

        const QStringList block_lines {
            QString(),
            QStringLiteral("game: ") % title,
            QStringLiteral("file: ") % QDir(dir).relativeFilePath(game_path),
            QStringLiteral("launch: ") % command.simplified(),
        };
        for (int i = 0; i < block_lines.size(); i++)
            lines.insert(insert_at + i, block_lines.at(i));
        parse();
    }

    /// Writes the file back; the first change by Pegasus X keeps a copy of
    /// the original as <file>.bak
    QString save() const {
        const QString backup = path % QStringLiteral(".bak");
        if (!QFileInfo::exists(backup) && QFileInfo(path).fileName() != IMPORT_FILE_NAME)
            QFile::copy(path, backup);

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return QStringLiteral("Could not write to %1 (no write access?)").arg(path);
        file.write((lines.join(newline) + newline).toUtf8());
        return {};
    }
};

bool is_metafile_name(const QString& name)
{
    return name == QLatin1String("metadata.pegasus.txt")
        || name == QLatin1String("metadata.txt")
        || name.endsWith(QLatin1String(".metadata.pegasus.txt"))
        || name.endsWith(QLatin1String(".metadata.txt"));
}

/// The metadata files Pegasus reads: in the game directories and in the
/// "metafiles" folder of the config directory
QStringList all_metafiles()
{
    QStringList dirs;
    {
        const providers::SearchContext probe;  // reads the game directory list
        dirs = probe.root_game_dirs();
    }
    dirs.append(paths::writableConfigDir() % QStringLiteral("/metafiles"));

    QStringList out;
    for (const QString& dir : dirs) {
        for (const QFileInfo& finfo : QDir(dir).entryInfoList(QDir::Files | QDir::Readable, QDir::Name)) {
            if (!is_metafile_name(finfo.fileName()))
                continue;
            const QString path = ::clean_abs_path(finfo);
            if (!out.contains(path))
                out.append(path);
        }
    }
    return out;
}

QString normalize_name(const QString& name)
{
    QString out;
    for (const QChar c : name.toLower()) {
        if (c.isLetterOrNumber())
            out += c;
    }
    return out;
}

/// Systems and emulators, from ES-DE's bundled files (and the custom ones of
/// the ES-DE folder, if there is one), with what's installed on this device
struct EmulatorSetup {
    Context ctx;
    HashMap<QString, EsdeSystem> systems;
    std::vector<QString> order;
    HashMap<QString, QString> name_to_system;  ///< normalized name -> ES-DE system

    EmulatorSetup() {
        init_esde_resources();
        ctx.log_tag = QStringLiteral("Emulators");
        ctx.primary_storage = ::android::primary_storage_path();

        // ES-DE's full lists, plus the emulators of the ES-DE folder's custom
        // systems (which in ES-DE replace a system's whole list; here they're
        // added to it, so no emulator goes missing)
        const QString esde_dir = esde_default_dir();
        {
            bool unused = false;
            load_systems(ctx.log_tag, QStringLiteral(":/esde/es_systems.xml"), systems, order, unused);
        }
        if (!esde_dir.isEmpty()) {
            HashMap<QString, EsdeSystem> custom;
            std::vector<QString> custom_order;
            bool unused = false;
            load_systems(ctx.log_tag, esde_dir % QStringLiteral("/custom_systems/es_systems.xml"), custom, custom_order, unused);
            for (const QString& name : custom_order) {
                const EsdeSystem& custom_sys = custom.at(name);
                auto it = systems.find(name);
                if (it == systems.end()) {
                    systems.emplace(name, custom_sys);
                    order.push_back(name);
                    continue;
                }
                std::vector<EsdeCommand> merged = custom_sys.commands;
                for (const EsdeCommand& cmd : it->second.commands) {
                    const bool known = std::any_of(merged.cbegin(), merged.cend(), [&cmd](const EsdeCommand& c) {
                        return c.label == cmd.label;
                    });
                    if (!known)
                        merged.push_back(cmd);
                }
                it->second.commands = std::move(merged);
            }
        }
        if (!esde_dir.isEmpty())
            load_find_rules(ctx.log_tag, esde_dir % QStringLiteral("/custom_systems/es_find_rules.xml"), ctx.emulators);
        load_find_rules(ctx.log_tag, QStringLiteral(":/esde/es_find_rules.xml"), ctx.emulators);

        // which emulator screens exist on this device, asked in one go
        QStringList components;
        for (const auto& entry : ctx.emulators)
            components.append(entry.second);
        components.removeDuplicates();
        const QStringList resolved = ::android::resolve_components(components);
        for (int i = 0; i < components.size() && i < resolved.size(); i++)
            ctx.resolved_components.emplace(components.at(i), resolved.at(i));

        // a collection is matched to a system by its short name or name:
        // ES-DE's own names first, then the common short names, then full names
        for (const QString& name : order)
            name_to_system.emplace(normalize_name(name), name);
        for (const QString& name : order)
            name_to_system.emplace(normalize_name(pegasus_short_name(name)), name);
        name_to_system.emplace(QStringLiteral("android"), QStringLiteral("androidapps"));
        for (const QString& name : order) {
            const EsdeSystem& sys = systems.at(name);
            if (!sys.fullname.isEmpty())
                name_to_system.emplace(normalize_name(sys.fullname), name);
        }
    }

    QString system_for(const QString& short_name, const QString& name) const {
        auto it = name_to_system.find(normalize_name(short_name));
        if (it == name_to_system.cend() || short_name.isEmpty())
            it = name_to_system.find(normalize_name(name));
        return it != name_to_system.cend() ? it->second : QString();
    }

    std::vector<EmulatorChoice> choices_for(const QString& system_name, const QString& rom_root) const {
        std::vector<EmulatorChoice> out;
        const auto sys_it = systems.find(system_name);
        if (sys_it == systems.cend())
            return out;
        QStringList seen;
        for (const EsdeCommand& cmd : sys_it->second.commands) {
            bool installed = false;
            QString error;
            const QString command = translate_command(cmd.raw, ctx, rom_root, installed, error);
            if (command.isEmpty())
                continue;
            const QString label = cmd.label.isEmpty() ? QStringLiteral("Default") : cmd.label;
            if (seen.contains(label))
                continue;
            seen.append(label);
            EmulatorChoice choice;
            choice.label = label;
            choice.command = command;
            choice.installed = installed;
            choice.retroarch = cmd.raw.startsWith(QLatin1String("%EMULATOR_RETROARCH%"));
            out.push_back(std::move(choice));
        }
        // installed ones first, keeping ES-DE's order otherwise
        std::stable_sort(out.begin(), out.end(), [](const EmulatorChoice& a, const EmulatorChoice& b) {
            return a.installed && !b.installed;
        });
        return out;
    }
};

QString same_command_key(const QString& command)
{
    return command.simplified();
}

/// The files that define a collection, loaded
std::vector<MetaFile> files_defining(const QString& collection_name)
{
    std::vector<MetaFile> out;
    for (const QString& path : all_metafiles()) {
        MetaFile file;
        if (!file.load(path))
            continue;
        const bool defines = std::any_of(file.blocks.cbegin(), file.blocks.cend(), [&collection_name](const MetaBlock& block) {
            return block.is_collection && block.name == collection_name;
        });
        if (defines)
            out.push_back(std::move(file));
    }
    return out;
}
} // namespace


std::vector<EmulatorCollection> emulator_collections()
{
    std::vector<EmulatorCollection> out;
    HashMap<QString, size_t> index_of;  // collection name -> index in `out`

    // the collections, from every metadata file
    for (const QString& path : all_metafiles()) {
        MetaFile file;
        if (!file.load(path))
            continue;
        for (size_t b = 0; b < file.blocks.size(); b++) {
            const MetaBlock& block = file.blocks[b];
            if (block.is_collection) {
                auto it = index_of.find(block.name);
                if (it == index_of.cend()) {
                    it = index_of.emplace(block.name, out.size()).first;
                    out.emplace_back();
                    out.back().name = block.name;
                }
                EmulatorCollection& coll = out[it->second];
                if (!coll.files.contains(path))
                    coll.files.append(path);
                const MetaEntry* short_name = block.entry(QStringLiteral("shortname"));
                if (short_name && coll.short_name.isEmpty() && !short_name->values.isEmpty())
                    coll.short_name = short_name->values.first();
                const MetaEntry* launch = block.entry(QStringLiteral("launch"));
                if (!launch)
                    launch = block.entry(QStringLiteral("command"));
                if (launch && coll.command.isEmpty())
                    coll.command = launch->values.join(QLatin1Char(' '));
            }
            else if (block.collection_index >= 0 && (block.entry(QStringLiteral("launch")) || block.entry(QStringLiteral("command")))) {
                const auto it = index_of.find(file.blocks[static_cast<size_t>(block.collection_index)].name);
                if (it != index_of.cend())
                    out[it->second].game_overrides++;
            }
        }
    }

    // what can launch them
    const EmulatorSetup setup;
    for (EmulatorCollection& coll : out) {
        coll.system = setup.system_for(coll.short_name, coll.name);
        if (coll.system.isEmpty())
            continue;
        const EsdeSystem& sys = setup.systems.at(coll.system);
        coll.system_fullname = sys.fullname.isEmpty() ? sys.name : sys.fullname;

        const QString rom_root = coll.files.isEmpty() ? QString()
            : QFileInfo(QFileInfo(coll.files.first()).absolutePath()).absolutePath();
        coll.options = setup.choices_for(coll.system, rom_root);

        const QString key = same_command_key(coll.command);
        for (const EmulatorChoice& choice : coll.options) {
            if (!key.isEmpty() && same_command_key(choice.command) == key) {
                coll.current_label = choice.label;
                coll.current_installed = choice.installed;
                break;
            }
        }
    }

    // a command written by hand: is its app installed?
    QStringList components;
    for (const EmulatorCollection& coll : out) {
        if (!coll.command.isEmpty() && coll.current_label.isEmpty()) {
            static const QRegularExpression rx_component(QStringLiteral("(?:^|\\s)-n\\s+\"?([^\\s\"]+)"));
            const auto match = rx_component.match(coll.command);
            if (match.hasMatch())
                components.append(match.captured(1));
        }
    }
    if (!components.isEmpty()) {
        const QStringList resolved = ::android::resolve_components(components);
        HashMap<QString, bool> installed;
        for (int i = 0; i < components.size() && i < resolved.size(); i++)
            installed.emplace(components.at(i), !resolved.at(i).isEmpty());
        for (EmulatorCollection& coll : out) {
            if (coll.command.isEmpty() || !coll.current_label.isEmpty())
                continue;
            static const QRegularExpression rx_component(QStringLiteral("(?:^|\\s)-n\\s+\"?([^\\s\"]+)"));
            const auto match = rx_component.match(coll.command);
            const auto it = match.hasMatch() ? installed.find(match.captured(1)) : installed.cend();
            coll.current_installed = it == installed.cend() || it->second;
            if (!coll.current_installed)
                coll.missing_app = match.captured(1).section(QLatin1Char('/'), 0, 0);
        }
    }
    else {
        for (EmulatorCollection& coll : out) {
            if (!coll.command.isEmpty() && coll.current_label.isEmpty())
                coll.current_installed = true;
        }
    }

    std::sort(out.begin(), out.end(), [](const EmulatorCollection& a, const EmulatorCollection& b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return out;
}


std::vector<GameEmulator> game_emulators(const QString& collection_name)
{
    std::vector<GameEmulator> out;
    for (const MetaFile& file : files_defining(collection_name)) {
        for (const MetaBlock& block : file.blocks) {
            if (block.is_collection || block.collection_index < 0)
                continue;
            if (file.blocks[static_cast<size_t>(block.collection_index)].name != collection_name)
                continue;
            const MetaEntry* launch = block.entry(QStringLiteral("launch"));
            if (!launch)
                launch = block.entry(QStringLiteral("command"));
            const QStringList files = file.game_files(block);
            if (!launch || files.isEmpty())
                continue;
            out.push_back({ files.first(), launch->values.join(QLatin1Char(' ')) });
        }
    }
    return out;
}


QString set_collection_emulator(const QString& collection_name, const QString& command)
{
    std::vector<MetaFile> files = files_defining(collection_name);
    if (files.empty())
        return QStringLiteral("No metadata file defines the collection `%1`").arg(collection_name);

    for (MetaFile& file : files) {
        for (size_t b = 0; b < file.blocks.size(); b++) {
            if (file.blocks[b].is_collection && file.blocks[b].name == collection_name)
                file.set_launch(static_cast<int>(b), command);
        }
        const QString error = file.save();
        if (!error.isEmpty())
            return error;
        Log::info(QStringLiteral("Emulators"), LOGMSG("`%1`: launch command written to `%2`").arg(collection_name, file.path));
    }
    return {};
}


QString set_game_emulator(const QString& collection_name, const QString& game_path,
                          const QString& game_title, const QString& command)
{
    const QString wanted = ::clean_abs_path(QFileInfo(game_path));
    std::vector<MetaFile> files = files_defining(collection_name);
    if (files.empty())
        return QStringLiteral("No metadata file defines the collection `%1`").arg(collection_name);

    // the game's entry, in any of the files (the user's own first, then the
    // ES-DE import's, which keeps it when imported again)
    std::stable_sort(files.begin(), files.end(), [](const MetaFile& a, const MetaFile& b) {
        const bool a_import = QFileInfo(a.path).fileName() == IMPORT_FILE_NAME;
        const bool b_import = QFileInfo(b.path).fileName() == IMPORT_FILE_NAME;
        return !a_import && b_import;
    });
    for (MetaFile& file : files) {
        for (size_t b = 0; b < file.blocks.size(); b++) {
            const MetaBlock& block = file.blocks[b];
            if (block.is_collection || !file.game_files(block).contains(wanted))
                continue;
            file.set_launch(static_cast<int>(b), command);
            return file.save();
        }
    }
    if (command.isEmpty())
        return {};  // nothing to remove

    // no entry yet: add one to the first file
    MetaFile& file = files.front();
    for (size_t b = 0; b < file.blocks.size(); b++) {
        if (file.blocks[b].is_collection && file.blocks[b].name == collection_name) {
            file.add_game(static_cast<int>(b), game_title.isEmpty() ? QFileInfo(wanted).completeBaseName() : game_title,
                          wanted, command);
            return file.save();
        }
    }
    return QStringLiteral("No metadata file defines the collection `%1`").arg(collection_name);
}


QString test_emulator_command(const QString& command, const QString& game_path)
{
    const QFileInfo game_file(game_path);
    if (!game_file.exists())
        return QStringLiteral("Game file not found");

    const QString abs_path = game_file.absoluteFilePath();
    QStringList args = ::utils::tokenize_command(command);
    for (QString& arg : args) {
        arg.replace(QLatin1String("{file.path}"), ::pretty_path(game_file))
           .replace(QLatin1String("{file.uri}"), ::android::to_content_uri(abs_path))
           .replace(QLatin1String("{file.documenturi}"), ::android::to_document_uri(abs_path))
           .replace(QLatin1String("{file.name}"), game_file.fileName())
           .replace(QLatin1String("{file.basename}"), game_file.completeBaseName())
           .replace(QLatin1String("{file.dir}"), ::pretty_dir(game_file));
        if (arg.contains(QLatin1String("{file.contents}"))) {
            QFile file(abs_path);
            QString contents;
            if (game_file.isFile() && file.size() < 64 * 1024 && file.open(QIODevice::ReadOnly | QIODevice::Text))
                contents = QString::fromUtf8(file.readAll()).trimmed().section(QLatin1Char('\n'), 0, 0).trimmed();
            arg.replace(QLatin1String("{file.contents}"), contents);
        }
    }
    if (args.isEmpty() || args.first() != QLatin1String("am"))
        return QStringLiteral("Only 'am start' commands can be tested here");
    args.removeFirst();  // "am"
    return ::android::run_am_call(args);
}

} // namespace esde
} // namespace providers
