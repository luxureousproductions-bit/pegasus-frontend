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

#include <QObject>
#include <QPointer>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;


// Pegasus X: Settings > Check for updates. Every signed build of the
// pegasus-x branch is published as the GitHub release "pegasus-x-latest"
// (its notes say "build: <number>", its APKs are named by ABI); a newer build
// is downloaded and handed to Android's installer. Builds are signed with the
// same key, so they install over each other, keeping everything.
class Updater : public QObject {
    Q_OBJECT
    /// idle, checking, available, uptodate, downloading, ready, error
    Q_PROPERTY(QString state READ state NOTIFY changed)
    /// { current, latest, title, notes, size, received, error, canInstall }
    Q_PROPERTY(QVariantMap info READ info NOTIFY changed)

public:
    explicit Updater(QObject* parent = nullptr);

    QString state() const { return m_state; }
    QVariantMap info() const;

    Q_INVOKABLE void check();
    /// the newer build, then Android's installer
    Q_INVOKABLE void download();
    Q_INVOKABLE void install();
    Q_INVOKABLE void cancel();

signals:
    void changed();

private:
    QNetworkAccessManager* m_net = nullptr;
    QPointer<QNetworkReply> m_reply;
    QFile* m_file = nullptr;
    QString m_state = QStringLiteral("idle");
    int m_latest = 0;
    QString m_title;
    QString m_notes;
    QString m_url;
    qint64 m_size = 0;
    qint64 m_received = 0;
    QString m_error;

    void setState(const QString& state, const QString& error = QString());
    QString apkPath() const;
};
