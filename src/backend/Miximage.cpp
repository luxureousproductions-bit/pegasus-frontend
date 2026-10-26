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


#include "Miximage.h"

#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <QtMath>
#include <vector>


namespace {

QImage load(const QString& path)
{
    if (path.isEmpty())
        return {};
    QImage image(path);
    if (image.isNull())
        return {};
    return image.convertToFormat(QImage::Format_ARGB32);
}

// rows / columns of only black (screenshots) or only transparent pixels
bool row_black(const QImage& img, int y)
{
    const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
    for (int x = 0; x < img.width(); x++) {
        if ((line[x] & 0x00FFFFFF) != 0)
            return false;
    }
    return true;
}
bool column_black(const QImage& img, int x)
{
    for (int y = 0; y < img.height(); y++) {
        if ((img.pixel(x, y) & 0x00FFFFFF) != 0)
            return false;
    }
    return true;
}
bool row_clear(const QImage& img, int y)
{
    const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
    for (int x = 0; x < img.width(); x++) {
        if (qAlpha(line[x]) != 0)
            return false;
    }
    return true;
}
bool column_clear(const QImage& img, int x)
{
    for (int y = 0; y < img.height(); y++) {
        if (qAlpha(img.pixel(x, y)) != 0)
            return false;
    }
    return true;
}

QImage crop_letterboxes(const QImage& img)
{
    int top = 0;
    int bottom = img.height() - 1;
    while (top < bottom && row_black(img, top)) top++;
    while (bottom > top && row_black(img, bottom)) bottom--;
    return (top == 0 && bottom == img.height() - 1) ? img : img.copy(0, top, img.width(), bottom - top + 1);
}
QImage crop_pillarboxes(const QImage& img)
{
    int left = 0;
    int right = img.width() - 1;
    while (left < right && column_black(img, left)) left++;
    while (right > left && column_black(img, right)) right--;
    return (left == 0 && right == img.width() - 1) ? img : img.copy(left, 0, right - left + 1, img.height());
}
QImage remove_transparent_padding(const QImage& img)
{
    if (!img.hasAlphaChannel() || img.isNull())
        return img;
    int top = 0, bottom = img.height() - 1, left = 0, right = img.width() - 1;
    while (top < bottom && row_clear(img, top)) top++;
    while (bottom > top && row_clear(img, bottom)) bottom--;
    while (left < right && column_clear(img, left)) left++;
    while (right > left && column_clear(img, right)) right--;
    return img.copy(left, top, right - left + 1, bottom - top + 1);
}

// box blur of one channel, horizontally then vertically
void blur_alpha(std::vector<float>& a, int w, int h, int radius, int iterations)
{
    if (radius < 1)
        return;
    std::vector<float> tmp(a.size());
    const float norm = 1.f / static_cast<float>(radius * 2 + 1);
    for (int it = 0; it < iterations; it++) {
        for (int y = 0; y < h; y++) {
            float sum = 0.f;
            for (int x = -radius; x <= radius; x++)
                sum += a[static_cast<size_t>(y * w + qBound(0, x, w - 1))];
            for (int x = 0; x < w; x++) {
                tmp[static_cast<size_t>(y * w + x)] = sum * norm;
                sum += a[static_cast<size_t>(y * w + qMin(w - 1, x + radius + 1))]
                     - a[static_cast<size_t>(y * w + qMax(0, x - radius))];
            }
        }
        for (int x = 0; x < w; x++) {
            float sum = 0.f;
            for (int y = -radius; y <= radius; y++)
                sum += tmp[static_cast<size_t>(qBound(0, y, h - 1) * w + x)];
            for (int y = 0; y < h; y++) {
                a[static_cast<size_t>(y * w + x)] = sum * norm;
                sum += tmp[static_cast<size_t>(qMin(h - 1, y + radius + 1) * w + x)]
                     - tmp[static_cast<size_t>(qMax(0, y - radius) * w + x)];
            }
        }
    }
}

// a soft shadow down and right of the picture (ES-DE: distance, 4 blur passes)
QImage add_drop_shadow(const QImage& img, int distance)
{
    const int w = img.width() + distance * 3;
    const int h = img.height() + distance * 3;
    std::vector<float> alpha(static_cast<size_t>(w * h), 0.f);
    for (int y = 0; y < img.height(); y++) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); x++)
            alpha[static_cast<size_t>((y + distance) * w + x + distance)] = static_cast<float>(qAlpha(line[x]));
    }
    blur_alpha(alpha, w, h, distance, 4);
    QImage out(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; y++) {
        QRgb* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < w; x++)
            line[x] = qRgba(0, 0, 0, qBound(0, static_cast<int>(alpha[static_cast<size_t>(y * w + x)] * 0.75f), 255));
    }
    QPainter p(&out);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.drawImage(0, 0, img);
    p.end();
    return remove_transparent_padding(out);
}

