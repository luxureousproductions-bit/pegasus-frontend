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


#include "AudioSpectrum.h"

#include "Log.h"

#define MINIMP3_FLOAT_OUTPUT
#include "minimp3_ex.h"
#include "dr_wav.h"
#include "dr_flac.h"
#include "stb_vorbis.h"

#include <QFile>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrent>
#include <cmath>
#include <complex>


namespace {
const QString LOG_TAG = QStringLiteral("AudioSpectrum");
const int FFT_SIZE = 2048;
const float PI_F = 3.14159265358979f;
const float LOW_HZ = 40.f;
const float HIGH_HZ = 16000.f;

struct Pcm {
    std::vector<float> mono;  ///< -1..1
    int rate = 0;
    QString error;
};

// the file as mono float samples
Pcm decode(const QString& path)
{
    Pcm out;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        out.error = QStringLiteral("Can't open the file");
        return out;
    }
    const QByteArray data = file.readAll();
    file.close();
    const QString ext = QFileInfo(path).suffix().toLower();
    const auto downmix = [&out](const float* samples, size_t frames, int channels) {
        out.mono.resize(frames);
        for (size_t i = 0; i < frames; i++) {
            float sum = 0.f;
            for (int c = 0; c < channels; c++)
                sum += samples[i * static_cast<size_t>(channels) + static_cast<size_t>(c)];
            out.mono[i] = sum / static_cast<float>(channels);
        }
    };
    if (ext == QLatin1String("mp3")) {
        mp3dec_t dec;
        mp3dec_file_info_t info;
        if (mp3dec_load_buf(&dec, reinterpret_cast<const uint8_t*>(data.constData()), static_cast<size_t>(data.size()), &info, nullptr, nullptr) != 0 || info.samples == 0) {
            out.error = QStringLiteral("Not an MP3 that can be read");
            return out;
        }
        out.rate = info.hz;
        downmix(info.buffer, info.samples / static_cast<size_t>(info.channels), info.channels);
        free(info.buffer);
        return out;
    }
    if (ext == QLatin1String("ogg") || ext == QLatin1String("oga")) {
        int channels = 0, rate = 0;
        short* samples = nullptr;
        const int frames = stb_vorbis_decode_memory(reinterpret_cast<const unsigned char*>(data.constData()), data.size(), &channels, &rate, &samples);
        if (frames <= 0 || !samples) {
            out.error = QStringLiteral("Not an OGG Vorbis file that can be read");
            return out;
        }
        out.rate = rate;
        out.mono.resize(static_cast<size_t>(frames));
        for (int i = 0; i < frames; i++) {
            float sum = 0.f;
            for (int c = 0; c < channels; c++)
                sum += samples[i * channels + c] / 32768.f;
            out.mono[static_cast<size_t>(i)] = sum / static_cast<float>(channels);
        }
        free(samples);
        return out;
    }
    if (ext == QLatin1String("wav")) {
        unsigned int channels = 0, rate = 0;
        drwav_uint64 frames = 0;
        float* samples = drwav_open_memory_and_read_pcm_frames_f32(data.constData(), static_cast<size_t>(data.size()), &channels, &rate, &frames, nullptr);
        if (!samples || frames == 0) {
            out.error = QStringLiteral("Not a WAV file that can be read");
            return out;
        }
        out.rate = static_cast<int>(rate);
        downmix(samples, static_cast<size_t>(frames), static_cast<int>(channels));
        drwav_free(samples, nullptr);
        return out;
    }
    if (ext == QLatin1String("flac")) {
        unsigned int channels = 0, rate = 0;
        drflac_uint64 frames = 0;
        float* samples = drflac_open_memory_and_read_pcm_frames_f32(data.constData(), static_cast<size_t>(data.size()), &channels, &rate, &frames, nullptr);
        if (!samples || frames == 0) {
            out.error = QStringLiteral("Not a FLAC file that can be read");
            return out;
        }
        out.rate = static_cast<int>(rate);
        downmix(samples, static_cast<size_t>(frames), static_cast<int>(channels));
        drflac_free(samples, nullptr);
        return out;
    }
    out.error = QStringLiteral("Only MP3, OGG, WAV and FLAC can be read");
    return out;
}

// in place, radix-2
void fft(std::vector<std::complex<float>>& a)
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

