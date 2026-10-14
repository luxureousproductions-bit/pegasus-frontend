// Pegasus Frontend
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

#include <QQuickImageProvider>


/// Renders a page of a PDF file, for showing game manuals in themes.
///
/// Usage from QML: `image://pdf/<page>/<percent-encoded absolute path>`,
/// with `page` starting from 0, eg.
/// `"image://pdf/" + page + "/" + encodeURIComponent(path)`.
/// The `sourceSize` of the Image, if set, is used as the maximum size.
class AndroidPdfProvider : public QQuickImageProvider {
public:
    AndroidPdfProvider();

    QImage requestImage(const QString&, QSize*, const QSize&) override;
};
