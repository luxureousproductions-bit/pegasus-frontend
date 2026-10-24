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

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantList>
#include <functional>
#include <vector>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;


// Pegasus X: themes downloaded in the app (Settings > Get themes). The list is
// the Pegasus theme gallery's (github.com/mmatyas/pegasus-theme-gallery-db,
// repos.txt) plus the GitHub links you add; for each theme its theme.cfg
// (name, author, version, summary) and the pictures of its README are read as
// plain files (raw.githubusercontent.com: no GitHub API, so no hourly limit),
// kept for a week. Installing downloads the repository's zip and unpacks the
// theme into Pegasus' themes folder.
class ThemeStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool installing READ installing NOTIFY installingChanged)

public:
    explicit ThemeStore(QObject* parent = nullptr);

    /// the folder of the theme in use (to tell it apart, and not remove it)
    void setCurrentDirGetter(std::function<QString()> getter) { m_current_dir = std::move(getter); }

    bool loading() const { return m_loading; }
    bool installing() const { return !m_installing.isEmpty(); }

    /// the list: from the copy kept for a week, else (or forced) online
    Q_INVOKABLE void refresh(bool force);
    /// [{ id, name, author, version, summary, description, pictures, url,
    ///    custom, installed, installedVersion, dir, update, current }]
    Q_INVOKABLE QVariantList themes() const;
    Q_INVOKABLE void install(const QString& id);
    Q_INVOKABLE void cancelInstall();
    /// "" or what went wrong
    Q_INVOKABLE QString remove(const QString& id);
    /// a GitHub link ("github.com/owner/repo", ".../tree/<branch>", "owner/repo"): "" or what's wrong
    Q_INVOKABLE QString addLink(const QString& link);
    Q_INVOKABLE void removeLink(const QString& id);

signals:
    void loadingChanged();
    void installingChanged();
    void listChanged();
    void refreshProgress(int done, int total);
    /// stage: "download" / "unpack"
    void installProgress(QString id, qint64 received, qint64 total, QString stage);
    void installFinished(QString id, bool ok, QString message, QString dir);
    /// a theme was added, updated or removed: Pegasus' theme list is read again
    void themesChanged(QString dir);

private:
    struct Entry {
        QString owner;
        QString repo;
        QString branch;      ///< empty: the default branch
        bool custom = false; ///< added by link
        QString name;
        QString author;
        QString version;
        QString summary;
        QString description;
        QStringList pictures;
        QString sha;         ///< the newest commit, if known
        QString id() const;
        QString ref() const { return branch.isEmpty() ? QStringLiteral("HEAD") : branch; }
    };
    struct Installed {
        QString dir;
        QString version;
        QString sha;
    };

    QNetworkAccessManager* m_net = nullptr;
    std::function<QString()> m_current_dir;
    std::vector<Entry> m_entries;
    bool m_loading = false;
    int m_pending = 0;
    int m_done = 0;
    int m_total = 0;
    QString m_installing;
    QPointer<QNetworkReply> m_download;
    QFile* m_zip = nullptr;

    static QString storeDir();
    static QString themesDir();
    QStringList links() const;
    void saveLinks(const QStringList& links) const;
    void loadCache();
    void saveCache() const;
    QHash<QString, Installed> installed() const;

    void get(const QString& url, std::function<void(int status, const QByteArray& body)> done);
    void fetchInfo(size_t index, std::function<void()> done);
    void fetchSha(size_t index, std::function<void()> done);
    void finishRefresh();
    void unpack(const QString& id, const QString& zip_path, const QString& sha);
};
