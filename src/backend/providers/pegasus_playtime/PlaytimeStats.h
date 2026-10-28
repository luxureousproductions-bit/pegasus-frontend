// Pegasus Frontend
// Copyright (C) 2017-2019  Mátyás Mustoha
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

#include "providers/Provider.h"

#include <QDateTime>
#include <QFuture>
#include <QMutex>


namespace providers {
namespace playtime {

class PlaytimeStats : public Provider {
    Q_OBJECT

public:
    explicit PlaytimeStats(QString db_path, QObject* parent = nullptr);
    explicit PlaytimeStats(QObject* parent = nullptr);
    ~PlaytimeStats();

    Provider& run(SearchContext&) final;

    void onGameLaunched(model::GameFile* const) final;
    void onGameFinished(model::GameFile* const) final;

signals:
    void startedWriting();
    void finishedWriting();

private:
    const QString m_db_path;

    QDateTime m_last_launch_time;

    struct QueueEntry {
        model::GameFile* const gamefile;
        const QDateTime launch_time;
        const qint64 duration;

        QueueEntry(model::GameFile* const gamefile, QDateTime launch_time, qint64 duration)
            : gamefile(std::move(gamefile))
            , launch_time(std::move(launch_time))
            , duration(std::move(duration))
        {}
    };
    std::vector<QueueEntry> m_pending_tasks;
    std::vector<QueueEntry> m_active_tasks;

    // (the paths, not the game file: the library may drop the game before
    // the migration runs)
    struct MigrationQueueEntry {
        QString uri;
        QString path;

        MigrationQueueEntry(QString uri, QString path)
            : uri(std::move(uri)), path(std::move(path))
        {}
    };
    std::vector<MigrationQueueEntry> m_pending_migrations;
    std::vector<MigrationQueueEntry> m_active_migrations;

    QMutex m_queue_guard;
    // the writer thread, waited for before this goes away (it emits
    // finishedWriting from its thread as its last step: the test's spy, and
    // this object, must still be there then)
    QFuture<void> m_writer;

    void start_processing();
};

} // namespace playtime
} // namespace providers
