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

#include "SolidArt.h"

#include <QBuffer>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QPainter>
#include <QTemporaryDir>
#include <random>


// Pegasus X: placeholder art (solid_art): rejected, and real art (a cover, a
// dark title screen, a logo on transparency, a 16 px icon) kept
namespace {
QByteArray encode(const QImage& image, const char* format, int quality = 90)
{
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, format, quality);
    return out;
}
QImage solid(int w, int h, QColor colour)
{
    QImage image(w, h, QImage::Format_RGB32);
    image.fill(colour);
    return image;
}
// one colour, every channel of every pixel up to `amp` off
QImage noisy(int w, int h, QColor colour, int amp)
{
    std::mt19937 random(42);
    std::uniform_int_distribution<int> off(-amp, amp);
    QImage image(w, h, QImage::Format_RGB32);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            image.setPixel(x, y, qRgb(qBound(0, colour.red() + off(random), 255), qBound(0, colour.green() + off(random), 255),
                                      qBound(0, colour.blue() + off(random), 255)));
        }
    }
    return image;
}
// something like a cover: a gradient, shapes, a title
QImage cover()
{
    QImage image(400, 560, QImage::Format_RGB32);
    QPainter p(&image);
    QLinearGradient sky(0, 0, 0, 560);
    sky.setColorAt(0, QColor(20, 40, 120));
    sky.setColorAt(1, QColor(230, 120, 40));
    p.fillRect(image.rect(), sky);
    p.setBrush(QColor(250, 220, 60));
    p.drawEllipse(QPoint(280, 180), 70, 70);
    p.setBrush(QColor(30, 90, 30));
    p.drawRect(0, 420, 400, 140);
    p.setPen(Qt::white);
    QFont font;
    font.setPixelSize(40);
    p.setFont(font);
    p.drawText(QRect(0, 20, 400, 60), Qt::AlignCenter, QStringLiteral("SUPER GAME"));
    return image;
}
QColor colour_of(const QString& hex) { return QColor(hex); }
int distance(const QColor& a, const QColor& b)
{
    return qMax(qAbs(a.red() - b.red()), qMax(qAbs(a.green() - b.green()), qAbs(a.blue() - b.blue())));
}
void write(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}
} // namespace


class test_SolidArt : public QObject {
    Q_OBJECT

private slots:
    void solid_green();
    void solid_black();
    void near_solid_with_noise();
    void real_art_kept();
    void tiny_real_icon_kept();
    void empty_and_transparent();
    void not_a_picture();
    void detailed_art_not_decoded();
    void quick();
    void scan_the_art_folder();
    void only_inside_the_art_folder();
};

// the case that started it: ScreenScraper's pure green PC box back (a ~2.8 KB PNG)
void test_SolidArt::solid_green()
{
    const QByteArray png = encode(solid(640, 900, QColor(0, 255, 0)), "PNG");
    qInfo() << "solid green PNG:" << png.size() << "bytes";
    const solid_art::Verdict v = solid_art::check(png);
    QVERIFY(v.reject);
    QCOMPARE(v.colour, QStringLiteral("#00FF00"));
    QCOMPARE(v.why, QStringLiteral("solid colour #00FF00"));
    QVERIFY(v.share >= 0.999);
}

void test_SolidArt::solid_black()
{
    const QByteArray jpg = encode(solid(1280, 720, Qt::black), "JPG");
    const solid_art::Verdict v = solid_art::check(jpg);
    QVERIFY(v.reject);
    QVERIFY2(distance(colour_of(v.colour), Qt::black) <= 3, qPrintable(v.colour));
}

void test_SolidArt::near_solid_with_noise()
{
    for (int amp : { 4, 10 }) {
        const QImage image = noisy(640, 480, QColor(0, 200, 0), amp);
        const QByteArray png = encode(image, "PNG");
        const QByteArray jpg = encode(image, "JPG", 85);
        qInfo() << "noise" << amp << ": PNG" << png.size() << "JPEG" << jpg.size();
        const solid_art::Verdict a = solid_art::check(png);
        QVERIFY2(a.reject, qPrintable(QString::number(a.share)));
        QVERIFY(distance(colour_of(a.colour), QColor(0, 200, 0)) <= amp);
        QVERIFY(solid_art::check(jpg).reject);
    }
}

