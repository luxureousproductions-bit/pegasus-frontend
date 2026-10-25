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


#include "ThemeStore.h"

#include "Log.h"
#include "Paths.h"
#include "utils/ZipReader.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStringBuilder>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <memory>


namespace {
const QString LOG_TAG = QStringLiteral("Themes");
const QString GALLERY_LIST = QStringLiteral("https://raw.githubusercontent.com/mmatyas/pegasus-theme-gallery-db/master/repos.txt");
const QString MARKER = QStringLiteral(".pegasusx-theme.json");
constexpr int KEEP_DAYS = 7;
constexpr int PARALLEL = 6;

// "key: value" lines of a theme.cfg (more lines: indented)
QHash<QString, QString> parse_cfg(const QString& text)
{
    QHash<QString, QString> out;
    QString key;
    for (const QString& raw : text.split(QLatin1Char('\n'))) {
        const QString line = QString(raw).remove(QLatin1Char('\r'));
        if (line.trimmed().isEmpty() || line.trimmed().startsWith(QLatin1Char('#')))
            continue;
        if (line.at(0).isSpace() && !key.isEmpty()) {
            const QString more = line.trimmed();
            out[key] += (more == QLatin1String(".") ? QStringLiteral("\n") : QLatin1Char(' ') + more);
            continue;
        }
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon <= 0)
            continue;
        key = line.left(colon).trimmed().toLower();
        out[key] = line.mid(colon + 1).trimmed();
    }
    return out;
}

// the pictures of a README (markdown and <img>), badges and icons left out
QStringList readme_pictures(const QString& text, const QString& raw_base, const QString& owner, const QString& repo)
{
    static const QRegularExpression rx_md(QStringLiteral("!\\[[^\\]]*\\]\\(\\s*<?([^)\\s>]+)"));
    static const QRegularExpression rx_html(QStringLiteral("<img[^>]+src\\s*=\\s*[\"']([^\"']+)[\"']"),
                                            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rx_skip(QStringLiteral("shields\\.io|badge|creativecommons|ko-fi|paypal|buymeacoffee|patreon|\\.svg($|\\?)"),
                                            QRegularExpression::CaseInsensitiveOption);
    QVector<QPair<int, QString>> found;
    for (const QRegularExpression* rx : { &rx_md, &rx_html }) {
        auto it = rx->globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            found.append({ m.capturedStart(), m.captured(1) });
        }
    }
    std::sort(found.begin(), found.end(), [](const QPair<int, QString>& a, const QPair<int, QString>& b){ return a.first < b.first; });

    QStringList out;
    const QString blob = QStringLiteral("https://github.com/%1/%2/blob/").arg(owner, repo);
    for (const auto& item : qAsConst(found)) {
        QString url = item.second.trimmed();
        if (url.isEmpty() || rx_skip.match(url).hasMatch())
            continue;
        if (url.startsWith(QLatin1String("//")))
            url.prepend(QLatin1String("https:"));
        else if (url.startsWith(blob, Qt::CaseInsensitive))
            url = QStringLiteral("https://raw.githubusercontent.com/%1/%2/").arg(owner, repo) + url.mid(blob.length());
        else if (!url.contains(QLatin1String("://"))) {
            while (url.startsWith(QLatin1String("./")))
                url = url.mid(2);
            if (url.startsWith(QLatin1Char('/')))
                url = url.mid(1);
            // many themes keep their pictures in .meta/ (the gallery's
            // custom), some READMEs point beside it: both are tried, the
            // one that doesn't load is skipped
            const QString meta = url.startsWith(QLatin1String(".meta/")) ? QString() : raw_base + QStringLiteral(".meta/") + url;
            url = raw_base + url;
            if (!out.contains(url))
                out.append(url);
            if (!meta.isEmpty() && !out.contains(meta))
                out.append(meta);
            if (out.size() >= 8)
                break;
            continue;
        }
        if (!url.startsWith(QLatin1String("http")))
            continue;
        if (!out.contains(url))
            out.append(url);
        if (out.size() >= 8)
            break;
    }
    return out;
}

