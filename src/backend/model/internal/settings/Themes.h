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

#include "utils/MoveOnly.h"

#include <QAbstractListModel>
#include <QTranslator>


namespace model {
struct ThemeEntry {
    QString root_dir;
    QString root_qml;

    QString name;
    QString author;
    QString version;
    QString summary;
    QString description;

    ThemeEntry(QString, QString, QString, QString, QString, QString, QString); // ugh
    MOVE_ONLY(ThemeEntry)
};


class Themes : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString currentName READ currentName NOTIFY themeChanged)
    Q_PROPERTY(QString currentQmlPath READ currentQmlPath NOTIFY themeChanged)

public:
    explicit Themes(QObject* parent = nullptr);
    void postInit();

    enum Roles {
        Name = Qt::UserRole + 1,
        Author,
        Version,
        Summary,
        Description,
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override { return m_role_names; }

    int currentIndex() const { return static_cast<int>(m_current_idx); }
    void setCurrentIndex(int);
    QString currentName() const { return m_themes.at(m_current_idx).name; }
    QString currentQmlDir() const { return m_themes.at(m_current_idx).root_dir; }
    QString currentQmlPath() const { return m_themes.at(m_current_idx).root_qml; }

    /// Pegasus X: the themes folder read again (a theme added or removed in
    /// the app), the theme in use kept
    void rescan();
    /// the index of the theme in this folder, or -1
    Q_INVOKABLE int indexOfDir(const QString& dir) const;

signals:
    void themeChanged(QString);
    void currentIndexChanged();

private:
    const QHash<int, QByteArray> m_role_names;
    std::vector<ThemeEntry> m_themes;

    size_t m_current_idx;
    QTranslator m_translator;

    void select_preferred_theme();
    bool select_theme(const QString&);
    void print_change() const;
};
} // namespace model
