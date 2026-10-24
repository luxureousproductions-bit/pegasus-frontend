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

#include <QByteArray>
#include <QString>
#include <QVariantMap>


// Pegasus X: miximages made on the device, as ES-DE makes them: the
// screenshot in a frame, the logo top right, the 3D box (or cover) bottom
// left and the cartridge or disc beside it. The layout and its options follow
// ES-DE's MiximageGenerator (es-app/src/MiximageGenerator.cpp, MIT license).
namespace miximage {

struct Options {
    int multiplier = 2;                  ///< 640x480: 1, 1280x960: 2, 1920x1440: 3
    QString format = QStringLiteral("png");
    QString horizontal_fit = QStringLiteral("crop");   ///< contain / crop / stretch
    QString vertical_fit = QStringLiteral("contain");
    bool high_threshold = true;          ///< only fit screenshots far from 4:3
    bool frame_fill = false;             ///< blank areas: the frame's color, else black
    bool smooth = false;                 ///< screenshot scaling: smooth, else sharp pixels
    QString box_size = QStringLiteral("medium");
    QString physical_size = QStringLiteral("medium");
    bool remove_letterboxes = true;
    bool remove_pillarboxes = true;
    bool rotate_boxes = true;
    bool include_marquee = true;
    bool include_box = true;
    bool cover_fallback = true;
    bool include_physical = true;

    /// from the Scraper settings (the "mix..." keys)
    static Options from(const QVariantMap& settings);
};

/// The art one miximage is made from (local files; empty: none)
struct Sources {
    QString screenshot;
    QString marquee;
    QString box3d;
    QString cover;
    QString physical;
};

struct Result {
    QByteArray data;   ///< the encoded picture, empty on failure
    QString ext;       ///< "png" / "webp"
    QString error;
};

/// Makes the miximage (slow: run it off the main thread)
Result make(const Sources& sources, const Options& options);

} // namespace miximage
