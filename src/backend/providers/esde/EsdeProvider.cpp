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
#include "Scraper.h"
#include <QCryptographicHash>
#include <QSaveFile>
#include "providers/GameDirectory.h"
#include "providers/SearchContext.h"
#include "providers/es2/Es2Metadata.h"
#include "providers/es2/Es2Systems.h"
#include "providers/pegasus_metadata/PegasusFilter.h"
#include "providers/pegasus_metadata/PegasusMetadata.h"
#include "providers/skraper/SkraperAssetsProvider.h"
#include "Paths.h"
#include "PegasusAssets.h"
#include "utils/CommandTokenizer.h"
#include "utils/GameFileText.h"
#include "utils/HashMap.h"
#include "utils/PathCase.h"
#include "utils/PathTools.h"

#include <QDir>
#include <memory>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QStringBuilder>
#include <QVariantMap>
#include <QTextStream>
#include <QUrl>
#include <QXmlStreamReader>
#include <algorithm>

#ifdef Q_OS_UNIX
#include <dirent.h>
#endif


// The bundled ES-DE configuration lives in a resource file of this static library
static void init_esde_resources() { Q_INIT_RESOURCE(esde); }

/// ES-DE's emulator list file: the one downloaded by "Update emulator list"
/// (in the config folder) if there is one, else the copy built into the app
static QString esde_list_file(const QString& name)
{
    const QString downloaded = paths::writableConfigDir() % QStringLiteral("/esde-lists/") % name;
    if (QFileInfo::exists(downloaded))
        return downloaded;
    return QStringLiteral(":/esde/") % name;
}


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
                          bool& emulator_installed, QString& error,
                          const QString& force_component = QString())  // a specific build to use
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
    if (!force_component.isEmpty()) {
        component = force_component;
        emulator_installed = true;
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
        // %INJECT%=<file>: the text inside a file. %ROM% is the game file
        // itself (eg. a Steam app ID in a .steam file); %BASENAME%.<ext> is
        // the file of that name next to the game, or inside a game folder
        // (eg. a PS Vita title ID in a .psvita file)
        value.replace(QLatin1String("%INJECT%=%ROM%"), QLatin1String("{file.contents}"));
        static const QRegularExpression rx_inject(QStringLiteral("%INJECT%=%BASENAME%\\.(\\w+)"));
        value.replace(rx_inject, QStringLiteral("{file.inject:\\1}"));
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

void find_media(const QString& wanted_media_dir, const QString& system_dir,
                const HashMap<QString, model::Game*>& extless_path_to_game, size_t& found_assets)
{
    // Pegasus X: "Downloaded_Media/PSX" too on case-sensitive storage
    const QString system_media_dir = pathcase::resolve(wanted_media_dir);
    if (system_media_dir.isEmpty() || !QFileInfo(system_media_dir).isDir())
        return;

    HashMap<QString, QString> existing_dirs;  // lowercase -> actual folder name
    const QStringList subdir_names = QDir(system_media_dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : subdir_names)
        existing_dirs.emplace(name.toLower(), name);

    // the games by their path in lower case: covers/game.png is for Game.iso too
    HashMap<QString, model::Game*> games_lower;
    for (const auto& pair : extless_path_to_game)
        games_lower.emplace(pair.first.toLower(), pair.second);

    constexpr auto DIR_FILTERS = QDir::Files | QDir::Readable | QDir::NoDotAndDotDot;
    constexpr auto DIR_FLAGS = QDirIterator::Subdirectories | QDirIterator::FollowSymlinks;

    // TODO: C++17
    // the kind folders spelled any way ("3D Boxes", "box3d"), each read once
    QSet<QString> claimed;
    for (const auto& asset_entry : providers::skraper::asset_dirs()) {
        for (const QString& wanted : asset_entry.second) {
          const QString wanted_plain = pegasus_assets::plain_name(wanted);
          for (const QString& folder : subdir_names) {
            if (claimed.contains(folder)
                    || !pegasus_assets::same_plain_name(pegasus_assets::plain_name(folder), wanted_plain))
                continue;
            claimed.insert(folder);

            const QDir type_dir(system_media_dir % QLatin1Char('/') % folder);
            QDirIterator file_it(type_dir.path(), DIR_FILTERS, DIR_FLAGS);
            while (file_it.hasNext()) {
                file_it.next();
                const QFileInfo finfo = file_it.fileInfo();

                // the media tree mirrors the ROM folder, eg. covers/Multidisk/Game.png
                const QString rel_dir = type_dir.relativeFilePath(finfo.absolutePath());
                const QString game_key = QDir::cleanPath(system_dir % QLatin1Char('/') % rel_dir
                                                         % QLatin1Char('/') % finfo.completeBaseName()).toLower();
                const auto game_it = games_lower.find(game_key);
                if (game_it == games_lower.cend())
                    continue;

                game_it->second->assetsMut().add_file(asset_entry.first, file_it.filePath());
                found_assets++;
            }
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
                                                     % QLatin1Char('/') % finfo.completeBaseName()).toLower();
            const auto game_it = games_lower.find(game_key);
            if (game_it == games_lower.cend())
                continue;

            QVariantMap& extra = game_it->second->extraMapMut();
            if (!extra.contains(QStringLiteral("manual"))) {
                extra.insert(QStringLiteral("manual"), QStringList { finfo.absoluteFilePath() });
                found_assets++;
            }
        }
    }
}

/// Pegasus' own layout: media/<game>/<kind>.<ext> (boxFront.png, logo.png,
/// video.mp4, manual.pdf), the folder named after the game's file or title
void find_per_game_media(const QString& wanted_media_dir, const QString& system_dir,
                         const HashMap<QString, model::Game*>& extless_path_to_game, size_t& found_assets)
{
    const QString media_dir = pathcase::resolve(wanted_media_dir);
    if (media_dir.isEmpty() || !QFileInfo(media_dir).isDir())
        return;
    HashMap<QString, model::Game*> games_lower;
    for (const auto& pair : extless_path_to_game)
        games_lower.emplace(pair.first.toLower(), pair.second);
    for (const QFileInfo& sub : QDir(media_dir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto game_it = games_lower.find((QDir::cleanPath(system_dir) % QLatin1Char('/') % sub.fileName()).toLower());
        if (game_it == games_lower.cend())
            continue;
        for (const QFileInfo& finfo : QDir(sub.absoluteFilePath()).entryInfoList(QDir::Files | QDir::Readable)) {
            const QString ext = finfo.suffix().toLower();
            if (ext == QLatin1String("pdf")) {
                QVariantMap& extra = game_it->second->extraMapMut();
                if (!extra.contains(QStringLiteral("manual")))
                    extra.insert(QStringLiteral("manual"), QStringList { finfo.absoluteFilePath() });
                continue;
            }
            const AssetType type = pegasus_assets::str_to_type(finfo.completeBaseName());
            if (type == AssetType::UNKNOWN)
                continue;
            game_it->second->assetsMut().add_file(type, finfo.absoluteFilePath());
            found_assets++;
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




// -- ES-DE import -----------------------------------------------------------------

namespace {
const QString IMPORT_FILE_NAME = QStringLiteral("esde-import.metadata.pegasus.txt");
// the import files' format: made again at start when older (2: every picture
// of a kind, so 3D boxes are kept beside the covers)
// 3: the ROM folder's own media folder read too
const QString IMPORT_FORMAT_LINE = QStringLiteral("# pegasusx-import-format: 4");

// Pegasus X: imports are written into the Pegasus X folder, never into the
// game folders: metadata/imports/<system>.metadata.pegasus.txt defines the
// games not in your own metadata (that folder is a game directory), and
// metadata/imports-fill/<system>.metadata.pegasus.txt has ES-DE's data for
// games your own files define, used on top of them as the ES-DE import
// setting says (see apply_import_fill in Overrides.cpp)
QString safe_file_stem(const QString& name)
{
    QString safe;
    for (const QChar c : name) {
        if (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_'))
            safe += c;
        else if (c == QLatin1Char(' '))
            safe += QLatin1Char('_');
    }
    return safe.isEmpty() ? QStringLiteral("other") : safe;
}
QString imports_dir() { return paths::writableConfigDir() % QStringLiteral("/metadata/imports"); }
QString imports_fill_dir() { return paths::writableConfigDir() % QStringLiteral("/metadata/imports-fill"); }
// a folder the same however it's reached: /sdcard/... and
// /storage/self/primary/... are /storage/emulated/0/...
QString folder_key(const QString& folder)
{
    QString path = QDir::cleanPath(folder);
    for (const char* alias : { "/sdcard", "/storage/self/primary", "/mnt/sdcard" }) {
        const QString prefix = QLatin1String(alias);
        if (path == prefix || path.startsWith(prefix % QLatin1Char('/'))) {
            path = QStringLiteral("/storage/emulated/0") % path.mid(prefix.size());
            break;
        }
    }
    return path;
}
// one file per system folder (a system can have folders on several storages),
// named by a SHA-1 of the folder: the same name on every start and Qt version
// (it used to be qHash, which isn't)
QString import_file_for(const QString& system, const QString& folder, bool fill)
{
    const QString stem = QDir(folder).dirName().isEmpty() ? system : system % QLatin1Char('-')
        % QString::fromLatin1(QCryptographicHash::hash(folder_key(folder).toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
    return (fill ? imports_fill_dir() : imports_dir()) % QLatin1Char('/') % safe_file_stem(stem) % QStringLiteral(".metadata.pegasus.txt");
}
// the import files made from a game folder (by the "# folder:" line in them)
QStringList import_files_of(const QString& folder)
{
    QStringList out;
    const QString prefix = QStringLiteral("# folder: ");
    const QString wanted = folder_key(folder);
    QStringList candidates = providers::gamedir::metafiles();
    for (const QString& dir : { imports_dir(), imports_fill_dir() }) {
        for (const QFileInfo& finfo : QDir(dir).entryInfoList({ QStringLiteral("*.metadata.pegasus.txt") }, QDir::Files))
            candidates.append(finfo.absoluteFilePath());
    }
    for (const QString& path : candidates) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        // the folder lines are among the leading comments
        for (int i = 0; i < 40 && !file.atEnd(); i++) {
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (!line.startsWith(QLatin1Char('#')) && !line.isEmpty())
                break;
            if (line.startsWith(prefix) && folder_key(line.mid(prefix.size())) == wanted) {
                out.append(path);
                break;
            }
        }
    }
    // an older Pegasus X wrote into the game folder
    if (QFileInfo::exists(folder % QLatin1Char('/') % IMPORT_FILE_NAME))
        out.append(folder % QLatin1Char('/') % IMPORT_FILE_NAME);
    return out;
}

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
        load_systems(log_tag, esde_list_file(QStringLiteral("es_systems.xml")), out.systems, out.order, unused);
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
    // else the name the last import used (renamed in Settings > Library, maybe)
    for (const QString& file : import_files_of(dir)) {
        const auto match = rx.match(read_file(file));
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
void migrate_in_folder_edits(const QString& old_path, const QString& collection_name);
QStringList aliases_of(const QString& key);
QString edits_file_path(const QString& collection_name);

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

/// The name the metadata editor writes a key with ("genres" -> "genre")
QString canonical_key(const QString& key)
{
    const QString k = key.trimmed().toLower();
    static const QStringList sort_keys {
        QStringLiteral("sorttitle"), QStringLiteral("sortname"), QStringLiteral("sort_title"),
        QStringLiteral("sort_name"), QStringLiteral("sort-title"), QStringLiteral("sort-name"),
        QStringLiteral("sortby"), QStringLiteral("sort_by"), QStringLiteral("sort-by"),
    };
    if (sort_keys.contains(k)) return QStringLiteral("sort-by");
    if (k == QLatin1String("developers")) return QStringLiteral("developer");
    if (k == QLatin1String("publishers")) return QStringLiteral("publisher");
    if (k == QLatin1String("genres")) return QStringLiteral("genre");
    if (k == QLatin1String("tags")) return QStringLiteral("tag");
    return k;
}

const std::vector<AssetType>& all_asset_types()
{
    static const std::vector<AssetType> types {
        AssetType::BOX_FRONT, AssetType::BOX_BACK, AssetType::BOX_SPINE, AssetType::BOX_FULL,
        AssetType::CARTRIDGE, AssetType::LOGO, AssetType::POSTER,
        AssetType::ARCADE_MARQUEE, AssetType::ARCADE_BEZEL, AssetType::ARCADE_PANEL,
        AssetType::ARCADE_CABINET_L, AssetType::ARCADE_CABINET_R,
        AssetType::UI_TILE, AssetType::UI_BANNER, AssetType::UI_STEAMGRID, AssetType::BACKGROUND,
        AssetType::MUSIC, AssetType::SCREENSHOT, AssetType::TITLESCREEN, AssetType::VIDEO,
    };
    return types;
}

/// Your own metadata for a game (its file in the ROM folder) onto the one
/// made from the folder and ES-DE: yours wins, the rest fills the gaps
void merge_own_into(model::Game& into, const model::Game& own)
{
    if (!own.title().isEmpty())
        into.setTitle(own.title());
    if (!own.sortBy().isEmpty() && own.sortBy() != own.title())
        into.setSortBy(own.sortBy());
    if (!own.summary().isEmpty()) into.setSummary(own.summary());
    if (!own.description().isEmpty()) into.setDescription(own.description());
    if (!own.developerListConst().isEmpty()) into.developerList() = own.developerListConst();
    if (!own.publisherListConst().isEmpty()) into.publisherList() = own.publisherListConst();
    if (!own.genreListConst().isEmpty()) into.genreList() = own.genreListConst();
    if (!own.tagListConst().isEmpty()) into.tagList() = own.tagListConst();
    if (own.playerCount() > 1) into.setPlayerCount(own.playerCount());
    if (own.releaseDate().isValid()) into.setReleaseDate(own.releaseDate());
    if (own.rating() > 0.f) into.setRating(own.rating());
    // your art first, the folder's and ES-DE's after it
    for (const AssetType type : all_asset_types()) {
        const QStringList& mine = own.assets().get(type);
        if (mine.isEmpty())
            continue;
        QStringList merged = mine;
        for (const QString& f : into.assets().get(type)) {
            if (!merged.contains(f))
                merged.append(f);
        }
        into.assetsMut().set_files(type, merged);
    }
    for (auto it = own.extraMap().cbegin(); it != own.extraMap().cend(); ++it)
        into.extraMapMut().insert(it.key(), it.value());
}

/// One game's entry in a metadata file. If the user's own metadata already
/// has the game (`own`), its title is kept and only what it lacks is written,
/// so the user's details win whichever file Pegasus reads first.
QString game_entry(const model::Game& game, const QStringList& files, const QDir& base_dir,
                   const model::Game* own, const QString& launch)
{
    QString out;
    QTextStream stream(&out);
    Q_UNUSED(base_dir);  // the paths are absolute: the file lives in the Pegasus X folder
    stream << "game: " << metafile_value(own ? own->title() : game.title()) << '\n';
    if (files.size() == 1) {
        stream << "file: " << files.first() << '\n';
    }
    else {
        stream << "files:\n";
        for (const QString& file : files)
            stream << "  " << file << '\n';
    }
    if (!own && !game.sortBy().isEmpty() && game.sortBy() != game.title())
        stream << "sort-by: " << game.sortBy() << '\n';
    // a list's values, empty ones left out (a gamelist's empty <developer/>
    // would make a "developer:" line Pegasus rejects)
    const auto joined = [](const QStringList& list) {
        QStringList out;
        for (const QString& value : list) {
            if (!value.trimmed().isEmpty())
                out.append(value.trimmed());
        }
        return out.join(QStringLiteral(", "));
    };
    const QString developers = joined(game.developerListConst());
    const QString publishers = joined(game.publisherListConst());
    const QString genres = joined(game.genreListConst());
    if (!developers.isEmpty() && !(own && !joined(own->developerListConst()).isEmpty()))
        stream << "developer: " << developers << '\n';
    if (!publishers.isEmpty() && !(own && !joined(own->publisherListConst()).isEmpty()))
        stream << "publisher: " << publishers << '\n';
    if (!genres.isEmpty() && !(own && !joined(own->genreListConst()).isEmpty()))
        stream << "genre: " << genres << '\n';
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
        // every picture of the kind (a cover and a 3D box are both box_front),
        // the first on the key's line, the others below it
        QStringList files;
        for (const QString& f : *lists[i].second) {
            const QString path = local_path(f);
            if (!path.isEmpty() && !path.startsWith(QLatin1String("image://")) && !files.contains(path))
                files.append(path);
        }
        if (files.isEmpty())
            continue;
        stream << "assets." << key << ": " << files.first() << '\n';
        for (int f = 1; f < files.size(); f++)
            stream << "  " << files.at(f) << '\n';
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


namespace {
/// A quick count of a folder's game files for the system list: the folder
/// is read directly (no details asked per file, which is slow on Android's
/// shared storage) and playlists aren't opened, so discs of a multi-disc
/// game count separately here; the import itself groups them
int quick_game_count(const QString& dir, const QStringList& extensions, int depth = 0)
{
    if (depth > 8)
        return 0;
    int count = 0;
#ifdef Q_OS_UNIX
    const QByteArray encoded = QFile::encodeName(dir);
    DIR* const handle = opendir(encoded.constData());
    if (!handle)
        return 0;
    QStringList subdirs;
    while (const dirent* const entry = readdir(handle)) {
        if (entry->d_name[0] == '.')
            continue;
        const QString name = QFile::decodeName(entry->d_name);
        bool is_dir = entry->d_type == DT_DIR;
        if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK)
            is_dir = QFileInfo(dir % QLatin1Char('/') % name).isDir();
        if (has_extension(name, extensions))
            count++;  // a game file, or a folder that is one game
        else if (is_dir)
            subdirs.append(dir % QLatin1Char('/') % name);
    }
    closedir(handle);
    for (const QString& subdir : subdirs)
        count += quick_game_count(subdir, extensions, depth + 1);
#else
    count = static_cast<int>(find_game_groups(dir, extensions, {}).size());
#endif
    return count;
}
} // namespace

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
        // Android apps always go into the one "Android" collection
        if (name == QLatin1String("androidapps")) {
            entry.fullname = QStringLiteral("Android");
            entry.short_name = QStringLiteral("android");
        }
        entry.folders = folders;
        for (const QString& folder : folders)
            entry.imported_files += import_files_of(folder);
        entry.has_gamelist = QFileInfo::exists(setup.esde_dir % QStringLiteral("/gamelists/") % name % QStringLiteral("/gamelist.xml"));
        entry.has_media = QFileInfo(setup.media_root % QLatin1Char('/') % name).isDir();

        const QStringList extensions = parse_extensions(sys.extensions);
        for (const QString& folder : folders)
            entry.games += quick_game_count(folder, extensions);
        if (entry.games > 0 || !entry.imported_files.isEmpty())
            out.push_back(std::move(entry));
    }
    std::sort(out.begin(), out.end(), [](const EsdeImportSystem& a, const EsdeImportSystem& b) {
        return a.fullname.compare(b.fullname, Qt::CaseInsensitive) < 0;
    });
    return out;
}


int esde_delete_import(const QStringList& folders)
{
    int removed = 0;
    for (const QString& folder : folders) {
        for (const QString& file : import_files_of(folder)) {
            if (QFile::remove(file)) {
                removed++;
                Log::info(QStringLiteral("ES-DE import"), LOGMSG("Removed `%1`").arg(file));
            }
        }
    }
    return removed;
}

QString imports_game_dir()
{
    return imports_dir();
}


namespace {
// the systems' games written into a metadata file in each of their folders
// (ES-DE import and ROM folders); `folders_of`: the folders of a system, if
// given (else its folders in the ROMs folders)
EsdeImportResult import_systems(const ImportSetup& setup, const QStringList& todo,
                                const HashMap<QString, QStringList>& folders_of,
                                const QStringList& android_systems, const QString& origin,
                                const std::function<void(int, int, const QString&)>& progress)
{
    EsdeImportResult result;
    const QString log_tag = QStringLiteral("ES-DE import");
    const es2::Metadata metahelper(log_tag, { setup.esde_dir });

    int done = 0;
    for (const QString& name : todo) {
        const auto sys_it = setup.systems.find(name);
        if (sys_it == setup.systems.cend())
            continue;
        const EsdeSystem& sys = sys_it->second;
        if (progress)
            progress(done, todo.size(), sys.fullname.isEmpty() ? name : sys.fullname);
        done++;

        const auto given = folders_of.find(name);
        const QStringList folders = given != folders_of.cend() ? given->second : system_folders(sys, setup.rom_roots);
        if (folders.isEmpty())
            continue;

        // Android Apps and Android Games can share one "Android" collection
        const bool as_android = android_systems.contains(name);
        const QString short_name = as_android ? QStringLiteral("android") : pegasus_short_name(sys.name);

        const QStringList extensions = parse_extensions(sys.extensions);
        const GamelistInfo gamelist = read_gamelist(setup.esde_dir, sys.name, folders);
        // a copy of ES-DE's gamelist in the Pegasus X folder (ES-DE's own is only read)
        if (!setup.esde_dir.isEmpty()) {
            const QString from = setup.esde_dir % QStringLiteral("/gamelists/") % sys.name % QStringLiteral("/gamelist.xml");
            if (QFileInfo::exists(from)) {
                const QString to_dir = paths::writableConfigDir() % QStringLiteral("/imported/") % sys.name;
                QDir().mkpath(to_dir);
                QFile::remove(to_dir % QStringLiteral("/gamelist.xml"));
                QFile::copy(from, to_dir % QStringLiteral("/gamelist.xml"));
            }
        }
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
            // art named after the game's title too (what scrapers do for games
            // without a file name of their own)
            for (const auto& entry : games) {
                const QString title_key = QDir::cleanPath(folder) % QLatin1Char('/') % entry.first->title();
                if (extless_path_to_game.find(title_key) == extless_path_to_game.cend())
                    extless_path_to_game.emplace(title_key, entry.first);
            }
            // the ROM folder's own art first (media/<kind>/<game>, Skraper's
            // skraper/, media/<game>/<kind>): the folder isn't a game directory
            // (its list is in the Pegasus X folder), so Pegasus doesn't read it
            for (const QString& sub : { QStringLiteral("media"), QStringLiteral(".media"), QStringLiteral("skraper") }) {
                const QString own_media = folder % QLatin1Char('/') % sub;
                find_media(own_media, folder, extless_path_to_game, found_assets);
                find_per_game_media(own_media, folder, extless_path_to_game, found_assets);
            }
            // then ES-DE's
            find_media(setup.media_root % QLatin1Char('/') % sys.name, folder, extless_path_to_game, found_assets);

            // the file: the system's in the game directory, this folder's games
            // merged in (games already there keep what they have: your edits,
            // scraped data, the emulators picked)
            QString collection_name;
            const QString key = providers::gamedir::key_for(sys.fullname.isEmpty() ? sys.name : sys.fullname, short_name);
            const QString file_path = providers::gamedir::system_file(key);
            MetaFile target;
            int coll_index = -1;
            if (target.load(file_path)) {
                for (size_t b = 0; b < target.blocks.size() && coll_index < 0; b++) {
                    if (target.blocks[b].is_collection)
                        coll_index = static_cast<int>(b);
                }
                if (coll_index >= 0)
                    collection_name = target.blocks[static_cast<size_t>(coll_index)].name;
            }
            if (collection_name.isEmpty())
                collection_name = existing_collection_name(folder);
            if (collection_name.isEmpty() && as_android)
                collection_name = QStringLiteral("Android");
            if (collection_name.isEmpty())
                collection_name = sys.fullname.isEmpty() ? sys.name : sys.fullname;
            const QString old_path = folder % QLatin1Char('/') % IMPORT_FILE_NAME;  // where imports used to go

            // your own metadata in the folder: its collection's emulator for the
            // system, its games' details and art into the entries below
            QString own_launch;
            for (const auto& pair : own_sctx.current_collection_games()) {
                if (!pair.first->commonLaunchCmd().isEmpty() && own_launch.isEmpty())
                    own_launch = pair.first->commonLaunchCmd();
            }
            if (coll_index < 0) {
                // a new system file
                target.path = file_path;
                target.dir = QFileInfo(file_path).absolutePath();
                target.lines = QStringList {
                    QStringLiteral("# Pegasus X game directory: ") % collection_name,
                    origin.trimmed(),
                    QStringLiteral("# Pegasus X writes here (imports, Settings > Library, the scraper)."),
                    QStringLiteral("# folder: ") % QDir::cleanPath(folder),
                    QString(),
                    QStringLiteral("collection: ") % collection_name,
                    QStringLiteral("shortname: ") % short_name,
                };
                QString collection_launch;
                previous_launches(old_path, collection_launch);
                if (collection_launch.isEmpty())
                    collection_launch = own_launch;
                if (!collection_launch.isEmpty())
                    target.lines.append(QStringLiteral("launch: ") % collection_launch);
                target.lines.append(QString());
                target.parse();
                coll_index = 0;
            }
            else if (!target.lines.contains(QStringLiteral("# folder: ") % QDir::cleanPath(folder))) {
                // this folder's line among the leading comments
                int at = 0;
                while (at < target.lines.size() && target.lines.at(at).startsWith(QLatin1Char('#')))
                    at++;
                target.lines.insert(at, QStringLiteral("# folder: ") % QDir::cleanPath(folder));
                target.parse();
            }
            // the files the system file lists already
            QSet<QString> listed;
            for (const MetaBlock& block : target.blocks) {
                if (!block.is_collection) {
                    for (const QString& f : target.game_files(block))
                        listed.insert(f);
                }
            }
            HashMap<QString, QString> old_launches;
            {
                QString unused;
                old_launches = previous_launches(old_path, unused);
            }
            const QDir base_dir(folder);
            int written_games = 0;
            int kept_games = 0;
            QStringList new_entries;
            for (const auto& entry : games) {
                model::Game* const game = entry.first;
                const QString clean = strip_disc_tag(game->title());
                if (entry.second.size() > 1 && !clean.isEmpty())
                    game->setTitle(clean);
                bool already = false;
                for (const QString& file : entry.second)
                    already = already || listed.contains(file);
                if (already) {
                    kept_games++;
                    continue;
                }
                QString launch;
                for (const QString& file : entry.second) {
                    const auto launch_it = old_launches.find(file);
                    if (launch_it != old_launches.cend()) {
                        launch = launch_it->second;
                        break;
                    }
                }
                // one of yours: your details and art first, ES-DE's for the gaps
                const model::Game* own = nullptr;
                for (const QString& file : entry.second)
                    own = own ? own : own_sctx.game_by_filepath(file);
                if (own) {
                    merge_own_into(*game, *own);
                    if (launch.isEmpty() && !own->launchCmd().isEmpty() && own->launchCmd() != own_launch)
                        launch = own->launchCmd();
                }
                new_entries.append(game_entry(*game, entry.second, base_dir, nullptr, launch));
                written_games++;
            }
            tmp_sctx.discard_all();
            own_sctx.discard_all();

            if (!new_entries.isEmpty()) {
                // after the collection's last block, before the next collection
                int insert_at = target.lines.size();
                for (size_t b = static_cast<size_t>(coll_index) + 1; b < target.blocks.size(); b++) {
                    if (target.blocks[b].is_collection) {
                        insert_at = target.blocks[b].first;
                        break;
                    }
                }
                while (insert_at > 0 && target.lines.at(insert_at - 1).trimmed().isEmpty())
                    insert_at--;
                QStringList add;
                for (const QString& text : new_entries) {
                    add.append(QString());
                    add += text.split(QLatin1Char('\n'));
                    if (!add.isEmpty() && add.last().isEmpty())
                        add.removeLast();
                }
                for (int i = 0; i < add.size(); i++)
                    target.lines.insert(insert_at + i, add.at(i));
                target.parse();
            }
            QDir().mkpath(QFileInfo(file_path).absolutePath());
            const QString error = target.save();
            if (!error.isEmpty()) {
                Log::warning(log_tag, LOGMSG("Could not write `%1`: %2").arg(file_path, error));
                if (result.error.isEmpty())
                    result.error = QStringLiteral("Could not write to %1").arg(QFileInfo(file_path).absolutePath());
                continue;
            }
            Log::info(log_tag, LOGMSG("`%1`: %2 games added, %3 already there").arg(file_path).arg(written_games).arg(kept_games));
            // the file an older Pegasus X wrote into the game folder would
            // define the games a second time: what was edited in it goes to
            // the system file, then it's set aside
            if (QFileInfo::exists(old_path)) {
                migrate_in_folder_edits(old_path, collection_name);
                const QString aside = old_path % QStringLiteral(".migrated");
                QFile::remove(aside);
                if (QFile::rename(old_path, aside))
                    Log::info(log_tag, LOGMSG("Set aside `%1` as `%2` (imports live in the Pegasus X folder now)").arg(old_path, aside));
            }

            result.games += written_games + kept_games;
            if (written_games > 0 || kept_games > 0)
                result.written_files.append(file_path);
            result.folders.append(folder);
            stamp_imported_folder(folder);
            system_written = true;
        }
        if (system_written)
            result.systems++;
    }
    if (progress)
        progress(todo.size(), todo.size(), QString());
    return result;
}
} // namespace

EsdeImportResult esde_import(const QString& esde_dir, const QStringList& system_names, const QStringList& android_systems,
                             const std::function<void(int, int, const QString&)>& progress)
{
    if (esde_dir.isEmpty() || !QFileInfo(esde_dir).isDir()) {
        EsdeImportResult result;
        result.error = QStringLiteral("ES-DE folder not found");
        return result;
    }
    const ImportSetup setup = load_import_setup(esde_dir);
    QStringList todo;
    for (const QString& name : setup.order) {
        if (system_names.isEmpty() || system_names.contains(name))
            todo.append(name);
    }
    const QString origin = QStringLiteral("# Made by Pegasus X from ES-DE (%1).\n").arg(setup.esde_dir);
    EsdeImportResult result = import_systems(setup, todo, {}, android_systems, origin, progress);
    // remembered, to be made again when games are added (not the Android
    // systems: their files list apps, not games in a folder)
    for (const QString& name : todo) {
        if (android_systems.contains(name) || name.startsWith(QLatin1String("android")))
            continue;
        QStringList written;
        for (const QString& dir : system_folders(setup.systems.at(name), setup.rom_roots)) {
            if (result.folders.contains(dir))
                written.append(dir);
        }
        remember_rom_folders(written, name);
    }
    return result;
}


// -- ROM folders (Settings > Game directories) ---------------------------------------

namespace {
const QString ROM_FOLDERS_FILE = QStringLiteral("pegasusx-rom-folders.txt");

QString rom_folders_path()
{
    return paths::writableConfigDir() % QLatin1Char('/') % ROM_FOLDERS_FILE;
}

// the system folders that have a list made by Pegasus X: "folder<TAB>system"
// when each ROM folder was last read into the game directory
// (gamedirectory/imported-folders.json): a folder changed since is read again
QString imported_stamps_path() { return providers::gamedir::dir() % QStringLiteral("/imported-folders.json"); }
QJsonObject read_imported_stamps()
{
    QFile file(imported_stamps_path());
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}
void stamp_imported_folder(const QString& folder)
{
    QJsonObject stamps = read_imported_stamps();
    stamps[folder_key(folder)] = QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(providers::gamedir::dir());
    QSaveFile file(imported_stamps_path());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(stamps).toJson(QJsonDocument::Indented));
        file.commit();
    }
}

std::vector<std::pair<QString, QString>> read_rom_folders()
{
    std::vector<std::pair<QString, QString>> out;
    QFile file(rom_folders_path());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;
    for (const QString& line : QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'))) {
        const int tab = line.indexOf(QLatin1Char('\t'));
        if (tab > 0)
            out.emplace_back(line.left(tab), line.mid(tab + 1).trimmed());
    }
    return out;
}

void write_rom_folders(const std::vector<std::pair<QString, QString>>& list)
{
    QString text;
    for (const auto& entry : list)
        text += entry.first % QLatin1Char('\t') % entry.second % QLatin1Char('\n');
    QSaveFile file(rom_folders_path());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(text.toUtf8());
        file.commit();
    }
}

// the folder name ES-DE gives a system ("%ROMPATH%/snes" -> "snes")
QString system_folder_name(const EsdeSystem& sys)
{
    if (!sys.path.contains(QLatin1String("%ROMPATH%")))
        return {};
    return QDir::cleanPath(sys.path).section(QLatin1Char('/'), -1).toLower();
}

// which system a folder is, by its name: ES-DE's folder names, else the
// usual names ("Nintendo 64", "PlayStation", "SNES" ...)
QString system_of_folder(const ImportSetup& setup, const QString& folder_name)
{
    const QString lower = folder_name.toLower();
    for (const QString& name : setup.order) {
        if (system_folder_name(setup.systems.at(name)) == lower)
            return name;
    }
    const QString guess = scraper::Scraper::system_of(folder_name, folder_name);
    return setup.systems.count(guess) ? guess : QString();
}

ImportSetup rom_folder_setup()
{
    // ES-DE's gamelists and art are used too when it's on the device
    ImportSetup setup = load_import_setup(esde_default_dir());
    setup.rom_roots.clear();
    return setup;
}

// a folder's systems: the folder itself, or the folders in it
HashMap<QString, QStringList> rom_folder_layout(const ImportSetup& setup, const QString& folder)
{
    HashMap<QString, QStringList> out;
    const QString clean = QDir::cleanPath(folder);
    const QString self = system_of_folder(setup, QFileInfo(clean).fileName());
    if (!self.isEmpty()) {
        out[self].append(clean);
        return out;
    }
    for (const QFileInfo& sub : QDir(clean).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name)) {
        const QString name = system_of_folder(setup, sub.fileName());
        if (!name.isEmpty())
            out[name].append(QDir::cleanPath(sub.absoluteFilePath()));
    }
    return out;
}
} // namespace

void remember_rom_folders(const QStringList& folders, const QString& system)
{
    if (folders.isEmpty())
        return;
    std::vector<std::pair<QString, QString>> list = read_rom_folders();
    for (const QString& folder : folders) {
        bool known = false;
        for (auto& entry : list) {
            if (entry.first == folder) {
                known = true;
                if (!system.isEmpty())
                    entry.second = system;
            }
        }
        if (!known)
            list.emplace_back(folder, system);
    }
    write_rom_folders(list);
}

std::vector<EsdeImportSystem> rom_folder_systems(const QString& folder)
{
    std::vector<EsdeImportSystem> out;
    const ImportSetup setup = rom_folder_setup();
    const HashMap<QString, QStringList> layout = rom_folder_layout(setup, folder);
    for (const auto& entry : layout) {
        const EsdeSystem& sys = setup.systems.at(entry.first);
        EsdeImportSystem item;
        item.name = entry.first;
        item.fullname = sys.fullname.isEmpty() ? entry.first : sys.fullname;
        item.short_name = pegasus_short_name(entry.first);
        item.folders = entry.second;
        const QStringList extensions = parse_extensions(sys.extensions);
        for (const QString& dir : entry.second) {
            item.games += quick_game_count(dir, extensions);
            item.has_gamelist = item.has_gamelist || QFileInfo::exists(dir % QStringLiteral("/gamelist.xml"));
            item.imported_files += import_files_of(dir);
        }
        if (item.games > 0)
            out.push_back(std::move(item));
    }
    std::sort(out.begin(), out.end(), [](const EsdeImportSystem& a, const EsdeImportSystem& b) {
        return a.fullname.compare(b.fullname, Qt::CaseInsensitive) < 0;
    });
    return out;
}

EsdeImportResult rom_folder_import(const QString& folder, const std::function<void(int, int, const QString&)>& progress)
{
    const ImportSetup setup = rom_folder_setup();
    const HashMap<QString, QStringList> layout = rom_folder_layout(setup, folder);
    if (layout.empty()) {
        EsdeImportResult result;
        result.error = QStringLiteral("No game systems found in %1 (folders are named like snes, gba or psx)").arg(folder);
        return result;
    }
    QStringList todo;
    for (const QString& name : setup.order) {
        if (layout.count(name))
            todo.append(name);
    }
    const QString origin = QStringLiteral("# Made by Pegasus X from the ROM folder %1.\n").arg(QDir::cleanPath(folder));
    EsdeImportResult result = import_systems(setup, todo, layout, {}, origin, progress);
    for (const QString& name : todo) {
        const auto it = layout.find(name);
        QStringList written;
        for (const QString& dir : it->second) {
            if (result.folders.contains(dir))
                written.append(dir);
        }
        remember_rom_folders(written, name);
    }
    return result;
}

int refresh_rom_folders()
{
    // a folder changed since its list was made (games added or removed): made again
    const std::vector<std::pair<QString, QString>> list = read_rom_folders();
    if (list.empty())
        return 0;
    std::unique_ptr<ImportSetup> setup;
    int refreshed = 0;
    for (const auto& entry : list) {
        const QString& dir = entry.first;
        const QFileInfo dir_info(dir);
        if (!dir_info.isDir())
            continue;
        // the newest of its list files (an older Pegasus X wrote into the folder)
        QDateTime listed;
        bool old_format = false;
        QString esde_dir;  // made from ES-DE: with that ES-DE's data again
        {
            const QString stamp = read_imported_stamps().value(folder_key(dir)).toString();
            if (!stamp.isEmpty())
                listed = QDateTime::fromMSecsSinceEpoch(stamp.toLongLong());
        }
        for (const QString& file : import_files_of(dir)) {
            if (!listed.isValid())
                listed = qMax(listed, QFileInfo(file).lastModified());
            QFile f(file);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            bool has_format = providers::gamedir::is_inside(file);  // the game directory's are current
            for (int i = 0; i < 8 && !f.atEnd(); i++) {
                const QString line = QString::fromUtf8(f.readLine()).trimmed();
                has_format = has_format || line == IMPORT_FORMAT_LINE;
                static const QRegularExpression rx_esde(QStringLiteral("^# Made by Pegasus X from ES-DE \\((.+)\\)\\.$"));
                const auto m = rx_esde.match(line);
                if (m.hasMatch())
                    esde_dir = m.captured(1);
            }
            old_format = old_format || !has_format;
        }
        // art added to the folder's media (media/, media/<kind>/) counts too
        QDateTime changed = dir_info.lastModified();
        for (const QString& sub : { QStringLiteral("media"), QStringLiteral(".media"), QStringLiteral("skraper") }) {
            const QFileInfo media(dir % QLatin1Char('/') % sub);
            if (!media.isDir())
                continue;
            changed = qMax(changed, media.lastModified());
            for (const QFileInfo& kind : QDir(media.absoluteFilePath()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
                changed = qMax(changed, kind.lastModified());
        }
        if (!listed.isValid() || (!old_format && changed <= listed))
            continue;
        if (!esde_dir.isEmpty() && QFileInfo(esde_dir).isDir()) {
            ImportSetup esde_setup = load_import_setup(esde_dir);
            QString system = entry.second;
            if (system.isEmpty() || !esde_setup.systems.count(system))
                system = system_of_folder(esde_setup, dir_info.fileName());
            if (system.isEmpty())
                continue;
            HashMap<QString, QStringList> only;
            only[system] = QStringList { dir };
            const QString origin = QStringLiteral("# Made by Pegasus X from ES-DE (%1).\n").arg(esde_setup.esde_dir);
            if (!import_systems(esde_setup, QStringList { system }, only, {}, origin, {}).folders.isEmpty())
                refreshed++;
            continue;
        }
        if (!setup)
            setup.reset(new ImportSetup(rom_folder_setup()));
        // the system: the one remembered, else by the folder's name
        QString system = entry.second;
        if (system.isEmpty() || !setup->systems.count(system))
            system = system_of_folder(*setup, dir_info.fileName());
        if (system.isEmpty())
            continue;
        HashMap<QString, QStringList> only;
        only[system] = QStringList { dir };
        const QString origin = QStringLiteral("# Made by Pegasus X from the ROM folder %1.\n").arg(dir);
        const EsdeImportResult result = import_systems(*setup, QStringList { system }, only, {}, origin, {});
        if (!result.folders.isEmpty())
            refreshed++;
    }
    if (refreshed > 0)
        Log::info(QStringLiteral("ROM folders"), LOGMSG("%1 game lists made again (new or removed games)").arg(refreshed));
    return refreshed;
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

    /// The files of a game block, as absolute paths (URIs like
    /// android:<package> as they are)
    QStringList game_files(const MetaBlock& block) const {
        QStringList out;
        const QDir base(dir);
        for (const MetaEntry& e : block.entries) {
            if (e.key != QLatin1String("file") && e.key != QLatin1String("files"))
                continue;
            for (const QString& value : e.values)
                out.append(is_uri(value) ? value : ::clean_abs_path(QFileInfo(base, value)));
        }
        return out;
    }

    static bool is_uri(const QString& value) {
        static const QRegularExpression rx_uri(QStringLiteral("^[a-zA-Z][a-zA-Z0-9+\\-.]+:.+"));
        return rx_uri.match(value).hasMatch() && !QFileInfo(value).isAbsolute();
    }

    /// A game file as written: relative when it's inside this file's folder
    /// (so it moves with the ROM folder), absolute otherwise (the override
    /// files live in the Pegasus X folder, which can move)
    QString file_value(const QString& game_path) const {
        if (is_uri(game_path))
            return game_path;
        const QString rel = QDir(dir).relativeFilePath(game_path);
        return rel.startsWith(QLatin1String("..")) ? game_path : rel;
    }

    /// The paths in this file, written for `old_dir`, made absolute (after
    /// the file moved to another folder)
    bool rebase_from(const QString& old_dir) {
        const QDir base(old_dir);
        bool changed = false;
        for (size_t b = 0; b < blocks.size(); b++) {
            // the keys with paths, each once (set_entry replaces every entry of a key)
            QStringList keys;
            for (const MetaEntry& e : blocks[b].entries) {
                const bool path_key = e.key == QLatin1String("file") || e.key == QLatin1String("files")
                    || e.key.startsWith(QLatin1String("assets.")) || e.key == QLatin1String("x-manual");
                if (path_key && !keys.contains(e.key))
                    keys.append(e.key);
            }
            for (const QString& key : keys) {
                QStringList values;
                bool any = false;
                for (const MetaEntry& e : blocks[b].entries) {
                    if (e.key != key)
                        continue;
                    for (const QString& v : e.values) {
                        const bool relative = !is_uri(v) && !QFileInfo(v).isAbsolute() && !v.startsWith(QLatin1String("image://"));
                        values.append(relative ? ::clean_abs_path(QFileInfo(base, v)) : v);
                        any = any || relative;
                    }
                }
                if (any) {
                    set_entry(b, { key }, key, values);
                    changed = true;
                }
            }
        }
        return changed;
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

    /// Pegasus X metadata editor: replaces every entry of a block named by
    /// `aliases` with `key` and `values` (one line each), or removes them if
    /// `values` is empty. Lists go one item per line; text keeps its lines.
    void set_entry(size_t block_index, const QStringList& aliases, const QString& key, const QStringList& values) {
        const MetaBlock& block = blocks.at(block_index);
        std::vector<std::pair<int, int>> ranges;
        for (const MetaEntry& e : block.entries) {
            if (aliases.contains(e.key))
                ranges.emplace_back(e.first, e.last);
        }
        int insert_at = -1;
        if (!ranges.empty()) {
            std::sort(ranges.begin(), ranges.end());
            insert_at = ranges.front().first;
            for (auto it = ranges.rbegin(); it != ranges.rend(); ++it) {
                for (int i = it->second; i >= it->first; i--)
                    lines.removeAt(i);
            }
        }
        else {
            // new: before the block's art and x- fields, else at its end
            insert_at = block.last + 1;
            if (!key.startsWith(QLatin1String("assets.")) && !key.startsWith(QLatin1String("x-"))) {
                for (const MetaEntry& e : block.entries) {
                    if (e.key.startsWith(QLatin1String("assets.")) || e.key.startsWith(QLatin1String("x-"))) {
                        insert_at = e.first;
                        break;
                    }
                }
            }
        }
        QStringList new_lines;
        for (int i = 0; i < values.size(); i++) {
            const QString value = values.at(i).trimmed();
            if (i == 0)
                new_lines.append(key % QStringLiteral(": ") % value);
            else
                new_lines.append(QStringLiteral("  ") % (value.isEmpty() ? QStringLiteral(".") : value));
        }
        if (!values.isEmpty() && !values.first().trimmed().isEmpty()) {
            for (int i = 0; i < new_lines.size(); i++)
                lines.insert(insert_at + i, new_lines.at(i));
        }
        parse();
    }

    /// Renames a block ("game: <title>" / "collection: <name>")
    void set_header(size_t block_index, const QString& name) {
        const MetaBlock& block = blocks.at(block_index);
        lines[block.first] = (block.is_collection ? QStringLiteral("collection: ") : QStringLiteral("game: ")) % name.simplified();
        parse();
    }

    /// Adds an empty game block (title and file) to the end of a collection's part
    int add_game_block(int collection_index, const QString& title, const QString& game_path) {
        int insert_at = lines.size();
        for (size_t i = static_cast<size_t>(collection_index) + 1; i < blocks.size(); i++) {
            if (blocks[i].is_collection) {
                insert_at = blocks[i].first;
                break;
            }
        }
        while (insert_at > 0 && lines.at(insert_at - 1).trimmed().isEmpty())
            insert_at--;
        lines.insert(insert_at, QString());
        lines.insert(insert_at + 1, QStringLiteral("game: ") % title.simplified());
        lines.insert(insert_at + 2, QStringLiteral("file: ") % file_value(game_path));
        parse();
        for (size_t i = 0; i < blocks.size(); i++) {
            if (blocks[i].first == insert_at + 1)
                return static_cast<int>(i);
        }
        return -1;
    }

    /// Pegasus X's own markers of who wrote what: x-pegasusx-edited (you,
    /// in the editor: wins, and a key listed without a value is a field you
    /// cleared) and x-pegasusx-scraped (the scraper)
    void mark(size_t block_index, const QString& marker, const QStringList& add, const QStringList& remove) {
        QStringList all;
        if (const MetaEntry* e = blocks.at(block_index).entry(marker)) {
            for (const QString& value : e->values) {
                for (const QString& k : value.split(QLatin1Char(','), QString::SkipEmptyParts))
                    all.append(k.trimmed());
            }
        }
        for (const QString& k : remove)
            all.removeAll(k);
        for (const QString& k : add) {
            if (!all.contains(k))
                all.append(k);
        }
        set_entry(block_index, { marker }, marker, { all.join(QStringLiteral(", ")) });
    }
    void mark_edited(size_t block_index, const QStringList& keys) {
        mark(block_index, QStringLiteral("x-pegasusx-edited"), keys, {});
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
            QStringLiteral("file: ") % file_value(game_path),
            QStringLiteral("launch: ") % command.simplified(),
        };
        for (int i = 0; i < block_lines.size(); i++)
            lines.insert(insert_at + i, block_lines.at(i));
        parse();
    }

    /// Writes the file back (backups: providers::gamedir::backup)
    QString save() const {
        // in one go: the library scan may be reading it
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return QStringLiteral("Could not write to %1 (no write access?)").arg(path);
        file.write((lines.join(newline) + newline).toUtf8());
        if (!file.commit())
            return QStringLiteral("Could not write to %1 (%2)").arg(path, file.errorString());
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

    QStringList out = providers::gamedir::metafiles();
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
    HashMap<QString, QString> recommended;     ///< ES-DE system -> label of ES-DE's default emulator

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
            load_systems(ctx.log_tag, esde_list_file(QStringLiteral("es_systems.xml")), systems, order, unused);
        }
        for (const QString& name : order) {
            const EsdeSystem& sys = systems.at(name);
            if (!sys.commands.empty())
                recommended.emplace(name, sys.commands.front().label.isEmpty() ? QStringLiteral("Default") : sys.commands.front().label);
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
                // custom ones first; ES-DE's after them, also those of the
                // same name, which are used if the custom one can't be
                // (the list keeps the first usable one of each name)
                std::vector<EsdeCommand> merged = custom_sys.commands;
                merged.insert(merged.end(), it->second.commands.cbegin(), it->second.commands.cend());
                it->second.commands = std::move(merged);
            }
        }
        if (!esde_dir.isEmpty())
            load_find_rules(ctx.log_tag, esde_dir % QStringLiteral("/custom_systems/es_find_rules.xml"), ctx.emulators);
        load_find_rules(ctx.log_tag, esde_list_file(QStringLiteral("es_find_rules.xml")), ctx.emulators);

        // Pegasus X: builds ES-DE doesn't know yet, added to its emulators
        // (the screen name doesn't have to be exact: if it's named
        // differently, the app's own game screen is found)
        static const std::vector<std::pair<const char*, const char*>> EXTRA_BUILDS {
            { "VITA3K", "org.vita3kplus.emulator/org.vita3k.emulator.Emulator" },
        };
        for (const auto& extra : EXTRA_BUILDS) {
            auto it = ctx.emulators.find(QLatin1String(extra.first));
            if (it != ctx.emulators.end() && !it->second.contains(QLatin1String(extra.second)))
                it->second.append(QLatin1String(extra.second));
        }

        // which emulator screens exist on this device, asked in one go
        QStringList components;
        for (const auto& entry : ctx.emulators)
            components.append(entry.second);
        components.removeDuplicates();
        QStringList resolved = ::android::resolve_components(components);

        // builds of an emulator under their own package name (eg. a fork
        // named org.vita3k.emulator.plus) count as that emulator too
        const QStringList installed = ::android::installed_packages();
        for (int i = 0; i < components.size() && i < resolved.size(); i++) {
            if (!resolved.at(i).isEmpty())
                continue;
            const QString package = components.at(i).section(QLatin1Char('/'), 0, 0);
            for (const QString& other : installed) {
                if (!other.startsWith(package % QLatin1Char('.')))
                    continue;
                const QString activity = ::android::find_game_activity(other);
                if (!activity.isEmpty()) {
                    resolved[i] = activity;
                    break;
                }
            }
        }
        for (int i = 0; i < components.size() && i < resolved.size(); i++)
            ctx.resolved_components.emplace(components.at(i), resolved.at(i));

        // ... and when the original is installed too (eg. Xendroid and its
        // ".fork" build), the fork is one more build of that emulator: listed
        // with the others, so it's offered for every system the emulator has
        QSet<QString> fork_done;
        for (auto& entry : ctx.emulators) {
            QStringList forks;
            for (const QString& component : entry.second) {
                const QString package = component.section(QLatin1Char('/'), 0, 0);
                for (const QString& other : installed) {
                    if (!other.startsWith(package % QLatin1Char('.')))
                        continue;
                    const QString activity = ::android::find_game_activity(other);
                    if (activity.isEmpty() || entry.second.contains(activity) || forks.contains(activity))
                        continue;
                    forks.append(activity);
                    if (!fork_done.contains(activity)) {
                        fork_done.insert(activity);
                        ctx.resolved_components.emplace(activity, activity);
                    }
                }
            }
            entry.second.append(forks);
        }

        // a collection is matched to a system by its short name or name:
        // ES-DE's own names first, then the common short names, then full names
        for (const QString& name : order)
            name_to_system.emplace(normalize_name(name), name);
        for (const QString& name : order)
            name_to_system.emplace(normalize_name(pegasus_short_name(name)), name);
        name_to_system.emplace(QStringLiteral("android"), QStringLiteral("androidapps"));

        // names people often give their collections
        static const std::vector<std::pair<const char*, const char*>> COMMON {
            {"supernintendo", "snes"}, {"supernes", "snes"}, {"superfamicom", "sfc"},
            {"nintendoentertainmentsystem", "nes"}, {"famicom", "famicom"},
            {"nintendo64", "n64"}, {"gamecube", "gc"}, {"ngc", "gc"}, {"nintendogamecube", "gc"},
            {"nintendowii", "wii"}, {"nintendowiiu", "wiiu"}, {"nintendoswitch", "switch"}, {"ns", "switch"},
            {"gameboy", "gb"}, {"gameboycolor", "gbc"}, {"gameboyadvance", "gba"},
            {"ds", "nds"}, {"nintendods", "nds"}, {"3ds", "n3ds"}, {"nintendo3ds", "n3ds"},
            {"ps1", "psx"}, {"psone", "psx"}, {"playstation", "psx"}, {"playstation1", "psx"},
            {"playstation2", "ps2"}, {"playstation3", "ps3"}, {"playstation4", "ps4"},
            {"playstationportable", "psp"}, {"vita", "psvita"}, {"psv", "psvita"}, {"playstationvita", "psvita"},
            {"genesis", "genesis"}, {"md", "megadrive"}, {"segamegadrive", "megadrive"}, {"segagenesis", "genesis"},
            {"sms", "mastersystem"}, {"gg", "gamegear"}, {"segacd", "segacd"}, {"32x", "sega32x"},
            {"dc", "dreamcast"}, {"turbografx16", "tg16"}, {"pce", "pcengine"},
            {"neogeopocket", "ngp"}, {"neogeopocketcolor", "ngpc"}, {"lynx", "atarilynx"}, {"jaguar", "atarijaguar"},
            {"ws", "wonderswan"}, {"wsc", "wonderswancolor"}, {"msdos", "dos"}, {"pc", "windows"},
            {"steam", "steam"}, {"x360", "xbox360"}, {"microsoftxbox", "xbox"}, {"switch", "switch"},
        };
        for (const auto& entry : COMMON) {
            const QString target = QLatin1String(entry.second);
            if (systems.count(target))
                name_to_system.emplace(QLatin1String(entry.first), target);
        }
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
            choice.retroarch = cmd.raw.startsWith(QLatin1String("%EMULATOR_RETROARCH%"));
            // ES-DE names RetroArch cores by the core alone ("Flycast")
            choice.label = choice.retroarch && !label.startsWith(QLatin1String("RetroArch"))
                ? QStringLiteral("RetroArch · ") % label : label;
            choice.command = command;
            choice.installed = installed;
            const auto rec_it = recommended.find(system_name);
            choice.recommended = rec_it != recommended.cend() && rec_it->second == label;
            out.push_back(choice);

            // more installed builds of the same emulator (eg. Vita3K and
            // Vita3K Plus) each get their own entry, named after the app
            static const QRegularExpression rx_emulator(QStringLiteral("^%EMULATOR_([^%]+)%"));
            const auto emu_match = rx_emulator.match(cmd.raw);
            const auto emu_it = emu_match.hasMatch() ? ctx.emulators.find(emu_match.captured(1)) : ctx.emulators.cend();
            if (emu_it == ctx.emulators.cend())
                continue;
            QStringList builds;
            for (const QString& entry : emu_it->second) {
                const auto res_it = ctx.resolved_components.find(entry);
                if (res_it == ctx.resolved_components.cend() || res_it->second.isEmpty())
                    continue;
                const QString package_name = res_it->second.section(QLatin1Char('/'), 0, 0);
                if (!builds.contains(package_name))
                    builds.append(package_name);
                if (builds.size() > 1 && builds.last() == package_name && !choice.command.contains(package_name % QLatin1Char('/'))) {
                    bool unused = false;
                    QString build_error;
                    const QString build_command = translate_command(cmd.raw, ctx, rom_root, unused, build_error, res_it->second);
                    if (build_command.isEmpty())
                        continue;
                    QString app_name = ::android::app_label(package_name);
                    if (app_name.isEmpty())
                        app_name = package_name;
                    // a fork named like the original: "Xendroid Fork"
                    if (app_name == ::android::app_label(builds.first()))
                        app_name += QStringLiteral(" Fork");
                    EmulatorChoice extra = choice;
                    extra.label = choice.label % QStringLiteral(" · ") % app_name;
                    extra.command = build_command;
                    extra.installed = true;
                    extra.recommended = false;
                    if (!seen.contains(extra.label)) {
                        seen.append(extra.label);
                        out.push_back(std::move(extra));
                    }
                }
            }
        }
        // installed ones first, keeping ES-DE's order otherwise
        std::stable_sort(out.begin(), out.end(), [](const EmulatorChoice& a, const EmulatorChoice& b) {
            return a.installed && !b.installed;
        });
        return out;
    }
};

// Pegasus X: building the setup reads ES-DE's lists and asks Android about
// every emulator app, which took long enough to pause Settings > Library each
// time a collection opened (two or three times per collection). It's built
// once and shared: again when the emulator list or your own emulators change,
// when Pegasus X comes back to the front (apps may have been installed), or
// after 10 minutes.
QMutex& setup_mutex()
{
    static QMutex mutex;
    return mutex;
}
std::shared_ptr<const EmulatorSetup>& setup_slot()
{
    static std::shared_ptr<const EmulatorSetup> slot;
    return slot;
}
qint64& setup_time()
{
    static qint64 time = 0;
    return time;
}
std::shared_ptr<const EmulatorSetup> shared_emulator_setup()
{
    QMutexLocker lock(&setup_mutex());
    std::shared_ptr<const EmulatorSetup>& slot = setup_slot();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!slot || now - setup_time() > 10 * 60 * 1000) {
        slot = std::make_shared<const EmulatorSetup>();
        setup_time() = now;
    }
    return slot;
}

