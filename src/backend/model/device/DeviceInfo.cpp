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


#include "DeviceInfo.h"

#ifdef WITH_SDL_POWER
#include <SDL.h>
#elif defined(Q_OS_ANDROID)
#include "platform/AndroidHelpers.h"
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <QtAndroidExtras/QAndroidJniObject>
#endif

#include <QDir>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QStorageInfo>
#include <QSysInfo>
#ifdef Q_OS_ANDROID
#include "platform/AndroidHelpers.h"
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtAndroidExtras/QtAndroid>
#endif

#include <cmath>


namespace {
#ifdef WITH_SDL_POWER
model::DeviceInfo::BatteryStatus sdl_state_to_qt(const SDL_PowerState sdl_state)
{
    using Status = model::DeviceInfo::BatteryStatus;
    switch (sdl_state) {
        case SDL_POWERSTATE_NO_BATTERY: return Status::NoBattery;
        case SDL_POWERSTATE_ON_BATTERY: return Status::Discharging;
        case SDL_POWERSTATE_CHARGING: return Status::Charging;
        case SDL_POWERSTATE_CHARGED: return Status::Charged;
        default: return Status::Unknown;
    }
}

model::DeviceInfo::BatteryInfo query_battery()
{
    int sdl_secs = 0;
    int sdl_pct = 0;
    const SDL_PowerState sdl_state = SDL_GetPowerInfo(&sdl_secs, &sdl_pct);

    return {
        sdl_state_to_qt(sdl_state),
        sdl_pct < 0 ? NAN : (sdl_pct / 100.f),
        sdl_secs,
    };
}
#elif defined(Q_OS_ANDROID)
model::DeviceInfo::BatteryInfo query_battery()
{
    static constexpr auto JNI_METHOD = "queryBattery";
    static constexpr auto JNI_SIGNATURE = "()Lorg/pegasus_frontend/android/BatteryInfo;";

    QAndroidJniEnvironment jni_env;

    const QAndroidJniObject jni_obj = QAndroidJniObject::callStaticObjectMethod(android::jni_classname(), JNI_METHOD, JNI_SIGNATURE);
    if (!jni_obj.isValid())
        return {model::DeviceInfo::BatteryStatus::Unknown, NAN, -1};

    const bool present = jni_obj.callMethod<jboolean>("present");
    const bool plugged = jni_obj.callMethod<jboolean>("plugged");
    const bool charged = jni_obj.callMethod<jboolean>("charged");
    const float percent = qBound(0.f, jni_obj.callMethod<jfloat>("percent"), 1.f);

    if (!present)
        return {model::DeviceInfo::BatteryStatus::NoBattery, NAN, -1};
    if (charged)
        return {model::DeviceInfo::BatteryStatus::Charged, 1.f, -1};
    if (plugged)
        return {model::DeviceInfo::BatteryStatus::Charging, percent, -1};

    return {model::DeviceInfo::BatteryStatus::Discharging, percent, -1};
}
#else
model::DeviceInfo::BatteryInfo query_battery()
{
    return {model::DeviceInfo::BatteryStatus::Unknown, NAN, -1};
}
#endif
} // namespace


namespace model {
DeviceInfo::DeviceInfo(QObject* parent)
    : QObject(parent)
    , m_battery { BatteryStatus::Unknown, NAN, 0 }
    , m_bat_temp(NAN)
{
    poll_battery();
    connect(&m_battery_poll, &QTimer::timeout, this, &DeviceInfo::poll_battery);
    m_battery_poll.start(30 * 1000);

    // Pegasus X: the clock, right on each minute
    m_clock.setSingleShot(true);
    m_clock.setTimerType(Qt::PreciseTimer);
    connect(&m_clock, &QTimer::timeout, this, &DeviceInfo::tick_clock);

#ifdef Q_OS_ANDROID
    // Android tells SystemStatus about changes; here, only "anything new?"
    QAndroidJniObject::callStaticMethod<void>("org/pegasus_frontend/android/SystemStatus", "start",
        "(Landroid/content/Context;)V", QtAndroid::androidContext().object());
    connect(&m_status_poll, &QTimer::timeout, this, &DeviceInfo::poll_status);
    m_status_poll.start(500);
    poll_status();
#else
    const QString fmt = QLocale::system().timeFormat(QLocale::ShortFormat);
    m_24h = !fmt.contains(QLatin1String("ap"), Qt::CaseInsensitive);
    m_model = QSysInfo::prettyProductName();
#endif
    tick_clock();
    read_storage();

    // nothing read while Pegasus X is in the background (a game is running);
    // everything again when it's back
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, &DeviceInfo::on_app_state);
}

