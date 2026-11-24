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

#include "SafeFile.h"

#include <QTemporaryDir>


// Pegasus X: the safe save (SafeFile.h), on a throwaway folder. A save cut at
// any step (the test hook stops there as a power cut would) leaves the old
// file whole, or the new one whole: never part of one, never an empty file.
// The fallback (the storage refusing the rename over the old file) is cut at
// each of its steps too.
class test_SafeFile : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmp;
    QString m_path;
    QByteArray m_old;
    QByteArray m_new;

    static QByteArray read(const QString& path);
    static void write_plain(const QString& path, const QByteArray& bytes);
    // the files in the folder, by name
    QStringList names() const;
    // a step that stops the save there; `fail`: a step that fails instead
    static void cut_at(const char* stop, const char* fail = nullptr);

private slots:
    void init();
    void cleanup();

    void saves();
    void saves_a_new_file();
    void text_mode_keeps_the_text();
    void cut_before_the_swap_keeps_the_old_file();
    void refused_rename_uses_the_fallback();
    void fallback_saves_a_new_file();
    void fallback_cut_at_each_step();
    void fallback_after_a_cut_save_still_works();
    void folder_refusing_new_files_keeps_the_old_file();
};

QByteArray test_SafeFile::read(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void test_SafeFile::write_plain(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(bytes), qint64(bytes.size()));
}