QString same_command_key(const QString& command)
{
    return command.simplified();
}

/// The files that define a collection, loaded
// Which files define each collection, remembered from the last full read
// (emulator_collections), so a change only reads the files it touches
// instead of every metadata file of every game directory
QMutex& collection_files_mutex()
{
    static QMutex mutex;
    return mutex;
}
HashMap<QString, QStringList>& collection_files_cache()
{
    static HashMap<QString, QStringList> cache;
    return cache;
}

std::vector<MetaFile> load_defining(const QStringList& paths, const QString& collection_name)
{
    std::vector<MetaFile> out;
    for (const QString& path : paths) {
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

std::vector<MetaFile> files_defining(const QString& collection_name)
{
    QStringList known;
    {
        QMutexLocker lock(&collection_files_mutex());
        const auto it = collection_files_cache().find(collection_name);
        if (it != collection_files_cache().cend())
            known = it->second;
    }
    if (!known.isEmpty()) {
        std::vector<MetaFile> out = load_defining(known, collection_name);
        if (!out.empty())
            return out;
    }

    // not known yet, or the files changed: look everywhere
    std::vector<MetaFile> out = load_defining(all_metafiles(), collection_name);
    QStringList paths;
    for (const MetaFile& file : out)
        paths.append(file.path);
    QMutexLocker lock(&collection_files_mutex());
    collection_files_cache()[collection_name] = paths;
    return out;
}
} // namespace


namespace {
// the ROMs root of a collection's files: the parent of the folder of its
// first metadata file, or, for an import (in the Pegasus X folder), of the
// folder it was made from (its "# folder:" line)
QString rom_root_of(const QStringList& files)
{
    for (const QString& path : files) {
        QString folder = QFileInfo(path).absolutePath();
        if (providers::gamedir::is_inside(path)
                || path.startsWith(imports_dir() % QLatin1Char('/')) || path.startsWith(imports_fill_dir() % QLatin1Char('/'))) {
            folder.clear();
            QFile file(path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                for (int i = 0; i < 40 && !file.atEnd(); i++) {
                    const QString line = QString::fromUtf8(file.readLine()).trimmed();
                    if (!line.startsWith(QLatin1Char('#')) && !line.isEmpty())
                        break;
                    if (line.startsWith(QLatin1String("# folder:"))) {
                        folder = line.mid(9).trimmed();
                        break;
                    }
                }
            }
            if (folder.isEmpty())
                continue;
        }
        return QFileInfo(folder).absolutePath();
    }
    return {};
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

    // (the game directory's files are read above already: nothing else to add)
    for (EmulatorCollection& coll : out) {
        MetaFile over;
        if (providers::gamedir::is_inside(edits_file_path(coll.name)) || !over.load(edits_file_path(coll.name)))
            continue;
        for (const MetaBlock& block : over.blocks) {
            if (block.is_collection) {
                if (block.name != coll.name)
                    continue;
                const MetaEntry* launch = block.entry(QStringLiteral("launch"));
                if (!launch)
                    launch = block.entry(QStringLiteral("command"));
                if (launch && !launch->values.isEmpty())
                    coll.command = launch->values.join(QLatin1Char(' '));
            }
            else if (block.collection_index >= 0
                     && over.blocks[static_cast<size_t>(block.collection_index)].name == coll.name
                     && (block.entry(QStringLiteral("launch")) || block.entry(QStringLiteral("command")))) {
                coll.game_overrides++;
            }
        }
    }

    // what can launch them
    const std::shared_ptr<const EmulatorSetup> setup_ptr = shared_emulator_setup();
    const EmulatorSetup& setup = *setup_ptr;
    for (EmulatorCollection& coll : out) {
        coll.system = setup.system_for(coll.short_name, coll.name);
        if (coll.system.isEmpty())
            continue;
        const EsdeSystem& sys = setup.systems.at(coll.system);
        coll.is_android = coll.system == QLatin1String("androidapps") || coll.system == QLatin1String("androidgames")
            || coll.system == QLatin1String("android") || coll.short_name.toLower() == QLatin1String("android");
        coll.system_fullname = sys.fullname.isEmpty() ? sys.name : sys.fullname;

        coll.options = setup.choices_for(coll.system, rom_root_of(coll.files));

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

    {
        QMutexLocker lock(&collection_files_mutex());
        HashMap<QString, QStringList>& cache = collection_files_cache();
        cache.clear();
        for (const EmulatorCollection& coll : out)
            cache[coll.name] = coll.files;
    }
    return out;
}


std::vector<DefaultLaunch> default_launches(const std::vector<std::pair<QString, QString>>& collections,
                                            const QStringList& rom_roots)
{
    std::vector<DefaultLaunch> out(collections.size());
    if (collections.empty())
        return out;
    const std::shared_ptr<const EmulatorSetup> setup_ptr = shared_emulator_setup();
    const EmulatorSetup& setup = *setup_ptr;
    for (size_t i = 0; i < collections.size(); i++) {
        const QString system = setup.system_for(collections[i].second, collections[i].first);
        if (system.isEmpty())
            continue;
        const std::vector<EmulatorChoice> choices = setup.choices_for(system, i < static_cast<size_t>(rom_roots.size()) ? rom_roots.at(static_cast<int>(i)) : QString());
        // ES-DE's default if it's installed, else the first installed
        // standalone emulator, else the first installed one
        const EmulatorChoice* pick = nullptr;
        for (const EmulatorChoice& c : choices) {
            if (c.installed && c.recommended) { pick = &c; break; }
        }
        for (size_t c = 0; !pick && c < choices.size(); c++) {
            if (choices[c].installed && !choices[c].retroarch)
                pick = &choices[c];
        }
        for (size_t c = 0; !pick && c < choices.size(); c++) {
            if (choices[c].installed)
                pick = &choices[c];
        }
        if (pick) {
            out[i].label = pick->label;
            out[i].command = pick->command;
        }
    }
    return out;
}


namespace {
QString custom_emulators_path()
{
    return paths::writableConfigDir() % QStringLiteral("/pegasusx-emulators.json");
}
QJsonArray load_custom_emulators()
{
    QFile file(custom_emulators_path());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).array();
}
// what a saved emulator belongs to: the ES-DE system, else the collection
QString custom_key(const EmulatorSetup& setup, const QString& collection_name, const QString& short_name)
{
    const QString system = setup.system_for(short_name, collection_name);
    return system.isEmpty() ? QStringLiteral("collection:") % collection_name.toLower() : system;
}
QString edits_file_path(const QString& collection_name)
{
    QString safe;
    for (const QChar c : collection_name) {
        if (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_'))
            safe += c;
        else if (c == QLatin1Char(' '))
            safe += QLatin1Char('_');
    }
    if (safe.isEmpty())
        safe = QStringLiteral("other");
    // Pegasus X: the collection's file in the game directory, where your
    // edits, scraped data and emulators are written (the one Pegasus reads)
    for (const QString& path : providers::gamedir::metafiles()) {
        MetaFile file;
        if (!file.load(path))
            continue;
        for (const MetaBlock& block : file.blocks) {
            if (block.is_collection && block.name == collection_name)
                return path;
        }
    }
    // a collection without a file yet (eg. games Pegasus found by itself)
    return providers::gamedir::system_file(providers::gamedir::key_for(collection_name, QString()));
}

// the override file of a collection, made when there's none yet, with the
// collection's block
MetaFile override_file(const QString& collection_name, int& coll_index)
{
    MetaFile file;
    const QString path = edits_file_path(collection_name);
    if (!file.load(path)) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        file.path = path;
        file.dir = QFileInfo(path).absolutePath();
        file.lines = QStringList {
            QStringLiteral("# Pegasus X game directory: ") % collection_name,
            QStringLiteral("# Pegasus X writes here (imports, Settings > Library, the scraper)."),
        };
        file.parse();
    }
    coll_index = -1;
    for (size_t b = 0; b < file.blocks.size(); b++) {
        if (file.blocks[b].is_collection && file.blocks[b].name == collection_name)
            coll_index = static_cast<int>(b);
    }
    if (coll_index < 0) {
        if (!file.lines.isEmpty() && !file.lines.last().trimmed().isEmpty())
            file.lines.append(QString());
        file.lines.append(QStringLiteral("collection: ") % collection_name);
        file.parse();
        for (size_t b = 0; b < file.blocks.size(); b++) {
            if (file.blocks[b].is_collection && file.blocks[b].name == collection_name)
                coll_index = static_cast<int>(b);
        }
    }
    return file;
}

// the game's block in the override file, added when there's none
int override_game_block(MetaFile& file, int coll_index, const QString& wanted, const QString& title)
{
    for (size_t b = 0; b < file.blocks.size(); b++) {
        const MetaBlock& block = file.blocks[b];
        if (!block.is_collection && file.game_files(block).contains(wanted))
            return static_cast<int>(b);
    }
    return file.add_game_block(coll_index, title.isEmpty() ? QFileInfo(wanted).completeBaseName() : title, wanted);
}

// the blocks an older Pegasus X marked as edited in an import file it wrote
// into the game folder: their edited values into the collection's override file
void migrate_in_folder_edits(const QString& old_path, const QString& collection_name)
{
    MetaFile old;
    if (!old.load(old_path))
        return;
    int coll_index = -1;
    MetaFile over = override_file(collection_name, coll_index);
    const QDir old_base(old.dir);
    int moved = 0;
    for (const MetaBlock& block : old.blocks) {
        if (block.is_collection)
            continue;
        const MetaEntry* const marker = block.entry(QStringLiteral("x-pegasusx-edited"));
        const MetaEntry* const launch = block.entry(QStringLiteral("launch"));
        if (!marker && !launch)
            continue;
        const QStringList files = old.game_files(block);
        if (files.isEmpty())
            continue;
        QStringList keys;
        if (marker) {
            for (const QString& value : marker->values) {
                for (const QString& k : value.split(QLatin1Char(','), QString::SkipEmptyParts))
                    keys.append(k.trimmed().toLower());
            }
        }
        const int bi = override_game_block(over, coll_index, files.first(), block.name);
        if (bi < 0)
            continue;
        QStringList edited;
        for (const QString& key : keys) {
            if (key == QLatin1String("title")) {
                over.set_header(static_cast<size_t>(bi), block.name);
                edited.append(key);
                continue;
            }
            QStringList values;
            for (const MetaEntry& e : block.entries) {
                if (!aliases_of(key).contains(e.key))
                    continue;
                for (const QString& v : e.values) {
                    const bool path_key = key.startsWith(QLatin1String("assets.")) || key == QLatin1String("x-manual");
                    values.append(path_key && !MetaFile::is_uri(v) && !QFileInfo(v).isAbsolute()
                                  ? ::clean_abs_path(QFileInfo(old_base, v)) : v);
                }
            }
            over.set_entry(static_cast<size_t>(bi), aliases_of(key), key, values);
            edited.append(key);
        }
        if (launch && !launch->values.isEmpty())
            over.set_launch(bi, launch->values.join(QLatin1Char(' ')));
        if (!edited.isEmpty())
            over.mark_edited(static_cast<size_t>(bi), edited);
        moved++;
    }
    if (moved > 0) {
        over.save();
        Log::info(QStringLiteral("Metadata"), LOGMSG("Kept %1 edited games from `%2` in `%3`").arg(moved).arg(old_path, over.path));
    }
}

} // namespace

std::vector<EmulatorChoice> collection_choices(const QString& collection_name, const QString& short_name, const QString& rom_root)
{
    const std::shared_ptr<const EmulatorSetup> setup_ptr = shared_emulator_setup();
    const EmulatorSetup& setup = *setup_ptr;
    const QString system = setup.system_for(short_name, collection_name);
    std::vector<EmulatorChoice> out;
    // the ones you saved for this system (or this collection) first
    const QString key = custom_key(setup, collection_name, short_name);
    for (const QJsonValue& value : load_custom_emulators()) {
        const QJsonObject obj = value.toObject();
        if (obj.value(QStringLiteral("for")).toString() != key)
            continue;
        EmulatorChoice choice;
        choice.label = obj.value(QStringLiteral("label")).toString();
        choice.command = obj.value(QStringLiteral("command")).toString();
        choice.installed = true;
        choice.custom = true;
        if (!choice.label.isEmpty() && !choice.command.isEmpty())
            out.push_back(std::move(choice));
    }
    if (!system.isEmpty()) {
        std::vector<EmulatorChoice> listed = setup.choices_for(system, rom_root);
        out.insert(out.end(), listed.begin(), listed.end());
    }
    return out;
}

QString save_custom_emulator(const QString& collection_name, const QString& short_name,
                             const QString& label, const QString& command, bool remove)
{
    const std::shared_ptr<const EmulatorSetup> setup_ptr = shared_emulator_setup();
    const EmulatorSetup& setup = *setup_ptr;
    const QString key = custom_key(setup, collection_name, short_name);
    QJsonArray list = load_custom_emulators();
    for (int i = list.size() - 1; i >= 0; i--) {
        const QJsonObject obj = list.at(i).toObject();
        if (obj.value(QStringLiteral("for")).toString() == key && obj.value(QStringLiteral("label")).toString() == label)
            list.removeAt(i);
    }
    if (!remove) {
        list.append(QJsonObject {
            { QStringLiteral("for"), key },
            { QStringLiteral("label"), label },
            { QStringLiteral("command"), command },
        });
    }
    QFile file(custom_emulators_path());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QStringLiteral("Could not write `%1`").arg(file.fileName());
    file.write(QJsonDocument(list).toJson());
    return {};
}


CollectionDetails collection_details(const QString& collection_name)
{
    CollectionDetails out;
    std::vector<MetaFile> files = files_defining(collection_name);
    std::stable_sort(files.begin(), files.end(), [](const MetaFile& a, const MetaFile& b) {
        const bool a_import = QFileInfo(a.path).fileName() == IMPORT_FILE_NAME;
        const bool b_import = QFileInfo(b.path).fileName() == IMPORT_FILE_NAME;
        return !a_import && b_import;
    });
    QString rom_root;
    for (const MetaFile& file : files) {
        for (const MetaBlock& block : file.blocks) {
            if (!block.is_collection || block.name != collection_name)
                continue;
            if (rom_root.isEmpty())
                rom_root = QFileInfo(file.dir).absolutePath();
            const auto value = [&block](std::initializer_list<const char*> keys) {
                for (const char* key : keys) {
                    if (const MetaEntry* e = block.entry(QString::fromLatin1(key)))
                        return e->values.join(QStringLiteral(", "));
                }
                return QString();
            };
            if (out.short_name.isEmpty())
                out.short_name = value({ "shortname" });
            if (out.extensions.isEmpty())
                out.extensions = value({ "extensions", "extension" });
            if (out.launch.isEmpty()) {
                const MetaEntry* e = block.entry(QStringLiteral("launch"));
                if (!e)
                    e = block.entry(QStringLiteral("command"));
                if (e)
                    out.launch = e->values.join(QLatin1Char(' '));
            }
        }
    }
    const std::vector<DefaultLaunch> defaults = default_launches({ { collection_name, out.short_name } }, { rom_root });
    out.rom_root = rom_root;
    out.default_label = defaults.front().label;
    out.default_command = defaults.front().command;
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
    // (the game directory's file is read above already)
    MetaFile over;
    if (!providers::gamedir::is_inside(edits_file_path(collection_name)) && over.load(edits_file_path(collection_name))) {
        for (const MetaBlock& block : over.blocks) {
            if (block.is_collection || block.collection_index < 0)
                continue;
            if (over.blocks[static_cast<size_t>(block.collection_index)].name != collection_name)
                continue;
            const MetaEntry* launch = block.entry(QStringLiteral("launch"));
            if (!launch)
                launch = block.entry(QStringLiteral("command"));
            const QStringList files = over.game_files(block);
            if (!launch || files.isEmpty())
                continue;
            const QString command = launch->values.join(QLatin1Char(' '));
            bool found = false;
            for (GameEmulator& e : out) {
                if (e.path == files.first()) {
                    e.command = command;
                    found = true;
                }
            }
            if (!found)
                out.push_back({ files.first(), command });
        }
    }
    return out;
}


QString set_collection_emulator(const QString& collection_name, const QString& command)
{
    int coll_index = -1;
    MetaFile file = override_file(collection_name, coll_index);
    if (coll_index < 0)
        return QStringLiteral("Could not write the game directory file for `%1`").arg(collection_name);
    file.set_launch(coll_index, command);
    providers::gamedir::backup(file.path, QStringLiteral("edit"));
    const QString error = file.save();
    if (error.isEmpty())
        Log::info(QStringLiteral("Emulators"), LOGMSG("`%1`: launch command written to `%2`").arg(collection_name, file.path));
    return error;
}


QString set_game_emulator(const QString& collection_name, const QString& game_path,
                          const QString& game_title, const QString& command)
{
    const QString wanted = MetaFile::is_uri(game_path) ? game_path : ::clean_abs_path(QFileInfo(game_path));
    int coll_index = -1;
    MetaFile file = override_file(collection_name, coll_index);
    if (coll_index < 0)
        return QStringLiteral("Could not write the game directory file for `%1`").arg(collection_name);
    const int block = override_game_block(file, coll_index, wanted, game_title);
    if (block < 0)
        return QStringLiteral("Could not add `%1` to `%2`").arg(game_title, file.path);
    file.set_launch(block, command);
    providers::gamedir::backup(file.path, QStringLiteral("edit"));
    return file.save();
}


OriginalLaunch original_launch(const QString& collection_name, const QString& game_path)
{
    OriginalLaunch out;
    const QString wanted = game_path.isEmpty() || MetaFile::is_uri(game_path)
        ? game_path : ::clean_abs_path(QFileInfo(game_path));
    for (const MetaFile& current : files_defining(collection_name)) {
        MetaFile backup;
        if (!backup.load(current.path % QStringLiteral(".bak")))
            continue;
        for (const MetaBlock& block : backup.blocks) {
            if (wanted.isEmpty()) {
                if (!block.is_collection || block.name != collection_name)
                    continue;
            }
            else {
                if (block.is_collection || block.collection_index < 0)
                    continue;
                if (backup.blocks[static_cast<size_t>(block.collection_index)].name != collection_name)
                    continue;
                if (!backup.game_files(block).contains(wanted))
                    continue;
            }
            const MetaEntry* launch = block.entry(QStringLiteral("launch"));
            if (!launch)
                launch = block.entry(QStringLiteral("command"));
            out.known = true;
            out.command = launch ? launch->values.join(QLatin1Char(' ')) : QString();
            return out;
        }
        // a backup without the game: the game had no launch line of its own
        if (!wanted.isEmpty())
            out.known = true;
    }
    return out;
}


OriginalLaunches original_launches(const QString& collection_name)
{
    OriginalLaunches out;
    for (const MetaFile& current : files_defining(collection_name)) {
        MetaFile backup;
        if (!backup.load(current.path % QStringLiteral(".bak")))
            continue;
        for (const MetaBlock& block : backup.blocks) {
            const MetaEntry* launch = block.entry(QStringLiteral("launch"));
            if (!launch)
                launch = block.entry(QStringLiteral("command"));
            if (block.is_collection) {
                if (block.name == collection_name && !out.collection_known) {
                    out.collection_known = true;
                    out.collection_command = launch ? launch->values.join(QLatin1Char(' ')) : QString();
                }
                continue;
            }
            if (!launch || block.collection_index < 0
                || backup.blocks[static_cast<size_t>(block.collection_index)].name != collection_name)
                continue;
            const QStringList files = backup.game_files(block);
            if (!files.isEmpty())
                out.games.push_back({ files.first(), launch->values.join(QLatin1Char(' ')) });
        }
    }
    return out;
}


// -- Metadata editor -----------------------------------------------------------

namespace {
// Pegasus X's own entries (games Pegasus found by itself, eg. Android apps),
// one file per collection: in Pegasus' format a game belongs to every
// collection declared above it in the same file, so with several collections
// in one file an Xbox game written after "collection: Android" was in both

QStringList aliases_of(const QString& key)
{
    if (key == QLatin1String("sort-by"))
        return { QStringLiteral("sorttitle"), QStringLiteral("sortname"), QStringLiteral("sort_title"),
                 QStringLiteral("sort_name"), QStringLiteral("sort-title"), QStringLiteral("sort-name"),
                 QStringLiteral("sortby"), QStringLiteral("sort_by"), QStringLiteral("sort-by") };
    if (key == QLatin1String("developer") || key == QLatin1String("publisher")
        || key == QLatin1String("genre") || key == QLatin1String("tag"))
        return { key, key % QLatin1Char('s') };
    if (key == QLatin1String("launch"))
        return { QStringLiteral("launch"), QStringLiteral("command") };
    if (key == QLatin1String("extensions"))
        return { QStringLiteral("extensions"), QStringLiteral("extension") };
    return { key };
}

// a changed value -> the lines it's written as
QStringList value_lines(const QString& key, const QVariant& value, const QDir& base)
{
    QStringList out;
    const bool is_file = key.startsWith(QLatin1String("assets.")) || key == QLatin1String("x-manual");
    // relative to the metadata file when it's inside its folder, so the art
    // moves with the ROM folder
    const auto file_value = [&base](const QString& path) {
        const QString rel = base.relativeFilePath(path);
        return rel.startsWith(QLatin1String("..")) ? path : rel;
    };
    if (value.type() == QVariant::StringList || value.type() == QVariant::List) {
        for (const QVariant& item : value.toList()) {
            const QString text = item.toString().trimmed();
            if (text.isEmpty() || (is_file && text.startsWith(QLatin1String("image://"))))
                continue;
            out.append(is_file ? file_value(text) : text);
        }
        return out;
    }
    QString text = value.toString().trimmed();
    if (text.isEmpty())
        return out;
    if (is_file)
        return { file_value(text) };
    return text.split(QLatin1Char('\n'));
}

QString apply_changes(MetaFile& file, size_t block_index, const QVariantMap& changes, const QString& header_key,
                      bool scraped = false)
{
    QStringList edited;
    const QDir base(file.dir);
    for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
        const QString key = it.key() == header_key ? it.key() : canonical_key(it.key());
        if (key == header_key) {
            const QString name = it.value().toString().simplified();
            if (!name.isEmpty())
                file.set_header(block_index, name);
            edited.append(QStringLiteral("title"));
            continue;
        }
        file.set_entry(block_index, aliases_of(key), key, value_lines(key, it.value(), base));
        edited.append(key);
    }
    // (the file is the one Pegasus reads: what's written is what shows; an
    // emptied field is gone from it)
    Q_UNUSED(scraped);
    Q_UNUSED(edited);
    return file.save();
}
// title without region/version tags and punctuation, to match your entry
QString match_name(const QString& title)
{
    static const QRegularExpression rx_tags(QStringLiteral("\\s*[\\(\\[][^\\)\\]]*[\\)\\]]"));
    QString t = strip_disc_tag(title);
    t.remove(rx_tags);
    return normalize_name(t);
}

} // namespace


void invalidate_emulator_setup()
{
    QMutexLocker lock(&setup_mutex());
    setup_slot().reset();
}

void split_legacy_edits_file()
{
    // the old file, all collections together: each collection's part goes to
    // its own file (added to it if there is one); the old file is kept, renamed
    const QString legacy = paths::writableConfigDir() % QStringLiteral("/metafiles/pegasusx-edits.metadata.pegasus.txt");
    QFile in(legacy);
    if (!in.exists() || !in.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    const QStringList lines = QString::fromUtf8(in.readAll()).split(QLatin1Char('\n'));
    in.close();

    QString current;
    QHash<QString, QStringList> parts;
    QStringList order;
    static const QRegularExpression rx_coll(QStringLiteral("^collection\\s*:\\s*(.+)$"));
    for (const QString& line : lines) {
        const QRegularExpressionMatch m = rx_coll.match(line.trimmed());
        if (m.hasMatch()) {
            current = m.captured(1).trimmed();
            if (!parts.contains(current))
                order.append(current);
            parts[current].append(line);
            continue;
        }
        if (current.isEmpty())
            continue;  // the comments at the top
        parts[current].append(line);
    }
    bool ok = true;
    for (const QString& name : order) {
        QFile out(edits_file_path(name));
        QDir().mkpath(QFileInfo(out.fileName()).absolutePath());
        const bool existed = out.exists();
        if (!out.open(QIODevice::Append | QIODevice::Text)) {
            ok = false;
            continue;
        }
        QTextStream stream(&out);
        stream.setCodec("UTF-8");
        if (!existed)
            stream << "# Made by Pegasus X: your edits to games Pegasus found by itself\n";
        stream << "\n" << parts.value(name).join(QLatin1Char('\n')) << "\n";
    }
    if (ok) {
        // the parts' relative paths were written for metafiles/
        for (const QString& name : order)
            rebase_override_file(edits_file_path(name), QFileInfo(legacy).absolutePath());
        QFile::rename(legacy, legacy + QStringLiteral(".split-old"));
        QMutexLocker lock(&collection_files_mutex());
        collection_files_cache().clear();
        Log::info(QStringLiteral("Metadata"), LOGMSG("Split `%1` into one file per collection").arg(legacy));
    }
}

void move_edits_to_overrides()
{
    // an older Pegasus X kept its edits files in metafiles/, where Pegasus
    // reads them as metadata; they're override files now (applied by Pegasus
    // X on top of the metadata), in metadata/overrides/
    const QString metafiles = paths::writableConfigDir() % QStringLiteral("/metafiles");
    const QStringList old = QDir(metafiles).entryList({ QStringLiteral("pegasusx-edits-*.metadata.pegasus.txt") }, QDir::Files);
    if (old.isEmpty())
        return;
    const QString overrides = paths::writableConfigDir() % QStringLiteral("/metadata/overrides");
    QDir().mkpath(overrides);
    for (const QString& name : old) {
        const QString target = overrides % QLatin1Char('/') % name.mid(QStringLiteral("pegasusx-edits-").length());
        if (QFileInfo::exists(target)) {
            // both: the old one's blocks appended
            QFile in(metafiles % QLatin1Char('/') % name);
            QFile out(target);
            if (in.open(QIODevice::ReadOnly | QIODevice::Text) && out.open(QIODevice::Append | QIODevice::Text)) {
                out.write("\n");
                out.write(in.readAll());
                in.close();
                in.remove();
            }
            continue;
        }
        QFile::rename(metafiles % QLatin1Char('/') % name, target);
    }
    // their relative paths were written for metafiles/
    for (const QString& name : old)
        rebase_override_file(overrides % QLatin1Char('/') % name.mid(QStringLiteral("pegasusx-edits-").length()), metafiles);
    Log::info(QStringLiteral("Metadata"), LOGMSG("Moved %1 edits files to `%2`").arg(old.size()).arg(overrides));
}

bool rebase_override_file(const QString& file_path, const QString& old_dir)
{
    MetaFile file;
    if (!file.load(file_path) || !file.rebase_from(old_dir))
        return false;
    const QString error = file.save();
    if (!error.isEmpty()) {
        Log::warning(QStringLiteral("Metadata"), error);
        return false;
    }
    Log::info(QStringLiteral("Metadata"), LOGMSG("Made the paths in `%1` absolute (it was in `%2`)").arg(file_path, old_dir));
    return true;
}

MetadataLocation metadata_location(const QString& collection_name, const QString& game_path)
{
    split_legacy_edits_file();
    MetadataLocation out;
    out.file = edits_file_path(collection_name);
    out.is_import = false;
    if (!game_path.isEmpty()) {
        const QString wanted = MetaFile::is_uri(game_path) ? game_path : ::clean_abs_path(QFileInfo(game_path));
        MetaFile file;
        out.is_new = true;
        if (file.load(out.file)) {
            for (const MetaBlock& block : file.blocks) {
                if (!block.is_collection && file.game_files(block).contains(wanted))
                    out.is_new = false;
            }
        }
    }
    return out;
}


QString save_game_metadata(const QString& collection_name, const QString& game_path,
                           const QString& game_title, const QVariantMap& changes, bool scraped)
{
    split_legacy_edits_file();
    if (changes.isEmpty())
        return {};
    const QString wanted = MetaFile::is_uri(game_path) ? game_path : ::clean_abs_path(QFileInfo(game_path));
    // into the collection's file in the game directory (the one Pegasus
    // reads), the game's block added when it's new
    int coll_index = -1;
    MetaFile file = override_file(collection_name, coll_index);
    if (coll_index < 0)
        return QStringLiteral("Could not write the game directory file for `%1`").arg(collection_name);
    const int block_index = override_game_block(file, coll_index, wanted, game_title);
    if (block_index < 0)
        return QStringLiteral("Could not add `%1` to `%2`").arg(game_title, file.path);
    // a hand edit: a copy of the file first (the scraper backs up once a run)
    if (!scraped)
        providers::gamedir::backup(file.path, QStringLiteral("edit"));
    const QString error = apply_changes(file, static_cast<size_t>(block_index), changes, QStringLiteral("title"), scraped);
    if (error.isEmpty())
        Log::info(QStringLiteral("Metadata"), LOGMSG("`%1` saved in `%2`").arg(game_title, file.path));
    return error;
}


QString save_collection_metadata(const QString& collection_name, const QVariantMap& changes)
{
    if (changes.isEmpty())
        return {};
    // a new name is the one change that has to go into the files defining
    // the collection (it's their key); everything else goes on top, into the
    // collection's override file
    const QString new_name = changes.value(QStringLiteral("name")).toString().simplified();
    QVariantMap rest = changes;
    rest.remove(QStringLiteral("name"));

    if (!new_name.isEmpty() && new_name != collection_name) {
        std::vector<MetaFile> files = files_defining(collection_name);
        if (files.empty())
            return QStringLiteral("No metadata file defines the collection `%1`").arg(collection_name);
        for (MetaFile& file : files) {
            for (size_t b = 0; b < file.blocks.size(); b++) {
                if (file.blocks[b].is_collection && file.blocks[b].name == collection_name)
                    file.set_header(b, new_name);
            }
            providers::gamedir::backup(file.path, QStringLiteral("edit"));
            const QString error = file.save();
            if (!error.isEmpty())
                return error;
        }
        QMutexLocker lock(&collection_files_mutex());
        HashMap<QString, QStringList>& cache = collection_files_cache();
        const auto it = cache.find(collection_name);
        if (it != cache.end()) {
            cache[new_name] = it->second;
            cache.erase(collection_name);
        }
    }
    if (rest.isEmpty())
        return {};
    const QString name = (!new_name.isEmpty() && new_name != collection_name) ? new_name : collection_name;
    int coll_index = -1;
    MetaFile file = override_file(name, coll_index);
    if (coll_index < 0)
        return QStringLiteral("Could not write the game directory file for `%1`").arg(name);
    const QDir base(file.dir);
    for (auto it = rest.cbegin(); it != rest.cend(); ++it) {
        const QString key = canonical_key(it.key());
        file.set_entry(static_cast<size_t>(coll_index), aliases_of(key), key, value_lines(key, it.value(), base));
    }
    providers::gamedir::backup(file.path, QStringLiteral("edit"));
    return file.save();
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
        game_file_text::fill_in(arg, game_file);
    }
    if (args.isEmpty() || args.first() != QLatin1String("am"))
        return QStringLiteral("Only 'am start' commands can be tested here");
    args.removeFirst();  // "am"
    return ::android::run_am_call(args);
}



// -- Android collections ------------------------------------------------------------

namespace {
QString package_of_command(const QString& command)
{
    static const QRegularExpression rx(QStringLiteral("(?:^|\\s)-[np]\\s+\"?([A-Za-z0-9_.]+)"));
    const auto match = rx.match(command);
    return match.hasMatch() ? match.captured(1) : QString();
}

/// The package a game of an Android collection opens, from its files
QString package_of_files(const QStringList& files)
{
    for (const QString& file : files) {
        if (file.startsWith(QLatin1String("android:")))
            return file.mid(8);
        if (file.endsWith(QLatin1String(".app"), Qt::CaseInsensitive)) {
            const QString text = game_file_text::first_line(file);
            if (!text.isEmpty())
                return text.section(QLatin1Char('/'), 0, 0);
            return QFileInfo(file).completeBaseName();
        }
    }
    return {};
}
} // namespace


QSet<QString> emulator_packages()
{
    init_esde_resources();
    HashMap<QString, QStringList> emulators;
    const QString tag = QStringLiteral("Library");
    const QString esde_dir = esde_default_dir();
    if (!esde_dir.isEmpty())
        load_find_rules(tag, esde_dir % QStringLiteral("/custom_systems/es_find_rules.xml"), emulators);
    load_find_rules(tag, esde_list_file(QStringLiteral("es_find_rules.xml")), emulators);

    QSet<QString> out;
    for (const auto& pair : emulators) {
        for (const QString& entry : pair.second) {
            const QString package = entry.section(QLatin1Char('/'), 0, 0).trimmed();
            if (!package.isEmpty())
                out.insert(package);
        }
    }
    return out;
}


std::vector<AndroidApp> android_apps()
{
    std::vector<AndroidApp> out;
    for (const QString& line : ::android::android_app_list()) {
        const QStringList parts = line.split(QLatin1Char('\t'));
        if (parts.size() < 4)
            continue;
        AndroidApp app;
        app.label = parts.at(0);
        app.package = parts.at(1);
        app.component = parts.at(2);
        app.game = parts.at(3) == QLatin1String("1");
        out.push_back(std::move(app));
    }
    std::sort(out.begin(), out.end(), [](const AndroidApp& a, const AndroidApp& b) {
        if (a.game != b.game)
            return a.game;
        return a.label.compare(b.label, Qt::CaseInsensitive) < 0;
    });
    return out;
}


QString android_app_command(const QString& component)
{
    return QStringLiteral("am start --user 0 -a android.intent.action.MAIN -c android.intent.category.LAUNCHER -n ") % component;
}


std::vector<std::pair<QString, QString>> android_game_apps(const QString& collection_name)
{
    std::vector<std::pair<QString, QString>> out;
    for (const MetaFile& file : files_defining(collection_name)) {
        for (const MetaBlock& block : file.blocks) {
            if (block.is_collection || block.collection_index < 0
                || file.blocks[static_cast<size_t>(block.collection_index)].name != collection_name)
                continue;
            const QStringList files = file.game_files(block);
            if (files.isEmpty())
                continue;
            const MetaEntry* launch = block.entry(QStringLiteral("launch"));
            QString package = launch ? package_of_command(launch->values.join(QLatin1Char(' '))) : QString();
            if (package.isEmpty())
                package = package_of_files(files);
            if (!package.isEmpty())
                out.emplace_back(files.first(), package);
        }
    }
    return out;
}


int match_android_games(const QString& collection_name)
{
    const std::vector<AndroidApp> apps = android_apps();
    HashMap<QString, const AndroidApp*> by_package;
    HashMap<QString, const AndroidApp*> by_name;
    for (const AndroidApp& app : apps) {
        by_package.emplace(app.package, &app);
        by_name.emplace(normalize_name(app.label), &app);
    }

    // the games that have an emulator already, in their own file or the override file
    QSet<QString> has_launch;
    for (const GameEmulator& e : game_emulators(collection_name))
        has_launch.insert(e.path);

    // the matches go to the override file: your files are never written to
    int coll_index = -1;
    MetaFile over = override_file(collection_name, coll_index);
    int matched = 0;
    for (const MetaFile& file : files_defining(collection_name)) {
        for (size_t b = 0; b < file.blocks.size(); b++) {
            const MetaBlock& block = file.blocks[b];
            if (block.is_collection || block.collection_index < 0
                || file.blocks[static_cast<size_t>(block.collection_index)].name != collection_name)
                continue;
            const QStringList game_files = file.game_files(block);
            if (game_files.isEmpty() || has_launch.contains(game_files.first()))
                continue;

            // by package first, then by name
            const AndroidApp* app = nullptr;
            const QString package = package_of_files(file.game_files(block));
            if (!package.isEmpty()) {
                const auto it = by_package.find(package);
                if (it != by_package.cend())
                    app = it->second;
            }
            if (!app) {
                const QString title = normalize_name(block.name);
                const auto it = by_name.find(title);
                if (it != by_name.cend()) {
                    app = it->second;
                }
                else if (title.length() >= 5) {
                    for (const AndroidApp& candidate : apps) {
                        const QString name = normalize_name(candidate.label);
                        if (name.length() >= 5 && (name.startsWith(title) || title.startsWith(name))) {
                            app = &candidate;
                            break;
                        }
                    }
                }
            }
            if (!app)
                continue;

            const int bi = override_game_block(over, coll_index, game_files.first(), block.name);
            if (bi < 0)
                continue;
            over.set_launch(bi, android_app_command(app->component));
            matched++;
        }
    }
    if (matched > 0 && !over.save().isEmpty())
        return 0;
    return matched;
}


int add_android_games(const QString& collection_name)
{
    std::vector<MetaFile> files = files_defining(collection_name);
    if (files.empty())
        return 0;

    // the apps the collection already has
    QSet<QString> present;
    for (const MetaFile& file : files) {
        for (const MetaBlock& block : file.blocks) {
            if (block.is_collection)
                continue;
            const MetaEntry* launch = block.entry(QStringLiteral("launch"));
            const QString from_launch = launch ? package_of_command(launch->values.join(QLatin1Char(' '))) : QString();
            if (!from_launch.isEmpty())
                present.insert(from_launch);
            const QString from_files = package_of_files(file.game_files(block));
            if (!from_files.isEmpty())
                present.insert(from_files);
        }
    }

    // into the collection's file in the game directory
    int collection_index = -1;
    MetaFile target = override_file(collection_name, collection_index);
    if (collection_index < 0)
        return 0;
    for (const MetaBlock& block : target.blocks) {
        if (block.is_collection)
            continue;
        const QString from_files = package_of_files(target.game_files(block));
        if (!from_files.isEmpty())
            present.insert(from_files);
    }

    int added = 0;
    for (const AndroidApp& app : android_apps()) {
        if (!app.game || present.contains(app.package))
            continue;
        target.add_game(collection_index, app.label, QStringLiteral("android:") % app.package,
                        android_app_command(app.component));
        added++;
    }
    if (added > 0 && !target.save().isEmpty())
        return 0;
    return added;
}

} // namespace esde
} // namespace providers