// the levels of every frame: an FFT every 1000/fps ms, its bins gathered
// into log-spaced bands, in dB scaled to 0..1
std::shared_ptr<AudioSpectrum::Analysis> analyze_pcm(const Pcm& pcm, int bands, int fps)
{
    auto out = std::make_shared<AudioSpectrum::Analysis>();
    out->bands = bands;
    out->fps = fps;
    if (!pcm.error.isEmpty() || pcm.mono.empty() || pcm.rate <= 0) {
        out->error = pcm.error.isEmpty() ? QStringLiteral("No audio") : pcm.error;
        return out;
    }
    out->duration_ms = static_cast<int>(pcm.mono.size() * 1000 / static_cast<size_t>(pcm.rate));
    const size_t hop = static_cast<size_t>(pcm.rate) / static_cast<size_t>(fps);
    const size_t n_frames = pcm.mono.size() / hop + 1;
    // which bins each band takes (log spaced between LOW_HZ and HIGH_HZ)
    std::vector<int> band_lo(static_cast<size_t>(bands)), band_hi(static_cast<size_t>(bands));
    const float bin_hz = static_cast<float>(pcm.rate) / FFT_SIZE;
    for (int b = 0; b < bands; b++) {
        const float f0 = LOW_HZ * std::pow(HIGH_HZ / LOW_HZ, static_cast<float>(b) / bands);
        const float f1 = LOW_HZ * std::pow(HIGH_HZ / LOW_HZ, static_cast<float>(b + 1) / bands);
        int lo = static_cast<int>(f0 / bin_hz);
        int hi = static_cast<int>(f1 / bin_hz);
        if (hi <= lo) hi = lo + 1;
        if (hi > FFT_SIZE / 2) hi = FFT_SIZE / 2;
        if (lo >= FFT_SIZE / 2) lo = FFT_SIZE / 2 - 1;
        band_lo[static_cast<size_t>(b)] = lo;
        band_hi[static_cast<size_t>(b)] = hi;
    }
    std::vector<float> window(FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; i++)
        window[static_cast<size_t>(i)] = 0.5f * (1.f - std::cos(2.f * PI_F * i / (FFT_SIZE - 1)));
    out->frames.assign(n_frames * static_cast<size_t>(bands), 0.f);
    std::vector<std::complex<float>> buf(FFT_SIZE);
    for (size_t f = 0; f < n_frames; f++) {
        const size_t start = f * hop;
        for (int i = 0; i < FFT_SIZE; i++) {
            const size_t at = start + static_cast<size_t>(i);
            buf[static_cast<size_t>(i)] = at < pcm.mono.size() ? pcm.mono[at] * window[static_cast<size_t>(i)] : 0.f;
        }
        fft(buf);
        for (int b = 0; b < bands; b++) {
            float sum = 0.f;
            const int lo = band_lo[static_cast<size_t>(b)], hi = band_hi[static_cast<size_t>(b)];
            for (int k = lo; k < hi; k++)
                sum += std::norm(buf[static_cast<size_t>(k)]);
            const float power = sum / static_cast<float>(hi - lo) / (FFT_SIZE * FFT_SIZE / 16.f);
            // -60 dB .. 0 dB -> 0..1
            const float db = 10.f * std::log10(power + 1e-12f);
            float level = (db + 60.f) / 60.f;
            if (level < 0.f) level = 0.f;
            if (level > 1.f) level = 1.f;
            out->frames[f * static_cast<size_t>(bands) + static_cast<size_t>(b)] = level;
        }
    }
    return out;
}
} // namespace


AudioSpectrum::AudioSpectrum(QObject* parent)
    : QObject(parent)
{
    m_levels.fill(0.f, m_bands);
    m_tick.setInterval(1000 / m_fps);
    connect(&m_tick, &QTimer::timeout, this, &AudioSpectrum::tick);
}

AudioSpectrum::~AudioSpectrum()
{
    m_run++;
}

void AudioSpectrum::setSource(const QUrl& url)
{
    if (url == m_source)
        return;
    m_source = url;
    emit sourceChanged();
    analyze();
}

void AudioSpectrum::setBands(int n)
{
    n = qBound(1, n, 256);
    if (n == m_bands)
        return;
    m_bands = n;
    m_levels.fill(0.f, m_bands);
    emit bandsChanged();
    emit levelsChanged();
    analyze();
}

