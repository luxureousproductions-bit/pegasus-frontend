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


#include <QtTest/QtTest>

#include "FolderCopy.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>


// Pegasus X: the Pegasus X folder's move (Settings > Pegasus X folder): what
// it copies, what it leaves out, and the progress the screen shows
class test_FolderCopy : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmp;
    QString from() const { return m_tmp.filePath(QStringLiteral("from")); }
    QString to() const { return m_tmp.filePath(QStringLiteral("to")); }
    void write(const QString& rel, const QByteArray& data) {
        const QString path = from() + QLatin1Char('/') + rel;
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(data);
    }
    QByteArray read(const QString& path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<missing>");
    }

private slots:
    void init();
    void counts_what_it_copies();
    void copies_with_progress();
    void big_file_in_parts();
    void replaces_whats_there();
    void reports_a_failed_copy();
};

void test_FolderCopy::init()
{
    QDir(from()).removeRecursively();
    QDir(to()).removeRecursively();
    QDir().mkpath(from());
    write(QStringLiteral("pegasusx.ini"), "a=1\n");
    write(QStringLiteral("metadata/aliases.json"), "{}");
    write(QStringLiteral("downloaded_media/snes/covers/Super Mario World.png"), QByteArray(3000, 'x'));
    write(QStringLiteral("downloaded_media/snes/covers/empty.png"), QByteArray());
    write(QStringLiteral(".hidden"), "h");
    write(QStringLiteral("pegasusx-root.txt"), "/somewhere\n");       // left out
    write(QStringLiteral("pegasusx-wizard/step"), "2");                // left out (a folder)
    QDir().mkpath(from() + QStringLiteral("/themes/empty-folder"));
}

void test_FolderCopy::counts_what_it_copies()
{
    const QStringList skip { QStringLiteral("pegasusx-root.txt"), QStringLiteral("pegasusx-wizard") };
    const folder_copy::Size size = folder_copy::size_of(from(), skip);
    QCOMPARE(size.files, 5);
    QCOMPARE(size.bytes, qint64(4 + 2 + 3000 + 0 + 1));
    QCOMPARE(folder_copy::size_of(from(), {}).files, 7);
}

void test_FolderCopy::copies_with_progress()
{
    const QStringList skip { QStringLiteral("pegasusx-root.txt"), QStringLiteral("pegasusx-wizard") };
    const folder_copy::Size size = folder_copy::size_of(from(), skip);
    QString error;
    qint64 last_bytes = -1;
    int last_files = -1;
    int calls = 0;
    bool backwards = false;
    const int copied = folder_copy::copy(from(), to(), skip, error, [&](qint64 bytes, int files, const QString& file) {
        calls++;
        if (bytes < last_bytes || files < last_files)
            backwards = true;
        last_bytes = bytes;
        last_files = files;
        QVERIFY(!file.isEmpty() && !file.startsWith(QLatin1Char('/')));
    });
    QCOMPARE(error, QString());
    QCOMPARE(copied, 5);
    QVERIFY(calls >= 5);
    QVERIFY(!backwards);
    // the end: everything counted beforehand, no more
    QCOMPARE(last_bytes, size.bytes);
    QCOMPARE(last_files, size.files);
    QCOMPARE(read(to() + QStringLiteral("/pegasusx.ini")), QByteArray("a=1\n"));
    QCOMPARE(read(to() + QStringLiteral("/downloaded_media/snes/covers/Super Mario World.png")), QByteArray(3000, 'x'));
    QCOMPARE(read(to() + QStringLiteral("/downloaded_media/snes/covers/empty.png")), QByteArray());
    QCOMPARE(read(to() + QStringLiteral("/.hidden")), QByteArray("h"));
    QVERIFY(QFileInfo(to() + QStringLiteral("/themes/empty-folder")).isDir());
    QVERIFY(!QFileInfo::exists(to() + QStringLiteral("/pegasusx-root.txt")));
    QVERIFY(!QFileInfo::exists(to() + QStringLiteral("/pegasusx-wizard")));
}

// a big file (a video, say): the progress moves while it's copied, not just at its end
void test_FolderCopy::big_file_in_parts()
{
    write(QStringLiteral("downloaded_media/snes/videos/big.mp4"), QByteArray(3 * 1048576 + 5, 'v'));
    QList<qint64> during;
    QString error;
    QVERIFY(folder_copy::copy(from(), to(), {}, error, [&](qint64 bytes, int, const QString& file) {
        if (file.endsWith(QLatin1String("big.mp4")))
            during.append(bytes);
    }) > 0);
    QVERIFY2(during.size() >= 4, qPrintable(QString::number(during.size())));   // 4 parts and the file's end
    QCOMPARE(QFileInfo(to() + QStringLiteral("/downloaded_media/snes/videos/big.mp4")).size(), qint64(3 * 1048576 + 5));
}

void test_FolderCopy::replaces_whats_there()
{
    QDir().mkpath(to());
    QFile old(to() + QStringLiteral("/pegasusx.ini"));
    QVERIFY(old.open(QIODevice::WriteOnly));
    old.write("old, much longer than the new one\n");
    old.close();
    QString error;
    QVERIFY(folder_copy::copy(from(), to(), {}, error) > 0);
    QCOMPARE(read(to() + QStringLiteral("/pegasusx.ini")), QByteArray("a=1\n"));
}

void test_FolderCopy::reports_a_failed_copy()
{
    // a folder where a file should go: that file can't be written
    QDir().mkpath(to() + QStringLiteral("/pegasusx.ini/in-the-way"));
    QString error;
    QCOMPARE(folder_copy::copy(from(), to(), {}, error), -1);
    QVERIFY(error.contains(QLatin1String("pegasusx.ini")));
    // a file that isn't there
    QVERIFY(!folder_copy::copy_file(m_tmp.filePath(QStringLiteral("nope")), to() + QStringLiteral("/x")));
    QVERIFY(!QFileInfo::exists(to() + QStringLiteral("/x")));
}


QTEST_MAIN(test_FolderCopy)
#include "test_FolderCopy.moc"