void DeviceInfo::on_app_state(Qt::ApplicationState state)
{
    // Pegasus X: away (a game in front, the screen off), the 30-second battery
    // read stops too, and Android's frequent broadcasts (battery, Wi-Fi
    // signal, the minute's controller and Bluetooth reads) aren't listened
    // to; back, everything is read again at once
    const bool active = state == Qt::ApplicationActive;
    if (active == m_active)
        return;
    m_active = active;
    if (active) {
#ifdef Q_OS_ANDROID
        QAndroidJniObject::callStaticMethod<void>("org/pegasus_frontend/android/SystemStatus", "resume", "()V");
        m_status_poll.start(500);
        poll_status();
#endif
        poll_battery();
        m_battery_poll.start(30 * 1000);
        tick_clock();
        read_storage();
    }
    else {
#ifdef Q_OS_ANDROID
        QAndroidJniObject::callStaticMethod<void>("org/pegasus_frontend/android/SystemStatus", "pause", "()V");
#endif
        m_status_poll.stop();
        m_battery_poll.stop();
        m_clock.stop();
    }
}

QString DeviceInfo::clockText() const
{
    return m_now.time().toString(m_24h ? QStringLiteral("HH:mm") : QStringLiteral("h:mm AP"));
}

void DeviceInfo::tick_clock()
{
    QDateTime current = QDateTime::currentDateTime();
    const QTime t = current.time();
    current.setTime(QTime(t.hour(), t.minute()));
    const bool moved = current != m_now;
    m_now = current;
    // the next one right on the minute (a little after, never before)
    const int ms_left = 60000 - (t.second() * 1000 + t.msec()) + 30;
    m_clock.start(ms_left);
    if (moved) {
        emit clockChanged();
        // once a minute: free space
        if (t.second() < 2)
            read_storage();
    }
}

void DeviceInfo::read_storage()
{
    QVariantList out;
#ifdef Q_OS_ANDROID
    const QStringList roots = android::storage_paths();
#else
    QStringList roots { QStorageInfo::root().rootPath() };
    const QString home_root = QStorageInfo(QDir::homePath()).rootPath();
    if (!roots.contains(home_root))
        roots.append(home_root);
#endif
    int sd = 0;
    for (const QString& path : roots) {
        const QStorageInfo info(path);
        if (!info.isValid() || !info.isReady() || info.bytesTotal() <= 0)
            continue;
#ifdef Q_OS_ANDROID
        const bool removable = !path.startsWith(QLatin1String("/storage/emulated"));
        const QString name = !removable ? QStringLiteral("Internal storage")
            : (sd++ == 0 ? QStringLiteral("SD card") : QStringLiteral("SD card %1").arg(sd));
#else
        const bool removable = false;
        const QString name = info.displayName();
        Q_UNUSED(sd);
#endif
        out.append(QVariantMap {
            { QStringLiteral("name"), name },
            { QStringLiteral("path"), path },
            { QStringLiteral("free"), static_cast<double>(info.bytesAvailable()) },
            { QStringLiteral("total"), static_cast<double>(info.bytesTotal()) },
            { QStringLiteral("removable"), removable },
        });
    }
    if (out != m_storage) {
        m_storage = out;
        emit storageChanged();
    }
}

