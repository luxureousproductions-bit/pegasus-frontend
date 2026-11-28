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


#include "SolidArt.h"

#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QVariantMap>
#include <algorithm>
#include <cstdlib>


namespace solid_art {
namespace {
// a picture this many pixels a side or fewer: nothing to show
constexpr int EMPTY_SIDE = 2;

// the file isn't big for its pixels: maybe one colour (a solid colour
// compresses to almost nothing, even with a little noise: a PNG ~1.3 bytes a
// pixel at most, a JPEG ~0.1). Bigger ones (detailed art) aren't decoded.
bool worth_a_look(qint64 bytes, const QSize& size)
{
    if (!size.isValid())
        return true;
    const double pixels = static_cast<double>(size.width()) * size.height();
    return bytes < 64 * 1024 || static_cast<double>(bytes) < pixels * 1.5;
}

QString hex(QRgb c)
{
    return QStringLiteral("#%1%2%3")
        .arg(qRed(c), 2, 16, QLatin1Char('0')).arg(qGreen(c), 2, 16, QLatin1Char('0')).arg(qBlue(c), 2, 16, QLatin1Char('0')).toUpper();
}

// `whole`: the picture decoded as it is (when a small read gives nothing)
Verdict read(QImageReader& reader, qint64 bytes, const std::function<QImage()>& whole)
{
    Verdict out;
    const QSize size = reader.size();
    if (size.isValid() && (size.width() <= EMPTY_SIDE || size.height() <= EMPTY_SIDE)) {
        out.reject = true;
        out.checked = true;
        out.why = QStringLiteral("empty (%1x%2)").arg(size.width()).arg(size.height());
        out.share = 1.0;
        return out;
    }
    if (!worth_a_look(bytes, size))
        return out;
    // a JPEG is decoded small straight away (libjpeg scales while it
    // decodes); the others whole, then made small (some builds' PNG reader
    // gives nothing for a small read)
    const QByteArray format = reader.format().toLower();
    const bool jpeg = format == "jpeg" || format == "jpg";
    if (jpeg && size.isValid() && (size.width() > SAMPLE || size.height() > SAMPLE))
        reader.setScaledSize(size.scaled(SAMPLE, SAMPLE, Qt::KeepAspectRatio).expandedTo(QSize(1, 1)));
    QImage image = reader.read();
    if (image.isNull() && whole)
        image = whole();
    if (image.isNull())
        return out;
    return check_image(image);
}
} // namespace

Verdict check_image(const QImage& source)
{
    Verdict out;
    out.checked = true;
    if (source.isNull())
        return out;
    if (source.width() <= EMPTY_SIDE || source.height() <= EMPTY_SIDE) {
        out.reject = true;
        out.why = QStringLiteral("empty (%1x%2)").arg(source.width()).arg(source.height());
        out.share = 1.0;
        return out;
    }
    QImage img = source;
    if (img.width() > SAMPLE || img.height() > SAMPLE)
        img = img.scaled(SAMPLE, SAMPLE, Qt::KeepAspectRatio, Qt::FastTransformation);
    img = img.convertToFormat(QImage::Format_ARGB32);
    const int n = img.width() * img.height();
    // the colour most of it is: the fullest of 16 levels a channel, its mean
    // (every fully transparent pixel is one colour, whatever its RGB)
    QHash<quint32, int> bins;
    const auto bin_of = [](QRgb p) -> quint32 {
        if (qAlpha(p) < 16)
            return 0xFFFFFFFFu;
        return (quint32(qRed(p) >> 4) << 12) | (quint32(qGreen(p) >> 4) << 8) | (quint32(qBlue(p) >> 4) << 4) | quint32(qAlpha(p) >> 4);
    };
    for (int y = 0; y < img.height(); y++) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); x++)
            bins[bin_of(line[x])]++;
    }
    quint32 top = 0;
    int top_n = -1;
    for (auto it = bins.cbegin(); it != bins.cend(); ++it) {
        if (it.value() > top_n) {
            top_n = it.value();
            top = it.key();
        }
    }
    const bool transparent = top == 0xFFFFFFFFu;
    qint64 r = 0, g = 0, b = 0, a = 0;
    int in_bin = 0;
    for (int y = 0; y < img.height(); y++) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); x++) {
            if (bin_of(line[x]) != top)
                continue;
            r += qRed(line[x]); g += qGreen(line[x]); b += qBlue(line[x]); a += qAlpha(line[x]);
            in_bin++;
        }
    }
    const QRgb centre = qRgba(int(r / in_bin), int(g / in_bin), int(b / in_bin), int(a / in_bin));
    // the pixels close to it
    int close = 0;
    for (int y = 0; y < img.height(); y++) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); x++) {
            const QRgb p = line[x];
            if (transparent) {
                close += qAlpha(p) < 16 ? 1 : 0;
                continue;
            }
            close += std::abs(qRed(p) - qRed(centre)) <= TOLERANCE && std::abs(qGreen(p) - qGreen(centre)) <= TOLERANCE
                  && std::abs(qBlue(p) - qBlue(centre)) <= TOLERANCE && std::abs(qAlpha(p) - qAlpha(centre)) <= TOLERANCE ? 1 : 0;
        }
    }
    out.share = static_cast<double>(close) / n;
    if (out.share >= SHARE) {
        out.reject = true;
        out.colour = transparent ? QStringLiteral("transparent") : hex(centre);
        out.why = transparent ? QStringLiteral("fully transparent") : QStringLiteral("solid colour %1").arg(out.colour);
    }
    return out;
}

