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


#include "Memory.h"

#include "Log.h"
#include "Paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSValue>
#include <QStringBuilder>


namespace {
QString default_settings_dir()
{
    return paths::writableConfigDir() % QStringLiteral("/theme_settings/");
}

QString json_path_for(const QString& settings_dir, const QString& theme_id)
{
    return settings_dir % theme_id % QLatin1String(".json");
}

void save_map_maybe(const QVariantMap& map, const QString& settings_dir, const QString& theme_id)
{
    if (theme_id.isEmpty())
        return;
    // nothing to keep and nothing kept before: no file needed (but a theme
    // that unset its last key gets an empty file, not its old settings back)
    if (map.isEmpty() && !QFileInfo::exists(json_path_for(settings_dir, theme_id)))
        return;

    if (!QDir(settings_dir).mkpath(QStringLiteral("."))) {
        Log::warning(LOGMSG("could not create directory `%1`. Theme settings will not be saved.")
            .arg(settings_dir));
        return;
    }

    // Pegasus X: written to a temporary file that replaces the old one only
    // when complete, so the app being killed mid-write can't leave broken
    // JSON behind (which used to reset every theme setting)
    const QString json_path = json_path_for(settings_dir, theme_id);
    QSaveFile json_file(json_path);
    if (!json_file.open(QIODevice::WriteOnly)) {
        Log::warning(LOGMSG("could not save theme settings file `%1`: %2")
            .arg(json_path, json_file.errorString()));
        return;
    }

    const auto json_doc = QJsonDocument::fromVariant(map);
    if (json_file.write(json_doc.toJson(QJsonDocument::Compact)) < 0) {
        Log::warning(LOGMSG("failed to write theme settings file `%1`: %2")
            .arg(json_path, json_file.errorString()));
        json_file.cancelWriting();
        return;
    }
    if (!json_file.commit()) {
        Log::warning(LOGMSG("failed to save theme settings file `%1`: %2")
            .arg(json_path, json_file.errorString()));
    }
}

// Pegasus X: the theme's name from its theme.cfg ("name: XboxOSv2")
QString theme_cfg_name(const QString& theme_root_dir)
{
    QFile file(theme_root_dir % QStringLiteral("theme.cfg"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (!line.startsWith(QLatin1String("name"), Qt::CaseInsensitive))
            continue;
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0 || line.leftRef(colon).trimmed().compare(QLatin1String("name"), Qt::CaseInsensitive) != 0)
            continue;
        return line.mid(colon + 1).trimmed();
    }
    return {};
}

// a name as a file name: letters, digits, '-', '_', '.' and spaces
QString file_safe(const QString& name)
{
    QString out;
    for (const QChar c : name) {
        if (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_') || c == QLatin1Char('.') || c == QLatin1Char(' '))
            out += c;
    }
    return out.trimmed();
}

// Pegasus X: a theme folder unpacked under another name (GitHub's "Download
// ZIP" gives XboxOSv2-main, XboxOSv2-refresh, ...) starts from the settings
// kept under the theme's name, copied once to the new folder's file. The
// old file is never removed.
void adopt_settings_by_name(const QString& settings_dir, const QString& theme_id, const QString& theme_root_dir)
{
    const QString own_path = json_path_for(settings_dir, theme_id);
    if (QFileInfo::exists(own_path))
        return;
    const QString name_id = file_safe(theme_cfg_name(theme_root_dir));
    if (name_id.isEmpty() || name_id == theme_id)
        return;
    const QString name_path = json_path_for(settings_dir, name_id);
    if (!QFileInfo::exists(name_path))
        return;
    if (QFile::copy(name_path, own_path))
        Log::info(LOGMSG("Theme settings: `%1` started from `%2` (the theme's name)").arg(theme_id + QStringLiteral(".json"), name_id + QStringLiteral(".json")));
}

QVariantMap load_map_maybe(const QString& settings_dir, const QString& theme_id)
{
    if (theme_id.isEmpty())
        return {};

    const QString json_path = json_path_for(settings_dir, theme_id);
    if (!QFileInfo::exists(json_path))
        return {};

    QFile json_file(json_path);
    if (!json_file.open(QIODevice::ReadOnly)) {
        Log::warning(LOGMSG("could not load theme settings file `%1`: %2")
            .arg(json_path, json_file.errorString()));
        return {};
    }

    QJsonParseError parse_error {};
    const auto json_doc = QJsonDocument::fromJson(json_file.readAll(), &parse_error);
    if (json_doc.isNull()) {
        Log::warning(LOGMSG("failed to parse theme settings file `%1`: %2")
            .arg(json_path, parse_error.errorString()));
        return {};
    }

    return json_doc.object().toVariantMap();
}
} // namespace


namespace model {
Memory::Memory(QObject* parent)
    : Memory(default_settings_dir(), parent)
{}

Memory::Memory(QString settings_dir, QObject* parent)
    : QObject(parent)
    , m_settings_dir(std::move(settings_dir))
{
    m_save_timer.setSingleShot(true);
    m_save_timer.setInterval(400);
    connect(&m_save_timer, &QTimer::timeout, this, &Memory::flushNow);

    // written right away when the app may not get another chance
    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &Memory::flushNow);
        if (auto* const gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
            connect(gui, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state){
                if (state == Qt::ApplicationSuspended || state == Qt::ApplicationInactive
                    || state == Qt::ApplicationHidden)
                    flushNow();
            });
        }
    }
}

void Memory::flush() const
{
    save_map_maybe(m_data, m_settings_dir, m_current_theme);
}

void Memory::markDirty()
{
    m_dirty = true;
    m_save_timer.start();  // restarts: a burst of changes is one save
}

void Memory::flushNow()
{
    m_save_timer.stop();
    if (!m_dirty)
        return;
    m_dirty = false;
    flush();
}

QVariant Memory::get(const QString& key) const
{
    return m_data.value(key);
}

bool Memory::has(const QString& key) const
{
    return m_data.contains(key);
}

void Memory::set(const QString& key, QVariant value)
{
    if (key.isEmpty()) {
        Log::warning(LOGMSG("`set(key,val)` called with empty `key`, ignored"));
        return;
    }
    if (!value.isValid()) {
        Log::warning(LOGMSG("`set(key,val)` called with invalid `val` type, ignored"));
        return;
    }

    // NOTE: QJSValue is a custom type for QVariant
    if (value.userType() == qMetaTypeId<QJSValue>())
        value = value.value<QJSValue>().toVariant();

    m_data[key] = std::move(value);
    emit dataChanged();

    markDirty();
}

void Memory::unset(const QString& key)
{
    m_data.remove(key);
    emit dataChanged();

    markDirty();
}

void Memory::changeTheme(const QString& theme_root_dir)
{
    Q_ASSERT(theme_root_dir.endsWith('/'));
    const int dir_name_start = theme_root_dir.lastIndexOf('/', -2) + 1;
    const int dir_name_len = theme_root_dir.length() - dir_name_start - 1;
    Q_ASSERT(dir_name_len > 0);
    // the old theme's pending changes go to its own file first
    flushNow();
    m_current_theme = theme_root_dir.mid(dir_name_start, dir_name_len);
    adopt_settings_by_name(m_settings_dir, m_current_theme, theme_root_dir);

    m_data = load_map_maybe(m_settings_dir, m_current_theme);
    emit dataChanged();
}
} // namespace model
