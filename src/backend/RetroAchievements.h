// Pegasus Frontend
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
#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <atomic>
#include <functional>
#include <vector>

class QNetworkAccessManager;


// Pegasus X: RetroAchievements. A game is found on RetroAchievements by
// the hash RA makes of its ROM (rcheevos' rules per console: headers
// stripped, a disc's executable, ...), never by its name. Each console's
// game list with hashes comes from the RA web API with your own account
// key, kept in metadata/ra/ for a week. What's found is written into the
// game's entry as x-ra-* fields (themes read them as game.extra["ra-id"],
// ["ra-achievements"], ["ra-earned"], ["ra-points"], ["ra-title"],
// ["ra-icon"]), and your progress is fetched with the same login.
namespace scraper {

class RetroAchievements : public QObject {
    Q_OBJECT

public:
    /// a game to match: its first file, where it's listed, its ES-DE system
    struct Job {
        QString path;
        QString title;
        QString collection;
        QString system;
        int ra_id = 0;   ///< already matched (progress only)
    };
    struct Result {
        int matched = 0;      ///< games found on RA
        int with_achievements = 0;
        int not_found = 0;    ///< hashed, not on RA
        int unsupported = 0;  ///< a system RA has no console for, or a file that couldn't be hashed
        int progress = 0;     ///< games whose progress was fetched
        QString error;
    };

    explicit RetroAchievements(QObject* parent = nullptr);

    /// RA's console id for an ES-DE system name (0: none)
    static int console_for(const QString& esde_system);
    /// the RA hash of a game file (a .zip's ROM inside hashed for cartridge
    /// systems); empty when it can't be made
    static QString hash_file(int console, const QString& path);
    /// <config>/metadata/ra
    static QString dir();
    /// the cached list of a console: { fetched, games: [{ id, title, icon, achievements, points, hashes }] }
    static QVariantMap console_list(int console);

    bool running() const { return m_running; }
    QVariantMap state() const;
    /// what the login allows: userLoginChecked({ ok, text })
    void checkLogin(const QString& user, const QString& key);
    /// matches every job (hash, look up, write, then your progress);
    /// `jobs` with ra_id set are progress-only. progress(), then finished()
    void start(std::vector<Job> jobs, const QString& user, const QString& key, bool icons, const QString& media_dir,
               bool progress_only);
    void stop();

signals:
    void progress(QVariantMap state);
    void finished(QVariantMap result);
    void loginChecked(QVariantMap result);
    /// a game's entry was written: its x-ra-* fields (the loaded game can take them)
    void gameWritten(QString path, QVariantMap changes);

private:
    struct Match {
        Job job;
        QString hash;
        int ra_id = 0;
        QString ra_title;
        QString icon;      ///< RA's icon path (/Images/...)
        int achievements = 0;
        int points = 0;
        bool unsupported = false;
        bool by_title = false;
        QString icon_file;         ///< the icon downloaded, when it was
        bool progress_known = false;
        bool has_progress = false;
        int earned = 0;
        int earned_hardcore = 0;
    };
    QNetworkAccessManager* m_net = nullptr;
    bool m_running = false;
    std::atomic<bool> m_stop { false };
    int m_run = 0;
    QString m_user;
    QString m_key;
    QString m_media_dir;
    bool m_icons = true;
    std::vector<Job> m_jobs;
    std::vector<Match> m_matches;
    Result m_result;
    QString m_stage;
    int m_done = 0;
    int m_total = 0;
    QFutureWatcher<std::vector<Match>>* m_hash_watcher = nullptr;

    void report();
    void finish();
    void fetchLists(QList<int> consoles, std::function<void()> then);
    void hashAndMatch();
    void downloadIcons(size_t index, std::function<void()> then);
    void fetchProgress(std::function<void()> then);
    void writeAll(std::function<void()> then);
    void get(const QUrl& url, std::function<void(int, const QByteArray&)> done);
    QUrl apiUrl(const QString& endpoint, const QList<QPair<QString, QString>>& params) const;
};

} // namespace scraper