Verdict check(const QByteArray& data)
{
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    return read(reader, data.size(), [&data]{ return QImage::fromData(data); });
}

Verdict check_file(const QString& path)
{
    QImageReader reader(path);
    return read(reader, QFileInfo(path).size(), [&path]{ return QImage(path); });
}

bool inside(const QString& art_dir, const QString& path)
{
    const QString root = QFileInfo(art_dir).canonicalFilePath();
    const QFileInfo info(path);
    if (root.isEmpty() || !info.exists() || !info.isFile() || info.isSymLink())
        return false;
    const QString real = info.canonicalFilePath();
    if (!real.startsWith(root + QLatin1Char('/')))
        return false;
    static const QStringList pictures { QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                        QStringLiteral("webp"), QStringLiteral("gif"), QStringLiteral("bmp") };
    return pictures.contains(info.suffix().toLower());
}

QVariantList scan(const QString& art_dir, const std::function<void(int, int)>& progress)
{
    const QString root = QFileInfo(art_dir).canonicalFilePath();
    QVariantList out;
    if (root.isEmpty())
        return out;
    // the picture files first (for the count), then each looked at
    QStringList files;
    QDirIterator it(root, { QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
                            QStringLiteral("*.webp"), QStringLiteral("*.gif"), QStringLiteral("*.bmp"),
                            QStringLiteral("*.PNG"), QStringLiteral("*.JPG"), QStringLiteral("*.JPEG") },
                    QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QStringList parts = path.mid(root.length() + 1).split(QLatin1Char('/'));
        // (<system>/videos, <system>/manuals: not pictures)
        if (parts.size() >= 2 && (parts.at(1) == QLatin1String("videos") || parts.at(1) == QLatin1String("manuals")))
            continue;
        files.append(path);
    }
    for (int i = 0; i < files.size(); i++) {
        const Verdict v = check_file(files.at(i));
        if (v.reject) {
            out.append(QVariantMap {
                { QStringLiteral("path"), files.at(i) },
                { QStringLiteral("rel"), files.at(i).mid(root.length() + 1) },
                { QStringLiteral("why"), v.why },
                { QStringLiteral("colour"), v.colour },
                { QStringLiteral("share"), v.share },
                { QStringLiteral("bytes"), QFileInfo(files.at(i)).size() },
            });
        }
        if (progress && (i % 50 == 0 || i + 1 == files.size()))
            progress(i + 1, files.size());
    }
    std::sort(out.begin(), out.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value(QStringLiteral("rel")).toString() < b.toMap().value(QStringLiteral("rel")).toString();
    });
    return out;
}

} // namespace solid_art
