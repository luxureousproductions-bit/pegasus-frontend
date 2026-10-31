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

#include <QMutex>
#include <QObject>
#include <QString>
#include <vector>


// Pegasus X: the device's audio output, as it sounds now, for AudioSpectrum
// (live: true). One source per platform:
//   Windows  - WASAPI loopback of the default output (no permission)
//   Linux    - the output's monitor device (PulseAudio / PipeWire), through Qt
//   Android  - the Visualizer on the output mix (needs the RECORD_AUDIO permission)
// A source keeps the latest samples (mono, -1..1) in a ring; AudioSpectrum
// reads a block of them at its own pace. Android's source gives the FFT
// Android made instead (bins()).
namespace audio {

class LiveSource : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~LiveSource() override = default;

    /// starts capturing; false (and status()) when it can't
    virtual bool start() = 0;
    virtual void stop() = 0;
    /// "capturing", or why not: "no permission", "no device", "not supported", ...
    QString status() const { return m_status; }
    /// samples per second of what's captured (0 until started)
    int rate() const { return m_rate; }

    /// the last `count` mono samples, oldest first (zeros when there aren't that many yet)
    void latest(float* out, int count) const;
    /// Android: the magnitudes of Android's FFT bins (empty elsewhere), and
    /// the width of a bin in Hz
    virtual std::vector<float> bins(float& bin_hz) const { bin_hz = 0.f; return {}; }
    bool givesBins() const { return m_gives_bins; }

protected:
    void push(const float* samples, int count);
    void setStatus(const QString& s) { m_status = s; }
    void setRate(int r) { m_rate = r; }
    bool m_gives_bins = false;

private:
    mutable QMutex m_lock;
    std::vector<float> m_ring = std::vector<float>(1 << 15, 0.f);
    size_t m_head = 0;
    QString m_status = QStringLiteral("idle");
    int m_rate = 0;
};

/// the source for this platform (never null; one that says "not supported" otherwise)
LiveSource* make_live_source(QObject* parent);

} // namespace audio
