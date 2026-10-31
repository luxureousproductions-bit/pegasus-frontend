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


package org.pegasus_frontend.android;

import android.media.audiofx.Visualizer;


// Pegasus X: the spectrum of whatever the device plays, from Android's
// Visualizer on the output mix (session 0; needs RECORD_AUDIO). Android
// hands FFT frames at its own pace; the last one is kept for Pegasus X.
public class LiveSpectrum {
    private static Visualizer m_visualizer;
    private static byte[] m_fft;
    private static int m_rate;  // samples per second of the capture

    // the sampling rate, or 0 when it couldn't start
    public static synchronized int start() {
        if (m_visualizer != null)
            return m_rate;
        try {
            final Visualizer v = new Visualizer(0);
            final int[] range = Visualizer.getCaptureSizeRange();
            int size = 1024;
            if (size > range[1]) size = range[1];
            if (size < range[0]) size = range[0];
            v.setCaptureSize(size);
            m_rate = v.getSamplingRate() / 1000;  // it's in mHz
            v.setDataCaptureListener(new Visualizer.OnDataCaptureListener() {
                @Override
                public void onWaveFormDataCapture(Visualizer visualizer, byte[] waveform, int samplingRate) {}
                @Override
                public void onFftDataCapture(Visualizer visualizer, byte[] fft, int samplingRate) {
                    synchronized (LiveSpectrum.class) {
                        m_fft = fft.clone();
                    }
                }
            }, Visualizer.getMaxCaptureRate(), false, true);
            v.setEnabled(true);
            m_visualizer = v;
            return m_rate;
        } catch (Exception e) {
            m_visualizer = null;
            return 0;
        }
    }

    public static synchronized void stop() {
        if (m_visualizer == null)
            return;
        try {
            m_visualizer.setEnabled(false);
            m_visualizer.release();
        } catch (Exception e) {
            // nothing to do
        }
        m_visualizer = null;
        m_fft = null;
    }

    // the last FFT frame Android gave (null when none yet)
    public static synchronized byte[] fft() {
        return m_fft;
    }
}