// the frame's color: the screenshot's average, a bit less saturated and lighter
QColor frame_color(const QImage& shot, int canvas_width)
{
    const int samples = qMax(1, static_cast<int>(static_cast<float>(canvas_width) * 0.03125f));
    qint64 r = 0, g = 0, b = 0, n = 0;
    for (int y = samples / 2; y < shot.height(); y += samples) {
        for (int x = samples / 2; x < shot.width(); x += samples) {
            const QRgb px = shot.pixel(x, y);
            r += qRed(px); g += qGreen(px); b += qBlue(px); n++;
        }
    }
    if (n == 0)
        return QColor(0, 0, 0);
    QColor c(static_cast<int>(r / n), static_cast<int>(g / n), static_cast<int>(b / n));
    qreal h, s, l;
    c.getHslF(&h, &s, &l);
    return QColor::fromHslF(qMax<qreal>(0., h), qBound<qreal>(0., s * 0.9, 1.), qBound<qreal>(0.10, l * 1.25, 1.));
}

// a logo's size: wide, short logos get more width than tall ones
QSize marquee_size(int target_w, int target_h, int w, int h)
{
    const float ratio = static_cast<float>(w) / static_cast<float>(h);
    float modifier = qBound(0.f, 0.5f + ratio / 6.5f, 1.f);
    if (ratio >= 4)
        modifier += qBound(0.f, ratio / 40.f, 0.3f);
    const float adjusted = static_cast<float>(target_w) * modifier;
    float scale = adjusted / static_cast<float>(w);
    if (scale * static_cast<float>(h) > static_cast<float>(target_h))
        scale = static_cast<float>(target_h) / static_cast<float>(h);
    return QSize(qMax(1, static_cast<int>(static_cast<float>(w) * scale)),
                 qMax(1, static_cast<int>(static_cast<float>(h) * scale)));
}

} // namespace


