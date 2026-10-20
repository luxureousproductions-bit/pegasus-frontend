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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

QString package_of(const model::Game& game)
{
    const QString key = providers::library::game_key(game);
    return key.startsWith(QLatin1String("android:")) ? key.mid(8) : QString();
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

void merge_android(std::vector<model::Collection*>& collections)
{
    std::vector<model::Collection*> sources;
    model::Collection* target = nullptr;
    for (model::Collection* const coll : collections) {
        if (!is_android_collection(*coll))
            continue;
        sources.push_back(coll);
        if (coll->name() == QLatin1String("Android"))
            target = coll;
    }
    if (sources.empty())
        return;

    // what each app is: from your metadata first, then what the app itself
    // says (Android's "game" flag), then whether it's a known emulator
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
    QSet<model::Game*> seen;
    QSet<model::Game*> from_games_collection;
    for (model::Collection* const coll : sources) {
        const bool games_coll = is_android_games_collection(*coll);
        for (model::Game* const game : coll->gameList()->entries()) {
            if (games_coll)
                from_games_collection.insert(game);
            if (!seen.contains(game)) {
                seen.insert(game);
                all.push_back(game);
            }
        }
    }

    std::vector<model::Game*> games_part;
    std::vector<model::Game*> apps_part;
    for (model::Game* const game : all) {
        QStringList& genres = game->genreList();
        QString kind;
        if (has_entry(genres, QStringLiteral("Emulator")))
            kind = QStringLiteral("Emulator");
        else if (has_entry(genres, QStringLiteral("Application")))
            kind = QStringLiteral("Application");
        else if (has_entry(genres, QStringLiteral("Game")) || from_games_collection.contains(game))
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

        QStringList& tags = game->tagList();
        if (!has_entry(tags, kind))
            tags.append(kind);
        if (kind != QLatin1String("Game") && !has_entry(genres, kind))
            genres.append(kind);

        (kind == QLatin1String("Game") ? games_part : apps_part).push_back(game);
    }
    std::sort(games_part.begin(), games_part.end(), model::sort_games);
    std::sort(apps_part.begin(), apps_part.end(), model::sort_games);
    std::vector<model::Game*> ordered = games_part;
    ordered.insert(ordered.end(), apps_part.cbegin(), apps_part.cend());

    const bool new_target = target == nullptr;
    if (new_target) {
        target = new model::Collection(QStringLiteral("Android"));
        target->setShortName(QStringLiteral("android"));
        target->setGames(std::vector<model::Game*>(ordered));
    }
    target->gameList()->update(std::vector<model::Game*>(ordered));

    const QSet<model::Collection*> merged(sources.cbegin(), sources.cend());
    for (model::Game* const game : all) {
        std::vector<model::Collection*> list;
        for (model::Collection* const coll : game->collectionsModel()->entries()) {
            if (!merged.contains(coll))
                list.push_back(coll);
        }
        list.push_back(target);
        game->collectionsModel()->update(sorted_unique(std::move(list)));
    }

    for (model::Collection* const coll : sources) {
        if (coll == target)
            continue;
        Log::info(LOG_TAG, LOGMSG("`%1` merged into `Android`").arg(coll->name()));
        collections.erase(std::remove(collections.begin(), collections.end(), coll), collections.end());
        delete coll;
    }
    if (new_target) {
        collections.push_back(target);
        std::sort(collections.begin(), collections.end(), model::sort_collections);
    }
    Log::info(LOG_TAG, LOGMSG("Android: %1 games, %2 apps").arg(games_part.size()).arg(apps_part.size()));
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

void apply_library_rules(std::vector<model::Collection*>& collections, std::vector<model::Game*>& games)
{
    if (merge_android_enabled())
        merge_android(collections);
    apply_hidden(collections, games);
}

} // namespace library
} // namespace providers
