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
    // emulator entry ("package/activity") -> what to launch on this device:
    // the entry itself, or the app's own game screen if that's named
    // differently in the installed build; missing entries aren't installed
    HashMap<QString, QString> resolved_components;
    // apps picked with "Other installed app": package -> its game screen
    mutable HashMap<QString, QString> app_components;
    HashMap<QString, QStringList> emulators;  // emulator name -> "package/activity" entries
    QStringList ignored_dirs;  // folders skipped when looking for games and media
};

/// Whether the path is one of the ignored folders or inside one
bool is_ignored(const QString& path, const QStringList& ignored_dirs)
{
    for (const QString& dir : ignored_dirs) {
        if (path.compare(dir, Qt::CaseInsensitive) == 0
            || path.startsWith(dir % QLatin1Char('/'), Qt::CaseInsensitive))
            return true;
    }
    return false;
}


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

    // ES-DE's Android apps: the game file holds the app's package name
    if (!tokens.isEmpty() && tokens.first().startsWith(QLatin1String("%ANDROIDAPP%="))) {
        emulator_installed = true;
        return QStringLiteral("am start -a android.intent.action.MAIN -c android.intent.category.LAUNCHER {inject-app:{file.path}}");
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

    // prefer the first of the emulator's known activities that exists on this
    // device (checking the activity, not just the package, as some releases
    // of an emulator name their activity differently)
    QString component = emu_it->second.first();
    emulator_installed = false;
    for (const QString& entry : emu_it->second) {
        const auto resolved_it = ctx.resolved_components.find(entry);
        if (resolved_it != ctx.resolved_components.cend()) {
            component = resolved_it->second;
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
        // %INJECT%=<file>: the contents of that file (eg. a Steam app id in
        // a .steam file), filled in for each game by resolve_injects()
        const bool inject = value.startsWith(QLatin1String("%INJECT%="));
        if (inject)
            value = value.mid(9);
        if (!expand_variables(value, package, rom_root, ctx.primary_storage)) {
            error = QStringLiteral("unsupported variable in `%1`").arg(token);
            return {};
        }

        if (inject)
            value = QStringLiteral("{inject:") % value % QLatin1Char('}');

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

/// Fills in the {inject:<file>} and {inject-app:<file>} parts of a command
/// with the contents of that file, for one game
QString resolve_injects(const QString& cmd, const QFileInfo& game_file)
{
    if (!cmd.contains(QLatin1String("{inject")))
        return cmd;

    const auto file_text = [&game_file](QString expr) {
        expr.replace(QLatin1String("{file.path}"), game_file.absoluteFilePath())
            .replace(QLatin1String("{file.basename}"), game_file.completeBaseName())
            .replace(QLatin1String("{file.name}"), game_file.fileName())
            .replace(QLatin1String("{file.dir}"), game_file.absolutePath());
        const QFileInfo finfo(QDir(game_file.absolutePath()), expr);
        if (!finfo.isFile())
            return QString();
        QFile file(finfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();
        return QString::fromUtf8(file.read(4096)).trimmed();
    };

    QString out = cmd;
    static const QRegularExpression rx(QStringLiteral("\\{inject(-app)?:((?:\\{[^}]*\\}|[^{}])*)\\}"));
    QRegularExpressionMatch match;
    int from = 0;
    while ((match = rx.match(out, from)).hasMatch()) {
        QString value = file_text(match.captured(2));
        QString replacement;
        if (!match.captured(1).isEmpty()) {
            // an app: "package" or "package/activity"; an empty file means
            // the file name is the package
            if (value.isEmpty())
                value = game_file.completeBaseName();
            replacement = value.contains(QLatin1Char('/'))
                ? QStringLiteral("-n ") % quote_arg(value)
                : QStringLiteral("-p ") % quote_arg(value);
        }
        else {
            replacement = quote_arg(value);
        }
        out.replace(match.capturedStart(), match.capturedLength(), replacement);
        from = match.capturedStart() + replacement.length();
    }
    return out;
}

/// The command for a label chosen in ES-DE (system-wide or per game), or else
/// the system's default command; if the default emulator isn't installed, the
/// first alternative whose emulator is installed is used instead
/// An app picked with "Other installed app", saved as the emulator label
/// "@app:<package>:<style>:<app name>"
struct AppChoice {
    QString package;
    QString style;  ///< view-saf, view-uri or bootpath
    QString name;
};

bool parse_app_label(const QString& label, AppChoice& out)
{
    if (!label.startsWith(QLatin1String("@app:")))
        return false;
    const QStringList parts = label.split(QLatin1Char(':'));
    if (parts.size() < 3 || parts.at(1).isEmpty())
        return false;
    out.package = parts.at(1);
    out.style = parts.at(2);
    out.name = parts.mid(3).join(QLatin1Char(':'));
    return true;
}

/// The launch command for a picked app, in one of the ways ES-DE launches
/// standalone emulators
QString app_command(const AppChoice& app, const Context& ctx, QString& error)
{
    auto it = ctx.app_components.find(app.package);
    if (it == ctx.app_components.end())
        it = ctx.app_components.emplace(app.package, ::android::find_game_activity(app.package)).first;
    const QString& component = it->second;
    if (component.isEmpty()) {
        error = QStringLiteral("the app `%1` isn't installed").arg(app.package);
        return {};
    }

    QStringList args { QStringLiteral("am"), QStringLiteral("start"), QStringLiteral("-n"), component };
    if (app.style == QLatin1String("view-uri")) {
        // the file shared directly by Pegasus
        args << QStringLiteral("-a") << QStringLiteral("android.intent.action.VIEW")
             << QStringLiteral("-d") << QStringLiteral("{file.uri}");
    }
    else if (app.style == QLatin1String("bootpath")) {
        // the AetherSX2 / NetherSX2 way
        args << QStringLiteral("--activity-clear-task") << QStringLiteral("--activity-clear-top")
             << QStringLiteral("-a") << QStringLiteral("android.intent.action.MAIN")
             << QStringLiteral("-e") << QStringLiteral("bootPath") << QStringLiteral("{file.documenturi}");
    }
    else {
        // a storage link to the file; the emulator needs access to the system folder
        args << QStringLiteral("-a") << QStringLiteral("android.intent.action.VIEW")
             << QStringLiteral("-d") << QStringLiteral("{file.documenturi}");
    }

    QStringList quoted;
    for (const QString& arg : args)
        quoted.append(quote_arg(arg));
    return quoted.join(QLatin1Char(' '));
}

QString pick_command(const EsdeSystem& sys, const QString& label, const Context& ctx, const QString& rom_root)
{
    QString error;
    bool installed = false;

    AppChoice app;
    if (parse_app_label(label, app)) {
        const QString result = app_command(app, ctx, error);
        if (!result.isEmpty())
            return result;
        Log::warning(ctx.log_tag, LOGMSG("`%1`: %2, using the default emulator").arg(sys.name, error));
    }

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

/// `to_front`: what's found here is put ahead of the art found before, so a
/// source read later wins
void find_media(const QString& system_media_dir, const QString& system_dir,
                const HashMap<QString, model::Game*>& extless_path_to_game, size_t& found_assets,
                bool to_front = false, const QStringList& ignored_dirs = {})
{
    if (!QFileInfo(system_media_dir).isDir() || is_ignored(QDir::cleanPath(system_media_dir), ignored_dirs))
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
                if (!ignored_dirs.isEmpty() && is_ignored(::clean_abs_path(finfo), ignored_dirs))
                    continue;

                // the media tree mirrors the ROM folder, eg. covers/Multidisk/Game.png
                const QString rel_dir = type_dir.relativeFilePath(finfo.absolutePath());
                const QString game_key = QDir::cleanPath(system_dir % QLatin1Char('/') % rel_dir
                                                         % QLatin1Char('/') % finfo.completeBaseName());
                const auto game_it = extless_path_to_game.find(game_key);
                if (game_it == extless_path_to_game.cend())
                    continue;

                if (to_front)
                    game_it->second->assetsMut().add_file_first(asset_entry.first, file_it.filePath());
                else
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
            if (to_front || !extra.contains(QStringLiteral("manual"))) {
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

    // which of the emulators' activities exist on this device, asked in one go
    QStringList all_components;
    for (const auto& entry : ctx.emulators)
        all_components.append(entry.second);
    all_components.removeDuplicates();
    const QStringList resolved = ::android::resolve_components(all_components);
    for (int i = 0; i < all_components.size() && i < resolved.size(); i++) {
        if (resolved.at(i).isEmpty())
            continue;
        ctx.resolved_components.emplace(all_components.at(i), resolved.at(i));
        if (resolved.at(i) != all_components.at(i)) {
            Log::info(ctx.log_tag, LOGMSG("Emulator `%1` is installed with a different game screen, using `%2`")
                .arg(all_components.at(i), resolved.at(i)));
        }
    }
    Log::info(ctx.log_tag, LOGMSG("%1 of %2 known emulator entries are installed")
        .arg(QString::number(ctx.resolved_components.size()), QString::number(all_components.size())));
}

Context make_context(const QString& log_tag)
{
    Context ctx;
    ctx.log_tag = log_tag;
    ctx.primary_storage = ::android::primary_storage_path();
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

/// What a system scan found, for the metadata and media steps after it
struct ScanOutput {
    HashMap<QString, model::Game*> extless_path_to_game;
    std::vector<model::Game*> multi_disc_games;  ///< games made of several discs
};

/// The short name given to a system's collection. Themes pick system logos
/// and art (and XboxOSv2 its RetroAchievements console) by short name; ES-DE
/// names a few systems differently from what Pegasus themes use, or splits
/// them into regional or hardware variants themes have no art for, so those
/// get the name of the system themes know. The ES-DE name is still used for
/// everything else (emulator choices, gamelists, media folders).
QString theme_short_name(const QString& esde_name)
{
    static const HashMap<QString, QString> NAMES {
        { QStringLiteral("n3ds"), QStringLiteral("3ds") },
        { QStringLiteral("psvita"), QStringLiteral("vita") },
        { QStringLiteral("pcfx"), QStringLiteral("pc-fx") },
        { QStringLiteral("tg-cd"), QStringLiteral("pcenginecd") },
        { QStringLiteral("megacd"), QStringLiteral("segacd") },
        { QStringLiteral("megacdjp"), QStringLiteral("segacd") },
        { QStringLiteral("famicom"), QStringLiteral("nes") },
        { QStringLiteral("mark3"), QStringLiteral("mastersystem") },
        { QStringLiteral("sega32xjp"), QStringLiteral("sega32x") },
        { QStringLiteral("sega32xna"), QStringLiteral("sega32x") },
        { QStringLiteral("neogeocdjp"), QStringLiteral("neogeocd") },
        { QStringLiteral("satellaview"), QStringLiteral("snes") },
        { QStringLiteral("sufami"), QStringLiteral("snes") },
        { QStringLiteral("amiga600"), QStringLiteral("amiga") },
        { QStringLiteral("amiga1200"), QStringLiteral("amiga") },
        { QStringLiteral("amigacd32"), QStringLiteral("amiga") },
        { QStringLiteral("cdtv"), QStringLiteral("amiga") },
        { QStringLiteral("apple2gs"), QStringLiteral("apple2") },
        { QStringLiteral("msxturbor"), QStringLiteral("msx2") },
        { QStringLiteral("mame-advmame"), QStringLiteral("mame") },
        { QStringLiteral("naomi"), QStringLiteral("arcade") },
        { QStringLiteral("naomi2"), QStringLiteral("arcade") },
        { QStringLiteral("naomigd"), QStringLiteral("arcade") },
        { QStringLiteral("stv"), QStringLiteral("arcade") },
        { QStringLiteral("triforce"), QStringLiteral("arcade") },
        { QStringLiteral("type-x"), QStringLiteral("arcade") },
        { QStringLiteral("consolearcade"), QStringLiteral("arcade") },
        { QStringLiteral("pcarcade"), QStringLiteral("arcade") },
        { QStringLiteral("lcdgames"), QStringLiteral("gameandwatch") },
        { QStringLiteral("fpinball"), QStringLiteral("futurepinball") },
        { QStringLiteral("dos"), QStringLiteral("pc") },
        { QStringLiteral("doom"), QStringLiteral("ports") },
        { QStringLiteral("quake"), QStringLiteral("ports") },
    };
    const auto it = NAMES.find(esde_name);
    return it != NAMES.cend() ? it->second : esde_name;
}

/// Removes a disc tag like "(Disc 1)", "[CD2]", "(Disk 1 of 3)" or "(Side B)"
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

/// Scans one system's folders for games and sets their launch commands.
/// Multi-disc games become one game: the discs and tracks listed in an
/// .m3u, .cue or .gdi file are not listed on their own, and files that only
/// differ in a disc tag ("Game (Disc 1)", "Game (Disc 2)") are combined.
size_t scan_system(providers::SearchContext& sctx, const Context& ctx, const EsdeSystem& sys,
                   const std::vector<std::pair<QString, QString>>& dirs,  // (system dir, ROMs folder)
                   const GamelistInfo& gamelist, const QString& system_label, bool show_hidden,
                   bool override_existing,
                   const QString& preset_cmd,  // used instead of the system's emulator, if set
                   ScanOutput& output)
{
    const QStringList extensions = parse_extensions(sys.extensions);

    model::Collection& collection = *sctx.get_or_create_collection(sys.fullname.isEmpty() ? sys.name : sys.fullname);
    collection.setShortName(theme_short_name(sys.name));

    size_t found_games = 0;
    for (const auto& dir_entry : dirs) {
        const QString& system_dir = dir_entry.first;
        const QString& rom_root = dir_entry.second;

        const QString default_cmd = preset_cmd.isEmpty()
            ? pick_command(sys, system_label, ctx, rom_root)
            : preset_cmd;
        if (default_cmd.isEmpty())
            Log::warning(ctx.log_tag, LOGMSG("`%1`: none of its emulators can be launched from Pegasus").arg(sys.name));
        if (collection.commonLaunchCmd().isEmpty())
            collection.setCommonLaunchCmd(default_cmd);

        // 1. every file (or folder) with one of the system's extensions
        std::vector<QFileInfo> candidates;
        constexpr auto entry_filters = QDir::Files | QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot;
        constexpr auto entry_flags = QDirIterator::FollowSymlinks | QDirIterator::Subdirectories;
        QDirIterator file_it(system_dir, entry_filters, entry_flags);
        while (file_it.hasNext()) {
            file_it.next();
            const QFileInfo finfo = file_it.fileInfo();
            if (!has_extension(finfo.fileName(), extensions))
                continue;
            if (is_ignored(::clean_abs_path(finfo), ctx.ignored_dirs))
                continue;
            if (!show_hidden && gamelist.hidden_paths.contains(::clean_abs_path(finfo)))
                continue;
            candidates.push_back(finfo);
        }
        std::sort(candidates.begin(), candidates.end(), [](const QFileInfo& a, const QFileInfo& b) {
            return a.absoluteFilePath().compare(b.absoluteFilePath(), Qt::CaseInsensitive) < 0;
        });

        // 2. discs and tracks that an .m3u, .cue or .gdi already covers
        QSet<QString> covered;
        for (const QFileInfo& finfo : candidates) {
            if (finfo.isFile()) {
                for (const QString& path : referenced_files(finfo))
                    covered.insert(path);
            }
        }

        // 3. group the rest: one entry per game, discs of the same game together
        std::vector<std::vector<QFileInfo>> groups;
        HashMap<QString, size_t> disc_group_index;  // dir/title without disc tag/ext -> group
        for (const QFileInfo& finfo : candidates) {
            const QString path = ::clean_abs_path(finfo);
            if (covered.contains(path))
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

        // 4. the games
        for (const std::vector<QFileInfo>& group : groups) {
            const QFileInfo& first = group.front();
            const QString path = ::clean_abs_path(first);

            model::Game* game_ptr = sctx.game_by_filepath(path);
            const bool is_new = !game_ptr;
            if (is_new) {
                game_ptr = sctx.create_game_for(collection);
                sctx.game_add_filepath(*game_ptr, path);
            }
            sctx.game_add_to(*game_ptr, collection);

            if (group.size() > 1) {
                for (size_t i = 1; i < group.size(); i++) {
                    const QString disc_path = ::clean_abs_path(group[i]);
                    if (!sctx.game_by_filepath(disc_path))  // unless another source already has it
                        sctx.game_add_filepath(*game_ptr, disc_path);
                }
                if (is_new)
                    game_ptr->setTitle(strip_disc_tag(first.completeBaseName()));
                output.multi_disc_games.push_back(game_ptr);
            }

            // A game-specific emulator picked in ES-DE, or this folder's default.
            // Games already found by another source (eg. a Pegasus metadata file
            // with its own launch command) keep theirs, unless override_existing
            const bool may_set = is_new || override_existing || game_ptr->launchCmd().isEmpty();
            const auto alt_it = gamelist.game_alt_labels.find(path);
            if (alt_it != gamelist.game_alt_labels.cend() && may_set) {
                const QString cmd = pick_command(sys, alt_it->second, ctx, rom_root);
                if (!cmd.isEmpty())
                    game_ptr->setLaunchCmd(resolve_injects(cmd, first));
            }
            else if (default_cmd.contains(QLatin1String("{inject")) && may_set) {
                // the command needs something from this game's file
                game_ptr->setLaunchCmd(resolve_injects(default_cmd, first));
            }
            else if (is_new ? default_cmd != collection.commonLaunchCmd() : may_set) {
                game_ptr->setLaunchCmd(default_cmd);
            }

            // media is matched by the file name of any disc, or of the playlist
            for (const QFileInfo& finfo : group) {
                QString extless = ::clean_abs_path(finfo);
                if (!finfo.isDir())
                    extless = ::clean_abs_dir(finfo) % QLatin1Char('/') % finfo.completeBaseName();
                output.extless_path_to_game.emplace(std::move(extless), game_ptr);
                if (group.size() > 1) {  // eg. covers/Game.png for "Game (Disc 1).chd"
                    output.extless_path_to_game.emplace(
                        ::clean_abs_dir(finfo) % QLatin1Char('/') % strip_disc_tag(finfo.completeBaseName()), game_ptr);
                }
            }
            found_games++;
        }
    }
    return found_games;
}

/// After the metadata step: names from a gamelist or metadata file may still
/// carry the disc tag of the first disc ("Final Fantasy VII (Disc 1)")
void clean_disc_titles(const std::vector<model::Game*>& games)
{
    for (model::Game* const game : games) {
        const QString clean = strip_disc_tag(game->title());
        if (clean != game->title() && !clean.isEmpty())
            game->setTitle(clean);
    }
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
    const QStringList& library_dirs, const HashMap<QString, QString>& aliases,
    const QStringList& ignored_dirs = {})
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
            const QString sub_path = QDir::cleanPath(dir % QLatin1Char('/') % sub);
            if (!system.isEmpty() && !is_ignored(sub_path, ignored_dirs))
                out[system].emplace_back(sub_path, dir);
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

/// Subfolders of `parent` whose name is recognized as the system `system_name`
/// (ES-DE's name, the full name or a common variant, ignoring capitals)
QStringList system_subdirs(const QString& parent, const QString& system_name,
                           const HashMap<QString, QString>& aliases)
{
    QStringList out;
    if (parent.isEmpty())
        return out;
    for (const QString& sub : QDir(parent).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto it = aliases.find(normalize_name(sub));
        if (it != aliases.cend() && it->second == system_name)
            out.append(parent % QLatin1Char('/') % sub);
    }
    return out;
}

QString system_of_folder(const QString& folder_name, const HashMap<QString, QString>& aliases)
{
    const auto it = aliases.find(normalize_name(folder_name));
    return it != aliases.cend() ? it->second : QString();
}

/// What was picked in the "ES-DE import" section, sorted out by kind
/// Each entry is (folder, system); an empty system means all systems.
/// Kept in the order picked, as a later pick wins over an earlier one.
struct EsdeImports {
    QStringList roots;  ///< whole ES-DE folders, for their settings
    std::vector<std::pair<QString, QString>> gamelists;  ///< ES-DE folders with gamelists/
    std::vector<std::pair<QString, QString>> media;      ///< folders with <system>/<type>/ (all systems) or <type>/

    /// The ES-DE folders whose gamelist of this system should be read, the
    /// latest pick first
    QStringList gamelist_roots(const QString& system_name) const {
        QStringList out;
        for (auto it = gamelists.crbegin(); it != gamelists.crend(); ++it) {
            if (it->second.isEmpty() || it->second == system_name)
                out.append(it->first);
        }
        out.removeDuplicates();
        return out;
    }
};

EsdeImports read_esde_imports(const QStringList& picked, const HashMap<QString, QString>& aliases)
{
    EsdeImports out;
    const auto parent_of = [](const QString& path) { return QFileInfo(path).absolutePath(); };
    const auto is_named = [](const QString& path, const char* name) {
        return QFileInfo(path).fileName().compare(QLatin1String(name), Qt::CaseInsensitive) == 0;
    };

    for (const QString& raw : picked) {
        const QFileInfo finfo(raw);
        if (!finfo.exists())
            continue;
        const QString path = ::clean_abs_path(finfo);

        // a single gamelist.xml (ES-DE/gamelists/<system>/gamelist.xml)
        if (finfo.isFile()) {
            const QString dir = parent_of(path);
            if (finfo.fileName().compare(QLatin1String("gamelist.xml"), Qt::CaseInsensitive) == 0
                && is_named(parent_of(dir), "gamelists")) {
                const QString system = system_of_folder(QFileInfo(dir).fileName(), aliases);
                if (!system.isEmpty())
                    out.gamelists.emplace_back(parent_of(parent_of(dir)), system);
            }
            continue;
        }

        // the whole ES-DE folder
        const QString gamelists_dir = find_subdir(path, { QStringLiteral("gamelists") });
        const QString media_dir = find_subdir(path, { QStringLiteral("downloaded_media") });
        if (!gamelists_dir.isEmpty() || !media_dir.isEmpty()) {
            out.roots.append(path);
            if (!gamelists_dir.isEmpty())
                out.gamelists.emplace_back(path, QString());
            const HashMap<QString, QString> settings = read_esde_settings(path);
            const auto media_it = settings.find(QStringLiteral("MediaDirectory"));
            if (media_it != settings.cend() && QFileInfo(media_it->second).isDir())
                out.media.emplace_back(QDir::cleanPath(media_it->second), QString());
            else if (!media_dir.isEmpty())
                out.media.emplace_back(media_dir, QString());
            continue;
        }

        if (is_named(path, "gamelists")) {             // ES-DE/gamelists
            out.gamelists.emplace_back(parent_of(path), QString());
            continue;
        }
        if (is_named(parent_of(path), "gamelists")) {  // ES-DE/gamelists/<system>
            const QString system = system_of_folder(finfo.fileName(), aliases);
            if (!system.isEmpty())
                out.gamelists.emplace_back(parent_of(parent_of(path)), system);
            continue;
        }
        if (is_named(path, "downloaded_media")) {      // ES-DE/downloaded_media
            out.media.emplace_back(path, QString());
            continue;
        }

        // ES-DE/downloaded_media/<system>, or any folder of system folders
        const QString own_system = system_of_folder(finfo.fileName(), aliases);
        if (!own_system.isEmpty())
            out.media.emplace_back(path, own_system);
        else
            out.media.emplace_back(path, QString());
    }
    return out;
}

/// Media folders inside the ROM folders: <system>/media/ (and the Skraper
/// names), and <ROMs folder>/media/<system>/
QStringList rom_folder_media_dirs(const QString& system_dir, const QString& rom_root,
                                  const QString& system_name, const HashMap<QString, QString>& aliases)
{
    QStringList out;
    for (const QString& name : { QStringLiteral("media"), QStringLiteral(".media"), QStringLiteral("skraper") }) {
        const QString dir = find_subdir(system_dir, { name });
        if (!dir.isEmpty())
            out.append(dir);
    }
    const QString root_media = find_subdir(rom_root, { QStringLiteral("media") });
    if (!root_media.isEmpty())
        out += system_subdirs(root_media, system_name, aliases);
    return out;
}

/// The folders of a picked media folder that hold this system's art: the
/// folder itself if it's named after the system, else <system>/, <system>/media/
/// or media/<system>/ in it
QStringList picked_media_dirs(const QString& picked, const QString& system_name,
                              const HashMap<QString, QString>& aliases)
{
    QStringList out;
    const QFileInfo finfo(picked);
    if (!finfo.isDir())
        return out;

    QStringList system_dirs;
    if (system_of_folder(finfo.fileName(), aliases) == system_name)
        system_dirs.append(::clean_abs_path(finfo));
    system_dirs += system_subdirs(picked, system_name, aliases);
    const QString media = find_subdir(picked, { QStringLiteral("media") });
    if (!media.isEmpty())
        system_dirs += system_subdirs(media, system_name, aliases);

    for (const QString& dir : system_dirs) {
        out.append(dir);
        const QString inner = find_subdir(dir, { QStringLiteral("media") });
        if (!inner.isEmpty())
            out.append(inner);
    }
    out.removeDuplicates();
    return out;
}

/// The "package/activity" a launch command starts ("am start -n ..."), or empty
QString command_component(const QString& cmd)
{
    const QStringList args = ::utils::tokenize_command(cmd);
    for (int i = 0; i + 1 < args.size(); i++) {
        if (args.at(i) == QLatin1String("-n") || args.at(i) == QLatin1String("--component"))
            return args.at(i + 1);
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

/// Reads the Pegasus metadata files in the library folders:
/// - games the library already found (by file) get the file's details, and
///   their own launch line if the entry has one (eg. a Windows game started
///   through GameNative, an Android app)
/// - games in them the library didn't find (files outside the system
///   folders or with other extensions, apps...) are added as they are, to the
///   library's collection of that system or else to the file's own collection
/// Games whose own launch line was used are added to `own_cmd_games`.
static size_t import_pegasus_metadata(const QStringList& metafiles, providers::SearchContext& sctx,
                                      const HashMap<QString, QString>& aliases,
                                      const HashMap<QString, EsdeSystem>& systems,
                                      const QStringList& ignored_dirs,
                                      QSet<model::Game*>& own_cmd_games, const QString& log_tag)
{
    if (metafiles.isEmpty())
        return 0;

    providers::SearchContext tmp_sctx(QStringList {});
    const providers::pegasus::Metadata metahelper(log_tag);
    for (const QString& path : metafiles) {
        Log::info(log_tag, LOGMSG("Reading `%1`").arg(::pretty_path(path)));
        metahelper.apply_metafile(path, tmp_sctx);  // its file filters aren't needed
    }

    // the files of each game in the metadata
    HashMap<model::Game*, std::vector<std::pair<QString, model::GameFile*>>> tmp_files;
    for (const auto& entry : tmp_sctx.current_filepath_to_entry_map())
        tmp_files[entry.second->parentGame()].emplace_back(entry.first, entry.second);

    size_t updated = 0;
    QSet<model::Game*> seen;
    for (const auto& coll_entry : tmp_sctx.current_collection_games()) {
        const model::Collection& tmp_coll = *coll_entry.first;
        QString system = system_of_folder(tmp_coll.shortName(), aliases);
        if (system.isEmpty())
            system = system_of_folder(tmp_coll.name(), aliases);

        for (model::Game* const src : coll_entry.second) {
            if (seen.contains(src))
                continue;
            seen.insert(src);

            const auto files_it = tmp_files.find(src);
            if (files_it == tmp_files.cend())
                continue;
            auto files = files_it->second;
            files.erase(std::remove_if(files.begin(), files.end(), [&ignored_dirs](const std::pair<QString, model::GameFile*>& f) {
                return !f.second->hasUri() && is_ignored(f.first, ignored_dirs);
            }), files.end());
            if (files.empty())
                continue;

            // an entry's own launch line, not the one it got from its collection
            const bool own_cmd = !src->launchCmd().isEmpty() && src->launchCmd() != tmp_coll.commonLaunchCmd();

            model::Game* dst = nullptr;
            for (const auto& f : files) {
                dst = sctx.game_by_filepath(f.first);
                if (dst)
                    break;
            }

            if (!dst) {
                // not found by the library: add it as it is
                model::Collection* coll = nullptr;
                const auto sys_it = systems.find(system);
                if (sys_it != systems.cend()) {
                    const EsdeSystem& sys = sys_it->second;
                    coll = sctx.get_or_create_collection(sys.fullname.isEmpty() ? sys.name : sys.fullname);
                    if (coll->shortName().isEmpty() || coll->shortName() == coll->name().toLower())
                        coll->setShortName(theme_short_name(sys.name));
                }
                else {
                    coll = sctx.get_or_create_collection(tmp_coll.name());
                    if (!tmp_coll.shortName().isEmpty())
                        coll->setShortName(tmp_coll.shortName());
                }
                dst = sctx.create_game_for(*coll);
                for (const auto& f : files) {
                    if (f.second->hasUri())
                        sctx.game_add_uri(*dst, f.second->uri());
                    else
                        sctx.game_add_filepath(*dst, f.first);
                }
                sctx.game_add_to(*dst, *coll);
                if (!own_cmd && !src->launchCmd().isEmpty()) {
                    dst->setLaunchCmd(src->launchCmd());
                    dst->setLaunchWorkdir(src->launchWorkdir());
                    dst->setLaunchCmdBasedir(src->launchCmdBasedir());
                }
            }

            copy_game_details(*src, *dst);
            if (own_cmd) {
                dst->setLaunchCmd(src->launchCmd());
                dst->setLaunchWorkdir(src->launchWorkdir());
                dst->setLaunchCmdBasedir(src->launchCmdBasedir());
                own_cmd_games.insert(dst);
            }
            updated++;
        }
    }

    tmp_sctx.discard_all();
    return updated;
}

/// The launch command of each system's collection in the user's Pegasus
/// metadata files (in the library folders and the 'Set game directories'
/// folders): system name -> command. Commands that start an app that isn't
/// installed are left out; one that names a renamed game screen of an
/// installed app is corrected.
static HashMap<QString, QString> metadata_commands(const HashMap<QString, QString>& aliases, const QString& log_tag)
{
    QStringList dirs = pegasusx::libraryRomDirs();
    {
        const providers::SearchContext probe;  // reads the 'Set game directories' list
        dirs += probe.root_game_dirs();
    }
    dirs.removeDuplicates();
    const QStringList metafiles = find_library_metafiles(dirs);
    if (metafiles.isEmpty())
        return {};

    HashMap<QString, QString> found;  // system -> command, as written
    {
        providers::SearchContext tmp_sctx(QStringList {});
        const providers::pegasus::Metadata metahelper(log_tag);
        for (const QString& path : metafiles)
            metahelper.apply_metafile(path, tmp_sctx);

        for (const auto& entry : tmp_sctx.current_collections()) {
            const model::Collection& collection = *entry.second;
            const QString cmd = collection.commonLaunchCmd().trimmed();
            if (cmd.isEmpty())
                continue;
            QString system = system_of_folder(collection.shortName(), aliases);
            if (system.isEmpty())
                system = system_of_folder(collection.name(), aliases);
            if (!system.isEmpty() && !found.count(system))
                found.emplace(system, cmd);
        }
        tmp_sctx.discard_all();
    }

    QStringList components;
    for (const auto& entry : found) {
        const QString component = command_component(entry.second);
        if (!component.isEmpty())
            components.append(component);
    }
    components.removeDuplicates();
    const QStringList resolved = ::android::resolve_components(components);

    HashMap<QString, QString> out;
    for (const auto& entry : found) {
        const QString component = command_component(entry.second);
        if (component.isEmpty()) {
            out.emplace(entry.first, entry.second);
            continue;
        }
        const int idx = components.indexOf(component);
        const QString now = (idx >= 0 && idx < resolved.size()) ? resolved.at(idx) : QString();
        if (now.isEmpty()) {
            Log::info(log_tag, LOGMSG("`%1`: the metadata's emulator `%2` isn't installed, using the system's emulator")
                .arg(entry.first, component));
            continue;
        }
        QString cmd = entry.second;
        cmd.replace(component, now);
        out.emplace(entry.first, cmd);
    }
    return out;
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
        ScanOutput scan;
        const size_t found_games = scan_system(sctx, ctx, sys, dirs, gamelist, system_label, show_hidden,
                                               picked_in_pegasusx, QString(), scan);
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
        clean_disc_titles(scan.multi_disc_games);

        // media from ES-DE/downloaded_media/<system>/<type>/
        for (const QString& system_dir : system_dirs)
            find_media(media_root % QLatin1Char('/') % sys.name, system_dir, scan.extless_path_to_game, found_assets);
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

/// The gamelists of the ES-DE folders picked in "ES-DE import"
static QStringList esde_import_cache_files()
{
    QStringList out;
    for (const QString& picked : pegasusx::esdeImports()) {
        const QFileInfo finfo(picked);
        if (finfo.isFile()) {
            out.append(::clean_abs_path(finfo));
            continue;
        }
        const QString dir = ::clean_abs_path(finfo);
        if (QFileInfo::exists(dir % QStringLiteral("/gamelist.xml")))
            out.append(dir % QStringLiteral("/gamelist.xml"));
        out += esde_cache_files(dir);
        if (finfo.fileName().compare(QLatin1String("gamelists"), Qt::CaseInsensitive) == 0)
            out += esde_cache_files(finfo.absolutePath());
    }
    out.removeDuplicates();
    return out;
}

QStringList LibraryProvider::cacheInputFiles() const
{
    // the settings file always counts, as the emulator choices also apply
    // to games found through 'Set game directories'
    QStringList out { pegasusx::settingsFilePath() };
    const QStringList library_dirs = pegasusx::libraryRomDirs();
    if (!library_dirs.isEmpty())
        out += esde_import_cache_files() + find_library_metafiles(library_dirs);
    return out;
}

/// Games that came from somewhere else than the library folders (eg. a
/// Pegasus metadata file found through 'Set game directories'), in a
/// collection recognized as a system:
/// - if an emulator was picked for that system in Pegasus X, they use it too
/// - otherwise, if their launch command starts an app that isn't installed
///   (eg. an emulator build that was renamed or removed), the system's
///   installed emulator is used instead, or the app's renamed game screen
static size_t fix_other_launch_commands(providers::SearchContext& sctx, const Context& ctx,
                                        const HashMap<QString, EsdeSystem>& systems,
                                        const HashMap<QString, QString>& aliases,
                                        const HashMap<QString, QString>& meta_cmds,
                                        const QSet<model::Game*>& library_games,
                                        const QSet<model::Game*>& keep_cmd_games = {})
{
    // a file of every game, for the ROMs folder some commands need
    HashMap<model::Game*, QString> game_paths;
    for (const auto& entry : sctx.current_filepath_to_entry_map())
        game_paths.emplace(entry.second->parentGame(), entry.first);

    // which apps the commands start, checked in one go
    QStringList components;
    for (const auto& entry : sctx.current_collection_games()) {
        for (model::Game* const game : entry.second) {
            const QString component = command_component(game->launchCmd());
            if (!component.isEmpty())
                components.append(component);
        }
    }
    components.removeDuplicates();
    HashMap<QString, QString> resolved;
    {
        const QStringList results = ::android::resolve_components(components);
        for (int i = 0; i < components.size() && i < results.size(); i++)
            resolved.emplace(components.at(i), results.at(i));
    }

    QSet<model::Game*> done;
    size_t changed = 0;
    for (const auto& entry : sctx.current_collection_games()) {
        const model::Collection& collection = *entry.first;
        QString system = system_of_folder(collection.shortName(), aliases);
        if (system.isEmpty())
            system = system_of_folder(collection.name(), aliases);
        const auto sys_it = systems.find(system);
        if (sys_it == systems.cend())
            continue;
        const EsdeSystem& sys = sys_it->second;
        const QString picked = pegasusx::systemEmulator(sys.name);

        for (model::Game* const game : entry.second) {
            if (library_games.contains(game) || done.contains(game))
                continue;
            done.insert(game);
            const bool keep_own = keep_cmd_games.contains(game);

            const auto path_it = game_paths.find(game);
            const QString rom_root = path_it != game_paths.cend()
                ? QFileInfo(QFileInfo(path_it->second).absolutePath()).absolutePath()
                : QString();
            const QString current = game->launchCmd();

            QString replacement;
            if (!picked.isEmpty() && !keep_own) {
                replacement = pick_command(sys, picked, ctx, rom_root);
                if (path_it != game_paths.cend())
                    replacement = resolve_injects(replacement, QFileInfo(path_it->second));
            }
            else {
                const QString component = command_component(current);
                if (component.isEmpty())
                    continue;
                const auto res_it = resolved.find(component);
                const QString found = res_it != resolved.cend() ? res_it->second : QString();
                if (found == component)
                    continue;  // installed as written
                if (!found.isEmpty()) {
                    replacement = current;
                    replacement.replace(component, found);
                }
                else {
                    const auto meta_it = meta_cmds.find(sys.name);
                    replacement = meta_it != meta_cmds.cend()
                        ? meta_it->second
                        : pick_command(sys, QString(), ctx, rom_root);
                }
            }
            if (replacement.isEmpty() || replacement == current)
                continue;

            game->setLaunchCmd(replacement);
            game->setLaunchWorkdir(QString());
            changed++;
        }
    }
    return changed;
}

Provider& LibraryProvider::run(SearchContext& sctx)
{
    const QStringList library_dirs = pegasusx::libraryRomDirs();
    Context ctx = make_context(display_name());

    HashMap<QString, EsdeSystem> systems;
    std::vector<QString> system_order;
    {
        // custom systems of a picked ES-DE folder, if any
        const QStringList picked = pegasusx::esdeImports();
        QString custom_dir;
        for (const QString& path : picked) {
            if (QFileInfo(path % QStringLiteral("/custom_systems")).isDir()) {
                custom_dir = path;
                break;
            }
        }
        load_config(ctx, custom_dir, systems, system_order);
    }
    const HashMap<QString, QString> aliases = build_aliases(systems, system_order);
    for (const QString& dir : pegasusx::ignoredDirs())
        ctx.ignored_dirs.append(QDir::cleanPath(dir));

    // the emulators named in the user's metadata files are each system's default
    const HashMap<QString, QString> meta_cmds = metadata_commands(aliases, display_name());

    if (library_dirs.isEmpty()) {
        const size_t fixed = fix_other_launch_commands(sctx, ctx, systems, aliases, meta_cmds, {});
        if (fixed > 0)
            Log::info(display_name(), LOGMSG("%1 games use the emulator picked in Pegasus X").arg(QString::number(fixed)));
        return *this;
    }

    // Only what was picked in "ES-DE import" is used from ES-DE
    const EsdeImports imports = read_esde_imports(pegasusx::esdeImports(), aliases);
    bool show_hidden = true;
    for (const QString& root : imports.roots) {
        const HashMap<QString, QString> esde_settings = read_esde_settings(root);
        const auto hidden_it = esde_settings.find(QStringLiteral("ShowHiddenGames"));
        if (hidden_it != esde_settings.cend() && hidden_it->second == QLatin1String("false"))
            show_hidden = false;
    }

    const auto library_systems = find_library_systems(library_dirs, aliases, ctx.ignored_dirs);
    Log::info(display_name(), LOGMSG("%1 system folders recognized").arg(QString::number(library_systems.size())));

    const QStringList media_dirs = pegasusx::libraryMediaDirs();

    size_t found_games_total = 0;
    size_t found_assets = 0;
    const float progress_step = library_systems.empty() ? 1.f : 1.f / library_systems.size();
    float progress = 0.f;

    struct FoundSystem {
        const EsdeSystem* sys;
        const std::vector<std::pair<QString, QString>>* dirs;
        ScanOutput scan;
    };
    std::vector<FoundSystem> found_systems;
    std::vector<model::Game*> all_multi_disc_games;
    QSet<model::Game*> library_games;

    // 1. games and their details: ES-DE gamelists (if picked), a gamelist.xml
    //    in the system folder (which wins over ES-DE's), then below, Pegasus
    //    metadata files (which win over both)
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

        const QStringList gamelist_roots = imports.gamelist_roots(sys.name);
        const GamelistInfo gamelist = gamelist_roots.isEmpty()
            ? GamelistInfo()
            : read_gamelist(gamelist_roots.first(), sys.name, system_dirs);

        // the emulator picked in Pegasus X, else the one in the metadata
        // files, else the one picked in ES-DE, else ES-DE's default
        QString system_label = pegasusx::systemEmulator(sys.name);
        QString preset_cmd;
        if (system_label.isEmpty()) {
            const auto meta_it = meta_cmds.find(sys.name);
            if (meta_it != meta_cmds.cend())
                preset_cmd = meta_it->second;
            system_label = gamelist.system_alt_label;
        }

        // The library's launch commands always win for the games in its
        // folders, also over a command from a metadata file found through
        // 'Set game directories', which may name an emulator that isn't installed
        FoundSystem found { &sys, &dirs, {} };
        const size_t found_games = scan_system(sctx, ctx, sys, dirs, gamelist, system_label, show_hidden,
                                               true, preset_cmd, found.scan);
        all_multi_disc_games.insert(all_multi_disc_games.end(),
                                    found.scan.multi_disc_games.cbegin(), found.scan.multi_disc_games.cend());
        if (found_games == 0)
            continue;
        found_games_total += found_games;
        Log::info(display_name(), LOGMSG("System `%1` provided %2 games").arg(sys.name, QString::number(found_games)));

        for (const auto& game_entry : found.scan.extless_path_to_game)
            library_games.insert(game_entry.second);

        const es2::Metadata metahelper(display_name(), std::vector<QString>(gamelist_roots.cbegin(), gamelist_roots.cend()));
        for (const QString& system_dir : system_dirs) {
            const es2::SystemEntry sysentry {
                sys.fullname, sys.name, system_dir, sys.extensions, sys.platform, QString(),
            };
            metahelper.find_metadata_for(sysentry, sctx);
        }
        found_systems.push_back(std::move(found));
    }

    QStringList metafiles = find_library_metafiles(library_dirs);
    metafiles.erase(std::remove_if(metafiles.begin(), metafiles.end(), [&ctx](const QString& path) {
        return is_ignored(path, ctx.ignored_dirs);
    }), metafiles.end());
    QSet<model::Game*> own_cmd_games;
    const size_t detailed = import_pegasus_metadata(metafiles, sctx, aliases, systems, ctx.ignored_dirs,
                                                    own_cmd_games, display_name());
    if (detailed > 0)
        Log::info(display_name(), LOGMSG("%1 games from Pegasus metadata files (%2 with their own launch line)")
            .arg(QString::number(detailed), QString::number(own_cmd_games.size())));

    // names from gamelists or metadata files may carry the first disc's tag
    clean_disc_titles(all_multi_disc_games);

    // 2. media, each source winning over the ones before it: art named in
    //    metadata files (already added), the ROM folders' own media folders,
    //    what was picked from ES-DE, then the picked media folders; within
    //    ES-DE picks and media folders, the one added later wins
    for (const FoundSystem& found : found_systems) {
        const EsdeSystem& sys = *found.sys;
        for (const auto& dir_entry : *found.dirs) {
            const QString& system_dir = dir_entry.first;
            const QString& rom_root = dir_entry.second;

            QStringList sources = rom_folder_media_dirs(system_dir, rom_root, sys.name, aliases);
            for (const auto& pick : imports.media) {
                if (pick.second.isEmpty())
                    sources += system_subdirs(pick.first, sys.name, aliases);
                else if (pick.second == sys.name)
                    sources.append(pick.first);
            }
            for (const QString& picked : media_dirs)
                sources += picked_media_dirs(picked, sys.name, aliases);

            // the same folder twice counts where it comes last
            QStringList ordered;
            for (int i = sources.size() - 1; i >= 0; i--) {
                if (!ordered.contains(sources.at(i)))
                    ordered.prepend(sources.at(i));
            }
            for (const QString& media_dir : ordered)
                find_media(media_dir, system_dir, found.scan.extless_path_to_game, found_assets, true, ctx.ignored_dirs);
        }
    }

    // 3. games from other places that belong to a known system
    const size_t fixed = fix_other_launch_commands(sctx, ctx, systems, aliases, meta_cmds, library_games, own_cmd_games);
    if (fixed > 0)
        Log::info(display_name(), LOGMSG("%1 games from other sources got a working emulator").arg(QString::number(fixed)));

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

    const HashMap<QString, QString> meta_cmds = metadata_commands(build_aliases(systems, order), ctx.log_tag);

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
            const bool retroarch = cmd.raw.startsWith(QLatin1String("%EMULATOR_RETROARCH%"));
            entry.options.push_back({ label, installed, retroarch });
            if (first_usable.isEmpty())
                first_usable = label;
            if (installed && entry.default_label.isEmpty())
                entry.default_label = label;
        }
        if (entry.default_label.isEmpty())
            entry.default_label = first_usable;
        if (meta_cmds.count(name))
            entry.default_label = QStringLiteral("your metadata");
        if (!entry.options.empty())
            out.push_back(std::move(entry));
    }
    return out;
}



// -- Testing a system's emulator from the settings screen ---------------------------

EmulatorTest test_emulator(const QString& system_name)
{
    init_esde_resources();
    Context ctx = make_context(QStringLiteral("Pegasus X"));
    EmulatorTest result;

    QString esde_dir;
    const providers::Provider* const esde_provider = find_provider("esde");
    if (esde_provider && esde_provider->enabled())
        esde_dir = find_esde_dir(esde_provider->options());

    HashMap<QString, EsdeSystem> systems;
    std::vector<QString> order;
    load_config(ctx, esde_dir, systems, order);

    const auto sys_it = systems.find(system_name);
    if (sys_it == systems.cend()) {
        result.error = QStringLiteral("Unknown system `%1`").arg(system_name);
        return result;
    }
    const EsdeSystem& sys = sys_it->second;

    // the system's folders: in the library, and in ES-DE's ROMs folders
    std::vector<std::pair<QString, QString>> dirs;
    const auto library_systems = find_library_systems(pegasusx::libraryRomDirs(), build_aliases(systems, order));
    const auto lib_it = library_systems.find(system_name);
    if (lib_it != library_systems.cend())
        dirs = lib_it->second;
    if (!esde_dir.isEmpty()) {
        const QStringList rom_roots = find_rom_roots(esde_provider->options(), read_esde_settings(esde_dir), ctx.primary_storage);
        for (const auto& entry : esde_system_dirs(sys, rom_roots))
            dirs.push_back(entry);
    }

    // the first game found
    const QStringList extensions = parse_extensions(sys.extensions);
    QFileInfo game_file;
    QString rom_root;
    for (const auto& dir_entry : dirs) {
        QDirIterator file_it(dir_entry.first, QDir::Files | QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot,
                             QDirIterator::FollowSymlinks | QDirIterator::Subdirectories);
        while (file_it.hasNext()) {
            file_it.next();
            if (has_extension(file_it.fileName(), extensions)) {
                game_file = file_it.fileInfo();
                rom_root = dir_entry.second;
                break;
            }
        }
        if (game_file.exists())
            break;
    }
    if (!game_file.exists()) {
        result.error = QStringLiteral("No game found for this system");
        return result;
    }
    result.game = game_file.fileName();

    QString command;
    if (pegasusx::systemEmulator(system_name).isEmpty()) {
        const HashMap<QString, QString> meta_cmds = metadata_commands(build_aliases(systems, order), ctx.log_tag);
        const auto meta_it = meta_cmds.find(system_name);
        if (meta_it != meta_cmds.cend())
            command = meta_it->second;
    }
    if (command.isEmpty())
        command = pick_command(sys, pegasusx::systemEmulator(system_name), ctx, rom_root);
    if (command.isEmpty()) {
        result.error = QStringLiteral("None of this system's emulators can be launched");
        return result;
    }
    command = resolve_injects(command, game_file);
    Log::info(ctx.log_tag, LOGMSG("Testing `%1` with: %2").arg(result.game, command));

    // the same variables the launcher fills in
    const QString abs_path = game_file.absoluteFilePath();
    QStringList args = ::utils::tokenize_command(command);
    for (QString& arg : args) {
        arg.replace(QLatin1String("{file.path}"), ::pretty_path(game_file))
           .replace(QLatin1String("{file.uri}"), ::android::to_content_uri(abs_path))
           .replace(QLatin1String("{file.documenturi}"), ::android::to_document_uri(abs_path))
           .replace(QLatin1String("{file.name}"), game_file.fileName())
           .replace(QLatin1String("{file.basename}"), game_file.completeBaseName())
           .replace(QLatin1String("{file.dir}"), ::pretty_dir(game_file));
    }
    if (!args.isEmpty())
        args.removeFirst();  // "am"
    result.error = ::android::run_am_call(args);
    return result;
}

} // namespace esde
} // namespace providers
