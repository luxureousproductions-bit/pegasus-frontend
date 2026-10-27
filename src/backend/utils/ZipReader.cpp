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


#include "ZipReader.h"

#include <algorithm>
#include <functional>

#include <cstring>


namespace {

quint16 u16(const uchar* p) { return static_cast<quint16>(p[0] | (p[1] << 8)); }
quint32 u32(const uchar* p) { return static_cast<quint32>(p[0]) | (static_cast<quint32>(p[1]) << 8)
                                   | (static_cast<quint32>(p[2]) << 16) | (static_cast<quint32>(p[3]) << 24); }

// -- inflate (RFC 1951), after puff.c ----------------------------------------------

constexpr int MAXBITS = 15;

struct Inflater {
    const uchar* in;
    size_t in_len;
    size_t in_pos = 0;
    quint32 bitbuf = 0;
    int bitcnt = 0;
    bool error = false;
    QByteArray out;
    // streaming: what's inflated goes to the sink in parts, the last 32 KiB
    // kept for the back-references (so a 1 GB entry needs no 1 GB of memory)
    std::function<bool(const char*, int)> sink;
    static constexpr int WINDOW = 32768;
    static constexpr int FLUSH_AT = 4 * 1024 * 1024;
    bool flush(bool final)
    {
        if (!sink)
            return true;
        const int keep = final ? 0 : WINDOW;
        if (out.size() <= keep || (!final && out.size() < FLUSH_AT))
            return true;
        const int give = out.size() - keep;
        if (!sink(out.constData(), give)) {
            error = true;
            return false;
        }
        out.remove(0, give);
        return true;
    }

    int bits(int need)
    {
        quint32 val = bitbuf;
        while (bitcnt < need) {
            if (in_pos >= in_len) {
                error = true;
                return 0;
            }
            val |= static_cast<quint32>(in[in_pos++]) << bitcnt;
            bitcnt += 8;
        }
        bitbuf = need < 32 ? val >> need : 0;
        bitcnt -= need;
        return static_cast<int>(val & ((1u << need) - 1u));
    }
};

struct Huffman {
    short count[MAXBITS + 1];
    short symbol[288];
};

int decode(Inflater& s, const Huffman& h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= MAXBITS; len++) {
        code |= s.bits(1);
        if (s.error)
            return -1;
        const int count = h.count[len];
        if (code - count < first)
            return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -10;  // ran out of codes
}

// 0: complete code, < 0: over-subscribed, > 0: incomplete
int construct(Huffman& h, const short* length, int n)
{
    for (int len = 0; len <= MAXBITS; len++)
        h.count[len] = 0;
    for (int sym = 0; sym < n; sym++)
        h.count[length[sym]]++;
    if (h.count[0] == n)
        return 0;
    int left = 1;
    for (int len = 1; len <= MAXBITS; len++) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0)
            return left;
    }
    short offs[MAXBITS + 1];
    offs[1] = 0;
    for (int len = 1; len < MAXBITS; len++)
        offs[len + 1] = static_cast<short>(offs[len] + h.count[len]);
    for (int sym = 0; sym < n; sym++) {
        if (length[sym] != 0)
            h.symbol[offs[length[sym]]++] = static_cast<short>(sym);
    }
    return left;
}

