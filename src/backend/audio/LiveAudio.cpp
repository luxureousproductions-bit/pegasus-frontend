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


#include "LiveAudio.h"

#include "Log.h"

#include <QMutexLocker>
#include <cstring>

#if defined(Q_OS_WIN)
#include <QThread>
#include <atomic>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#elif defined(Q_OS_ANDROID)
#include <QHash>
#include <QtAndroid>
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <QtAndroidExtras/QAndroidJniObject>
#else
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QAudioInput>
#include <QIODevice>
#endif


namespace audio {

// -- the ring of samples ---------------------------------------------------------

void LiveSource::push(const float* samples, int count)
{
    QMutexLocker lock(&m_lock);
    for (int i = 0; i < count; i++) {
        m_ring[m_head] = samples[i];
        m_head = (m_head + 1) % m_ring.size();
    }
}

void LiveSource::latest(float* out, int count) const
{
    QMutexLocker lock(&m_lock);
    const size_t n = m_ring.size();
    size_t at = (m_head + n - static_cast<size_t>(count) % n) % n;
    for (int i = 0; i < count; i++) {
        out[i] = m_ring[at];
        at = (at + 1) % n;
    }
}


#if defined(Q_OS_WIN)
// -- Windows: WASAPI loopback ----------------------------------------------------------

namespace {
class WasapiSource : public LiveSource {
public:
    using LiveSource::LiveSource;
    ~WasapiSource() override { stop(); }

    bool start() override
    {
        if (m_thread)
            return true;
        m_stop = false;
        m_thread = QThread::create([this]{ run(); });
        m_thread->start();
        // the status is known once the thread opened the device
        for (int i = 0; i < 50 && m_opening; i++)
            QThread::msleep(10);
        return m_ok;
    }
    void stop() override
    {
        if (!m_thread)
            return;
        m_stop = true;
        m_thread->wait(2000);
        delete m_thread;
        m_thread = nullptr;
        setStatus(QStringLiteral("idle"));
    }

private:
    QThread* m_thread = nullptr;
    std::atomic<bool> m_stop { false };
    std::atomic<bool> m_opening { true };
    std::atomic<bool> m_ok { false };

    void fail(const QString& why)
    {
        setStatus(why);
        m_ok = false;
        m_opening = false;
        Log::warning(QStringLiteral("AudioSpectrum"), LOGMSG("Live audio (WASAPI): %1").arg(why));
    }

    void run()
    {
        m_opening = true;
        const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        IMMDeviceEnumerator* enumerator = nullptr;
        IMMDevice* device = nullptr;
        IAudioClient* client = nullptr;
        IAudioCaptureClient* capture = nullptr;
        WAVEFORMATEX* format = nullptr;
        do {
            if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator)))) {
                fail(QStringLiteral("no audio service"));
                break;
            }
            if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))) {
                fail(QStringLiteral("no output device"));
                break;
            }
            if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client)))) {
                fail(QStringLiteral("the output can't be opened"));
                break;
            }
            if (FAILED(client->GetMixFormat(&format))) {
                fail(QStringLiteral("no mix format"));
                break;
            }
            const bool is_float = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT
                || (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE
                    && reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat.Data1 == 3 /* KSDATAFORMAT_SUBTYPE_IEEE_FLOAT */);
            const int channels = format->nChannels;
            const int bits = format->wBitsPerSample;
            if (FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000 /* 1 s */, 0, format, nullptr))) {
                fail(QStringLiteral("loopback refused"));
                break;
            }
            if (FAILED(client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&capture)))) {
                fail(QStringLiteral("no capture service"));
                break;
            }
            if (FAILED(client->Start())) {
                fail(QStringLiteral("couldn't start"));
                break;
            }
            setRate(static_cast<int>(format->nSamplesPerSec));
            setStatus(QStringLiteral("capturing"));
            m_ok = true;
            m_opening = false;
            std::vector<float> mono;
            while (!m_stop) {
                UINT32 packet = 0;
                if (FAILED(capture->GetNextPacketSize(&packet))) break;
                if (packet == 0) {
                    Sleep(5);
                    continue;
                }
                BYTE* data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
                mono.resize(frames);
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    std::fill(mono.begin(), mono.end(), 0.f);
                }
                else if (is_float && bits == 32) {
                    const float* f = reinterpret_cast<const float*>(data);
                    for (UINT32 i = 0; i < frames; i++) {
                        float sum = 0.f;
                        for (int c = 0; c < channels; c++) sum += f[i * channels + c];
                        mono[i] = sum / channels;
                    }
                }
                else if (bits == 16) {
                    const int16_t* s = reinterpret_cast<const int16_t*>(data);
                    for (UINT32 i = 0; i < frames; i++) {
                        float sum = 0.f;
                        for (int c = 0; c < channels; c++) sum += s[i * channels + c] / 32768.f;
                        mono[i] = sum / channels;
                    }
                }
                else {
                    std::fill(mono.begin(), mono.end(), 0.f);
                }
                push(mono.data(), static_cast<int>(frames));
                capture->ReleaseBuffer(frames);
            }
            client->Stop();
        } while (false);
        if (format) CoTaskMemFree(format);
        if (capture) capture->Release();
        if (client) client->Release();
        if (device) device->Release();
        if (enumerator) enumerator->Release();
        if (SUCCEEDED(co)) CoUninitialize();
        m_opening = false;
    }
};
} // namespace

