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


#pragma once

#include <QDate>
#include <QFile>
#include <QFuture>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <atomic>
#include <memory>
#include <vector>

class QNetworkAccessManager;
class QNetworkReply;


namespace scraper {

// Pegasus X: the LaunchBox Games Database as a scraper source, without a
// login. Its whole database comes as one zip (gamesdb.launchbox-app.com/
// Metadata.zip, well over 100 MB), downloaded once into
// <Pegasus X folder>/launchbox/ and read in the background into one small
// file per platform you have (launchbox/index/<platform>.json), so matching
// is then instant and offline. Pictures are fetched from
// images.launchbox-app.com per game. It has details and most kinds of art
// (box front/back/3D, cart, screenshots, title screens, fan art, logos,
// banners); not videos or manuals.
class LaunchBoxDb : public QObject {
    Q_OBJECT

public:
    struct Image {
        QString type;    ///< "Box - Front", "Screenshot - Gameplay", ...
        QString file;    ///< the file on images.launchbox-app.com
        QString region;  ///< "North America", "Europe", "Japan", "World", ...
    };
    struct Game {
        int id = 0;
        QString name;
        QStringList alt_names;
        QString platform;
        QDate release;
        int year = 0;
        QString overview;
        QStringList developers;
        QStringList publishers;
        QStringList genres;
        int players = 0;
        float rating = 0.f;   ///< 0..1
        std::vector<Image> images;
        int score = 0;        ///< how sure the match is, 0..100 (candidates())
    };
    /// Pegasus X: what best() found
    struct Match {
        Game game;                  ///< the match to use; id 0 if none was sure enough
        std::vector<Game> closest;  ///< none sure enough: the closest, best first; a tie: the tied ones
        bool tie = false;           ///< fallback platforms' best matches about as good: none is guessed
    };
    /// two platforms' best matches this close in score are a tie (best())
    static constexpr int TIE_MARGIN = 5;

    explicit LaunchBoxDb(QObject* parent = nullptr);

    static QString dir();
    static QString zip_path();
    static QString index_dir();
    /// LaunchBox's name for an ES-DE system, empty if none known
    static QString platform_for(const QString& esde_system);
    /// Pegasus X: the platforms Update LaunchboxDB saves for a system: its
    /// own, and for Nintendo Switch 2 also Nintendo Switch (its fallback)
    static QStringList saved_platforms_for(const QString& esde_system);
    /// Pegasus X: the platforms searched for a system, in order: its own,
    /// then its fallbacks: Nintendo Switch for Nintendo Switch 2; for the
    /// Android collections every other platform that's saved (the ones of
    /// your collections). Only platforms with an index.
    QStringList search_platforms(const QString& esde_system) const;
    static QString image_url(const QString& file);

    /// { present, date, size (MB), platforms, games, working, stage, progress, error }
    QVariantMap status() const;
    bool working() const { return m_working; }
    /// is there an index for this system?
    bool hasSystem(const QString& esde_system) const;
    /// any index at all
    bool ready() const;

    /// Downloads the database when it's missing or older than a week (or
    /// `force`), then reads it into the indexes of `esde_systems` (and of
    /// `extra_platforms`, LaunchBox names: Pegasus X's extra systems for
    /// Android, or one picked in Needs your check); progress and finished
    /// tell how it went
    void update(const QStringList& esde_systems, bool force, const QStringList& extra_platforms = {});
    void cancel();
    /// Pegasus X: every platform in the download, with its number of games
    /// (from the last read; empty before the first)
    static QVariantMap all_platforms();
    /// Pegasus X: the platforms saved (with an index)
    static QStringList saved_platforms();
    /// Pegasus X: a saved platform's index removed (an extra system unticked)
    void remove_platform(const QString& platform);
    /// Pegasus X: the games of one platform closest to a name (a search in
    /// Needs your check), best first, at most `limit`
    std::vector<Game> candidates_on(const QString& platform, const QString& name, int limit);
    /// Pegasus X: one game of one platform by its id, or a game with id 0
    Game game_on(const QString& platform, int id);

    /// The games of a system closest to a name, best first (score set),
    /// at most `limit` (its fallback platforms' too: search_platforms; on
    /// the same score its own platform's first)
    std::vector<Game> candidates(const QString& esde_system, const QString& name, int limit);
    /// Pegasus X: the match to use without asking: the system's own
    /// platform's best if it's at least `needed`; else its fallbacks' (the
    /// best of them; two platforms within TIE_MARGIN of it: a tie, none used).
    /// For the Android collections the Android platform's best isn't taken
    /// first: it's one of them (an Android port and its console original).
    Match best(const QString& esde_system, const QString& name, int needed);
    /// one game by its id (from the library index; its fallbacks' too), or a game with id 0
    Game game(const QString& esde_system, int id);

signals:
    void progress(QVariantMap status);
    void finished(QVariantMap status);

private:
    void download(const QStringList& esde_systems);
    void build(const QStringList& esde_systems);
    QStringList m_extra;   ///< update()'s extra platforms, for build()
    const std::vector<Game>& platform_games(const QString& platform);
    std::vector<Game> platform_candidates(const QString& platform, const QString& name, int limit, bool strip_edition);

    QNetworkAccessManager* m_net = nullptr;
    QNetworkReply* m_reply = nullptr;
    std::unique_ptr<QFile> m_download;
    QFutureWatcher<QString>* m_watcher = nullptr;
    std::atomic<bool> m_cancel { false };
    std::atomic<int> m_progress { 0 };   ///< 0..100 of the stage
    QString m_stage;                     ///< "downloading" / "reading" / ""
    QString m_error;
    bool m_working = false;
    QHash<QString, std::vector<Game>> m_loaded;   ///< platform -> its games (this run)
};

} // namespace scraper