void AudioSpectrum::setFps(int n)
{
    n = qBound(5, n, 60);
    if (n == m_fps)
        return;
    m_fps = n;
    m_tick.setInterval(1000 / m_fps);
    emit fpsChanged();
    analyze();
}

void AudioSpectrum::setSmoothing(qreal v)
{
    v = qBound(0.0, v, 0.98);
    if (qFuzzyCompare(v, m_smoothing))
        return;
    m_smoothing = v;
    emit smoothingChanged();
}

void AudioSpectrum::setGain(qreal v)
{
    v = qBound(0.1, v, 4.0);
    if (qFuzzyCompare(v, m_gain))
        return;
    m_gain = v;
    emit gainChanged();
}

void AudioSpectrum::setPosition(int ms)
{
    m_position = ms;
    m_since_position.restart();
    emit positionChanged();
    if (!m_playing)
        showFrame(ms);
}

void AudioSpectrum::setPlaying(bool on)
{
    if (on == m_playing)
        return;
    m_playing = on;
    emit playingChanged();
    if (on) {
        m_since_position.restart();
        m_tick.start();
    }
    else {
        m_tick.stop();
        zero();
    }
}

QVariantList AudioSpectrum::levels() const
{
    QVariantList out;
    out.reserve(m_levels.size());
    for (const float v : m_levels)
        out.append(static_cast<qreal>(v));
    return out;
}

void AudioSpectrum::zero()
{
    bool any = false;
    for (float& v : m_levels) {
        any = any || v > 0.f;
        v = 0.f;
    }
    m_peak = 0;
    if (any)
        emit levelsChanged();
}

// the file decoded and its frames computed, off the main thread
void AudioSpectrum::analyze()
{
    m_run++;
    m_analysis.reset();
    m_error.clear();
    zero();
    const QString path = m_source.isLocalFile() ? m_source.toLocalFile()
                       : m_source.scheme().isEmpty() ? m_source.toString() : QString();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        m_decoding = false;
        if (!m_source.isEmpty())
            m_error = QStringLiteral("Only local files can be read");
        emit readyChanged();
        return;
    }
    m_decoding = true;
    emit readyChanged();
    const int run = m_run;
    const int bands = m_bands;
    const int fps = m_fps;
    auto* const watcher = new QFutureWatcher<std::shared_ptr<Analysis>>(this);
    connect(watcher, &QFutureWatcher<std::shared_ptr<Analysis>>::finished, this, [this, watcher, run]{
        const std::shared_ptr<Analysis> result = watcher->result();
        watcher->deleteLater();
        if (run != m_run)
            return;
        m_decoding = false;
        m_analysis = result;
        m_error = result ? result->error : QStringLiteral("No audio");
        if (!m_error.isEmpty())
            Log::warning(LOG_TAG, LOGMSG("%1: %2").arg(m_source.toString(), m_error));
        emit readyChanged();
        showFrame(m_position);
    });
    watcher->setFuture(QtConcurrent::run([path, bands, fps]{
        return analyze_pcm(decode(path), bands, fps);
    }));
}

// while playing: the position the player last said, plus the time since
void AudioSpectrum::tick()
{
    showFrame(m_position + static_cast<int>(m_since_position.elapsed()));
}

void AudioSpectrum::showFrame(int ms)
{
    if (!m_analysis || m_analysis->frames.empty() || m_analysis->bands != m_bands) {
        zero();
        return;
    }
    size_t frame = static_cast<size_t>(qMax(0, ms)) * static_cast<size_t>(m_analysis->fps) / 1000;
    const size_t n_frames = m_analysis->frames.size() / static_cast<size_t>(m_bands);
    if (frame >= n_frames)
        frame = n_frames - 1;
    const float* src = &m_analysis->frames[frame * static_cast<size_t>(m_bands)];
    const float keep = static_cast<float>(m_smoothing);
    float peak = 0.f;
    for (int b = 0; b < m_bands; b++) {
        float target = src[b] * static_cast<float>(m_gain);
        if (target > 1.f) target = 1.f;
        float& now = m_levels[b];
        // quick up, smooth down
        now = target > now ? target : now * keep + target * (1.f - keep);
        if (now > peak) peak = now;
    }
    m_peak = peak;
    emit levelsChanged();
}
