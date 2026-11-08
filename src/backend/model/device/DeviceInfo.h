// Pegasus Frontend
// Copyright (C) 2017-2021  Mátyás Mustoha
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

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QVariantList>


namespace model {
class DeviceInfo : public QObject {
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

    Q_PROPERTY(BatteryStatus batteryStatus READ batteryStatus NOTIFY batteryStatusChanged)
    Q_PROPERTY(bool batteryCharging READ batteryCharging NOTIFY batteryStatusChanged)
    Q_PROPERTY(float batteryPercent READ batteryPercent NOTIFY batteryLifeChanged)
    Q_PROPERTY(int batterySeconds READ batterySeconds NOTIFY batteryLifeChanged)

    // Pegasus X: more about the device, updated as Android reports changes.
    // A theme checks `typeof api.device.wifiLevel !== "undefined"` first:
    // stock Pegasus has none of these.
    // the network
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY networkChanged)
    Q_PROPERTY(bool wifiConnected READ wifiConnected NOTIFY networkChanged)
    Q_PROPERTY(int wifiLevel READ wifiLevel NOTIFY networkChanged)          ///< 0..4 bars
    Q_PROPERTY(int wifiRssi READ wifiRssi NOTIFY networkChanged)            ///< dBm (-127: none)
    Q_PROPERTY(QString network READ network NOTIFY networkChanged)          ///< wifi, mobile, ethernet, other, none
    Q_PROPERTY(bool internet READ internet NOTIFY networkChanged)           ///< the network reaches the internet
    Q_PROPERTY(bool airplaneMode READ airplaneMode NOTIFY networkChanged)
    // the clock: `now` changes right on each minute
    Q_PROPERTY(QDateTime now READ now NOTIFY clockChanged)
    Q_PROPERTY(QString clockText READ clockText NOTIFY clockChanged)        ///< "9:41 PM" or "21:41", as the system is set
    Q_PROPERTY(bool use24Hour READ use24Hour NOTIFY clockChanged)
    // sound
    Q_PROPERTY(qreal volume READ volume NOTIFY soundChanged)                ///< 0..1
    Q_PROPERTY(bool muted READ muted NOTIFY soundChanged)
    Q_PROPERTY(QString audioOutput READ audioOutput NOTIFY soundChanged)    ///< speaker, headphones, bluetooth, usb, hdmi
    // the battery, more
    Q_PROPERTY(qreal batteryTemperature READ batteryTemperature NOTIFY batteryDetailsChanged)  ///< degrees C (NaN: unknown)
    Q_PROPERTY(QString batterySource READ batterySource NOTIFY batteryDetailsChanged)  ///< ac, usb, wireless, dock, "" (on battery)
    Q_PROPERTY(QString batteryHealth READ batteryHealth NOTIFY batteryDetailsChanged)  ///< good, overheat, cold, dead, ...
    // how hot it runs: 0 none, 1 light, 2 moderate, 3 severe, 4 critical, 5 emergency, 6 shutdown
    Q_PROPERTY(int thermalStatus READ thermalStatus NOTIFY thermalChanged)
    // the system
    Q_PROPERTY(bool darkMode READ darkMode NOTIFY systemChanged)
    Q_PROPERTY(QString deviceModel READ deviceModel NOTIFY systemChanged)
    /// [{ name, builtIn, battery (0..100, -1: not told), charging }]
    Q_PROPERTY(QVariantList gamepads READ gamepads NOTIFY gamepadsChanged)
    /// [{ name, path, free, total (bytes), removable }], read each minute
    Q_PROPERTY(QVariantList storage READ storage NOTIFY storageChanged)

public:
    explicit DeviceInfo(QObject* parent = nullptr);

    enum class BatteryStatus {
        Unknown,
        NoBattery,
        Discharging,
        Charging,
        Charged,
    };
    Q_ENUM(BatteryStatus)

    struct BatteryInfo {
        BatteryStatus status;
        float percent;
        int seconds;
    };

    BatteryStatus batteryStatus() const { return m_battery.status; }
    bool batteryCharging() const {
        return m_battery.status == BatteryStatus::Charging || m_battery.status == BatteryStatus::Charged;
    }
    float batteryPercent() const { return m_battery.percent; }
    int batterySeconds() const { return m_battery.seconds; }

    bool wifiEnabled() const { return m_wifi_on; }
    bool wifiConnected() const { return m_network == QLatin1String("wifi"); }
    int wifiLevel() const { return m_wifi_level; }
    int wifiRssi() const { return m_wifi_rssi; }
    QString network() const { return m_network; }
    bool internet() const { return m_internet; }
    bool airplaneMode() const { return m_airplane; }
    QDateTime now() const { return m_now; }
    QString clockText() const;
    bool use24Hour() const { return m_24h; }
    qreal volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    QString audioOutput() const { return m_output; }
    qreal batteryTemperature() const { return m_bat_temp; }
    QString batterySource() const { return m_bat_source; }
    QString batteryHealth() const { return m_bat_health; }
    int thermalStatus() const { return m_thermal; }
    bool darkMode() const { return m_dark; }
    QString deviceModel() const { return m_model; }
    QVariantList gamepads() const { return m_gamepads; }
    QVariantList storage() const { return m_storage; }

signals:
    void batteryStatusChanged();
    void batteryLifeChanged();
    void networkChanged();
    void clockChanged();
    void soundChanged();
    void batteryDetailsChanged();
    void thermalChanged();
    void systemChanged();
    void gamepadsChanged();
    void storageChanged();

private slots:
    void poll_battery();

private:
    QTimer m_battery_poll;
    BatteryInfo m_battery;

    QTimer m_status_poll;    ///< Android: has anything changed (a cheap call)
    int m_status_version = -1;
    QTimer m_clock;          ///< fires right on the next minute
    bool m_wifi_on = false;
    int m_wifi_level = 0;
    int m_wifi_rssi = -127;
    QString m_network = QStringLiteral("none");
    bool m_internet = false;
    bool m_airplane = false;
    QDateTime m_now;
    bool m_24h = true;
    qreal m_volume = 0;
    bool m_muted = false;
    QString m_output = QStringLiteral("speaker");
    qreal m_bat_temp;
    QString m_bat_source;
    QString m_bat_health;
    int m_thermal = 0;
    bool m_dark = false;
    QString m_model;
    QVariantList m_gamepads;
    QVariantList m_storage;

    void poll_status();
    void tick_clock();
    void read_storage();
    void on_app_state(Qt::ApplicationState);
};
} // namespace model
