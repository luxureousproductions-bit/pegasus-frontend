// Pegasus Frontend
// Copyright (C) 2017-2018  Mátyás Mustoha
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

#include <QJsonArray>
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QUrl>
#include <functional>
#include <vector>

class QJsonObject;
class QNetworkAccessManager;
class QNetworkReply;


// Pegasus X: the scraper (Settings > Library > Scraper). Game details and
// art from ScreenScraper, art from SteamGridDB; downloaded into the data
// folder and written into your metadata like the Library editor does: only
// what's empty, unless you turned on Overwrite. One game at a time, so it
// stays within what your ScreenScraper account allows.
namespace scraper {

/// One game to scrape, with what it has now (read when scraping starts, so
/// the library can be read again meanwhile)
struct Job {
    QString collection;
    QString system;          ///< ES-DE's name for the collection's system, if known
    QString title;
    QString path;            ///< the first file, or the game's android: URI
    QString search_name;     ///< the file name without extension and tags
    bool has_description = false;
    bool has_developer = false;
    bool has_publisher = false;
    bool has_genre = false;
    bool has_release = false;
    bool has_players = false;
    bool has_rating = false;
    bool title_is_file_name = false;
    QVariantMap slot_files;       ///< metadata key ("assets.screenshot", "x-manual") -> its files now
    bool has_box2d = false;
    bool has_box3d = false;
    QStringList hidden_art;  ///< art removed in Library ("<kind>|<file>")
};

/// What a source gave for one game
struct Media {
    QString setting;   ///< the Content setting that decides it, eg. "getScreenshots"
    QString slot;      ///< metadata key, eg. "assets.screenshot"
    QString folder;    ///< folder in the data folder, eg. "screenshots"
    QString kind;      ///< for the boxes: "2d" / "3d"
    QUrl url;
    QString ext;
};

class Scraper : public QObject {
    Q_OBJECT

public:
    explicit Scraper(QObject* parent = nullptr);

    void start(std::vector<Job> jobs, const QVariantMap& settings,
               const QString& dev_id, const QString& dev_password, const QString& data_dir);
    void stop();
    bool running() const { return m_running; }
    QVariantMap state() const;

    /// The ES-DE system of a collection, for ScreenScraper's system id
    static QString system_of(const QString& collection_name, const QString& short_name);

    // interactive mode: the matches found for the game, waiting for a pick
    QVariantMap pendingChoice() const { return m_waiting ? m_choice : QVariantMap(); }
    void choose(int index);
    void refine(const QString& search_name);
    void skip();

signals:
    /// { running, done, total, current, title, result, ok, saved, notFound, failed }
    void progress(QVariantMap state);
    void finished(QVariantMap state);
    /// { title, collection, searchName, source, candidates: [{ name, info,
    /// description, thumb, developer, publisher, genre, players, release, rating }] }
    void choiceNeeded(QVariantMap choice);

private:
    QNetworkAccessManager* m_net = nullptr;
    std::vector<Job> m_jobs;
    QVariantMap m_settings;
    QString m_dev_id;
    QString m_dev_password;
    QString m_data_dir;
    size_t m_index = 0;
    bool m_running = false;
    bool m_stop = false;
    int m_saved = 0;
    int m_not_found = 0;
    int m_failed = 0;
    QString m_current;
    QString m_last_title;
    QString m_last_result;
    bool m_last_ok = true;
    QString m_stop_reason;
    QVariantMap m_last_changes;  ///< what the last game saved (shown at once for one game)

    // interactive mode
    bool m_waiting = false;
    QVariantMap m_choice;
    QJsonArray m_ss_results;                ///< ScreenScraper's games, as found
    std::vector<int> m_sgdb_ids;            ///< SteamGridDB's games, as found
    QString m_choice_source;                ///< "ss" / "sgdb"
    bool interactive() const;
    void search(const QString& name);
    void ssSearch(const QString& name);
    void sgdbSearch(const QString& name);
    void offerChoice(const QVariantList& candidates);

    // the game being scraped
    QVariantMap m_changes;
    std::vector<Media> m_media;
    QStringList m_got;

    void next();
    void finishGame(const QString& result, bool ok);
    void report();
    bool wants(const QString& setting) const;
    bool slotFree(const Job& job, const Media& media) const;

    void screenScraper(const QString& md5);
    void parseScreenScraper(const QJsonObject& game);
    void steamGridDb(int known_id = 0);
    void downloadNext();
    void storeMedia(const Media& media, const Job& job, const QByteArray& body, const QString& ext);
    void save();

    // a GET with retries (429, 5xx and dropped connections)
    void get(const QUrl& url, const QByteArray& auth, int tries_left,
             std::function<void(int status, const QByteArray& body)> done);
};

} // namespace scraper
