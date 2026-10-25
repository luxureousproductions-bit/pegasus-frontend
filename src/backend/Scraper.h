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

#include <QDateTime>
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
// art from ScreenScraper, details and art from IGDB (your own Twitch app
// login), art from SteamGridDB; downloaded into the data
// folder and written into your metadata like the Library editor does: only
// what's empty, unless you turned on Overwrite. One game at a time, so it
// stays within what your ScreenScraper account allows.
namespace scraper {

/// One game to scrape, with what it has now (read when scraping starts, so
/// the library can be read again meanwhile)
struct Job {
    QString collection;
    QString media_folder;    ///< the collection's folder in the media folder (its short name, eg. "snes")
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
    /// what the scraper did lately (Library > Scraper > Log), newest last
    const QStringList& log() const { return m_log; }

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
    int m_run = 0;  // which start() the in-flight answers belong to
    QString m_current;
    QString m_last_title;
    QString m_last_result;
    bool m_last_ok = true;
    QString m_stop_reason;
    QVariantMap m_last_changes;  ///< what the last game saved (shown at once for one game)
    QStringList m_log;           ///< the last few hundred lines of what happened
    void note(const QString& line);
    QVariantMap m_preview;       ///< the last game as the Scraper page shows it (picture, details)
    int m_picture_failures = 0;  ///< pictures of this game that didn't download
    int m_picture_error = 0;     ///< ... and the last answer
    QVariantMap previewOf(const Job& job) const;
    /// the matches' pictures downloaded here first (shown from the device)
    void prefetchThumbs(const QVariantList& candidates, std::function<void(const QVariantList&)> done);

    // interactive mode
    bool m_waiting = false;
    QVariantMap m_choice;
    QJsonArray m_ss_results;                ///< ScreenScraper's games, as found
    std::vector<int> m_sgdb_ids;            ///< SteamGridDB's games, as found
    QString m_choice_source;                ///< "ss" / "igdb" / "sgdb"
    bool interactive() const;
    void search(const QString& name);
    void ssSearch(const QString& name);
    void sgdbSearch(const QString& name);
    void offerChoice(const QVariantList& candidates);

    // IGDB: details and art (cover, screenshot, artwork); the first source, or
    // after ScreenScraper for what it didn't have
    QString m_igdb_token;
    QDateTime m_igdb_expiry;
    QJsonArray m_igdb_results;              ///< IGDB's games, as found
    bool m_igdb_done = false;               ///< asked IGDB for this game already
    bool igdbReady() const;
    void igdbToken(std::function<void(int status)> done);
    void igdbPost(const QByteArray& query, std::function<void(int status, const QByteArray& body)> done,
                  int tries_left, bool retried = false);
    void igdbFind(const QString& name, std::function<void(int status, const QJsonArray& games)> done);
    void igdbSearch(const QString& name);
    void igdbAuto(bool after_other);
    void parseIgdb(const QJsonObject& game, bool after_other);
    QVariantMap igdbCandidate(const QJsonObject& game) const;
    /// after the first source: IGDB for what's missing, then SteamGridDB's art
    void fillMore();

    // miximages made here (as ES-DE), after the downloads; the offline
    // generator makes only these, from the art the games have
    bool m_mix_done = false;
    QString m_mix_error;
    bool offline() const;
    bool wantsMiximage() const;
    void makeMiximage();

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