// real art: a cover, a dark title screen with only a logo and a line of
// text, fan art that's mostly sky, a logo on transparency
void test_SolidArt::real_art_kept()
{
    const solid_art::Verdict c = solid_art::check(encode(cover(), "JPG"));
    QVERIFY(!c.reject);

    QImage title = solid(640, 480, Qt::black);
    {
        QPainter p(&title);
        p.fillRect(220, 160, 200, 60, QColor(200, 30, 30));
        p.setPen(Qt::white);
        QFont font;
        font.setPixelSize(14);
        p.setFont(font);
        p.drawText(QRect(0, 400, 640, 20), Qt::AlignCenter, QStringLiteral("PRESS START   (C) 1991"));
    }
    const solid_art::Verdict t = solid_art::check(encode(title, "PNG"));
    QVERIFY2(!t.reject, qPrintable(QString::number(t.share)));

    // a blue sky, and a strip of land at the bottom (~4%)
    QImage sky = solid(1280, 720, QColor(90, 150, 230));
    {
        QPainter p(&sky);
        p.fillRect(0, 690, 1280, 30, QColor(40, 120, 40));
    }
    QVERIFY(!solid_art::check(encode(sky, "JPG")).reject);

    QImage logo(600, 200, QImage::Format_ARGB32);
    logo.fill(Qt::transparent);
    {
        // (shapes, not text: the test machine may have no fonts)
        QPainter p(&logo);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundedRect(40, 50, 160, 100, 20, 20);
        p.drawEllipse(240, 40, 120, 120);
        p.drawRect(400, 60, 160, 80);
    }
    const solid_art::Verdict l = solid_art::check(encode(logo, "PNG"));
    QVERIFY2(!l.reject, qPrintable(QString::number(l.share)));
}

void test_SolidArt::tiny_real_icon_kept()
{
    QImage icon(16, 16, QImage::Format_ARGB32);
    icon.fill(Qt::transparent);
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if ((x - 8) * (x - 8) + (y - 8) * (y - 8) < 40)
                icon.setPixel(x, y, x < 8 ? qRgb(220, 40, 40) : qRgb(250, 250, 250));
        }
    }
    const QByteArray png = encode(icon, "PNG");
    qInfo() << "16 px icon:" << png.size() << "bytes";
    const solid_art::Verdict v = solid_art::check(png);
    QVERIFY(v.checked);   // (small: looked at)
    QVERIFY(!v.reject);
    // ...and a 4 px one, two colours
    QImage four(4, 4, QImage::Format_RGB32);
    four.fill(Qt::blue);
    four.setPixel(1, 1, qRgb(255, 255, 0));
    four.setPixel(2, 2, qRgb(255, 255, 0));
    QVERIFY(!solid_art::check(encode(four, "PNG")).reject);
}

void test_SolidArt::empty_and_transparent()
{
    const solid_art::Verdict one = solid_art::check(encode(solid(1, 1, Qt::red), "PNG"));
    QVERIFY(one.reject);
    QCOMPARE(one.why, QStringLiteral("empty (1x1)"));
    QVERIFY(solid_art::check(encode(solid(300, 2, Qt::red), "PNG")).reject);

    QImage clear(300, 100, QImage::Format_ARGB32);
    clear.fill(Qt::transparent);
    const solid_art::Verdict t = solid_art::check(encode(clear, "PNG"));
    QVERIFY(t.reject);
    QCOMPARE(t.colour, QStringLiteral("transparent"));
    QCOMPARE(t.why, QStringLiteral("fully transparent"));
}

void test_SolidArt::not_a_picture()
{
    QVERIFY(!solid_art::check("<html><body>Error</body></html>").reject);
    QVERIFY(!solid_art::check(QByteArray()).reject);
}

// a big detailed file (noise in every channel): passed on its size alone, not decoded
void test_SolidArt::detailed_art_not_decoded()
{
    std::mt19937 random(7);
    QImage image(512, 512, QImage::Format_RGB32);
    for (int y = 0; y < 512; y++)
        for (int x = 0; x < 512; x++)
            image.setPixel(x, y, random() & 0xFFFFFF);
    const QByteArray png = encode(image, "PNG");
    const solid_art::Verdict v = solid_art::check(png);
    QVERIFY(!v.reject);
    QVERIFY(!v.checked);
}