// "https://github.com/owner/repo/tree/branch", "github.com/owner/repo.git", "owner/repo"
bool parse_link(const QString& link, QString& owner, QString& repo, QString& branch)
{
    QString text = link.trimmed();
    text.remove(QRegularExpression(QStringLiteral("^(https?://)?(www\\.)?github\\.com/"), QRegularExpression::CaseInsensitiveOption));
    static const QRegularExpression rx(QStringLiteral("^([A-Za-z0-9_.-]+)/([A-Za-z0-9_.-]+?)(\\.git)?(/tree/(.+?))?/?$"));
    const QRegularExpressionMatch m = rx.match(text);
    if (!m.hasMatch())
        return false;
    owner = m.captured(1);
    repo = m.captured(2);
    branch = m.captured(5);
    return !owner.isEmpty() && !repo.isEmpty();
}

bool remove_dir(const QString& path)
{
    return path.isEmpty() || !QFileInfo::exists(path) || QDir(path).removeRecursively();
}

struct UnpackResult {
    bool ok = false;
    QString message;
    QString dir;
};

// the theme out of the zip: the folder with the theme.qml nearest the top
UnpackResult unpack_zip(const QString& zip_path, const QString& themes_dir, const QString& folder,
                        const QString& old_dir, const QJsonObject& marker)
{
    UnpackResult result;
    const utils::ZipReader zip(zip_path);
    if (!zip.isReadable()) {
        result.message = QStringLiteral("The download isn't a zip file.");
        return result;
    }
    const std::vector<utils::ZipReader::Entry>& entries = zip.entries();
    QString root;
    int depth = 1 << 20;
    for (const utils::ZipReader::Entry& e : entries) {
        if (e.is_dir || e.is_symlink)
            continue;
        const QString path = QString(e.path).replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (path == QLatin1String("theme.qml") || path.endsWith(QLatin1String("/theme.qml"))) {
            const int d = path.count(QLatin1Char('/'));
            if (d < depth) {
                depth = d;
                root = path.left(path.length() - QStringLiteral("theme.qml").length());
            }
        }
    }
    if (depth == (1 << 20)) {
        result.message = QStringLiteral("There's no theme in it (no theme.qml).");
        return result;
    }

    QDir().mkpath(themes_dir);
    const QString staging = themes_dir + QStringLiteral("/.pegasusx-new-") + folder;
    remove_dir(staging);
    QDir().mkpath(staging);
    for (const utils::ZipReader::Entry& e : entries) {
        const QString path = QString(e.path).replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (!path.startsWith(root) || path.contains(QLatin1String("../")) || e.is_symlink)
            continue;
        const QString rel = path.mid(root.length());
        if (rel.isEmpty())
            continue;
        const QString target = staging + QLatin1Char('/') + rel;
        if (e.is_dir) {
            QDir().mkpath(target);
            continue;
        }
        QDir().mkpath(QFileInfo(target).absolutePath());
        bool read_ok = false;
        const QByteArray data = zip.fileData(e, &read_ok);
        QFile out(target);
        if (!read_ok || !out.open(QIODevice::WriteOnly) || out.write(data) < 0) {
            remove_dir(staging);
            result.message = QStringLiteral("Couldn't write `%1`.").arg(rel);
            return result;
        }
    }
    if (!QFileInfo::exists(staging + QStringLiteral("/theme.cfg"))) {
        remove_dir(staging);
        result.message = QStringLiteral("There's no theme.cfg in it.");
        return result;
    }
    {
        QSaveFile file(staging + QLatin1Char('/') + MARKER);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(marker).toJson());
            file.commit();
        }
    }

    // the old copy goes, the new one takes its place
    const QString target = themes_dir + QLatin1Char('/') + folder;
    if (!old_dir.isEmpty() && QDir(old_dir).absolutePath() != QDir(target).absolutePath())
        remove_dir(old_dir);
    if (!remove_dir(target) || !QDir().rename(staging, target)) {
        remove_dir(staging);
        result.message = QStringLiteral("Couldn't replace `%1`.").arg(target);
        return result;
    }
    result.ok = true;
    result.dir = target + QLatin1Char('/');
    return result;
}
} // namespace


QString ThemeStore::Entry::id() const
{
    const QString base = owner.toLower() % QLatin1Char('/') % repo.toLower();
    return branch.isEmpty() ? base : base % QLatin1Char('@') % branch;
}

