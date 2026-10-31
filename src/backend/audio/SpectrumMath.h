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

#include <cmath>
#include <complex>
#include <vector>


// Pegasus X: the spectrum sums shared by AudioSpectrum (a decoded file)
// and the live sources: a windowed FFT of a block of mono samples, its
// bins gathered into log-spaced bands, in dB scaled to 0..1.
namespace spectrum {

constexpr int FFT_SIZE = 2048;
constexpr float LOW_HZ = 40.f;
constexpr float HIGH_HZ = 16000.f;
constexpr float PI_F = 3.14159265358979f;

// in place, radix-2 (a.size() a power of two)
inline void fft(std::vector<std::complex<float>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const float ang = -2.f * PI_F / static_cast<float>(len);
        const std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.f, 0.f);
            for (size_t j = 0; j < len / 2; j++) {
                const std::complex<float> u = a[i + j];
                const std::complex<float> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

// which FFT bins (or any linear bins of `bin_hz` each) make up each band
struct Bands {
    std::vector<int> lo, hi;
    void init(int bands, float bin_hz, int max_bin)
    {
        lo.assign(static_cast<size_t>(bands), 0);
        hi.assign(static_cast<size_t>(bands), 1);
        for (int b = 0; b < bands; b++) {
            const float f0 = LOW_HZ * std::pow(HIGH_HZ / LOW_HZ, static_cast<float>(b) / bands);
            const float f1 = LOW_HZ * std::pow(HIGH_HZ / LOW_HZ, static_cast<float>(b + 1) / bands);
            int l = static_cast<int>(f0 / bin_hz);
            int h = static_cast<int>(f1 / bin_hz);
            if (h <= l) h = l + 1;
            if (h > max_bin) h = max_bin;
            if (l >= max_bin) l = max_bin - 1;
            if (l < 0) l = 0;
            lo[static_cast<size_t>(b)] = l;
            hi[static_cast<size_t>(b)] = h;
        }
    }
};

// -60 dB .. 0 dB -> 0..1
inline float db_level(float power)
{
    const float db = 10.f * std::log10(power + 1e-12f);
    float level = (db + 60.f) / 60.f;
    return level < 0.f ? 0.f : level > 1.f ? 1.f : level;
}

// one block of FFT_SIZE mono samples -> `bands` levels
class Analyzer {
public:
    void init(int rate, int bands)
    {
        m_bands.init(bands, static_cast<float>(rate) / FFT_SIZE, FFT_SIZE / 2);
        m_window.resize(FFT_SIZE);
        for (int i = 0; i < FFT_SIZE; i++)
            m_window[static_cast<size_t>(i)] = 0.5f * (1.f - std::cos(2.f * PI_F * i / (FFT_SIZE - 1)));
        m_buf.resize(FFT_SIZE);
        m_n = bands;
    }
    int bands() const { return m_n; }
    // `samples`: at least FFT_SIZE mono samples (-1..1); `out`: m_n levels
    void levels(const float* samples, float* out) const
    {
        for (int i = 0; i < FFT_SIZE; i++)
            m_buf[static_cast<size_t>(i)] = samples[i] * m_window[static_cast<size_t>(i)];
        fft(m_buf);
        for (int b = 0; b < m_n; b++) {
            float sum = 0.f;
            const int lo = m_bands.lo[static_cast<size_t>(b)], hi = m_bands.hi[static_cast<size_t>(b)];
            for (int k = lo; k < hi; k++)
                sum += std::norm(m_buf[static_cast<size_t>(k)]);
            out[b] = db_level(sum / static_cast<float>(hi - lo) / (FFT_SIZE * FFT_SIZE / 16.f));
        }
    }
private:
    Bands m_bands;
    std::vector<float> m_window;
    mutable std::vector<std::complex<float>> m_buf;
    int m_n = 0;
};

} // namespace spectrum