QStringList test_SafeFile::names() const
{
    QStringList out = QDir(m_tmp.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
    return out;
}

void test_SafeFile::cut_at(const char* stop, const char* fail)
{
    const QByteArray stop_at(stop ? stop : "");
    const QByteArray fail_at(fail ? fail : "");
    safefile::test_hook = [stop_at, fail_at](const char* step) {
        if (stop_at == step)
            return safefile::Step::Die;
        if (fail_at == step)
            return safefile::Step::Fail;
        return safefile::Step::Go;
    };
}

void test_SafeFile::init()
{
    QVERIFY(m_tmp.isValid());
    for (const QString& name : names())
        QFile::remove(m_tmp.filePath(name));
    m_path = m_tmp.filePath(QStringLiteral("game_dirs.txt"));
    m_old = QByteArray("/storage/ROMs/snes\n/storage/ROMs/gba\n/storage/ROMs/psx\n");
    m_new = QByteArray("/storage/ROMs/snes\n/storage/ROMs/gba\n/storage/ROMs/psx\n/storage/ROMs/n64\n/storage/ROMs/nds\n");
    write_plain(m_path, m_old);
    safefile::test_hook = nullptr;
}

void test_SafeFile::cleanup()
{
    safefile::test_hook = nullptr;
    QFile(m_tmp.path()).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}

void test_SafeFile::saves()
{
    QVERIFY(safefile::write(m_path, m_new));
    QCOMPARE(read(m_path), m_new);
    QCOMPARE(names(), QStringList({ QStringLiteral("game_dirs.txt") }));  // nothing left beside it
}

void test_SafeFile::saves_a_new_file()
{
    const QString path = m_tmp.filePath(QStringLiteral("sub/folder/pegasusx-root.txt"));
    QVERIFY(safefile::write(path, "/storage/1234-5678/PegasusX\n", true));
    QCOMPARE(read(path), QByteArray("/storage/1234-5678/PegasusX\n"));
}

void test_SafeFile::text_mode_keeps_the_text()
{
    QVERIFY(safefile::write(m_path, m_new, true));
    QFile file(m_path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(file.readAll(), m_new);
}

void test_SafeFile::cut_before_the_swap_keeps_the_old_file()
{
    for (const char* step : { "usual-written", "usual-commit" }) {
        write_plain(m_path, m_old);
        cut_at(step);
        QVERIFY2(!safefile::write(m_path, m_new), step);
        QCOMPARE(read(m_path), m_old);  // the old file, whole
        QVERIFY(!QFileInfo::exists(safefile::fallback_old_path(m_path)));
    }
}

void test_SafeFile::refused_rename_uses_the_fallback()
{
    cut_at(nullptr, "usual-commit");  // the storage refuses the rename over the old file
    QVERIFY(safefile::write(m_path, m_new));
    QCOMPARE(read(m_path), m_new);
    QCOMPARE(names(), QStringList({ QStringLiteral("game_dirs.txt") }));  // .pxsave and .pxold gone
}

void test_SafeFile::fallback_saves_a_new_file()
{
    const QString path = m_tmp.filePath(QStringLiteral("favorites.txt"));
    cut_at(nullptr, "usual-commit");
    QVERIFY(safefile::write(path, "/roms/snes/Zelda.sfc\n", true));
    QCOMPARE(read(path), QByteArray("/roms/snes/Zelda.sfc\n"));
    QVERIFY(!QFileInfo::exists(safefile::fallback_new_path(path)));
}

// the fallback cut at each step: the file is the old one whole, or the new
// one whole, or (only between its two renames) not there with both whole
// copies beside it; never part of one, never empty
void test_SafeFile::fallback_cut_at_each_step()
{
    const QString pxsave = safefile::fallback_new_path(m_path);
    const QString pxold = safefile::fallback_old_path(m_path);

    // a. while the new text is written (half of it)
    init();
    cut_at("fallback-half", "usual-commit");
    QVERIFY(!safefile::write(m_path, m_new));
    QCOMPARE(read(m_path), m_old);
    QVERIFY(read(pxsave).size() < m_new.size());  // (the part written: beside it, not in it)

    // a. done: the new text whole beside it
    init();
    cut_at("fallback-written", "usual-commit");
    QVERIFY(!safefile::write(m_path, m_new));
    QCOMPARE(read(m_path), m_old);
    QCOMPARE(read(pxsave), m_new);

    // b. the old file moved aside: the name is free for an instant, both whole
    init();
    cut_at("fallback-old-moved", "usual-commit");
    QVERIFY(!safefile::write(m_path, m_new));
    QVERIFY(!QFileInfo::exists(m_path));
    QCOMPARE(read(pxold), m_old);
    QCOMPARE(read(pxsave), m_new);

    // c. the new text in place
    init();
    cut_at("fallback-new-moved", "usual-commit");
    QVERIFY(!safefile::write(m_path, m_new));
    QCOMPARE(read(m_path), m_new);
    QCOMPARE(read(pxold), m_old);
}

// a cut save leaves .pxsave / .pxold: the next save still works and tidies them
void test_SafeFile::fallback_after_a_cut_save_still_works()
{
    cut_at("fallback-new-moved", "usual-commit");
    QVERIFY(!safefile::write(m_path, m_new));
    cut_at(nullptr, "usual-commit");
    const QByteArray newer("/storage/ROMs/ps2\n");
    QVERIFY(safefile::write(m_path, newer));
    QCOMPARE(read(m_path), newer);
    QCOMPARE(names(), QStringList({ QStringLiteral("game_dirs.txt") }));
}

// a folder where the file can be written but no new file made (as some
// shared storage is): no temporary file, no fallback file, so no save; the
// old file isn't written into
void test_SafeFile::folder_refusing_new_files_keeps_the_old_file()
{
    QVERIFY(QFile(m_tmp.path()).setPermissions(QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    {
        QFile probe(m_tmp.filePath(QStringLiteral("probe")));
        if (probe.open(QIODevice::WriteOnly)) {
            probe.close();
            probe.remove();
            QSKIP("this user can write in a read-only folder (root): nothing to test");
        }
    }
    QVERIFY(QFileInfo(m_path).isWritable());  // the file itself could be written into
    QVERIFY(!safefile::write(m_path, m_new));
    QCOMPARE(read(m_path), m_old);
    QCOMPARE(names(), QStringList({ QStringLiteral("game_dirs.txt") }));
}


QTEST_MAIN(test_SafeFile)
#include "test_SafeFile.moc"