ThemeStore::ThemeStore(QObject* parent)
    : QObject(parent)
{}

QString ThemeStore::storeDir()
{
    return paths::writableConfigDir() + QStringLiteral("/pegasusx-themes");
}

QString ThemeStore::themesDir()
{
    return paths::writableConfigDir() + QStringLiteral("/themes");
}

QStringList ThemeStore::links() const
{
    QFile file(storeDir() + QStringLiteral("/links.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QStringList out;
    for (const QJsonValue& v : QJsonDocument::fromJson(file.readAll()).array())
        out.append(v.toString());
    return out;
}

void ThemeStore::saveLinks(const QStringList& list) const
{
    QDir().mkpath(storeDir());
    QSaveFile file(storeDir() + QStringLiteral("/links.json"));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(QJsonArray::fromStringList(list)).toJson());
        file.commit();
    }
}

void ThemeStore::loadCache()
{
    QFile file(storeDir() + QStringLiteral("/cache.json"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    m_entries.clear();
    for (const QJsonValue& v : root.value(QStringLiteral("themes")).toArray()) {
        const QJsonObject o = v.toObject();
        Entry e;
        e.owner = o.value(QStringLiteral("owner")).toString();
        e.repo = o.value(QStringLiteral("repo")).toString();
        e.branch = o.value(QStringLiteral("branch")).toString();
        e.custom = o.value(QStringLiteral("custom")).toBool();
        e.name = o.value(QStringLiteral("name")).toString();
        e.author = o.value(QStringLiteral("author")).toString();
        e.version = o.value(QStringLiteral("version")).toString();
        e.summary = o.value(QStringLiteral("summary")).toString();
        e.description = o.value(QStringLiteral("description")).toString();
        e.sha = o.value(QStringLiteral("sha")).toString();
        for (const QJsonValue& p : o.value(QStringLiteral("pictures")).toArray())
            e.pictures.append(p.toString());
        if (!e.owner.isEmpty() && !e.repo.isEmpty())
            m_entries.push_back(e);
    }
}

void ThemeStore::saveCache() const
{
    QJsonArray list;
    for (const Entry& e : m_entries) {
        list.append(QJsonObject {
            { QStringLiteral("owner"), e.owner },
            { QStringLiteral("repo"), e.repo },
            { QStringLiteral("branch"), e.branch },
            { QStringLiteral("custom"), e.custom },
            { QStringLiteral("name"), e.name },
            { QStringLiteral("author"), e.author },
            { QStringLiteral("version"), e.version },
            { QStringLiteral("summary"), e.summary },
            { QStringLiteral("description"), e.description },
            { QStringLiteral("sha"), e.sha },
            { QStringLiteral("pictures"), QJsonArray::fromStringList(e.pictures) },
        });
    }
    QDir().mkpath(storeDir());
    QSaveFile file(storeDir() + QStringLiteral("/cache.json"));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(QJsonObject {
            { QStringLiteral("fetched"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
            { QStringLiteral("themes"), list },
        }).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

// the themes in the themes folder: by the marker written when installed here,
// else by the folder's name (installed by hand)
namespace {
// a name as it's compared: letters and digits only, lower case, without the
// usual decoration of theme repositories
QString plain_name(QString text)
{
    text = text.toLower();
    for (const char* fluff : { "pegasus-theme-", "pegasus_theme_", "pegasustheme", "-theme", "_theme", "-master", "-main", "theme" }) {
        const QString f = QLatin1String(fluff);
        if (text.startsWith(f) && text.length() > f.length())
            text = text.mid(f.length());
        if (text.endsWith(f) && text.length() > f.length())
            text.chop(f.length());
    }
    QString out;
    for (const QChar ch : text) {
        if (ch.isLetterOrNumber())
            out += ch;
    }
    return out;
}
} // namespace

// the themes in every themes folder Pegasus reads (the Pegasus X folder, the
// app's own, a config folder beside the program): by the marker written when
// installed here, else by the folder's name or the theme's name and author
// (installed by hand or shipped with the device), so a theme shows as
// installed once, wherever it is
QHash<QString, ThemeStore::Installed> ThemeStore::installed() const
{
    QHash<QString, Installed> out;
    struct Found { Installed info; QString folder; QString name; QString author; };
    std::vector<Found> by_hand;
    QStringList dirs { themesDir() };
    for (const QString& config_dir : paths::configDirs()) {
        if (!config_dir.startsWith(QLatin1Char(':')) && !dirs.contains(config_dir + QStringLiteral("/themes")))
            dirs.append(config_dir + QStringLiteral("/themes"));
    }
    for (const QString& themes : dirs) {
        QDirIterator it(themes, QDir::Dirs | QDir::NoDotAndDotDot);
        while (it.hasNext()) {
            const QString dir = it.next();
            const QString folder = QFileInfo(dir).fileName();
            if (folder.startsWith(QLatin1Char('.')) || !QFileInfo::exists(dir + QStringLiteral("/theme.cfg")))
                continue;
            Found found;
            found.info.dir = dir + QLatin1Char('/');
            found.folder = folder;
            QFile cfg(dir + QStringLiteral("/theme.cfg"));
            if (cfg.open(QIODevice::ReadOnly)) {
                const QHash<QString, QString> values = parse_cfg(QString::fromUtf8(cfg.readAll()));
                found.info.version = values.value(QStringLiteral("version"));
                found.name = values.value(QStringLiteral("name"));
                found.author = values.value(QStringLiteral("author"));
            }
            QFile marker(dir + QLatin1Char('/') + MARKER);
            if (marker.open(QIODevice::ReadOnly)) {
                const QJsonObject o = QJsonDocument::fromJson(marker.readAll()).object();
                found.info.sha = o.value(QStringLiteral("sha")).toString();
                const QString id = o.value(QStringLiteral("id")).toString();
                if (!out.contains(id))
                    out.insert(id, found.info);
                continue;
            }
            by_hand.push_back(found);
        }
    }
    // installed by hand: the folder named like the repository, else the
    // theme's name (and author, when both have one)
    for (const Entry& e : m_entries) {
        if (out.contains(e.id()))
            continue;
        const Found* match = nullptr;
        for (const Found& f : by_hand) {
            if (plain_name(f.folder) == plain_name(e.repo)) {
                match = &f;
                break;
            }
        }
        if (!match && !e.name.isEmpty()) {
            for (const Found& f : by_hand) {
                if (plain_name(f.name) != plain_name(e.name) || f.name.isEmpty())
                    continue;
                const bool authors_known = !f.author.isEmpty() && !e.author.isEmpty();
                if (!authors_known || plain_name(f.author) == plain_name(e.author)) {
                    match = &f;
                    break;
                }
            }
        }
        if (match)
            out.insert(e.id(), match->info);
    }
    return out;
}

void ThemeStore::get(const QString& url, std::function<void(int, const QByteArray&)> done)
{
    if (!m_net)
        m_net = new QNetworkAccessManager(this);
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);
    QNetworkReply* const reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, done]{
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        done(status, body);
    });
}

void ThemeStore::refresh(bool force)
{
    if (m_loading)
        return;
    // the copy of the last week
    if (!force) {
        QFile file(storeDir() + QStringLiteral("/cache.json"));
        if (file.open(QIODevice::ReadOnly)) {
            const QDateTime fetched = QDateTime::fromString(
                QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("fetched")).toString(), Qt::ISODate);
            file.close();
            if (fetched.isValid() && fetched.daysTo(QDateTime::currentDateTimeUtc()) < KEEP_DAYS) {
                loadCache();
                if (!m_entries.empty()) {
                    emit listChanged();
                    return;
                }
            }
        }
    }

    m_loading = true;
    emit loadingChanged();
    get(GALLERY_LIST, [this](int status, const QByteArray& body){
        std::vector<Entry> fresh;
        QSet<QString> seen;
        if (status == 200) {
            for (const QString& line : QString::fromUtf8(body).split(QLatin1Char('\n'))) {
                Entry e;
                if (!parse_link(line, e.owner, e.repo, e.branch) || seen.contains(e.id()))
                    continue;
                seen.insert(e.id());
                fresh.push_back(e);
            }
        }
        else {
            Log::warning(LOG_TAG, LOGMSG("Theme gallery list: error %1").arg(status));
            loadCache();  // offline: what's known
            for (const Entry& e : m_entries) {
                if (!e.custom && !seen.contains(e.id())) {
                    seen.insert(e.id());
                    fresh.push_back(e);
                }
            }
        }
        for (const QString& link : links()) {
            Entry e;
            e.custom = true;
            if (parse_link(link, e.owner, e.repo, e.branch) && !seen.contains(e.id())) {
                seen.insert(e.id());
                fresh.push_back(e);
            }
        }
        m_entries = std::move(fresh);
        m_total = static_cast<int>(m_entries.size());
        m_done = 0;
        m_pending = 0;
        if (m_entries.empty()) {
            finishRefresh();
            return;
        }
        // a few at a time
        auto next = std::make_shared<size_t>(0);
        auto step = std::make_shared<std::function<void()>>();
        *step = [this, next, step]{
            while (m_pending < PARALLEL && *next < m_entries.size()) {
                const size_t index = (*next)++;
                m_pending++;
                fetchInfo(index, [this, step]{
                    m_pending--;
                    m_done++;
                    emit refreshProgress(m_done, m_total);
                    if (m_done >= m_total)
                        finishRefresh();
                    else
                        (*step)();
                });
            }
        };
        (*step)();
    });
}

// a theme's theme.cfg and README (and its newest commit, if it's installed)
void ThemeStore::fetchInfo(size_t index, std::function<void()> done)
{
    const Entry e = m_entries[index];
    const QString raw = QStringLiteral("https://raw.githubusercontent.com/%1/%2/%3/").arg(e.owner, e.repo, e.ref());
    get(raw + QStringLiteral("theme.cfg"), [this, index, raw, done](int status, const QByteArray& body){
        if (index >= m_entries.size()) {
            done();
            return;
        }
        if (status == 200) {
            const QHash<QString, QString> cfg = parse_cfg(QString::fromUtf8(body));
            Entry& entry = m_entries[index];
            entry.name = cfg.value(QStringLiteral("name"));
            entry.author = cfg.value(QStringLiteral("author"));
            entry.version = cfg.value(QStringLiteral("version"));
            entry.summary = cfg.value(QStringLiteral("summary"));
            entry.description = cfg.value(QStringLiteral("description"));
        }
        // README.md, else its other spellings
        auto names = std::make_shared<QStringList>(QStringList { QStringLiteral("README.md"), QStringLiteral("README.MD"), QStringLiteral("readme.md"), QStringLiteral("Readme.md") });
        auto try_next = std::make_shared<std::function<void()>>();
        *try_next = [this, index, raw, names, try_next, done]{
            if (names->isEmpty() || index >= m_entries.size()) {
                if (index < m_entries.size() && !installed().value(m_entries[index].id()).dir.isEmpty())
                    fetchSha(index, done);
                else
                    done();
                return;
            }
            const QString name = names->takeFirst();
            get(raw + name, [this, index, raw, try_next, names, done](int st, const QByteArray& b){
                if (st == 200 && index < m_entries.size()) {
                    Entry& entry = m_entries[index];
                    entry.pictures = readme_pictures(QString::fromUtf8(b), raw, entry.owner, entry.repo);
                    names->clear();
                }
                if (names->isEmpty() && index < m_entries.size() && m_entries[index].pictures.isEmpty()) {
                    for (const char* guess : { ".meta/screenshot.png", ".meta/screenshot.jpg", ".meta/screenshots/0.png", "screenshot.png", "screenshot.jpg" })
                        m_entries[index].pictures.append(raw + QLatin1String(guess));
                }
                (*try_next)();
            });
        };
        (*try_next)();
    });
}

// the newest commit of the branch, from GitHub's commit feed (no API limit)
void ThemeStore::fetchSha(size_t index, std::function<void()> done)
{
    const Entry e = m_entries[index];
    get(QStringLiteral("https://github.com/%1/%2/commits/%3.atom").arg(e.owner, e.repo, e.ref()),
        [this, index, done](int status, const QByteArray& body){
            static const QRegularExpression rx(QStringLiteral("Commit/([0-9a-f]{40})"));
            const QRegularExpressionMatch m = rx.match(QString::fromUtf8(body));
            if (status == 200 && m.hasMatch() && index < m_entries.size())
                m_entries[index].sha = m.captured(1);
            done();
        });
}

void ThemeStore::finishRefresh()
{
    // nameless ones (no theme.cfg found): by the repository's name
    for (Entry& e : m_entries) {
        if (e.name.isEmpty())
            e.name = e.repo;
    }
    std::sort(m_entries.begin(), m_entries.end(), [](const Entry& a, const Entry& b){
        return QString::localeAwareCompare(a.name.toLower(), b.name.toLower()) < 0;
    });
    saveCache();
    m_loading = false;
    emit loadingChanged();
    emit listChanged();
}

QVariantList ThemeStore::themes() const
{
    const QHash<QString, Installed> have = installed();
    const QString current = m_current_dir ? QDir(m_current_dir()).absolutePath() : QString();
    QVariantList out;
    for (const Entry& e : m_entries) {
        const Installed inst = have.value(e.id());
        const bool is_installed = !inst.dir.isEmpty();
        // newer: another commit (when both are known), else another version
        const bool update = is_installed
            && ((!inst.sha.isEmpty() && !e.sha.isEmpty()) ? inst.sha != e.sha
                : (!e.version.isEmpty() && !inst.version.isEmpty() && e.version != inst.version));
        out.append(QVariantMap {
            { QStringLiteral("id"), e.id() },
            { QStringLiteral("name"), e.name.isEmpty() ? e.repo : e.name },
            { QStringLiteral("author"), e.author.isEmpty() ? e.owner : e.author },
            { QStringLiteral("version"), e.version },
            { QStringLiteral("summary"), e.summary },
            { QStringLiteral("description"), e.description },
            { QStringLiteral("pictures"), e.pictures },
            { QStringLiteral("url"), QStringLiteral("github.com/%1/%2").arg(e.owner, e.repo)
                                     + (e.branch.isEmpty() ? QString() : QStringLiteral(" · ") + e.branch) },
            { QStringLiteral("custom"), e.custom },
            { QStringLiteral("installed"), is_installed },
            { QStringLiteral("installedVersion"), inst.version },
            { QStringLiteral("dir"), inst.dir },
            { QStringLiteral("update"), update },
            { QStringLiteral("current"), is_installed && QDir(inst.dir).absolutePath() == current },
        });
    }
    return out;
}

QString ThemeStore::addLink(const QString& link)
{
    Entry e;
    e.custom = true;
    if (!parse_link(link, e.owner, e.repo, e.branch))
        return tr("That isn't a GitHub link, eg. github.com/owner/theme (or .../tree/branch).");
    QStringList list = links();
    for (const Entry& known : m_entries) {
        if (known.id() == e.id())
            return tr("That theme is in the list already.");
    }
    list.append(QStringLiteral("%1/%2").arg(e.owner, e.repo) + (e.branch.isEmpty() ? QString() : QStringLiteral("/tree/") + e.branch));
    saveLinks(list);
    m_entries.push_back(e);
    const size_t index = m_entries.size() - 1;
    emit listChanged();
    m_loading = true;
    emit loadingChanged();
    fetchInfo(index, [this, index]{
        if (index < m_entries.size()) {
            fetchSha(index, [this]{
                saveCache();
                m_loading = false;
                emit loadingChanged();
                emit listChanged();
            });
            return;
        }
        m_loading = false;
        emit loadingChanged();
        emit listChanged();
    });
    return {};
}

void ThemeStore::removeLink(const QString& id)
{
    QStringList list = links();
    QStringList kept;
    for (const QString& link : qAsConst(list)) {
        Entry e;
        if (parse_link(link, e.owner, e.repo, e.branch) && e.id() != id)
            kept.append(link);
    }
    saveLinks(kept);
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [&id](const Entry& e){
        return e.custom && e.id() == id;
    }), m_entries.end());
    saveCache();
    emit listChanged();
}

