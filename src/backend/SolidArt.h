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


#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <functional>


// Pegasus X: placeholder art (a picture that's one solid colour, fully
// transparent, or a pixel or two): never saved by the scraper (it tries the
// next source), and found among the art scraped before (Scraper > Other
// settings > Find solid-colour art). ScreenScraper has, eg., a pure green
// PC box back for Grand Theft Auto III.
namespace solid_art {

/// A picture's verdict
struct Verdict {
    bool reject = false;
    bool checked = false;    ///< looked at (false: passed the quick size test)
    QString why;             ///< "solid colour #00FF00", "fully transparent", "empty (1x1)"
    QString colour;          ///< "#RRGGBB", "transparent" ("" when not rejected)
    double share = 0.0;      ///< of the picture in that colour (0..1)
};

/// how close to one colour a picture must be to be rejected: this share of
/// its pixels (at most SAMPLE px a side) within TOLERANCE of it, per channel
constexpr double SHARE = 0.995;
constexpr int TOLERANCE = 20;
constexpr int SAMPLE = 96;

/// a decoded picture
Verdict check_image(const QImage& image);
/// a picture's file data. Quick first: a picture whose file is big for its
/// size in pixels (real art) isn't decoded; the others are, at SAMPLE px a
/// side at most. Not a picture: not rejected (the caller checks that).
Verdict check(const QByteArray& data);
/// a file
Verdict check_file(const QString& path);

/// the picture files under `art_dir` (Pegasus X's downloaded_media; videos
/// and manuals left out), each checked: the rejected ones,
/// [{ path, rel, why, colour, share, bytes }], by `rel`. `progress` (now and
/// then): (files looked at, files in all).
QVariantList scan(const QString& art_dir, const std::function<void(int, int)>& progress = {});
/// `path` is a picture file inside `art_dir` (no link, no ".." out of it)
bool inside(const QString& art_dir, const QString& path);

} // namespace solid_art