LiveSource* make_live_source(QObject* parent) { return new WasapiSource(parent); }


#elif defined(Q_OS_ANDROID)
// -- Android: the Visualizer on the output mix ---------------------------------------------

namespace {
class VisualizerSource : public LiveSource {
public:
    explicit VisualizerSource(QObject* parent) : LiveSource(parent) { m_gives_bins = true; }
    ~VisualizerSource() override { stop(); }

    bool start() override
    {
        const QString perm = QStringLiteral("android.permission.RECORD_AUDIO");
        if (QtAndroid::checkPermission(perm) != QtAndroid::PermissionResult::Granted) {
            const auto result = QtAndroid::requestPermissionsSync({ perm });
            if (result.value(perm, QtAndroid::PermissionResult::Denied) != QtAndroid::PermissionResult::Granted) {
                setStatus(QStringLiteral("no permission"));
                return false;
            }
        }
        QAndroidJniEnvironment env;
        const jint rate = QAndroidJniObject::callStaticMethod<jint>("org/pegasus_frontend/android/LiveSpectrum", "start", "()I");
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            setStatus(QStringLiteral("not supported"));
            return false;
        }
        if (rate <= 0) {
            setStatus(QStringLiteral("not supported"));
            return false;
        }
        setRate(rate);
        setStatus(QStringLiteral("capturing"));
        return true;
    }
    void stop() override
    {
        QAndroidJniEnvironment env;
        QAndroidJniObject::callStaticMethod<void>("org/pegasus_frontend/android/LiveSpectrum", "stop", "()V");
        if (env->ExceptionCheck())
            env->ExceptionClear();
        setStatus(QStringLiteral("idle"));
    }
    // Android's FFT: byte pairs (real, imag) per bin, the first the DC and
    // the Nyquist magnitudes; bins of rate / capture size Hz
    std::vector<float> bins(float& bin_hz) const override
    {
        std::vector<float> out;
        bin_hz = 0.f;
        QAndroidJniEnvironment env;
        const QAndroidJniObject arr = QAndroidJniObject::callStaticObjectMethod("org/pegasus_frontend/android/LiveSpectrum", "fft", "()[B");
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return out;
        }
        const jbyteArray bytes = arr.object<jbyteArray>();
        if (!bytes)
            return out;
        const jsize n = env->GetArrayLength(bytes);
        if (n < 4)
            return out;
        std::vector<jbyte> buf(static_cast<size_t>(n));
        env->GetByteArrayRegion(bytes, 0, n, buf.data());
        out.reserve(static_cast<size_t>(n / 2));
        out.push_back(std::abs(static_cast<float>(buf[0])) / 128.f);  // DC
        for (jsize k = 2; k + 1 < n; k += 2) {
            const float re = buf[static_cast<size_t>(k)] / 128.f;
            const float im = buf[static_cast<size_t>(k + 1)] / 128.f;
            out.push_back(std::sqrt(re * re + im * im));
        }
        bin_hz = rate() > 0 ? static_cast<float>(rate()) / static_cast<float>(n) : 0.f;
        return out;
    }
};
} // namespace