bool codes(Inflater& s, const Huffman& lencode, const Huffman& distcode)
{
    static const short lens[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
    static const short lext[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
    static const short dists[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                     257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                     8193, 12289, 16385, 24577 };
    static const short dext[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
    for (;;) {
        int symbol = decode(s, lencode);
        if (symbol < 0 || s.error)
            return false;
        if (symbol < 256) {
            s.out.append(static_cast<char>(symbol));
            continue;
        }
        if (symbol == 256)
            return true;
        symbol -= 257;
        if (symbol >= 29)
            return false;
        const int len = lens[symbol] + s.bits(lext[symbol]);
        symbol = decode(s, distcode);
        if (symbol < 0 || symbol >= 30 || s.error)
            return false;
        const int dist = dists[symbol] + s.bits(dext[symbol]);
        if (s.error || dist > s.out.size())
            return false;
        const int from = s.out.size() - dist;
        for (int i = 0; i < len; i++)
            s.out.append(s.out.at(from + i));
        if (!s.flush(false))
            return false;
    }
}

bool stored(Inflater& s)
{
    s.bitbuf = 0;
    s.bitcnt = 0;
    if (s.in_pos + 4 > s.in_len)
        return false;
    const unsigned len = u16(s.in + s.in_pos);
    const unsigned nlen = u16(s.in + s.in_pos + 2);
    s.in_pos += 4;
    if (len != (~nlen & 0xffffu) || s.in_pos + len > s.in_len)
        return false;
    s.out.append(reinterpret_cast<const char*>(s.in + s.in_pos), static_cast<int>(len));
    s.in_pos += len;
    return s.flush(false);
}

bool fixed(Inflater& s)
{
    struct Fixed { Huffman len; Huffman dist; };
    // built once (thread-safe static)
    static const Fixed tables = []{
        Fixed t;
        short lengths[288];
        int sym = 0;
        for (; sym < 144; sym++) lengths[sym] = 8;
        for (; sym < 256; sym++) lengths[sym] = 9;
        for (; sym < 280; sym++) lengths[sym] = 7;
        for (; sym < 288; sym++) lengths[sym] = 8;
        construct(t.len, lengths, 288);
        for (sym = 0; sym < 30; sym++) lengths[sym] = 5;
        construct(t.dist, lengths, 30);
        return t;
    }();
    return codes(s, tables.len, tables.dist);
}

bool dynamic(Inflater& s)
{
    static const short order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    short lengths[320];
    const int nlen = s.bits(5) + 257;
    const int ndist = s.bits(5) + 1;
    const int ncode = s.bits(4) + 4;
    if (s.error || nlen > 286 || ndist > 30)
        return false;
    int index = 0;
    for (; index < ncode; index++)
        lengths[order[index]] = static_cast<short>(s.bits(3));
    for (; index < 19; index++)
        lengths[order[index]] = 0;
    Huffman lencode, distcode;
    if (construct(lencode, lengths, 19) != 0)
        return false;

    index = 0;
    while (index < nlen + ndist) {
        int symbol = decode(s, lencode);
        if (symbol < 0 || s.error)
            return false;
        if (symbol < 16) {
            lengths[index++] = static_cast<short>(symbol);
            continue;
        }
        short len = 0;
        if (symbol == 16) {
            if (index == 0)
                return false;
            len = lengths[index - 1];
            symbol = 3 + s.bits(2);
        }
        else if (symbol == 17) {
            symbol = 3 + s.bits(3);
        }
        else {
            symbol = 11 + s.bits(7);
        }
        if (s.error || index + symbol > nlen + ndist)
            return false;
        while (symbol--)
            lengths[index++] = len;
    }
    if (lengths[256] == 0)
        return false;
    int err = construct(lencode, lengths, nlen);
    if (err && (err < 0 || nlen != lencode.count[0] + lencode.count[1]))
        return false;
    err = construct(distcode, lengths + nlen, ndist);
    if (err && (err < 0 || ndist != distcode.count[0] + distcode.count[1]))
        return false;
    return codes(s, lencode, distcode);
}

bool inflate(const uchar* in, size_t in_len, quint32 size_hint, QByteArray& out,
             const std::function<bool(const char*, int)>& sink = {})
{
    Inflater s;
    s.in = in;
    s.in_len = in_len;
    s.sink = sink;
    if (!sink)
        s.out.reserve(static_cast<int>(size_hint));
    int last = 0;
    do {
        last = s.bits(1);
        const int type = s.bits(2);
        if (s.error)
            return false;
        bool ok = false;
        if (type == 0) ok = stored(s);
        else if (type == 1) ok = fixed(s);
        else if (type == 2) ok = dynamic(s);
        if (!ok)
            return false;
    } while (!last);
    if (sink)
        return s.flush(true);
    out = std::move(s.out);
    return true;
}

} // namespace


namespace utils {

ZipReader::ZipReader(const QString& path)
    : m_file(path)
{
    if (!m_file.open(QIODevice::ReadOnly))
        return;
    m_size = m_file.size();
    if (m_size < 22)
        return;
    m_data = m_file.map(0, m_size);
    if (!m_data)
        return;

    // the end of the central directory: searched from the end (after a comment)
    qint64 eocd = -1;
    const qint64 lowest = qMax<qint64>(0, m_size - 22 - 65535);
    for (qint64 pos = m_size - 22; pos >= lowest; pos--) {
        if (u32(m_data + pos) == 0x06054b50u) {
            eocd = pos;
            break;
        }
    }
    if (eocd < 0)
        return;
    const quint16 count = u16(m_data + eocd + 10);
    const quint32 dir_offset = u32(m_data + eocd + 16);
    if (dir_offset == 0xffffffffu || dir_offset >= m_size)
        return;  // ZIP64: not for themes

    qint64 pos = dir_offset;
    for (int i = 0; i < count; i++) {
        if (pos + 46 > m_size || u32(m_data + pos) != 0x02014b50u)
            return;
        const uchar* h = m_data + pos;
        Entry e;
        e.method = u16(h + 10);
        e.packed_size = u32(h + 20);
        e.size = u32(h + 24);
        const quint16 name_len = u16(h + 28);
        const quint16 extra_len = u16(h + 30);
        const quint16 comment_len = u16(h + 32);
        const quint32 attributes = u32(h + 38);
        e.header_offset = u32(h + 42);
        if (pos + 46 + name_len > m_size)
            return;
        e.path = QString::fromUtf8(reinterpret_cast<const char*>(h + 46), name_len);
        e.is_dir = e.path.endsWith(QLatin1Char('/'));
        e.is_symlink = ((attributes >> 16) & 0170000u) == 0120000u;
        m_entries.push_back(e);
        pos += 46 + name_len + extra_len + comment_len;
    }
    m_ok = true;
}

ZipReader::~ZipReader()
{
    if (m_data)
        m_file.unmap(const_cast<uchar*>(m_data));
}

QByteArray ZipReader::fileData(const Entry& e, bool* ok) const
{
    if (ok)
        *ok = false;
    if (!m_ok || static_cast<qint64>(e.header_offset) + 30 > m_size
            || u32(m_data + e.header_offset) != 0x04034b50u)
        return {};
    const uchar* h = m_data + e.header_offset;
    const qint64 start = static_cast<qint64>(e.header_offset) + 30 + u16(h + 26) + u16(h + 28);
    if (start + e.packed_size > m_size)
        return {};
    QByteArray out;
    if (e.method == 0) {
        out = QByteArray(reinterpret_cast<const char*>(m_data + start), static_cast<int>(e.packed_size));
    }
    else if (e.method == 8) {
        if (!inflate(m_data + start, e.packed_size, e.size, out))
            return {};
    }
    else {
        return {};
    }
    if (ok)
        *ok = static_cast<quint32>(out.size()) == e.size;
    return out;
}

bool ZipReader::extract(const Entry& e, const std::function<bool(const char*, int)>& sink) const
{
    if (!m_ok || static_cast<qint64>(e.header_offset) + 30 > m_size
            || u32(m_data + e.header_offset) != 0x04034b50u)
        return false;
    const uchar* h = m_data + e.header_offset;
    const qint64 start = static_cast<qint64>(e.header_offset) + 30 + u16(h + 26) + u16(h + 28);
    if (start + e.packed_size > m_size)
        return false;
    if (e.method == 0) {
        // in parts, as the inflater does
        const char* data = reinterpret_cast<const char*>(m_data + start);
        for (quint32 at = 0; at < e.packed_size; at += 4 * 1024 * 1024) {
            const int part = static_cast<int>(std::min<quint32>(4 * 1024 * 1024, e.packed_size - at));
            if (!sink(data + at, part))
                return false;
        }
        return true;
    }
    if (e.method != 8)
        return false;
    QByteArray unused;
    return inflate(m_data + start, e.packed_size, 0, unused, sink);
}

} // namespace utils
