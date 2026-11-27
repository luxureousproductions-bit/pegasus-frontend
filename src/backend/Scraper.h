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

#include "LaunchBoxDb.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QUrl>
#include <functional>
#include <vector>
#include <atomic>
#include <QPointer>
#include <QHash>
#include <QJsonObject>
#include <QSet>

class QTimer;

class QJsonObject;
class QNetworkAccessManager;
class QNetworkReply;
class test_Scraper;


// Pegasus X: the scraper (Settings > Library > Scraper). Game details and
// art from ScreenScraper and from the LaunchBox database (no login), the
// free pictures from libretro's thumbnails; downloaded into the data
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
    bool has_esrb = false;       ///< an x-esrb line (any value, one typed by hand too)
    QString esrb;                ///< ...its value (the ESRB descriptors step: a letter this run wrote too)
    bool has_esrb_descriptors = false;  ///< an x-esrb-descriptors line (one typed by hand too)
    bool has_esrb_interactive = false;  ///< an x-esrb-interactive line
    int igdb_id = 0;             ///< IGDB's id for it, when Hasheous gave one (library-index.json, or this run)
    bool title_is_file_name = false;
    QVariantMap slot_files;       ///< metadata key ("assets.screenshot", "x-manual") -> its files now
    bool has_box2d = false;
    bool has_box3d = false;
    QStringList hidden_art;  ///< art removed in Library ("<kind>|<file>")
    int launchbox_id = 0;    ///< the LaunchBox game you picked (Needs your check)
    int year = 0;            ///< the year the file's name says ("(2005)"), else the game's release year; 0: none
    QString launchbox_platform;  ///< ...on this LaunchBox platform (picked in a system search), else the system's
};

/// One game's ESRB descriptors as saved (the ESRB descriptors step)
struct EsrbEdit {
    QString path;
    QString title;
    QVariantMap changes;
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
    friend class ::test_Scraper;  // the waits and the downloads, against a local server

public:
    explicit Scraper(QObject* parent = nullptr);
    /// the LaunchBox database (owned by Settings), a source without a login
    void setLaunchBox(LaunchBoxDb* db) { m_lb = db; }

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
    /// the picker highlighted a match: its picture is fetched next
    void thumbFirst(int index);
    void refine(const QString& search_name);
    void skip();

    // Needs your check (metadata/review.json): games a run wasn't sure about,
    // and the ones you never want scraped
    /// [{ path, title, collection, system, closest, score, launchbox, date }], by title
    static QVariantList reviewList();
    static void reviewRemove(const QString& path);
    static void setNeverScrape(const QString& path, const QString& title, bool never);
    static QSet<QString> neverScrape();
    /// libretro has free pictures for this system (ES-DE's name)
    static bool hasFreeArt(const QString& system);
    /// Pegasus X: a source's own ESRB value as x-esrb has it (EC, E, E10+,
    /// T, M, AO or RP), or "" for anything else (KA, Not Rated, blank, a
    /// PEGI or age value...). ScreenScraper's are the codes ("E10+");
    /// LaunchBox's have the name after them ("E10+ - Everyone 10+").
    static QString esrbCode(const QString& value);

signals:
    /// a match's picture arrived (url: a local file) or didn't (error)
    void thumbReady(int index, QString url, int error);
    /// { running, done, total, current, title, result, ok, saved, notFound, failed,
    /// phase ("games" / "esrb"), esrbTotal, esrbSaved, esrbNone, esrbLeft, esrbRemembered, esrbMessage }
    void progress(QVariantMap state);
    void finished(QVariantMap state);
    /// a game was saved: what changed (the loaded game takes it in place)
    void gameSaved(QString path, QVariantMap changes);
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
    int m_run = 0;
    // LaunchBox's database (no login): details and most art, offline once downloaded
    LaunchBoxDb* m_lb = nullptr;
    bool m_lb_done = false;
    std::vector<LaunchBoxDb::Game> m_lb_results;   ///< its games, as found (interactive)
    bool lbReady() const;
    void lbSearch(const QString& name);
    void lbAuto(bool after_other);
    void parseLaunchBox(const LaunchBoxDb::Game& game, bool after_other);
    QVariantMap lbCandidate(const LaunchBoxDb::Game& game) const;
    // keyless sources: Hasheous (who the game is, by its checksum) and
    // libretro's thumbnails (box, screenshot and title screen by name)
    void hashThen(std::function<void(const QString&)> then);
    void identify(std::function<void()> then);
    void libretroArt();
    QString m_hash_name;          ///< the game's name by its checksum (Hasheous), eg. "Chrono Trigger (USA)"
    int m_known_lb_id = 0;        ///< the LaunchBox game picked in Needs your check
    bool m_identified = false;
    bool m_libretro_done = false;
    QHash<QString, QStringList> m_libretro_lists;  ///< a system's thumbnail names, per kind (this run)
    QVariantMap m_review_hint;  ///< the closest match when it wasn't sure enough (closest, score, launchbox)
    void reviewAdd(const Job& job);
    QVariantMap m_match;   ///< what the game was matched to (source, id, name)
    QString m_md5;         ///< its file's MD5, when one was made
    void setMatch(const QString& source, const QString& id, const QString& name);
    void recordMatch(const Job& job);
    static QString indexPath();
    QSet<QString> m_backed_up;   ///< the files backed up this run (before overwriting)
    QSet<QNetworkReply*> m_replies;   ///< requests on their way (Stop aborts them)
    std::atomic<bool> m_cancel_hash { false };
    int m_transport_streak = 0;  ///< requests in a row with no answer
    void countTransport(int status);  // which start() the in-flight answers belong to
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
    void nextThumb();
    std::vector<int> m_thumb_queue;  ///< matches whose picture is still to fetch
    bool m_thumb_busy = false;