LiveSource* make_live_source(QObject* parent) { return new VisualizerSource(parent); }


#else
// -- Linux and the rest: the output's monitor device, through Qt --------------------------

namespace {
class MonitorSource : public LiveSource {
public:
    using LiveSource::LiveSource;
    ~MonitorSource() override { stop(); }

    bool start() override
    {
        if (m_input)
            return true;
        QAudioDeviceInfo found;
        for (const QAudioDeviceInfo& dev : QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
            const QString name = dev.deviceName().toLower();
            if (name.contains(QLatin1String("monitor")) || name.contains(QLatin1String("loopback"))) {
                found = dev;
                break;
            }
        }
        if (found.isNull()) {
            setStatus(QStringLiteral("no monitor device"));
            Log::warning(QStringLiteral("AudioSpectrum"), LOGMSG("Live audio: no monitor/loopback input device (PulseAudio or PipeWire expose one)"));
            return false;
        }
        QAudioFormat format;
        format.setSampleRate(48000);
        format.setChannelCount(1);
        format.setSampleSize(16);
        format.setCodec(QStringLiteral("audio/pcm"));
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setSampleType(QAudioFormat::SignedInt);
        if (!found.isFormatSupported(format))
            format = found.nearestFormat(format);
        m_format = format;
        m_input = new QAudioInput(found, format, this);
        m_io = m_input->start();
        if (!m_io) {
            setStatus(QStringLiteral("the monitor can't be opened"));
            delete m_input;
            m_input = nullptr;
            return false;
        }
        connect(m_io, &QIODevice::readyRead, this, [this]{ read(); });
        setRate(format.sampleRate());
        setStatus(QStringLiteral("capturing"));
        return true;
    }
    void stop() override
    {
        if (!m_input)
            return;
        m_input->stop();
        delete m_input;
        m_input = nullptr;
        m_io = nullptr;
        setStatus(QStringLiteral("idle"));
    }

private:
    QAudioInput* m_input = nullptr;
    QIODevice* m_io = nullptr;
    QAudioFormat m_format;

    void read()
    {
        const QByteArray data = m_io->readAll();
        const int channels = qMax(1, m_format.channelCount());
        std::vector<float> mono;
        if (m_format.sampleSize() == 16 && m_format.sampleType() == QAudioFormat::SignedInt) {
            const int16_t* s = reinterpret_cast<const int16_t*>(data.constData());
            const int frames = data.size() / 2 / channels;
            mono.resize(static_cast<size_t>(frames));
            for (int i = 0; i < frames; i++) {
                float sum = 0.f;
                for (int c = 0; c < channels; c++) sum += s[i * channels + c] / 32768.f;
                mono[static_cast<size_t>(i)] = sum / channels;
            }
        }
        else if (m_format.sampleSize() == 32 && m_format.sampleType() == QAudioFormat::Float) {
            const float* f = reinterpret_cast<const float*>(data.constData());
            const int frames = data.size() / 4 / channels;
            mono.resize(static_cast<size_t>(frames));
            for (int i = 0; i < frames; i++) {
                float sum = 0.f;
                for (int c = 0; c < channels; c++) sum += f[i * channels + c];
                mono[static_cast<size_t>(i)] = sum / channels;
            }
        }
        if (!mono.empty())
            push(mono.data(), static_cast<int>(mono.size()));
    }
};
} // namespace

LiveSource* make_live_source(QObject* parent) { return new MonitorSource(parent); }

#endif

} // namespace audio
