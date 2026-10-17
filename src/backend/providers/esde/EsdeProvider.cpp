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

#include "AppSettings.h"
#include "Log.h"
#include "PegasusXSettings.h"
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
#include <QXmlStreamReader>


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

using ProviderOptions = HashMap<QString, std::vector<QString>>;

QString find_esde_dir(const ProviderOptions& options)
{
    const auto option_it = options.find(QStringLiteral("installdir"));
    if (option_it != options.cend() && !option_it->second.empty())
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

QStringList find_rom_roots(const ProviderOptions& options, const HashMap<QString, QString>& settings, const QString& primary_storage)
{
    QStringList out;
    const auto add = [&out](QString dir) {
        dir = QDir::cleanPath(dir);
        if (!dir.isEmpty() && QFileInfo(dir).isDir() && !out.contains(dir))
            out.append(dir);
    };

    const auto option_it = options.find(QStringLiteral("romdir"));
    if (option_it != options.cend()) {
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
    QString component = emu_it->second.first();
    emulator_installed = false;
    for (const QString& entry : emu_it->second) {
        if (ctx.installed_packages.contains(entry.section(QLatin1Char('/'), 0, 0))) {
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



// -- Shared by both data sources --------------------------------------------------

/// Loads the system and emulator definitions: the user's custom ES-DE files
/// first (if there's an ES-DE folder), then ES-DE's own bundled ones
void load_config(Context& ctx, const QString& esde_dir, HashMap<QString, EsdeSystem>& systems, std::vector<QString>& order)
{
    bool load_exclusive = false;
    if (!esde_dir.isEmpty())
        load_systems(ctx.log_tag, esde_dir % QStringLiteral("/custom_systems/es_systems.xml"), systems, order, load_exclusive);
    if (!load_exclusive) {
        bool unused = false;
        load_systems(ctx.log_tag, QStringLiteral(":/esde/es_systems.xml"), systems, order, unused);
    }
    if (!esde_dir.isEmpty())
        load_find_rules(ctx.log_tag, esde_dir % QStringLiteral("/custom_systems/es_find_rules.xml"), ctx.emulators);
    load_find_rules(ctx.log_tag, QStringLiteral(":/esde/es_find_rules.xml"), ctx.emulators);
}

Context make_context(const QString& log_tag)
{
    Context ctx;
    ctx.log_tag = log_tag;
    ctx.primary_storage = ::android::primary_storage_path();
    for (const QString& package : ::android::installed_packages())
        ctx.installed_packages.insert(package);
    return ctx;
}

const providers::Provider* find_provider(const char* codename)
{
    for (const auto& provider : AppSettings::providers()) {
        if (provider->codename() == QLatin1String(codename))
            return provider.get();
    }
    return nullptr;
}

/// Scans one system's folders for games and sets their launch commands
size_t scan_system(providers::SearchContext& sctx, const Context& ctx, const EsdeSystem& sys,
                   const std::vector<std::pair<QString, QString>>& dirs,  // (system dir, ROMs folder)
                   const GamelistInfo& gamelist, const QString& system_label, bool show_hidden,
                   bool override_existing,
                   HashMap<QString, model::Game*>& extless_path_to_game)
{
    const QStringList extensions = parse_extensions(sys.extensions);

    model::Collection& collection = *sctx.get_or_create_collection(sys.fullname.isEmpty() ? sys.name : sys.fullname);
    collection.setShortName(sys.name);

    size_t found_games = 0;
    for (const auto& dir_entry : dirs) {
        const QString& system_dir = dir_entry.first;
        const QString& rom_root = dir_entry.second;

        const QString default_cmd = pick_command(sys, system_label, ctx, rom_root);
        if (default_cmd.isEmpty())
            Log::warning(ctx.log_tag, LOGMSG("`%1`: none of its emulators can be launched from Pegasus").arg(sys.name));
        if (collection.commonLaunchCmd().isEmpty())
            collection.setCommonLaunchCmd(default_cmd);

        constexpr auto entry_filters = QDir::Files | QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot;
        constexpr auto entry_flags = QDirIterator::FollowSymlinks | QDirIterator::Subdirectories;
        QDirIterator file_it(system_dir, entry_filters, entry_flags);
        while (file_it.hasNext()) {
            file_it.next();
            const QFileInfo finfo = file_it.fileInfo();
            if (!has_extension(finfo.fileName(), extensions))
                continue;

            QString path = ::clean_abs_path(finfo);
            if (!show_hidden && gamelist.hidden_paths.contains(path))
                continue;

            model::Game* game_ptr = sctx.game_by_filepath(path);
            const bool is_new = !game_ptr;
            if (is_new) {
                game_ptr = sctx.create_game_for(collection);
                sctx.game_add_filepath(*game_ptr, path);
            }
            sctx.game_add_to(*game_ptr, collection);

            // A game-specific emulator picked in ES-DE, or this folder's default.
            // Games already found by another source (eg. a Pegasus metadata file
            // with its own launch command) keep theirs, unless an emulator was
            // picked for this system in Pegasus X
            const bool may_set = is_new || override_existing || game_ptr->launchCmd().isEmpty();
            const auto alt_it = gamelist.game_alt_labels.find(path);
            if (alt_it != gamelist.game_alt_labels.cend() && may_set) {
                const QString cmd = pick_command(sys, alt_it->second, ctx, rom_root);
                if (!cmd.isEmpty())
                    game_ptr->setLaunchCmd(cmd);
            }
            else if (is_new ? default_cmd != collection.commonLaunchCmd() : may_set) {
                game_ptr->setLaunchCmd(default_cmd);
            }

            QString extless = path;
            if (!finfo.isDir())
                extless = ::clean_abs_dir(finfo) % QLatin1Char('/') % finfo.completeBaseName();
            extless_path_to_game.emplace(std::move(extless), game_ptr);
            found_games++;
        }
    }
    return found_games;
}

/// The ES-DE system dirs (%ROMPATH% expanded for every ROMs folder that has it)
std::vector<std::pair<QString, QString>> esde_system_dirs(const EsdeSystem& sys, const QStringList& rom_roots)
{
    std::vector<std::pair<QString, QString>> dirs;
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
    return dirs;
}


// -- Pegasus X library: recognizing system folders by name --------------------------

QString normalize_name(const QString& name)
{
    QString out;
    for (const QChar c : name.toLower()) {
        if (c.isLetterOrNumber())
            out += c;
    }
    return out;
}

/// Folder name (normalized) -> ES-DE system name
HashMap<QString, QString> build_aliases(const HashMap<QString, EsdeSystem>& systems, const std::vector<QString>& order)
{
    HashMap<QString, QString> out;

    // 1. ES-DE's own system names (eg. "sg-1000" also as "sg1000")
    for (const QString& name : order)
        out.emplace(normalize_name(name), name);

    // 2. common names people use for their folders
    static const std::vector<std::pair<const char*, const char*>> COMMON {
        {"supernintendo", "snes"}, {"supernes", "snes"}, {"nintendosnes", "snes"},
        {"superfamicom", "sfc"},
        {"nintendoentertainmentsystem", "nes"}, {"nintendones", "nes"}, {"nintendo", "nes"},
        {"nintendo64", "n64"},
        {"gamecube", "gc"}, {"ngc", "gc"}, {"nintendogamecube", "gc"},
        {"nintendowii", "wii"}, {"nintendowiiu", "wiiu"},
        {"nintendoswitch", "switch"}, {"ns", "switch"},
        {"gameboy", "gb"}, {"nintendogameboy", "gb"},
        {"gameboycolor", "gbc"}, {"nintendogameboycolor", "gbc"},
        {"gameboyadvance", "gba"}, {"nintendogameboyadvance", "gba"},
        {"ds", "nds"}, {"nintendods", "nds"},
        {"3ds", "n3ds"}, {"nintendo3ds", "n3ds"},
        {"ps1", "psx"}, {"psone", "psx"}, {"playstation", "psx"}, {"playstation1", "psx"}, {"sonyplaystation", "psx"},
        {"playstation2", "ps2"}, {"sonyplaystation2", "ps2"},
        {"playstation3", "ps3"}, {"sonyplaystation3", "ps3"},
        {"playstation4", "ps4"},
        {"playstationportable", "psp"}, {"sonypsp", "psp"},
        {"vita", "psvita"}, {"playstationvita", "psvita"},
        {"segagenesis", "genesis"},
        {"md", "megadrive"}, {"segamegadrive", "megadrive"},
        {"sms", "mastersystem"}, {"segamastersystem", "mastersystem"},
        {"gg", "gamegear"}, {"segagamegear", "gamegear"},
        {"segamegacd", "megacd"},
        {"32x", "sega32x"},
        {"segasaturn", "saturn"},
        {"dc", "dreamcast"}, {"segadreamcast", "dreamcast"},
        {"turbografx", "tg16"}, {"turbografx16", "tg16"}, {"pce", "pcengine"},
        {"turbografxcd", "tg-cd"}, {"pcecd", "pcenginecd"},
        {"neogeopocket", "ngp"}, {"neogeopocketcolor", "ngpc"},
        {"lynx", "atarilynx"}, {"jaguar", "atarijaguar"},
        {"2600", "atari2600"}, {"5200", "atari5200"}, {"7800", "atari7800"},
        {"ws", "wonderswan"}, {"wsc", "wonderswancolor"},
        {"msdos", "dos"}, {"commodore64", "c64"}, {"spectrum", "zxspectrum"},
        {"microsoftxbox", "xbox"},
    };
    for (const auto& entry : COMMON) {
        const QString target = QLatin1String(entry.second);
        if (systems.count(target))
            out.emplace(QLatin1String(entry.first), target);
    }

    // 3. ES-DE's full names, eg. "Sony PlayStation", and the part in
    //    parentheses of names like "Nintendo SNES (Super Nintendo)"
    static const QRegularExpression rx_parens(QStringLiteral("^(.*?)\\s*\\((.*)\\)\\s*$"));
    for (const QString& name : order) {
        const QString& fullname = systems.at(name).fullname;
        const auto match = rx_parens.match(fullname);
        if (match.hasMatch()) {
            out.emplace(normalize_name(match.captured(1)), name);
            out.emplace(normalize_name(match.captured(2)), name);
        }
        out.emplace(normalize_name(fullname), name);
    }
    return out;
}

/// ES-DE system name -> its folders in the library (system dir, ROMs folder)
HashMap<QString, std::vector<std::pair<QString, QString>>> find_library_systems(
    const QStringList& library_dirs, const HashMap<QString, QString>& aliases)
{
    HashMap<QString, std::vector<std::pair<QString, QString>>> out;
    const auto resolve = [&aliases](const QString& folder_name) {
        const auto it = aliases.find(normalize_name(folder_name));
        return it != aliases.cend() ? it->second : QString();
    };

    for (const QString& dir_raw : library_dirs) {
        const QFileInfo dir_info(dir_raw);
        if (!dir_info.isDir())
            continue;
        const QString dir = ::clean_abs_path(dir_info);

        // the picked folder can be a single system folder...
        const QString own_system = resolve(dir_info.fileName());
        if (!own_system.isEmpty()) {
            out[own_system].emplace_back(dir, ::clean_abs_dir(dir_info));
            continue;
        }
        // ...or a ROMs folder with system folders in it
        for (const QString& sub : QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString system = resolve(sub);
            if (!system.isEmpty())
                out[system].emplace_back(QDir::cleanPath(dir % QLatin1Char('/') % sub), dir);
        }
    }
    return out;
}

/// Finds a folder by name, ignoring capitals
QString find_subdir(const QString& parent, const QStringList& names)
{
    if (parent.isEmpty())
        return {};
    const QStringList subdirs = QDir(parent).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : names) {
        for (const QString& sub : subdirs) {
            if (sub.compare(name, Qt::CaseInsensitive) == 0)
                return parent % QLatin1Char('/') % sub;
        }
    }
    return {};
}

} // namespace


namespace providers {
namespace esde {

/// Pegasus metadata files (metadata.pegasus.txt, metadata.txt and the
/// *.metadata.pegasus.txt / *.metadata.txt variants) in the library folders
/// and the folders right below them
static QStringList find_library_metafiles(const QStringList& library_dirs)
{
    const auto is_metafile = [](const QString& name) {
        return name == QLatin1String("metadata.pegasus.txt")
            || name == QLatin1String("metadata.txt")
            || name.endsWith(QLatin1String(".metadata.pegasus.txt"))
            || name.endsWith(QLatin1String(".metadata.txt"));
    };
    const auto add_from = [&is_metafile](const QString& dir, QStringList& out) {
        for (const QFileInfo& finfo : QDir(dir).entryInfoList(QDir::Files | QDir::Readable, QDir::Name)) {
            const QString path = ::clean_abs_path(finfo);
            if (is_metafile(finfo.fileName()) && !out.contains(path))
                out.append(path);
        }
    };

    QStringList out;
    for (const QString& dir : library_dirs) {
        add_from(dir, out);
        for (const QString& sub : QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            add_from(dir % QLatin1Char('/') % sub, out);
    }
    return out;
}

/// Copies the details read from a metadata file to the game Pegasus X found
static void copy_game_details(const model::Game& src, model::Game& dst)
{
    if (!src.title().isEmpty())
        dst.setTitle(src.title());
    if (!src.sortBy().isEmpty())
        dst.setSortBy(src.sortBy());
    if (!src.summary().isEmpty())
        dst.setSummary(src.summary());
    if (!src.description().isEmpty())
        dst.setDescription(src.description());
    if (!src.developerListConst().isEmpty())
        dst.developerList() = src.developerListConst();
    if (!src.publisherListConst().isEmpty())
        dst.publisherList() = src.publisherListConst();
    if (!src.genreListConst().isEmpty())
        dst.genreList() = src.genreListConst();
    if (!src.tagListConst().isEmpty())
        dst.tagList() = src.tagListConst();
    if (src.playerCount() > 1)
        dst.setPlayerCount(src.playerCount());
    if (src.rating() > 0.f)
        dst.setRating(src.rating());
    if (src.releaseDate().isValid())
        dst.setReleaseDate(src.releaseDate());

    // NOTE: Assets only has a public getter per type
    const model::Assets& a = src.assets();
    const std::vector<std::pair<AssetType, const QStringList*>> asset_lists {
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
    for (const auto& entry : asset_lists) {
        for (const QString& uri : *entry.second)
            dst.assetsMut().add_uri(entry.first, uri);
    }

    // x- fields, eg. x-manual
    for (auto it = src.extraMap().cbegin(); it != src.extraMap().cend(); ++it)
        dst.extraMapMut().insert(it.key(), it.value());
}

/// Reads Pegasus metadata files only for their game details (names,
/// descriptions, ratings, art, x- fields...), applied to the games Pegasus X
/// found by file path. Their collections and launch commands aren't used, so
/// the games aren't listed twice and keep the emulator picked in Pegasus X.
static size_t apply_pegasus_details(const QStringList& metafiles, providers::SearchContext& sctx, const QString& log_tag)
{
    if (metafiles.isEmpty())
        return 0;

    providers::SearchContext tmp_sctx(QStringList {});
    const providers::pegasus::Metadata metahelper(log_tag);
    for (const QString& path : metafiles) {
        Log::info(log_tag, LOGMSG("Reading game details from `%1`").arg(::pretty_path(path)));
        metahelper.apply_metafile(path, tmp_sctx);  // its file filters aren't needed
    }

    size_t updated = 0;
    // TODO: C++17
    for (const auto& entry : tmp_sctx.current_filepath_to_entry_map()) {
        model::Game* const game = sctx.game_by_filepath(entry.first);
        if (!game)
            continue;
        copy_game_details(*entry.second->parentGame(), *game);
        updated++;
    }

    tmp_sctx.discard_all();
    return updated;
}

/// ES-DE's settings, custom systems and gamelists: when they change (eg. after
/// scraping in ES-DE), the game list is scanned again
static QStringList esde_cache_files(const QString& esde_dir)
{
    QStringList out;
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

/// The ES-DE folder, if there is one (also used when the ES-DE data source is off)
static QString any_esde_dir()
{
    const providers::Provider* const esde_provider = find_provider("esde");
    return find_esde_dir(esde_provider ? esde_provider->options() : ProviderOptions());
}

// -- ES-DE ----------------------------------------------------------------------------

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
    // after scraping), or the emulator choices made in Pegasus X
    return QStringList { pegasusx::settingsFilePath() } + esde_cache_files(find_esde_dir(options()));
}

Provider& EsdeProvider::run(SearchContext& sctx)
{
    const QString esde_dir = find_esde_dir(options());
    if (esde_dir.isEmpty()) {
        Log::info(display_name(), LOGMSG("No ES-DE folder found"));
        return *this;
    }
    Log::info(display_name(), LOGMSG("Using the ES-DE folder `%1`").arg(esde_dir));

    Context ctx = make_context(display_name());

    const HashMap<QString, QString> settings = read_esde_settings(esde_dir);
    const auto setting = [&settings](const char* key) {
        const auto it = settings.find(QLatin1String(key));
        return it != settings.cend() ? it->second : QString();
    };

    const QStringList rom_roots = find_rom_roots(options(), settings, ctx.primary_storage);
    if (rom_roots.isEmpty())
        Log::warning(display_name(), LOGMSG("No ROMs folder found, only systems with an absolute path will work"));
    for (const QString& root : rom_roots)
        Log::info(display_name(), LOGMSG("ROMs folder: `%1`").arg(root));

    QString media_root = setting("MediaDirectory");
    if (media_root.isEmpty())
        media_root = esde_dir % QStringLiteral("/downloaded_media");
    const bool show_hidden = setting("ShowHiddenGames") != QLatin1String("false");

    HashMap<QString, EsdeSystem> systems;
    std::vector<QString> system_order;
    load_config(ctx, esde_dir, systems, system_order);
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

        const std::vector<std::pair<QString, QString>> dirs = esde_system_dirs(sys, rom_roots);
        if (dirs.empty())
            continue;

        QStringList system_dirs;
        for (const auto& entry : dirs)
            system_dirs.append(entry.first);

        const GamelistInfo gamelist = read_gamelist(esde_dir, sys.name, system_dirs);

        // an emulator chosen in Pegasus X wins over the one chosen in ES-DE
        QString system_label = pegasusx::systemEmulator(sys.name);
        if (system_label.isEmpty())
            system_label = gamelist.system_alt_label;

        const bool picked_in_pegasusx = !pegasusx::systemEmulator(sys.name).isEmpty();
        HashMap<QString, model::Game*> extless_path_to_game;
        const size_t found_games = scan_system(sctx, ctx, sys, dirs, gamelist, system_label, show_hidden,
                                               picked_in_pegasusx, extless_path_to_game);
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

        // media from ES-DE/downloaded_media/<system>/<type>/
        for (const QString& system_dir : system_dirs)
            find_media(media_root % QLatin1Char('/') % sys.name, system_dir, extless_path_to_game, found_assets);
    }

    Log::info(display_name(), LOGMSG("%1 games and %2 media files found")
        .arg(QString::number(found_games_total), QString::number(found_assets)));
    return *this;
}


// -- Pegasus X library ----------------------------------------------------------------

LibraryProvider::LibraryProvider(QObject* parent)
    : Provider(QLatin1String("pegasusx_library"), QStringLiteral("Pegasus X library"), PROVIDER_FLAG_CACHEABLE, parent)
{
    init_esde_resources();
}

QStringList LibraryProvider::cacheInputFiles() const
{
    const QStringList library_dirs = pegasusx::libraryRomDirs();
    if (library_dirs.isEmpty())
        return {};
    return QStringList { pegasusx::settingsFilePath() }
        + esde_cache_files(any_esde_dir())
        + find_library_metafiles(library_dirs);
}

Provider& LibraryProvider::run(SearchContext& sctx)
{
    const QStringList library_dirs = pegasusx::libraryRomDirs();
    if (library_dirs.isEmpty())
        return *this;

    Context ctx = make_context(display_name());

    // If there's an ES-DE folder, its scraped metadata (gamelists), media and
    // emulator choices are used too, even when the ES-DE data source is off
    const QString esde_dir = any_esde_dir();
    QString esde_media_root;
    bool show_hidden = true;
    if (!esde_dir.isEmpty()) {
        Log::info(display_name(), LOGMSG("Using metadata and media from the ES-DE folder `%1`").arg(esde_dir));
        const HashMap<QString, QString> esde_settings = read_esde_settings(esde_dir);
        const auto media_it = esde_settings.find(QStringLiteral("MediaDirectory"));
        esde_media_root = (media_it != esde_settings.cend() && !media_it->second.isEmpty())
            ? media_it->second
            : esde_dir % QStringLiteral("/downloaded_media");
        const auto hidden_it = esde_settings.find(QStringLiteral("ShowHiddenGames"));
        show_hidden = hidden_it == esde_settings.cend() || hidden_it->second != QLatin1String("false");
    }

    HashMap<QString, EsdeSystem> systems;
    std::vector<QString> system_order;
    load_config(ctx, esde_dir, systems, system_order);

    const HashMap<QString, QString> aliases = build_aliases(systems, system_order);
    const auto library_systems = find_library_systems(library_dirs, aliases);
    Log::info(display_name(), LOGMSG("%1 system folders recognized").arg(QString::number(library_systems.size())));

    const QString media_root = pegasusx::libraryMediaDir();
    const es2::Metadata metahelper(display_name(),
        esde_dir.isEmpty() ? std::vector<QString>() : std::vector<QString> { esde_dir });

    size_t found_games_total = 0;
    size_t found_assets = 0;
    const float progress_step = library_systems.empty() ? 1.f : 1.f / library_systems.size();
    float progress = 0.f;

    for (const QString& system_name : system_order) {
        const auto lib_it = library_systems.find(system_name);
        if (lib_it == library_systems.cend())
            continue;

        progress += progress_step;
        emit progressChanged(progress);

        const EsdeSystem& sys = systems.at(system_name);
        const std::vector<std::pair<QString, QString>>& dirs = lib_it->second;

        QStringList system_dirs;
        for (const auto& entry : dirs)
            system_dirs.append(entry.first);
        const GamelistInfo gamelist = esde_dir.isEmpty()
            ? GamelistInfo()
            : read_gamelist(esde_dir, sys.name, system_dirs);

        QString system_label = pegasusx::systemEmulator(sys.name);
        const bool picked_in_pegasusx = !system_label.isEmpty();
        if (system_label.isEmpty())
            system_label = gamelist.system_alt_label;

        HashMap<QString, model::Game*> extless_path_to_game;
        const size_t found_games = scan_system(sctx, ctx, sys, dirs, gamelist, system_label, show_hidden,
                                               picked_in_pegasusx, extless_path_to_game);
        if (found_games == 0)
            continue;
        found_games_total += found_games;
        Log::info(display_name(), LOGMSG("System `%1` provided %2 games").arg(sys.name, QString::number(found_games)));

        for (const auto& dir_entry : dirs) {
            const QString& system_dir = dir_entry.first;

            // a gamelist.xml in the system folder, or else ES-DE's for this system
            const es2::SystemEntry sysentry {
                sys.fullname, sys.name, system_dir, sys.extensions, sys.platform, QString(),
            };
            metahelper.find_metadata_for(sysentry, sctx);

            // media in <media folder>/<system folder or system name>/<type>/
            const QString folder_name = QFileInfo(system_dir).fileName();
            const QString system_media = find_subdir(media_root, { folder_name, sys.name });
            if (!system_media.isEmpty())
                find_media(system_media, system_dir, extless_path_to_game, found_assets);

            // then ES-DE's downloaded media for anything still missing
            if (!esde_media_root.isEmpty()) {
                const QString esde_media = find_subdir(esde_media_root, { sys.name });
                if (!esde_media.isEmpty() && QDir(esde_media) != QDir(system_media))
                    find_media(esde_media, system_dir, extless_path_to_game, found_assets);
            }

            // and the usual <system folder>/media/ for the Skraper-style reader
            sctx.pegasus_add_game_dir(system_dir);
        }
    }

    // details from Pegasus metadata files in the library folders, if any
    const size_t detailed = apply_pegasus_details(find_library_metafiles(library_dirs), sctx, display_name());
    if (detailed > 0)
        Log::info(display_name(), LOGMSG("%1 games got details from Pegasus metadata files").arg(QString::number(detailed)));

    Log::info(display_name(), LOGMSG("%1 games and %2 media files found")
        .arg(QString::number(found_games_total), QString::number(found_assets)));
    return *this;
}


// -- Emulator choices for the settings screen ---------------------------------------

std::vector<SystemEmulators> emulator_choices()
{
    init_esde_resources();
    Context ctx = make_context(QStringLiteral("Pegasus X"));

    // the ES-DE folder only counts if that data source is on
    QString esde_dir;
    const providers::Provider* const esde_provider = find_provider("esde");
    if (esde_provider && esde_provider->enabled())
        esde_dir = find_esde_dir(esde_provider->options());

    HashMap<QString, EsdeSystem> systems;
    std::vector<QString> order;
    load_config(ctx, esde_dir, systems, order);

    // the systems that have a folder, in the library or in ES-DE's ROMs folders
    QSet<QString> present;
    const auto library_systems = find_library_systems(pegasusx::libraryRomDirs(), build_aliases(systems, order));
    for (const auto& entry : library_systems)
        present.insert(entry.first);
    if (!esde_dir.isEmpty()) {
        const QStringList rom_roots = find_rom_roots(esde_provider->options(), read_esde_settings(esde_dir), ctx.primary_storage);
        for (const QString& name : order) {
            if (!esde_system_dirs(systems.at(name), rom_roots).empty())
                present.insert(name);
        }
    }

    std::vector<SystemEmulators> out;
    for (const QString& name : order) {
        if (!present.contains(name))
            continue;

        const EsdeSystem& sys = systems.at(name);
        SystemEmulators entry;
        entry.name = name;
        entry.fullname = sys.fullname.isEmpty() ? name : sys.fullname;
        entry.current = pegasusx::systemEmulator(name);

        QString first_usable;
        for (const EsdeCommand& cmd : sys.commands) {
            bool installed = false;
            QString error;
            if (translate_command(cmd.raw, ctx, QString(), installed, error).isEmpty())
                continue;

            const QString label = cmd.label.isEmpty() ? QStringLiteral("Default") : cmd.label;
            entry.options.push_back({ label, installed });
            if (first_usable.isEmpty())
                first_usable = label;
            if (installed && entry.default_label.isEmpty())
                entry.default_label = label;
        }
        if (entry.default_label.isEmpty())
            entry.default_label = first_usable;
        if (!entry.options.empty())
            out.push_back(std::move(entry));
    }
    return out;
}

} // namespace esde
} // namespace providers
