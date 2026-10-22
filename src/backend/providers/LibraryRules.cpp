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


#include "LibraryRules.h"

#include "Log.h"
#include "Paths.h"
#include "model/gaming/Collection.h"
#include "model/gaming/CollectionListModel.h"
#include "model/gaming/Assets.h"
#include "model/gaming/Game.h"
#include "model/gaming/GameFile.h"
#include "model/gaming/GameFileListModel.h"
#include "model/gaming/GameListModel.h"
#include "utils/HashMap.h"
#ifdef Q_OS_ANDROID
#include "platform/AndroidHelpers.h"
#endif
#ifdef WITH_COMPAT_ESDE
#include "providers/esde/EsdeProvider.h"
#endif

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStringBuilder>
#include <algorithm>


namespace {
const QString LOG_TAG = QStringLiteral("Library");

QString hidden_path()
{
    return paths::writableConfigDir() % QStringLiteral("/pegasusx-hidden.json");
}
QString kinds_path()
{
    return paths::writableConfigDir() % QStringLiteral("/pegasusx-android-kinds.json");
}
QString settings_path()
{
    return paths::writableConfigDir() % QStringLiteral("/pegasusx.ini");
}

bool is_android_collection(const model::Collection& coll)
{
    const QString name = coll.name().toLower();
    const QString short_name = coll.shortName().toLower();
    return name == QLatin1String("android")
        || name == QLatin1String("android apps")
        || name == QLatin1String("android games")
        || short_name == QLatin1String("android")
        || short_name == QLatin1String("androidapps")
        || short_name == QLatin1String("androidgames");
}
bool is_android_games_collection(const model::Collection& coll)
{
    return coll.shortName().toLower() == QLatin1String("androidgames")
        || coll.name().toLower() == QLatin1String("android games");
}

// the app an entry is, or opens: its android: file, else the "-n <package>/"
// of its launch command (your own entry for an app often has only that)
QString package_of(const model::Game& game)
{
    const QString key = providers::library::game_key(game);
    if (key.startsWith(QLatin1String("android:")))
        return key.mid(8);
    static const QRegularExpression rx_component(QStringLiteral("-n\\s+([\\w.]+)/"));
    const QRegularExpressionMatch match = rx_component.match(game.launchCmd());
    return match.hasMatch() ? match.captured(1) : QString();
}

bool has_entry(const QStringList& list, const QString& value)
{
    for (const QString& item : list) {
        if (item.compare(value, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

std::vector<model::Collection*> sorted_unique(std::vector<model::Collection*> list)
{
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
    std::sort(list.begin(), list.end(), model::sort_collections);
    return list;
}


// -- Android ------------------------------------------------------------------

int source_rank(const model::Game& game)
{
    const QString source = providers::library::source_of(game);
    if (source == QLatin1String("user")) return 0;
    if (source == QLatin1String("esde")) return 1;
    return 2;
}

// Entries with the exact same title show once: your own metadata first, then
// an ES-DE import, then Pegasus' own data import. The others are returned.
//
// Two apps that are both installed (two packages, eg. a fork with the same
// name) are different apps, so both stay; an entry without a package (your
// own, pointing at a file) matches any of the same title.
std::vector<model::Game*> dedupe_by_title(std::vector<model::Game*>& list)
{
    HashMap<QString, std::vector<size_t>> at;  // title -> positions in `kept`
    std::vector<model::Game*> kept;
    std::vector<QString> kept_package;  // the app each kept entry stands for
    std::vector<model::Game*> dropped;
    for (model::Game* const game : list) {
        const QString package = package_of(*game);
        std::vector<size_t>& same_title = at[game->title()];
        size_t match = kept.size();
        // the same app first; an entry without a package stands for one app
        // only (the first it meets), so a fork of the same name still stays
        for (const size_t pos : same_title) {
            if (!package.isEmpty() && kept_package[pos] == package) {
                match = pos;
                break;
            }
        }
        for (size_t i = 0; match == kept.size() && i < same_title.size(); i++) {
            const size_t pos = same_title[i];
            if (package.isEmpty() || kept_package[pos].isEmpty()) {
                match = pos;
                if (kept_package[pos].isEmpty())
                    kept_package[pos] = package;
            }
        }
        if (match == kept.size()) {
            same_title.push_back(kept.size());
            kept.push_back(game);
            kept_package.push_back(package);
            continue;
        }
        model::Game*& current = kept[match];
        if (source_rank(*game) < source_rank(*current)) {
            dropped.push_back(current);
            current = game;
        }
        else {
            dropped.push_back(game);
        }
    }
    list = std::move(kept);
    return dropped;
}

// Two installed apps with the same name (eg. Xendroid and a fork of it):
// both stay, the second as "Xendroid Fork" (more: "Fork 2", ...). The one
// from your own metadata, else the first package A to Z, keeps the name.
void name_forks(std::vector<model::Game*>& list)
{
    HashMap<QString, std::vector<model::Game*>> by_title;
    for (model::Game* const game : list) {
        if (!package_of(*game).isEmpty())
            by_title[game->title().toLower()].push_back(game);
    }
    for (auto& pair : by_title) {
        std::vector<model::Game*>& same = pair.second;
        if (same.size() < 2)
            continue;
        std::stable_sort(same.begin(), same.end(), [](model::Game* a, model::Game* b) {
            if (source_rank(*a) != source_rank(*b))
                return source_rank(*a) < source_rank(*b);
            return package_of(*a) < package_of(*b);
        });
        for (size_t i = 1; i < same.size(); i++) {
            const QString suffix = i == 1 ? QStringLiteral(" Fork") : QStringLiteral(" Fork %1").arg(i);
            same[i]->setTitle(same[i]->title() + suffix);
        }
    }
}

void merge_android(std::vector<model::Collection*>& collections, std::vector<model::Game*>& games, bool merge_games)
{
    std::vector<model::Collection*> app_sources;
    std::vector<model::Collection*> game_sources;
    model::Collection* target = nullptr;
    for (model::Collection* const coll : collections) {
        if (!is_android_collection(*coll))
            continue;
        if (is_android_games_collection(*coll))
            game_sources.push_back(coll);
        else
            app_sources.push_back(coll);
        if (coll->name() == QLatin1String("Android"))
            target = coll;
    }
    if (app_sources.empty() && game_sources.empty())
        return;

    // what each app is: a move you made in Settings > Library first, then
    // your metadata, then what the app itself says (Android's "game" flag),
    // then whether it's a known emulator
    const QVariantMap moved = providers::library::load_kinds();
    HashMap<QString, bool> flagged_game;
#ifdef Q_OS_ANDROID
    for (const QString& line : ::android::android_app_list()) {
        const QStringList parts = line.split(QLatin1Char('\t'));
        if (parts.size() >= 4)
            flagged_game.emplace(parts.at(1), parts.at(3) == QLatin1String("1"));
    }
#endif
    QSet<QString> emulators;
#ifdef WITH_COMPAT_ESDE
    emulators = providers::esde::emulator_packages();
#endif
    const auto is_emulator = [&emulators](const QString& package) {
        if (package.isEmpty())
            return false;
        if (emulators.contains(package))
            return true;
        // forks and other builds of a known one ("org.ppsspp.ppsspp" -> "org.ppsspp.ppssppgold")
        for (const QString& known : emulators) {
            if (package.startsWith(known % QLatin1Char('.')))
                return true;
        }
        return package.contains(QLatin1String("retroarch"));
    };

    // every game once, remembering which came from a games collection
    std::vector<model::Game*> all;
    std::vector<model::Game*> app_side;   // in Android
    std::vector<model::Game*> game_side;  // in Android Games
    QSet<model::Game*> seen;
    QSet<model::Game*> from_games_collection;
    for (model::Collection* const coll : app_sources) {
        for (model::Game* const game : coll->gameList()->entries()) {
            if (!seen.contains(game)) {
                seen.insert(game);
                all.push_back(game);
                app_side.push_back(game);
            }
        }
    }
    QSet<model::Game*> in_game_side;
    for (model::Collection* const coll : game_sources) {
        for (model::Game* const game : coll->gameList()->entries()) {
            from_games_collection.insert(game);
            if (!in_game_side.contains(game)) {
                in_game_side.insert(game);
                game_side.push_back(game);
            }
            if (!seen.contains(game)) {
                seen.insert(game);
                all.push_back(game);
            }
        }
    }

    HashMap<model::Game*, QString> kind_of;
    for (model::Game* const game : all) {
        QStringList& genres = game->genreList();
        QStringList& tags = game->tagList();
        const QString key = providers::library::game_key(*game);
        QString kind = moved.value(key).toString();
        if (!kind.isEmpty()) {
            // your move replaces what the entry was tagged before
            for (const QString& other : { QStringLiteral("Game"), QStringLiteral("Emulator"), QStringLiteral("Application") }) {
                tags.removeAll(other);
                genres.removeAll(other);
            }
        }
        else if (has_entry(genres, QStringLiteral("Emulator")))
            kind = QStringLiteral("Emulator");
        else if (has_entry(genres, QStringLiteral("Application")))
            kind = QStringLiteral("Application");
        else if (has_entry(genres, QStringLiteral("Game")) || has_entry(tags, QStringLiteral("Game"))
                 || from_games_collection.contains(game))
            kind = QStringLiteral("Game");
        else {
            const QString package = package_of(*game);
            const auto flag = flagged_game.find(package);
            if (flag != flagged_game.cend() && flag->second)
                kind = QStringLiteral("Game");
            else if (is_emulator(package))
                kind = QStringLiteral("Emulator");
            else if (!package.isEmpty())
                kind = QStringLiteral("Application");
            else
                kind = QStringLiteral("Game");  // not an app: something you added to an Android collection
        }

        if (!has_entry(tags, kind))
            tags.append(kind);
        if (kind != QLatin1String("Game") && !has_entry(genres, kind))
            genres.append(kind);
        kind_of.emplace(game, kind);
    }

    // what goes where
    std::vector<model::Game*> android_list;
    std::vector<model::Game*> games_list;
    if (merge_games) {
        android_list = all;
    }
    else {
        // Android Games is its own collection: every entry in the Games
        // section goes there (also apps you moved to Games in Settings >
        // Library), the apps and emulators stay in Android
        for (model::Game* const game : all) {
            if (kind_of[game] == QLatin1String("Game"))
                games_list.push_back(game);
            else
                android_list.push_back(game);
        }
    }
    const std::vector<model::Game*> dropped_a = dedupe_by_title(android_list);
    const std::vector<model::Game*> dropped_g = dedupe_by_title(games_list);
    name_forks(android_list);
    name_forks(games_list);

    const auto order = [](std::vector<model::Game*>& list) {
        // plain A to Z: games, apps and emulators together, upper and lower
        // case alike (themes and Settings > Library group them by their tags)
        std::stable_sort(list.begin(), list.end(), [](model::Game* a, model::Game* b) {
            // by the title you see (a sort-by of your metadata could put an
            // entry far from its name)
            const int by_title = a->title().compare(b->title(), Qt::CaseInsensitive);
            if (by_title != 0)
                return by_title < 0;
            return a->sortBy().compare(b->sortBy(), Qt::CaseInsensitive) < 0;
        });
    };
    order(android_list);
    order(games_list);

    if (!android_list.empty() && !target) {
        target = new model::Collection(QStringLiteral("Android"));
        target->setShortName(QStringLiteral("android"));
        target->setGames(std::vector<model::Game*>(android_list));
        collections.push_back(target);
    }
    if (target && android_list.empty())
        target = nullptr;  // nothing left in it: goes with the merged ones below
    if (target)
        target->gameList()->update(std::vector<model::Game*>(android_list));
    model::Collection* games_target = nullptr;
    if (!merge_games && !games_list.empty()) {
        if (!game_sources.empty()) {
            games_target = game_sources.front();
        }
        else {
            games_target = new model::Collection(QStringLiteral("Android Games"));
            games_target->setShortName(QStringLiteral("androidgames"));
            games_target->setGames(std::vector<model::Game*>(games_list));
            collections.push_back(games_target);
        }
    }
    if (games_target)
        games_target->gameList()->update(std::vector<model::Game*>(games_list));

    // each game's collections
    const QSet<model::Game*> in_android(android_list.cbegin(), android_list.cend());
    const QSet<model::Game*> in_games(games_list.cbegin(), games_list.cend());
    QSet<model::Collection*> sources(app_sources.cbegin(), app_sources.cend());
    for (model::Collection* const coll : game_sources)
        sources.insert(coll);
    QSet<model::Game*> removed;
    for (model::Game* const game : all) {
        std::vector<model::Collection*> list;
        for (model::Collection* const coll : game->collectionsModel()->entries()) {
            if (!sources.contains(coll))
                list.push_back(coll);
        }
        if (target && in_android.contains(game))
            list.push_back(target);
        if (games_target && in_games.contains(game))
            list.push_back(games_target);
        if (list.empty())
            removed.insert(game);
        else
            game->collectionsModel()->update(sorted_unique(std::move(list)));
    }

    // the merged collections go (Android, and Android Games while it's kept)
    for (model::Collection* const coll : sources) {
        if (coll == target || coll == games_target)
            continue;
        Log::info(LOG_TAG, LOGMSG("`%1` merged into `%2`").arg(coll->name(),
            (games_target && is_android_games_collection(*coll)) ? games_target->name() : QStringLiteral("Android")));
        collections.erase(std::remove(collections.begin(), collections.end(), coll), collections.end());
        delete coll;
    }
    std::sort(collections.begin(), collections.end(), model::sort_collections);

    // duplicates that are in nothing else any more
    if (!removed.isEmpty()) {
        games.erase(std::remove_if(games.begin(), games.end(),
            [&removed](model::Game* const game) { return removed.contains(game); }), games.end());
        for (model::Game* const game : removed)
            delete game;
    }
    Log::info(LOG_TAG, LOGMSG("Android: %1 entries, %2 duplicates left out")
        .arg(android_list.size() + games_list.size()).arg(dropped_a.size() + dropped_g.size()));
}


// -- Hidden -------------------------------------------------------------------

void apply_hidden(std::vector<model::Collection*>& collections, std::vector<model::Game*>& games)
{
    const providers::library::HiddenList hidden = providers::library::load_hidden();
    if (hidden.collections.isEmpty() && hidden.games.empty())
        return;

    QSet<QString> hidden_keys;
    for (const providers::library::HiddenGame& entry : hidden.games)
        hidden_keys.insert(entry.path);
    const QSet<QString> hidden_names(hidden.collections.cbegin(), hidden.collections.cend());

    QSet<model::Collection*> removed_colls;
    for (model::Collection* const coll : collections) {
        if (hidden_names.contains(coll->name()))
            removed_colls.insert(coll);
    }
    QSet<model::Game*> removed_games;
    for (model::Game* const game : games) {
        if (hidden_keys.contains(providers::library::game_key(*game))) {
            removed_games.insert(game);
            continue;
        }
        // a game only in hidden collections goes with them
        bool visible_somewhere = false;
        bool changed = false;
        std::vector<model::Collection*> list;
        for (model::Collection* const coll : game->collectionsModel()->entries()) {
            if (removed_colls.contains(coll)) {
                changed = true;
                continue;
            }
            visible_somewhere = true;
            list.push_back(coll);
        }
        if (!visible_somewhere)
            removed_games.insert(game);
        else if (changed)
            game->collectionsModel()->update(std::move(list));
    }

    // the collections that are left, without the hidden games
    for (model::Collection* const coll : collections) {
        if (removed_colls.contains(coll) || removed_games.isEmpty())
            continue;
        const std::vector<model::Game*>& current = coll->gameList()->entries();
        std::vector<model::Game*> kept;
        kept.reserve(current.size());
        for (model::Game* const game : current) {
            if (!removed_games.contains(game))
                kept.push_back(game);
        }
        if (kept.size() != current.size()) {
            if (kept.empty())
                removed_colls.insert(coll);
            else
                coll->gameList()->update(std::move(kept));
        }
    }

    games.erase(std::remove_if(games.begin(), games.end(),
        [&removed_games](model::Game* const game) { return removed_games.contains(game); }), games.end());
    collections.erase(std::remove_if(collections.begin(), collections.end(),
        [&removed_colls](model::Collection* const coll) { return removed_colls.contains(coll); }), collections.end());
    for (model::Game* const game : removed_games)
        delete game;
    for (model::Collection* const coll : removed_colls)
        delete coll;

    Log::info(LOG_TAG, LOGMSG("Hidden: %1 games, %2 collections").arg(removed_games.size()).arg(removed_colls.size()));
}
} // namespace


namespace providers {
namespace library {

QString game_key(const model::Game& game)
{
    const std::vector<model::GameFile*>& files = game.filesModel()->entries();
    if (files.empty())
        return {};
    const QString path = files.front()->path();
    return path.isEmpty() ? files.front()->uri() : path;
}

QString source_of(const model::Game& game)
{
    const QStringList source = game.extraMap().value(QStringLiteral("pegasusx-source")).toStringList();
    if (!source.isEmpty() && (source.first() == QLatin1String("user") || source.first() == QLatin1String("esde")))
        return source.first();
    return QStringLiteral("pegasus");
}

QVariantMap load_kinds()
{
    QFile file(kinds_path());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
}

bool set_kind(const QString& key, const QString& kind)
{
    QVariantMap kinds = load_kinds();
    if (kind.isEmpty())
        kinds.remove(key);
    else
        kinds.insert(key, kind);
    QFile file(kinds_path());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Log::warning(LOG_TAG, LOGMSG("Could not write `%1`").arg(kinds_path()));
        return false;
    }
    file.write(QJsonDocument(QJsonObject::fromVariantMap(kinds)).toJson(QJsonDocument::Indented));
    return true;
}

HiddenList load_hidden()
{
    HiddenList out;
    QFile file(hidden_path());
    if (!file.open(QIODevice::ReadOnly))
        return out;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    for (const QJsonValue& value : root.value(QStringLiteral("collections")).toArray()) {
        const QString name = value.toString();
        if (!name.isEmpty())
            out.collections.append(name);
    }
    for (const QJsonValue& value : root.value(QStringLiteral("games")).toArray()) {
        const QJsonObject obj = value.toObject();
        HiddenGame entry;
        entry.path = obj.value(QStringLiteral("path")).toString();
        entry.title = obj.value(QStringLiteral("title")).toString();
        for (const QJsonValue& coll : obj.value(QStringLiteral("collections")).toArray())
            entry.collections.append(coll.toString());
        if (!entry.path.isEmpty())
            out.games.push_back(std::move(entry));
    }
    return out;
}

bool save_hidden(const HiddenList& list)
{
    QJsonArray collections;
    for (const QString& name : list.collections)
        collections.append(name);
    QJsonArray games;
    for (const HiddenGame& entry : list.games) {
        games.append(QJsonObject {
            { QStringLiteral("path"), entry.path },
            { QStringLiteral("title"), entry.title },
            { QStringLiteral("collections"), QJsonArray::fromStringList(entry.collections) },
        });
    }
    const QJsonObject root {
        { QStringLiteral("collections"), collections },
        { QStringLiteral("games"), games },
    };

    QFile file(hidden_path());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Log::warning(LOG_TAG, LOGMSG("Could not write `%1`").arg(hidden_path()));
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool merge_android_enabled()
{
    const QSettings file(settings_path(), QSettings::IniFormat);
    return file.value(QStringLiteral("library/mergeAndroid"), true).toBool();
}

void set_merge_android_enabled(bool value)
{
    QSettings file(settings_path(), QSettings::IniFormat);
    file.setValue(QStringLiteral("library/mergeAndroid"), value);
}

// A collection without a launch command in your metadata uses the
// recommended emulator of its system (what Settings > Emulators marks as
// recommended, if it's installed), until you pick one. Nothing is written.
void apply_default_launches(std::vector<model::Collection*>& collections)
{
#ifdef WITH_COMPAT_ESDE
    std::vector<model::Collection*> todo;
    std::vector<std::pair<QString, QString>> keys;
    QStringList rom_roots;
    for (model::Collection* const coll : collections) {
        if (!coll->commonLaunchCmd().isEmpty() || is_android_collection(*coll))
            continue;
        // only when some game has nothing to launch with
        bool needed = false;
        for (model::Game* const game : coll->gameList()->entries()) {
            if (game->launchCmd().isEmpty()) {
                needed = true;
                break;
            }
        }
        if (!needed)
            continue;
        todo.push_back(coll);
        keys.emplace_back(coll->name(), coll->shortName());
        const QString basedir = coll->commonLaunchCmdBasedir();
        rom_roots.append(basedir.isEmpty() ? QString() : QFileInfo(basedir).absolutePath());
    }
    if (todo.empty())
        return;
    const std::vector<providers::esde::DefaultLaunch> defaults = providers::esde::default_launches(keys, rom_roots);
    for (size_t i = 0; i < todo.size() && i < defaults.size(); i++) {
        if (defaults[i].command.isEmpty())
            continue;
        model::Collection* const coll = todo[i];
        coll->setCommonLaunchCmd(defaults[i].command);
        for (model::Game* const game : coll->gameList()->entries()) {
            if (game->launchCmd().isEmpty()) {
                game->setLaunchCmd(defaults[i].command);
                if (game->launchCmdBasedir().isEmpty())
                    game->setLaunchCmdBasedir(coll->commonLaunchCmdBasedir());
            }
        }
        Log::info(LOG_TAG, LOGMSG("`%1` launches with %2 (recommended; none set in its metadata)")
            .arg(coll->name(), defaults[i].label));
    }
#else
    Q_UNUSED(collections);
#endif
}

// title without region/version/disc tags, case and punctuation, so
// "Crash Bandicoot (USA)" and "crash bandicoot" are the same game
QString same_game_key(const QString& title)
{
    static const QRegularExpression rx_tags(QStringLiteral("\\s*[\\(\\[][^\\)\\]]*[\\)\\]]"));
    static const QRegularExpression rx_disc(QStringLiteral("\\b(disc|disk|cd)\\s*\\d+\\b"), QRegularExpression::CaseInsensitiveOption);
    QString t = title;
    t.remove(rx_tags);
    t.remove(rx_disc);
    QString out;
    for (const QChar c : t.toLower()) {
        if (c.isLetterOrNumber())
            out += c;
    }
    return out;
}

// A game that's both in your own metadata and in an ES-DE import of the same
// collection (or found by Pegasus itself) shows once: yours, else ES-DE's,
// else Pegasus'. Entries from the same source with one name all stay.
void dedupe_collections(std::vector<model::Collection*>& collections, std::vector<model::Game*>& games)
{
    QSet<model::Game*> dropped_somewhere;
    for (model::Collection* const coll : collections) {
        const std::vector<model::Game*>& current = coll->gameList()->entries();
        HashMap<QString, std::vector<model::Game*>> by_name;
        for (model::Game* const game : current)
            by_name[same_game_key(game->title())].push_back(game);

        QSet<model::Game*> drop;
        for (const auto& pair : by_name) {
            const std::vector<model::Game*>& same = pair.second;
            if (same.size() < 2 || pair.first.isEmpty())
                continue;
            // the best one (yours first), then everything not yours goes
            model::Game* best = same.front();
            for (model::Game* const g : same) {
                if (source_rank(*g) < source_rank(*best))
                    best = g;
            }
            // only entries from a lower source go: two ES-DE entries of one
            // name (eg. two regions) both stay, and so do two different
            // installed apps (a fork: another package)
            const QString best_package = package_of(*best);
            for (model::Game* const g : same) {
                const QString package = package_of(*g);
                const bool other_app = !package.isEmpty() && !best_package.isEmpty() && package != best_package;
                if (source_rank(*g) > source_rank(*best) && !other_app)
                    drop.insert(g);
            }
        }
        if (drop.isEmpty())
            continue;

        std::vector<model::Game*> kept;
        kept.reserve(current.size());
        for (model::Game* const game : current) {
            if (!drop.contains(game))
                kept.push_back(game);
        }
        coll->gameList()->update(std::move(kept));
        for (model::Game* const game : drop) {
            std::vector<model::Collection*> left;
            for (model::Collection* const c : game->collectionsModel()->entries()) {
                if (c != coll)
                    left.push_back(c);
            }
            game->collectionsModel()->update(std::vector<model::Collection*>(left));
            if (left.empty())
                dropped_somewhere.insert(game);
        }
        Log::info(LOG_TAG, LOGMSG("`%1`: %2 duplicates left out").arg(coll->name(), QString::number(drop.size())));
    }
    if (dropped_somewhere.isEmpty())
        return;
    games.erase(std::remove_if(games.begin(), games.end(),
        [&dropped_somewhere](model::Game* const game) { return dropped_somewhere.contains(game); }), games.end());
    for (model::Game* const game : dropped_somewhere)
        delete game;
}

void apply_library_rules(std::vector<model::Collection*>& collections, std::vector<model::Game*>& games)
{
    merge_android(collections, games, merge_android_enabled());
    dedupe_collections(collections, games);
    // the same picture found twice (eg. in your metadata and in a media
    // folder, under two paths) shows once; art you removed in Settings >
    // Library stays out, wherever else it's listed (eg. an ES-DE import)
    for (model::Game* const game : games) {
        game->assetsMut().remove_duplicate_names();
        const QStringList removed = game->extraMap().value(QStringLiteral("pegasusx-hidden-art")).toStringList();
        if (!removed.isEmpty())
            game->assetsMut().remove_listed(removed);
    }
    apply_hidden(collections, games);
    apply_default_launches(collections);
}

} // namespace library
} // namespace providers
