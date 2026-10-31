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

#include <QElapsedTimer>
#include "audio/SpectrumMath.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVector>
#include <memory>
#include <vector>

namespace audio { class LiveSource; }


// Pegasus X: a music visualizer for themes.
//
//     import Pegasus.Audio 1.0
//     Audio { id: music; source: "music.mp3"; autoPlay: true }
//     AudioSpectrum {
//         id: spectrum
//         source: music.source        // the file: decoded once, in the background
//         position: music.position    // where it is (ms); the levels follow it
//         playing: music.playbackState === Audio.PlayingState
//         bands: 32                   // bars, from the bass up
//     }
//
// or, for whatever the device is playing right now (video previews,
// emulators, anything): `live: true` instead of source/position/playing.
// Windows reads its output back (WASAPI loopback), Linux the output's
// monitor device (PulseAudio / PipeWire), Android the output mix (it asks
// for the microphone permission once: that's what Android requires to
// listen to the mix). `liveStatus` says "capturing" or why not.
//     Repeater { model: spectrum.bands; Rectangle { height: 100 * spectrum.levels[index] } }
//
// `levels` is a list of `bands` numbers between 0 and 1, updated `fps`
// times a second while playing (between the player's position updates it
// keeps time by itself). `peak` is the loudest band, `ready` says the file
// is decoded. MP3, OGG Vorbis, WAV and FLAC are read; a file that can't be
// read leaves `levels` at zero and `error` set.
class AudioSpectrum : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(int bands READ bands WRITE setBands NOTIFY bandsChanged)
    Q_PROPERTY(int fps READ fps WRITE setFps NOTIFY fpsChanged)
    Q_PROPERTY(qreal smoothing READ smoothing WRITE setSmoothing NOTIFY smoothingChanged)
    Q_PROPERTY(qreal gain READ gain WRITE setGain NOTIFY gainChanged)
    Q_PROPERTY(QVariantList levels READ levels NOTIFY levelsChanged)
    Q_PROPERTY(qreal peak READ peak NOTIFY levelsChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool decoding READ decoding NOTIFY readyChanged)
    Q_PROPERTY(QString error READ error NOTIFY readyChanged)
    Q_PROPERTY(int duration READ duration NOTIFY readyChanged)
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)
    Q_PROPERTY(QString liveStatus READ liveStatus NOTIFY liveChanged)

public:
    explicit AudioSpectrum(QObject* parent = nullptr);
    ~AudioSpectrum();

    QUrl source() const { return m_source; }
    void setSource(const QUrl&);
    int position() const { return m_position; }
    void setPosition(int ms);
    bool playing() const { return m_playing; }
    void setPlaying(bool);
    int bands() const { return m_bands; }
    void setBands(int);
    int fps() const { return m_fps; }
    void setFps(int);
    qreal smoothing() const { return m_smoothing; }
    void setSmoothing(qreal);
    qreal gain() const { return m_gain; }
    void setGain(qreal);
    QVariantList levels() const;
    qreal peak() const { return m_peak; }
    bool ready() const { return m_analysis && !m_analysis->frames.empty(); }
    bool decoding() const { return m_decoding; }
    QString error() const { return m_error; }
    int duration() const { return m_analysis ? m_analysis->duration_ms : 0; }
    bool live() const { return m_live; }
    void setLive(bool);
    QString liveStatus() const { return m_live_status; }

    /// the frames of a file: `bands` levels every 1000/fps ms
    struct Analysis {
        int bands = 0;
        int fps = 0;
        int duration_ms = 0;
        std::vector<float> frames;  ///< frame-major: frames[frame * bands + band]
        QString error;
    };

signals:
    void sourceChanged();
    void positionChanged();
    void playingChanged();
    void bandsChanged();
    void fpsChanged();
    void smoothingChanged();
    void gainChanged();
    void levelsChanged();
    void readyChanged();
    void liveChanged();

private:
    QUrl m_source;
    int m_position = 0;
    bool m_playing = false;
    int m_bands = 32;
    int m_fps = 30;
    qreal m_smoothing = 0.6;
    qreal m_gain = 1.0;
    QVector<float> m_levels;
    qreal m_peak = 0;
    bool m_decoding = false;
    QString m_error;
    std::shared_ptr<Analysis> m_analysis;
    QFutureWatcher<std::shared_ptr<Analysis>>* m_watcher = nullptr;
    QTimer m_tick;
    QElapsedTimer m_since_position;  ///< time since the player last said where it is
    int m_run = 0;
    bool m_live = false;
    QString m_live_status = QStringLiteral("off");
    audio::LiveSource* m_live_source = nullptr;
    QTimer m_live_tick;
    std::vector<float> m_live_block;
    std::unique_ptr<spectrum::Analyzer> m_live_analyzer;
    int m_live_rate = 0;
    spectrum::Bands m_live_bins;   ///< Android: which of Android's bins make a band
    float m_live_bin_hz = 0.f;

    void liveTick();
    void applyLevels(const float* target);
    void analyze();
    void tick();
    void showFrame(int ms);
    void zero();
};
