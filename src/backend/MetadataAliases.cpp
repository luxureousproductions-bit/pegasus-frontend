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


#include "MetadataAliases.h"

#include "Paths.h"
#include "SafeFile.h"
#include "model/gaming/Game.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <numeric>


namespace {
const char* const FIELD_NAMES[aliases::FieldCount] = { "genre", "publisher", "developer" };

QMutex s_mutex;
aliases::Table s_table;
bool s_loaded = false;

QStringList& shown_ref(model::GameData& d, int field)
{
    return field == aliases::Genre ? d.genres : field == aliases::Publisher ? d.publishers : d.developers;
}
QStringList& raw_ref(model::GameData& d, int field)
{
    return field == aliases::Genre ? d.raw_genres : field == aliases::Publisher ? d.raw_publishers : d.raw_developers;
}
const QStringList& raw_cref(const model::GameData& d, int field)
{
    return field == aliases::Genre ? d.raw_genres : field == aliases::Publisher ? d.raw_publishers : d.raw_developers;
}
const QStringList& shown_cref(const model::GameData& d, int field)
{
    return field == aliases::Genre ? d.genres : field == aliases::Publisher ? d.publishers : d.developers;
}
void keep_raw(model::GameData& d)
{
    if (d.has_raw)
        return;
    d.raw_genres = d.genres;
    d.raw_publishers = d.publishers;
    d.raw_developers = d.developers;
    d.has_raw = true;
}

bool is_apostrophe(QChar c)
{
    switch (c.unicode()) {
        case 0x0027: case 0x2018: case 0x2019: case 0x201B: case 0x02BC:
        case 0x0060: case 0x00B4: case 0x2032:
            return true;
        default:
            return false;
    }
}

// a name's words, as company_key makes them (before the suffixes go)
QStringList words_of(const QString& name)
{
    const QString decomposed = name.normalized(QString::NormalizationForm_KD);
    QString out;
    out.reserve(decomposed.size() + 8);
    for (const QChar c : decomposed) {
        if (c.category() == QChar::Mark_NonSpacing)
            continue;
        if (c == QLatin1Char('&')) {
            out += QStringLiteral(" and ");
            continue;
        }
        // (S.A., K.K., Co.: one word; Beat'em: one word)
        if (c == QLatin1Char('.') || is_apostrophe(c))
            continue;
        out += c.isLetterOrNumber() ? c.toLower() : QChar(QLatin1Char(' '));
    }
    return out.split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

const QSet<QString>& suffixes()
{
    static const QSet<QString> set {
        QStringLiteral("inc"), QStringLiteral("ltd"), QStringLiteral("co"), QStringLiteral("corp"),
        QStringLiteral("llc"), QStringLiteral("gmbh"), QStringLiteral("sa"), QStringLiteral("kk"),
    };
    return set;
}
const QSet<QString>& regions()
{
    static const QSet<QString> set {
        QStringLiteral("na"), QStringLiteral("usa"), QStringLiteral("europe"), QStringLiteral("eu"),
        QStringLiteral("uk"), QStringLiteral("japan"), QStringLiteral("jp"),
    };
    return set;
}

QStringList company_words(const QString& name)
{
    QStringList w = words_of(name);
    if (w.size() > 1 && w.first() == QLatin1String("the"))
        w.removeFirst();
    while (w.size() > 1 && suffixes().contains(w.last()))
        w.removeLast();
    return w;
}
QStringList region_words(const QString& name)
{
    QStringList w = company_words(name);
    while (w.size() > 1) {
        if (regions().contains(w.last()) || suffixes().contains(w.last()))
            w.removeLast();
        else if (w.size() > 2 && w.last() == QLatin1String("america") && w.at(w.size() - 2) == QLatin1String("of")) {
            w.removeLast();
            w.removeLast();
        }
        else
            break;
    }
    return w;
}

// edits from a to b, or more than `limit` (a quick no)
int distance(const QString& a, const QString& b, int limit)
{
    const int n = a.size(), m = b.size();
    if (std::abs(n - m) > limit)
        return limit + 1;
    std::vector<int> prev(m + 1), cur(m + 1);
    std::iota(prev.begin(), prev.end(), 0);
    for (int i = 1; i <= n; i++) {
        cur[0] = i;
        int row_best = cur[0];
        for (int j = 1; j <= m; j++) {
            const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            cur[j] = std::min({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost });
            row_best = std::min(row_best, cur[j]);
        }
        if (row_best > limit)
            return limit + 1;
        std::swap(prev, cur);
    }
    return prev[m];
}

bool all_in(const QStringList& small, const QStringList& big)
{
    for (const QString& w : small) {
        if (!big.contains(w))
            return false;
    }
    return true;
}
} // namespace


namespace aliases {

int field_of(const QString& name)
{
    for (int f = 0; f < FieldCount; f++) {
        if (name == QLatin1String(FIELD_NAMES[f]))
            return f;
    }
    return -1;
}
QString field_name(int field)
{
    return field >= 0 && field < FieldCount ? QLatin1String(FIELD_NAMES[field]) : QString();
}

bool Table::empty() const
{
    for (int f = 0; f < FieldCount; f++) {
        if (!map[f].isEmpty())
            return false;
    }
    return true;
}

QString file_path()
{
    return paths::writableConfigDir() + QStringLiteral("/metadata/aliases.json");
}

Table load(const QString& path)
{
    Table out;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return out;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    for (int f = 0; f < FieldCount; f++) {
        const QJsonObject obj = root.value(QLatin1String(FIELD_NAMES[f])).toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            const QString to = it.value().toString().trimmed();
            if (!it.key().isEmpty() && !to.isEmpty() && to != it.key())
                out.map[f].insert(it.key(), to);
        }
    }
    return out;
}

bool save(const QString& path, const Table& table)
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    for (int f = 0; f < FieldCount; f++) {
        QJsonObject obj;
        for (auto it = table.map[f].cbegin(); it != table.map[f].cend(); ++it)
            obj.insert(it.key(), it.value());
        root.insert(QLatin1String(FIELD_NAMES[f]), obj);
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    // (never an empty file: with no alias left, the empty lists)
    return safefile::write(path, QJsonDocument(root).toJson(QJsonDocument::Indented));
}

Table current()
{
    QMutexLocker lock(&s_mutex);
    if (!s_loaded) {
        s_table = load(file_path());
        s_loaded = true;
    }
    return s_table;
}
void set_current(const Table& table)
{
    QMutexLocker lock(&s_mutex);
    s_table = table;
    s_loaded = true;
}
void reload()
{
    QMutexLocker lock(&s_mutex);
    s_loaded = false;
}

QString shown(const Table& table, int field, const QString& spelling)
{
    if (field < 0 || field >= FieldCount)
        return spelling;
    const auto it = table.map[field].constFind(spelling);
    if (it != table.map[field].cend())
        return it.value();
    const QString trimmed = spelling.trimmed();
    return table.map[field].value(trimmed, spelling);
}

QStringList shown_list(const Table& table, int field, const QStringList& raw)
{
    if (field < 0 || field >= FieldCount || table.map[field].isEmpty())
        return raw;
    QStringList out;
    out.reserve(raw.size());
    for (const QString& s : raw) {
        const QString v = shown(table, field, s);
        if (!out.contains(v))
            out.append(v);
    }
    return out;
}

QString add(Table& table, int field, const QStringList& from, const QString& to_in)
{
    if (field < 0 || field >= FieldCount)
        return QStringLiteral("not a field");
    QHash<QString, QString>& map = table.map[field];
    const QString to = to_in.trimmed();
    if (to.isEmpty())
        return QStringLiteral("no name to show");
    // `to` shown as another name already: that one, so there's never a chain
    const QString end = map.value(to, to);
    QSet<QString> sources;
    for (const QString& f : from) {
        const QString s = f.trimmed().isEmpty() ? QString() : f;
        if (!s.isEmpty())
            sources.insert(s);
    }
    // ...unless it's one of the spellings being merged: a loop
    if (end != to && sources.contains(end))
        return QStringLiteral("\"%1\" is shown as \"%2\" already: that would be a loop").arg(to, end);
    int added = 0;
    for (const QString& s : sources) {
        if (s == end) {
            map.remove(s);  // (the name shown is itself again)
            continue;
        }
        if (map.value(s) != end) {
            map.insert(s, end);
            added++;
        }
        // the ones shown as this spelling: shown as its end now (A->B, B->C: A->C)
        for (auto it = map.begin(); it != map.end(); ++it) {
            if (it.value() == s)
                it.value() = end;
        }
    }
    return added > 0 ? QString() : QStringLiteral("nothing to merge");
}

int remove(Table& table, int field, const QStringList& from)
{
    if (field < 0 || field >= FieldCount)
        return 0;
    int n = 0;
    for (const QString& s : from)
        n += table.map[field].remove(s);
    return n;
}

QStringList spellings_of(const Table& table, int field, const QString& to)
{
    QStringList out;
    if (field < 0 || field >= FieldCount)
        return out;
    for (auto it = table.map[field].cbegin(); it != table.map[field].cend(); ++it) {
        if (it.value() == to)
            out.append(it.key());
    }
    out.sort(Qt::CaseInsensitive);
    return out;
}

void apply(const std::vector<model::Game*>& games, const Table& table)
{
    for (model::Game* game : games) {
        if (!game)
            continue;
        model::GameData& d = game->dataMut();
        d.raw_genres = d.genres;
        d.raw_publishers = d.publishers;
        d.raw_developers = d.developers;
        d.has_raw = true;
        if (table.empty())
            continue;
        for (int f = 0; f < FieldCount; f++)
            shown_ref(d, f) = shown_list(table, f, raw_cref(d, f));
    }
}

void set_raw(model::Game& game, int field, const QStringList& raw)
{
    if (field < 0 || field >= FieldCount)
        return;
    model::GameData& d = game.dataMut();
    keep_raw(d);
    raw_ref(d, field) = raw;
    shown_ref(d, field) = shown_list(current(), field, raw);
}

bool reapply(model::Game& game, const Table& table)
{
    model::GameData& d = game.dataMut();
    keep_raw(d);
    bool changed = false;
    for (int f = 0; f < FieldCount; f++) {
        QStringList now = shown_list(table, f, raw_cref(d, f));
        if (now != shown_cref(d, f)) {
            shown_ref(d, f) = std::move(now);
            changed = true;
        }
    }
    return changed;
}

QStringList raw_list(const model::Game& game, int field)
{
    if (field < 0 || field >= FieldCount)
        return {};
    const model::GameData& d = game.data();
    return d.has_raw ? raw_cref(d, field) : shown_cref(d, field);
}


QString invisible_key(const QString& name)
{
    static const QRegularExpression spaces(QStringLiteral("[\\s\\x{00A0}\\x{2007}\\x{202F}\\x{2009}\\x{3000}]+"));
    static const QRegularExpression slash(QStringLiteral(" ?/ ?"));
    QString s;
    s.reserve(name.size());
    for (const QChar c : name)
        s += (is_apostrophe(c) ? QChar(QLatin1Char('\'')) : c);
    s.replace(spaces, QStringLiteral(" "));
    s = s.trimmed();
    s.replace(slash, QStringLiteral(" / "));
    return s.toLower();
}

QString company_key(const QString& name)
{
    return company_words(name).join(QLatin1Char(' '));
}

QString region_key(const QString& name)
{
    return region_words(name).join(QLatin1Char(' '));
}

namespace {
// a region_key, made ready for many comparisons
struct Close {
    QStringList words;
    QString joined;    ///< without spaces
    QString letters;   ///< ...and without digits
};
Close close_of(const QString& key)
{
    Close c;
    c.words = key.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    c.joined = c.words.join(QString());
    for (const QChar ch : c.joined) {
        if (!ch.isDigit())
            c.letters += ch;
    }
    return c;
}
bool close_pair(const Close& a, const Close& b)
{
    const QString& sa = a.joined;
    const QString& sb = b.joined;
    const int la = sa.size(), lb = sb.size();
    if (std::min(la, lb) < 4)
        return false;
    // (quick no: lengths too far apart, and neither inside the other)
    const bool inside = la < lb ? sb.contains(sa) : sa.contains(sb);
    if (std::abs(la - lb) > 2 && !inside)
        return false;
    // a division and its parent ("Sega AM2", "Sega"; "Nintendo EAD", "Nintendo"): never
    if (all_in(a.words, b.words) || all_in(b.words, a.words))
        return false;
    if (sa == sb)
        return true;   // only the spaces differ ("Hudson Soft", "HudsonSoft")
    // numbered names ("Studio 2", "Studio 3"): not one name
    if (a.letters == b.letters)
        return false;
    if (inside)
        return true;   // one inside the other
    const int shortest = std::min(la, lb);
    const int allowed = shortest >= 8 ? 2 : shortest >= 5 ? 1 : 0;
    return allowed > 0 && distance(sa, sb, allowed) <= allowed;
}
} // namespace

bool maybe_same(const QString& a, const QString& b)
{
    if (a.isEmpty() || b.isEmpty() || a == b)
        return false;
    return close_pair(close_of(a), close_of(b));
}

std::vector<Suggestion> suggest(const std::vector<Value>& values)
{
    const int n = static_cast<int>(values.size());
    std::vector<QString> k1(n), k2(n);
    for (int i = 0; i < n; i++) {
        k1[i] = company_key(values[i].name);
        k2[i] = region_key(values[i].name);
    }
    const auto make = [&values](int level, std::vector<int> members) -> Suggestion {
        Suggestion s;
        s.level = level;
        std::sort(members.begin(), members.end(), [&values](int a, int b) {
            if (values[a].count != values[b].count)
                return values[a].count > values[b].count;
            return values[a].name.compare(values[b].name, Qt::CaseInsensitive) < 0;
        });
        for (int m : members)
            s.games += values[m].count;
        s.values = std::move(members);
        return s;
    };
    std::vector<Suggestion> out;

    // near-certain: the same company_key
    QHash<QString, std::vector<int>> by_k1, by_k2;
    for (int i = 0; i < n; i++) {
        if (k1[i].isEmpty())
            continue;
        by_k1[k1[i]].push_back(i);
        by_k2[k2[i]].push_back(i);
    }
    for (auto it = by_k1.cbegin(); it != by_k1.cend(); ++it) {
        if (it.value().size() > 1)
            out.push_back(make(0, it.value()));
    }
    // regional: the same region_key, more than one company_key
    for (auto it = by_k2.cbegin(); it != by_k2.cend(); ++it) {
        QSet<QString> companies;
        for (int i : it.value())
            companies.insert(k1[i]);
        if (companies.size() > 1)
            out.push_back(make(1, it.value()));
    }
    // maybe: close region_keys (joined: a close to b, b close to c)
    QStringList keys = by_k2.keys();
    keys.sort();
    const int nk = keys.size();
    std::vector<Close> close(nk);
    for (int i = 0; i < nk; i++)
        close[i] = close_of(keys[i]);
    std::vector<int> parent(nk);
    std::iota(parent.begin(), parent.end(), 0);
    const std::function<int(int)> root = [&parent, &root](int x) { return parent[x] == x ? x : parent[x] = root(parent[x]); };
    for (int i = 0; i < nk; i++) {
        for (int j = i + 1; j < nk; j++) {
            if (close_pair(close[i], close[j]))
                parent[root(i)] = root(j);
        }
    }
    QHash<int, std::vector<int>> clusters;   // root -> key indexes
    for (int i = 0; i < nk; i++)
        clusters[root(i)].push_back(i);
    for (auto it = clusters.cbegin(); it != clusters.cend(); ++it) {
        if (it.value().size() < 2)
            continue;
        std::vector<int> members;
        for (int ki : it.value()) {
            const std::vector<int>& vs = by_k2.value(keys[ki]);
            members.insert(members.end(), vs.begin(), vs.end());
        }
        out.push_back(make(2, members));
    }
    std::stable_sort(out.begin(), out.end(), [](const Suggestion& a, const Suggestion& b) {
        if (a.level != b.level)
            return a.level < b.level;
        return a.games > b.games;
    });
    return out;
}

} // namespace aliases