// cheap enough not to be noticed: a big solid PNG and a cover, 20 times each
void test_SolidArt::quick()
{
    const QByteArray big = encode(solid(2560, 1440, QColor(0, 255, 0)), "PNG");
    const QByteArray art = encode(cover(), "JPG");
    QElapsedTimer t;
    t.start();
    for (int i = 0; i < 20; i++) {
        QVERIFY(solid_art::check(big).reject);
        QVERIFY(!solid_art::check(art).reject);
    }
    qInfo() << "40 checks:" << t.elapsed() << "ms";
    QVERIFY(t.elapsed() < 20000);
}

void test_SolidArt::scan_the_art_folder()
{
    QTemporaryDir dir;
    const QString art = dir.filePath(QStringLiteral("downloaded_media"));
    write(art + QStringLiteral("/windows/backcovers/Grand Theft Auto III.png"), encode(solid(640, 900, QColor(0, 255, 0)), "PNG"));
    write(art + QStringLiteral("/windows/covers/Grand Theft Auto III.jpg"), encode(cover(), "JPG"));
    write(art + QStringLiteral("/snes/fanart/Black.jpg"), encode(solid(1280, 720, Qt::black), "JPG"));
    write(art + QStringLiteral("/snes/videos/Solid.png"), encode(solid(64, 64, Qt::red), "PNG"));      // (not art)
    write(art + QStringLiteral("/snes/covers/notes.txt"), "solid");                                     // (not a picture)
    int last_done = 0, last_total = 0;
    const QVariantList found = solid_art::scan(art, [&](int done, int total) { last_done = done; last_total = total; });
    QCOMPARE(found.size(), 2);
    QCOMPARE(found.at(0).toMap().value(QStringLiteral("rel")).toString(), QStringLiteral("snes/fanart/Black.jpg"));
    QCOMPARE(found.at(1).toMap().value(QStringLiteral("rel")).toString(), QStringLiteral("windows/backcovers/Grand Theft Auto III.png"));
    QCOMPARE(found.at(1).toMap().value(QStringLiteral("colour")).toString(), QStringLiteral("#00FF00"));
    QVERIFY(found.at(1).toMap().value(QStringLiteral("bytes")).toLongLong() > 0);
    QCOMPARE(last_total, 3);
    QCOMPARE(last_done, 3);
    // nothing deleted by looking
    QVERIFY(QFileInfo::exists(art + QStringLiteral("/windows/backcovers/Grand Theft Auto III.png")));
    QVERIFY(solid_art::scan(dir.filePath(QStringLiteral("nope"))).isEmpty());
}

void test_SolidArt::only_inside_the_art_folder()
{
    QTemporaryDir dir;
    const QString art = dir.filePath(QStringLiteral("downloaded_media"));
    const QString in = art + QStringLiteral("/snes/covers/a.png");
    const QString out = dir.filePath(QStringLiteral("elsewhere/a.png"));
    const QString text = art + QStringLiteral("/snes/covers/a.txt");
    write(in, encode(solid(64, 64, Qt::red), "PNG"));
    write(out, encode(solid(64, 64, Qt::red), "PNG"));
    write(text, "x");
    QVERIFY(solid_art::inside(art, in));
    QVERIFY(!solid_art::inside(art, out));
    QVERIFY(!solid_art::inside(art, art + QStringLiteral("/snes/covers/../../../elsewhere/a.png")));
    QVERIFY(!solid_art::inside(art, text));
    QVERIFY(!solid_art::inside(art, art + QStringLiteral("/snes/covers/missing.png")));
    QVERIFY(!solid_art::inside(dir.filePath(QStringLiteral("nope")), in));
#ifndef Q_OS_WIN
    const QString link = art + QStringLiteral("/snes/covers/link.png");
    QVERIFY(QFile::link(out, link));
    QVERIFY(!solid_art::inside(art, link));
#endif
}


QTEST_MAIN(test_SolidArt)
#include "test_SolidArt.moc"