namespace miximage {

Options Options::from(const QVariantMap& s)
{
    Options o;
    const QString res = s.value(QStringLiteral("mixResolution")).toString();
    o.multiplier = res == QLatin1String("640x480") ? 1 : res == QLatin1String("1920x1440") ? 3 : 2;
    o.format = s.value(QStringLiteral("mixFormat"), QStringLiteral("png")).toString();
    o.horizontal_fit = s.value(QStringLiteral("mixHorizontalFit"), QStringLiteral("crop")).toString();
    o.vertical_fit = s.value(QStringLiteral("mixVerticalFit"), QStringLiteral("contain")).toString();
    o.high_threshold = s.value(QStringLiteral("mixThreshold")).toString() != QLatin1String("low");
    o.frame_fill = s.value(QStringLiteral("mixBlankColor")).toString() == QLatin1String("frame");
    o.smooth = s.value(QStringLiteral("mixScaling")).toString() == QLatin1String("smooth");
    o.box_size = s.value(QStringLiteral("mixBoxSize"), QStringLiteral("medium")).toString();
    o.physical_size = s.value(QStringLiteral("mixPhysicalSize"), QStringLiteral("medium")).toString();
    o.remove_letterboxes = s.value(QStringLiteral("mixRemoveLetterboxes"), true).toBool();
    o.remove_pillarboxes = s.value(QStringLiteral("mixRemovePillarboxes"), true).toBool();
    o.rotate_boxes = s.value(QStringLiteral("mixRotateBoxes"), true).toBool();
    o.include_marquee = s.value(QStringLiteral("mixIncludeMarquee"), true).toBool();
    o.include_box = s.value(QStringLiteral("mixIncludeBox"), true).toBool();
    o.cover_fallback = s.value(QStringLiteral("mixCoverFallback"), true).toBool();
    o.include_physical = s.value(QStringLiteral("mixIncludePhysical"), true).toBool();
    return o;
}

Result make(const Sources& sources, const Options& o)
{
    Result result;
    QImage shot = load(sources.screenshot);
    if (shot.isNull()) {
        result.error = QStringLiteral("no screenshot for the miximage");
        return result;
    }
    const int m = o.multiplier;
    const int width = 640 * m;
    const int height = 480 * m;
    const int ss_w = 530 * m;
    const int ss_h = 400 * m;
    const int ss_offset = 20 * m;
    const int frame = 6 * m;
    const int shadow = 6 * m;
    const Qt::TransformationMode shot_mode = o.smooth ? Qt::SmoothTransformation : Qt::FastTransformation;

    // -- the screenshot -----------------------------------------------------------
    // (a mostly black screenshot would crop down to a sliver: then it stays whole)
    const QImage uncropped = shot;
    if (o.remove_letterboxes)
        shot = crop_letterboxes(shot);
    if (o.remove_pillarboxes)
        shot = crop_pillarboxes(shot);
    if (shot.width() < uncropped.width() / 4 || shot.height() < uncropped.height() / 4)
        shot = uncropped;
    float aspect = static_cast<float>(shot.width()) / static_cast<float>(qMax(1, shot.height()));
    if (aspect > 5.f || aspect < 0.2f) {
        Result bad;
        bad.error = QStringLiteral("the screenshot's shape is unusable (%1 x %2)").arg(shot.width()).arg(shot.height());
        return bad;
    }
    const QString fit = aspect >= 1.f ? o.horizontal_fit : o.vertical_fit;
    // only screenshots far from the miximage's 1.325 are fitted or cropped
    const float mix_aspect = 1.325f;
    const float max_aspect = o.high_threshold ? 1.6f : 1.375f;
    const float min_aspect = o.high_threshold ? 1.05f : 1.275f;
    const bool off_ratio = aspect > max_aspect || aspect < min_aspect;
    const QColor frame_col = frame_color(shot, width);

    QImage screen;
    if (fit == QLatin1String("crop") && off_ratio) {
        if (aspect > mix_aspect) {
            const QImage scaled = shot.scaled(qRound(static_cast<float>(ss_h) * aspect), ss_h, Qt::IgnoreAspectRatio, shot_mode);
            screen = scaled.copy((scaled.width() - ss_w) / 2, 0, ss_w, ss_h);
        }
        else {
            const QImage scaled = shot.scaled(ss_w, qRound(static_cast<float>(ss_w) / aspect), Qt::IgnoreAspectRatio, shot_mode);
            screen = scaled.copy(0, (scaled.height() - ss_h) / 2, ss_w, ss_h);
        }
    }
    else if (fit == QLatin1String("contain") && off_ratio) {
        screen = QImage(ss_w, ss_h, QImage::Format_ARGB32);
        screen.fill(o.frame_fill ? frame_col : QColor(0, 0, 0));
        const QImage scaled = shot.scaled(ss_w, ss_h, Qt::KeepAspectRatio, shot_mode);
        QPainter p(&screen);
        p.drawImage((ss_w - scaled.width()) / 2, (ss_h - scaled.height()) / 2, scaled);
    }
    else {
        screen = shot.scaled(ss_w, ss_h, Qt::IgnoreAspectRatio, shot_mode);
    }
    // no transparency in the screenshot
    screen = screen.convertToFormat(QImage::Format_RGB32).convertToFormat(QImage::Format_ARGB32);

    // -- the logo, the box, the disc ------------------------------------------------
    QImage marquee;
    if (o.include_marquee) {
        marquee = remove_transparent_padding(load(sources.marquee));
        if (!marquee.isNull()) {
            const QSize size = marquee_size(310 * m, 230 * m, marquee.width(), marquee.height());
            marquee = add_drop_shadow(marquee.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation), shadow);
        }
    }