    // interactive mode
    bool m_waiting = false;
    QVariantMap m_choice;
    QJsonArray m_ss_results;                ///< ScreenScraper's games, as found
    QString m_choice_source;                ///< "ss" / "launchbox"
    bool interactive() const;
    void search(const QString& name);
    void ssSearch(const QString& name);
    void offerChoice(const QVariantList& candidates);

    /// after the first source: LaunchBox for what's missing, then libretro's pictures
    void fillMore();

    // Pegasus X: the ESRB descriptors step (EsrbDescriptors.h): after the
    // games (Content > ESRB descriptors on), or alone (esrbOnly: Get ESRB
    // descriptors now). Batches of 10, one request at a time, 2.2 s apart at
    // least (the server takes 30 a minute from an address); "later" and a
    // busy server wait and ask again, they never fail the run.
    struct EsrbItem {
        size_t job = 0;
        int tries = 0;           ///< "later" answers so far
        qint64 ready_at = 0;     ///< not asked before this (m_esrb_clock ms)
    };
    bool wantsEsrb() const;
    bool esrbOnly() const;
    void esrbBegin();
    void esrbNext();
    void esrbSend(const std::vector<size_t>& items);
    void esrbAnswer(const std::vector<size_t>& items, int status, const QByteArray& body, const QByteArray& retry_header);
    void esrbRetry(const std::vector<size_t>& items, int wait_s);
    void esrbEnd(const QString& message = QString());
    QString esrbSave(const QString& collection, const std::vector<EsrbEdit>& edits);
    bool m_esrb_phase = false;     ///< asking now
    bool m_esrb_over = false;      ///< asked this run (or nothing to ask)
    std::vector<EsrbItem> m_esrb_items;
    std::vector<size_t> m_esrb_waiting;   ///< items still to ask, in order
    QElapsedTimer m_esrb_clock;
    qint64 m_esrb_last = -1;       ///< the last request's start
    qint64 m_esrb_hold = 0;        ///< no request before this (the server was busy: it waits for everyone)
    QTimer* m_esrb_timer = nullptr;
    QNetworkReply* m_esrb_reply = nullptr;
    QUrl m_esrb_url;
    int m_esrb_total = 0, m_esrb_done = 0, m_esrb_saved = 0, m_esrb_none = 0, m_esrb_left = 0;
    int m_esrb_failures = 0;       ///< requests in a row with no usable answer
    QString m_esrb_message;
    // games with nothing found before (esrb::load_nothing): not asked again
    void esrbSaveNothing();
    QHash<QString, QJsonObject> m_esrb_nothing;
    bool m_esrb_nothing_dirty = false;
    int m_esrb_remembered = 0;     ///< skipped this run: nothing found before
    int m_esrb_batches = 0;
    QString m_esrb_nothing_path;   ///< (tests; empty: esrb::nothing_path())
    QString m_hash_igdb;           ///< IGDB's id for the game, from Hasheous (this game)
    // (tests: shorter waits, saving captured)
    int m_esrb_spacing_ms = 2200;
    int m_esrb_second_ms = 1000;   ///< a second of the server's retryAfter
    std::function<QString(const QString&, const std::vector<EsrbEdit>&)> m_esrb_save_hook;

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
    QElapsedTimer m_game_timer;   ///< since the game started (the log's "Game done in")

    // the game's downloads, several at once (downloadNext): stored in the
    // order they were asked for, whatever order they arrive in
    struct Download {
        Media media;
        int state = 0;   ///< 0 waiting, 1 on its way (or being resized), 2 ready, 3 failed, 4 stored
        QByteArray body;
        QString ext;
    };
    std::vector<Download> m_downloads;
    size_t m_dl_stored = 0;       ///< the first one not stored yet
    bool m_dl_begun = false;
    int m_dl_gen = 0;             ///< which game's downloads an answer belongs to
    int m_dl_most = 0;            ///< the most on their way at once (this game)
    QElapsedTimer m_dl_timer;
    int m_ss_threads = 1;         ///< ScreenScraper's ssuser.maxthreads (1 without it)
    QHash<QString, int> m_host_limits;  ///< downloads at once per server (the others: 1)
    int downloadLimit(const QUrl& url) const;
    void startDownload(size_t index);
    void downloadsDone();
    std::function<void()> m_downloads_done_hook;  ///< (tests: called instead of the miximage and saving)

    void next();
    void finishGame(const QString& result, bool ok);
    void report();
    bool wants(const QString& setting) const;
    bool slotFree(const Job& job, const Media& media) const;

    void screenScraper(const QString& md5);
    void parseScreenScraper(const QJsonObject& game);
    void downloadNext();
    void storeMedia(const Media& media, const Job& job, const QByteArray& body, const QString& ext);
    void save();

    // a GET with retries. A lookup (ScreenScraper, Hasheous, libretro's
    // lists) gives up after 15 s with no data and is tried once more after
    // 2 s (a 429: as many times as `tries_left`, after Retry delay); a
    // download (pictures, videos, manuals) gives up after 20 s with no data
    // and isn't tried again. A transfer that keeps receiving is never cut.
    enum class Fetch { Lookup, Download };
    enum : int { LOOKUP_IDLE_MS = 15000, DOWNLOAD_IDLE_MS = 20000, RETRY_WAIT_MS = 2000 };
    void get(const QUrl& url, const QByteArray& auth, int tries_left,
             std::function<void(int status, const QByteArray& body)> done,
             Fetch fetch = Fetch::Lookup, int again_left = -1);
};

} // namespace scraper