void DeviceInfo::poll_status()
{
#ifdef Q_OS_ANDROID
    static constexpr auto CLASS = "org/pegasus_frontend/android/SystemStatus";
    const int version = QAndroidJniObject::callStaticMethod<jint>(CLASS, "version");
    if (version == m_status_version)
        return;
    m_status_version = version;
    const QString json = QAndroidJniObject::callStaticObjectMethod(CLASS, "state", "()Ljava/lang/String;").toString();
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    if (o.isEmpty())
        return;

    const bool wifi_on = o.value(QStringLiteral("wifiOn")).toBool();
    const int wifi_level = o.value(QStringLiteral("wifiLevel")).toInt();
    const int wifi_rssi = o.value(QStringLiteral("wifiRssi")).toInt(-127);
    const QString network = o.value(QStringLiteral("network")).toString(QStringLiteral("none"));
    const bool internet = o.value(QStringLiteral("internet")).toBool();
    const bool airplane = o.value(QStringLiteral("airplane")).toBool();
    if (wifi_on != m_wifi_on || wifi_level != m_wifi_level || wifi_rssi != m_wifi_rssi
            || network != m_network || internet != m_internet || airplane != m_airplane) {
        m_wifi_on = wifi_on;
        m_wifi_level = wifi_level;
        m_wifi_rssi = wifi_rssi;
        m_network = network;
        m_internet = internet;
        m_airplane = airplane;
        emit networkChanged();
    }

    const int vol_max = qMax(1, o.value(QStringLiteral("volumeMax")).toInt(1));
    const qreal volume = qBound(0.0, o.value(QStringLiteral("volume")).toInt() / static_cast<qreal>(vol_max), 1.0);
    const bool muted = o.value(QStringLiteral("muted")).toBool();
    const QString output = o.value(QStringLiteral("output")).toString(QStringLiteral("speaker"));
    if (!qFuzzyCompare(volume + 1, m_volume + 1) || muted != m_muted || output != m_output) {
        m_volume = volume;
        m_muted = muted;
        m_output = output;
        emit soundChanged();
    }

    const double temp_raw = o.value(QStringLiteral("batteryTemp")).toDouble(-1000);
    const qreal temp = temp_raw <= -999 ? NAN : temp_raw;
    const QString source = o.value(QStringLiteral("batterySource")).toString();
    const QString health = o.value(QStringLiteral("batteryHealth")).toString();
    const bool temp_same = (std::isnan(temp) && std::isnan(m_bat_temp)) || qFuzzyCompare(temp + 1, m_bat_temp + 1);
    if (!temp_same || source != m_bat_source || health != m_bat_health) {
        m_bat_temp = temp;
        m_bat_source = source;
        m_bat_health = health;
        emit batteryDetailsChanged();
    }

    const int thermal = o.value(QStringLiteral("thermal")).toInt();
    if (thermal != m_thermal) {
        m_thermal = thermal;
        emit thermalChanged();
    }

    const bool dark = o.value(QStringLiteral("dark")).toBool();
    const QString model = o.value(QStringLiteral("model")).toString();
    if (dark != m_dark || model != m_model) {
        m_dark = dark;
        m_model = model;
        emit systemChanged();
    }

    const QVariantList pads = o.value(QStringLiteral("gamepads")).toArray().toVariantList();
    if (pads != m_gamepads) {
        m_gamepads = pads;
        emit gamepadsChanged();
    }

    const bool bt_on = o.value(QStringLiteral("btOn")).toBool();
    const QVariantList bt_devices = o.value(QStringLiteral("btDevices")).toArray().toVariantList();
    if (bt_on != m_bt_on || bt_devices != m_bt_devices) {
        m_bt_on = bt_on;
        m_bt_devices = bt_devices;
        emit bluetoothChanged();
    }

    const bool h24 = o.value(QStringLiteral("h24")).toBool(true);
    if (h24 != m_24h) {
        m_24h = h24;
        emit clockChanged();
    }
    // the system's clock may have been set: line the minute up again
    tick_clock();
#endif
}
void DeviceInfo::poll_battery()
{
    BatteryInfo new_info = query_battery();

    const bool state_changed = m_battery.status != new_info.status;
    const bool value_changed = m_battery.seconds != new_info.seconds
        || !qFuzzyCompare(m_battery.percent, new_info.percent);

    m_battery = std::move(new_info);

    if (state_changed)
        emit batteryStatusChanged();
    if (value_changed)
        emit batteryLifeChanged();
}
} // namespace model
