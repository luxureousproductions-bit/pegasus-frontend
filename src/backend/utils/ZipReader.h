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
#include <QFile>
#include <QString>
#include <functional>
#include <vector>


// Pegasus X: reads zip files (stored and deflated entries), for themes
// downloaded from GitHub. Qt's own zip reader is private and left out of the
// Android Qt build; this needs nothing but Qt Core. The inflate code follows
// Mark Adler's puff.c (zlib license).
namespace utils {

class ZipReader {
public:
    struct Entry {
        QString path;          ///< as in the zip, '/' separated
        bool is_dir = false;
        bool is_symlink = false;
        quint16 method = 0;    ///< 0 stored, 8 deflated
        quint32 packed_size = 0;
        quint32 size = 0;
        quint32 header_offset = 0;
    };

    explicit ZipReader(const QString& path);
    ~ZipReader();

    bool isReadable() const { return m_ok; }
    const std::vector<Entry>& entries() const { return m_entries; }
    /// the entry's contents; ok is false if it couldn't be read
    QByteArray fileData(const Entry& entry, bool* ok = nullptr) const;
    /// the entry's contents in parts, to `sink` (which returns false to stop);
    /// for entries too big to hold at once
    bool extract(const Entry& entry, const std::function<bool(const char*, int)>& sink) const;

private:
    QFile m_file;
    const uchar* m_data = nullptr;
    qint64 m_size = 0;
    bool m_ok = false;
    std::vector<Entry> m_entries;
};

} // namespace utils