QString ThemeStore::remove(const QString& id)
{
    const Installed inst = installed().value(id);
    if (inst.dir.isEmpty())
        return tr("It isn't installed.");
    if (m_current_dir && QDir(inst.dir).absolutePath() == QDir(m_current_dir()).absolutePath())
        return tr("It's the theme in use: pick another theme first.");
    if (!QDir(inst.dir).removeRecursively())
        return tr("Couldn't remove `%1`.").arg(inst.dir);
    emit themesChanged(inst.dir);
    emit listChanged();
    return {};
}

void ThemeStore::install(const QString& id)
{
    if (!m_installing.isEmpty())
        return;
    size_t index = m_entries.size();
    for (size_t i = 0; i < m_entries.size(); i++) {
        if (m_entries[i].id() == id)
            index = i;
    }
    if (index >= m_entries.size()) {
        emit installFinished(id, false, tr("That theme isn't in the list."), QString());
        return;
    }
    m_installing = id;
    emit installingChanged();
    emit installProgress(id, 0, 0, QStringLiteral("download"));

    // the newest commit first (to know about updates later), then the zip
    fetchSha(index, [this, index, id]{
        if (m_installing != id || index >= m_entries.size())
            return;
        const Entry e = m_entries[index];
        const QString url = e.branch.isEmpty()
            ? QStringLiteral("https://github.com/%1/%2/archive/HEAD.zip").arg(e.owner, e.repo)
            : QStringLiteral("https://github.com/%1/%2/archive/refs/heads/%3.zip").arg(e.owner, e.repo, e.branch);
        QDir().mkpath(storeDir());
        const QString zip_path = storeDir() + QStringLiteral("/download.zip");
        delete m_zip;
        m_zip = new QFile(zip_path, this);
        if (!m_zip->open(QIODevice::WriteOnly)) {
            m_installing.clear();
            emit installingChanged();
            emit installFinished(id, false, tr("Couldn't save the download."), QString());
            return;
        }
        if (!m_net)
            m_net = new QNetworkAccessManager(this);
        QNetworkRequest request{QUrl(url)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setTransferTimeout(60000);
        QNetworkReply* const reply = m_net->get(request);
        m_download = reply;
        connect(reply, &QNetworkReply::readyRead, this, [this, reply]{
            if (m_zip)
                m_zip->write(reply->readAll());
        });
        connect(reply, &QNetworkReply::downloadProgress, this, [this, id](qint64 received, qint64 total){
            emit installProgress(id, received, total, QStringLiteral("download"));
        });
        const QString sha = e.sha;
        connect(reply, &QNetworkReply::finished, this, [this, reply, id, zip_path, sha]{
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool aborted = reply->error() == QNetworkReply::OperationCanceledError;
            if (m_zip) {
                m_zip->write(reply->readAll());
                m_zip->close();
            }
            reply->deleteLater();
            if (aborted || status != 200) {
                QFile::remove(zip_path);
                m_installing.clear();
                emit installingChanged();
                emit installFinished(id, false, aborted ? tr("Stopped.")
                    : status == 404 ? tr("GitHub doesn't have it (404): check the link and branch.")
                    : status == 0 ? tr("No connection.") : tr("GitHub answered %1.").arg(status), QString());
                return;
            }
            unpack(id, zip_path, sha);
        });
    });
}

void ThemeStore::cancelInstall()
{
    if (m_download)
        m_download->abort();
}

void ThemeStore::unpack(const QString& id, const QString& zip_path, const QString& sha)
{
    emit installProgress(id, 0, 0, QStringLiteral("unpack"));
    const Installed before = installed().value(id);
    QString folder;
    for (const Entry& e : m_entries) {
        if (e.id() == id)
            folder = e.repo;
    }
    folder.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
    const QJsonObject marker {
        { QStringLiteral("id"), id },
        { QStringLiteral("sha"), sha },
        { QStringLiteral("installed"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
    };
    const QString themes = themesDir();
    const QString old_dir = before.dir;
    auto* const watcher = new QFutureWatcher<UnpackResult>(this);
    connect(watcher, &QFutureWatcher<UnpackResult>::finished, this, [this, watcher, id, zip_path]{
        const UnpackResult result = watcher->result();
        watcher->deleteLater();
        QFile::remove(zip_path);
        m_installing.clear();
        emit installingChanged();
        if (result.ok) {
            Log::info(LOG_TAG, LOGMSG("Installed `%1` into `%2`").arg(id, result.dir));
            emit themesChanged(result.dir);
            emit listChanged();
        }
        emit installFinished(id, result.ok, result.message, result.dir);
    });
    watcher->setFuture(QtConcurrent::run([zip_path, themes, folder, old_dir, marker]{
        return unpack_zip(zip_path, themes, folder, old_dir, marker);
    }));
}
