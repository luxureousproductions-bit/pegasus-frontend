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
class QFile;


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

    explicit LaunchBoxDb(QObject* parent = nullptr);

    static QString dir();
    static QString zip_path();
    static QString index_dir();
    /// LaunchBox's name for an ES-DE system, empty if none known
    static QString platform_for(const QString& esde_system);
    static QString image_url(const QString& file);

    /// { present, date, size (MB), platforms, games, working, stage, progress, error }
    QVariantMap status() const;
    bool working() const { return m_working; }
    /// is there an index for this system?
    bool hasSystem(const QString& esde_system) const;
    /// any index at all
    bool ready() const;

    /// Downloads the database when it's missing or older than a week (or
    /// `force`), then reads it into the indexes of `esde_systems`; progress
    /// and finished tell how it went
    void update(const QStringList& esde_systems, bool force);
    void cancel();

    /// The games of a system closest to a name, best first (score set),
    /// at most `limit`
    std::vector<Game> candidates(const QString& esde_system, const QString& name, int limit);
    /// one game by its id (from the library index), or a game with id 0
    Game game(const QString& esde_system, int id);

signals:
    void progress(QVariantMap status);
    void finished(QVariantMap status);

private:
    void download(const QStringList& esde_systems);
    void build(const QStringList& esde_systems);
    const std::vector<Game>& platform_games(const QString& platform);

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