    QImage box;
    if (o.include_box) {
        bool is3d = true;
        box = load(sources.box3d);
        if (box.isNull() && o.cover_fallback) {
            box = load(sources.cover);
            is3d = false;
        }
        box = remove_transparent_padding(box);
        if (!box.isNull()) {
            const int box_w = o.box_size == QLatin1String("small") ? 264 : o.box_size == QLatin1String("large") ? 372 : 310;
            const int box_h = o.box_size == QLatin1String("small") ? 254 : o.box_size == QLatin1String("large") ? 360 : 300;
            const int cover_w = o.box_size == QLatin1String("small") ? 212 : o.box_size == QLatin1String("large") ? 300 : 250;
            if (o.rotate_boxes && static_cast<float>(box.width()) / static_cast<float>(box.height()) > 1.14f)
                box = box.transformed(QTransform().rotate(90), Qt::SmoothTransformation);
            const int target_h = box_h * m;
            const int target_w = (is3d ? box_w : cover_w) * m;
            const float scale = static_cast<float>(target_h) / static_cast<float>(box.height());
            const int w = static_cast<int>(static_cast<float>(box.width()) * scale);
            if (w > target_w)
                box = box.scaledToWidth(target_w, Qt::SmoothTransformation);
            else
                box = box.scaled(qMax(1, w), target_h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            box = add_drop_shadow(box, shadow);
        }
    }

    QImage physical;
    if (o.include_physical) {
        physical = remove_transparent_padding(load(sources.physical));
        if (!physical.isNull()) {
            const int pw = (o.physical_size == QLatin1String("small") ? 120 : o.physical_size == QLatin1String("large") ? 196 : 150) * m;
            const int ph = (o.physical_size == QLatin1String("small") ? 96 : o.physical_size == QLatin1String("large") ? 156 : 120) * m;
            physical = add_drop_shadow(physical.scaled(pw, ph, Qt::KeepAspectRatio, Qt::SmoothTransformation), shadow);
        }
    }

    // -- together -------------------------------------------------------------------
    QImage canvas(width, height, QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);
    QPainter p(&canvas);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const int x_shot = width / 2 - ss_w / 2 + ss_offset;
    const int y_shot = height / 2 - ss_h / 2;
    // the frame, with rounded corners
    QPainterPath path;
    path.addRoundedRect(QRectF(x_shot - frame, y_shot - frame, ss_w + frame * 2, ss_h + frame * 2), 8 * m, 8 * m);
    p.fillPath(path, frame_col);
    p.drawImage(x_shot, y_shot, screen);
    if (!marquee.isNull())
        p.drawImage(width - marquee.width(), 0, marquee);
    if (!box.isNull())
        p.drawImage(0, height - box.height(), box);
    if (!physical.isNull())
        p.drawImage((box.isNull() ? 0 : box.width()) + 16 * m, height - physical.height(), physical);
    p.end();

    // -- saved -----------------------------------------------------------------------
    QString ext = o.format == QLatin1String("webp") && QImageWriter::supportedImageFormats().contains("webp")
        ? QStringLiteral("webp") : QStringLiteral("png");
    QBuffer buffer(&result.data);
    buffer.open(QIODevice::WriteOnly);
    if (!canvas.save(&buffer, ext.toLatin1().constData(), ext == QLatin1String("webp") ? 85 : -1)) {
        result.data.clear();
        result.error = QStringLiteral("couldn't save the miximage");
        return result;
    }
    result.ext = ext;
    return result;
}

} // namespace miximage
