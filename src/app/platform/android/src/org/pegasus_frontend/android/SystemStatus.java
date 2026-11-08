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

import android.content.BroadcastReceiver;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Configuration;
import android.hardware.input.InputManager;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.provider.Settings;
import android.text.format.DateFormat;
import android.view.InputDevice;

import org.json.JSONArray;
import org.json.JSONObject;


// Pegasus X: what Android says about the device, for themes (api.device):
// Wi-Fi and the network, volume and where the sound goes, the battery's
// temperature and source, how hot the device runs, dark mode, the clock's
// 12/24-hour setting, and the controllers (and their batteries, Android 12+).
//
// Android tells us when any of it changes (broadcasts and callbacks, on the
// main thread); each change bumps `version`. Pegasus X reads version() now
// and then (a cheap call) and state() only when it moved. Nothing here needs
// a permission prompt (ACCESS_WIFI_STATE and ACCESS_NETWORK_STATE are normal
// ones); the Wi-Fi network's name isn't read (that needs location).
public class SystemStatus {
    private static Context s_ctx;
    private static boolean s_started = false;
    private static volatile int s_version = 0;
    private static final Handler s_main = new Handler(Looper.getMainLooper());

    // network
    private static boolean s_wifi_on = false;
    private static String s_network = "none";     // wifi, mobile, ethernet, other, none
    private static boolean s_internet = false;
    private static int s_wifi_rssi = -127;
    private static boolean s_airplane = false;
    // sound
    private static int s_volume = 0;
    private static int s_volume_max = 15;
    private static boolean s_muted = false;
    private static String s_output = "speaker";   // speaker, headphones, bluetooth, usb, hdmi
    // battery extras
    private static float s_bat_temp = Float.NaN;  // degrees C
    private static String s_bat_source = "";      // ac, usb, wireless, dock, "" (on battery)
    private static String s_bat_health = "";      // good, overheat, dead, cold, ...
    // the rest
    private static int s_thermal = 0;             // PowerManager.THERMAL_STATUS_*
    private static boolean s_dark = false;
    private static boolean s_24h = false;
    private static String s_gamepads = "[]";

    private static void changed() { s_version++; }

    public static int version() { return s_version; }

    public static synchronized void start(final Context context) {
        if (s_started || context == null)
            return;
        s_started = true;
        s_ctx = context.getApplicationContext();
        // everything Android calls back runs on the main thread
        s_main.post(new Runnable() { @Override public void run() { setup(); } });
    }

    private static void setup() {
        final Context ctx = s_ctx;

        // -- broadcasts: Wi-Fi on/off and signal, airplane mode, volume,
        //    battery, the clock's minute and its settings
        final IntentFilter filter = new IntentFilter();
        filter.addAction(WifiManager.WIFI_STATE_CHANGED_ACTION);
        filter.addAction(WifiManager.RSSI_CHANGED_ACTION);
        filter.addAction(Intent.ACTION_AIRPLANE_MODE_CHANGED);
        filter.addAction("android.media.VOLUME_CHANGED_ACTION");
        filter.addAction(AudioManager.ACTION_HEADSET_PLUG);
        filter.addAction(Intent.ACTION_BATTERY_CHANGED);
        filter.addAction(Intent.ACTION_TIME_TICK);
        filter.addAction(Intent.ACTION_TIME_CHANGED);
        filter.addAction(Intent.ACTION_TIMEZONE_CHANGED);
        final BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context c, Intent intent) {
                final String a = intent.getAction();
                if (a == null)
                    return;
                if (a.equals(Intent.ACTION_BATTERY_CHANGED)) {
                    readBattery(intent);
                }
                else if (a.equals(Intent.ACTION_TIME_TICK)) {
                    // once a minute: what has no event of its own
                    readGamepads();
                    readClock();
                }
                else if (a.equals(Intent.ACTION_TIME_CHANGED) || a.equals(Intent.ACTION_TIMEZONE_CHANGED)) {
                    readClock();
                    changed();  // the clock moved: Pegasus X lines its minute up again
                }
                else if (a.equals("android.media.VOLUME_CHANGED_ACTION") || a.equals(AudioManager.ACTION_HEADSET_PLUG)) {
                    readSound();
                }
                else {
                    readWifi();
                    readAirplane();
                }
            }
        };
        try {
            final Intent sticky = ctx.registerReceiver(receiver, filter);
            if (sticky != null && Intent.ACTION_BATTERY_CHANGED.equals(sticky.getAction()))
                readBattery(sticky);
        } catch (Exception e) {
            // some builds refuse one action or another: the rest still works
        }

        // -- the network in use: Wi-Fi / mobile / none, internet or not,
        //    and (Android 10+) Wi-Fi's signal as it changes
        try {
            final ConnectivityManager cm = (ConnectivityManager) ctx.getSystemService(Context.CONNECTIVITY_SERVICE);
            if (cm != null && Build.VERSION.SDK_INT >= 24) {
                // (called on Android's network thread: handed to the main one)
                cm.registerDefaultNetworkCallback(new ConnectivityManager.NetworkCallback() {
                    @Override
                    public void onCapabilitiesChanged(Network network, final NetworkCapabilities caps) {
                        s_main.post(new Runnable() { @Override public void run() { readNetwork(caps); } });
                    }
                    @Override
                    public void onLost(Network network) {
                        s_main.post(new Runnable() { @Override public void run() { readNetwork(null); } });
                    }
                });
            }
        } catch (Exception e) {
            // no network callbacks: the broadcasts above still tell Wi-Fi
        }

        // -- where the sound goes (headphones, Bluetooth, USB, HDMI)
        try {
            final AudioManager am = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);
            if (am != null && Build.VERSION.SDK_INT >= 23) {
                am.registerAudioDeviceCallback(new AudioDeviceCallback() {
                    @Override public void onAudioDevicesAdded(AudioDeviceInfo[] added) { readSound(); }
                    @Override public void onAudioDevicesRemoved(AudioDeviceInfo[] removed) { readSound(); }
                }, s_main);
            }
        } catch (Exception e) {
            // the volume broadcast still works
        }

        // -- controllers coming and going
        try {
            final InputManager im = (InputManager) ctx.getSystemService(Context.INPUT_SERVICE);
            if (im != null) {
                im.registerInputDeviceListener(new InputManager.InputDeviceListener() {
                    @Override public void onInputDeviceAdded(int id) { readGamepads(); }
                    @Override public void onInputDeviceRemoved(int id) { readGamepads(); }
                    @Override public void onInputDeviceChanged(int id) { readGamepads(); }
                }, s_main);
            }
        } catch (Exception e) {
            // read once a minute instead
        }

        // -- how hot it runs (Android 10+)
        try {
            final PowerManager pm = (PowerManager) ctx.getSystemService(Context.POWER_SERVICE);
            if (pm != null && Build.VERSION.SDK_INT >= 29) {
                s_thermal = pm.getCurrentThermalStatus();
                pm.addThermalStatusListener(new PowerManager.OnThermalStatusChangedListener() {
                    @Override public void onThermalStatusChanged(int status) {
                        if (status != s_thermal) { s_thermal = status; changed(); }
                    }
                });
            }
        } catch (Exception e) {
            // unknown: stays 0
        }

        // -- dark mode changes
        ctx.registerComponentCallbacks(new ComponentCallbacks() {
            @Override public void onConfigurationChanged(Configuration config) { readDark(config); }
            @Override public void onLowMemory() {}
        });

        readWifi();
        readAirplane();
        readSound();
        readGamepads();
        readClock();
        readDark(ctx.getResources().getConfiguration());
        changed();
    }

    // -- readers (main thread) ------------------------------------------------------

    private static synchronized void readNetwork(NetworkCapabilities caps) {
        String net = "none";
        boolean internet = false;
        if (caps != null) {
            if (caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) net = "wifi";
            else if (caps.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)) net = "mobile";
            else if (caps.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)) net = "ethernet";
            else net = "other";
            internet = caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                && (Build.VERSION.SDK_INT < 23 || caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED));
            if (net.equals("wifi") && Build.VERSION.SDK_INT >= 29) {
                final int rssi = caps.getSignalStrength();
                if (rssi != NetworkCapabilities.SIGNAL_STRENGTH_UNSPECIFIED && rssi > -127)
                    s_wifi_rssi = rssi;
            }
        }
        if (!net.equals(s_network) || internet != s_internet) {
            s_network = net;
            s_internet = internet;
        }
        readWifi();
        changed();
    }

    @SuppressWarnings("deprecation")
    private static synchronized void readWifi() {
        try {
            final WifiManager wm = (WifiManager) s_ctx.getSystemService(Context.WIFI_SERVICE);
            if (wm == null)
                return;
            final boolean on = wm.isWifiEnabled();
            int rssi = s_wifi_rssi;
            if (on) {
                final WifiInfo info = wm.getConnectionInfo();
                if (info != null && info.getNetworkId() != -1 && info.getRssi() > -127)
                    rssi = info.getRssi();
            }
            if (!on)
                rssi = -127;
            if (on != s_wifi_on || rssi != s_wifi_rssi) {
                s_wifi_on = on;
                s_wifi_rssi = rssi;
                changed();
            }
        } catch (Exception e) {
            // no Wi-Fi on this device
        }
    }

    private static synchronized void readAirplane() {
        try {
            final boolean on = Settings.Global.getInt(s_ctx.getContentResolver(), Settings.Global.AIRPLANE_MODE_ON, 0) != 0;
            if (on != s_airplane) { s_airplane = on; changed(); }
        } catch (Exception e) {
            // unknown
        }
    }

    private static synchronized void readSound() {
        try {
            final AudioManager am = (AudioManager) s_ctx.getSystemService(Context.AUDIO_SERVICE);
            if (am == null)
                return;
            final int vol = am.getStreamVolume(AudioManager.STREAM_MUSIC);
            final int max = Math.max(1, am.getStreamMaxVolume(AudioManager.STREAM_MUSIC));
            final boolean muted = Build.VERSION.SDK_INT >= 23 ? am.isStreamMute(AudioManager.STREAM_MUSIC) : vol == 0;
            String out = "speaker";
            if (Build.VERSION.SDK_INT >= 23) {
                for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
                    final int t = d.getType();
                    if (t == AudioDeviceInfo.TYPE_WIRED_HEADPHONES || t == AudioDeviceInfo.TYPE_WIRED_HEADSET) { out = "headphones"; break; }
                    if (t == AudioDeviceInfo.TYPE_BLUETOOTH_A2DP || t == AudioDeviceInfo.TYPE_BLUETOOTH_SCO) out = "bluetooth";
                    else if ((t == AudioDeviceInfo.TYPE_USB_HEADSET || t == AudioDeviceInfo.TYPE_USB_DEVICE) && out.equals("speaker")) out = "usb";
                    else if (t == AudioDeviceInfo.TYPE_HDMI && out.equals("speaker")) out = "hdmi";
                }
            }
            else if (am.isWiredHeadsetOn()) {
                out = "headphones";
            }
            if (vol != s_volume || max != s_volume_max || muted != s_muted || !out.equals(s_output)) {
                s_volume = vol;
                s_volume_max = max;
                s_muted = muted;
                s_output = out;
                changed();
            }
        } catch (Exception e) {
            // unknown
        }
    }

    private static synchronized void readBattery(Intent intent) {
        final int t = intent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, Integer.MIN_VALUE);
        final float temp = t == Integer.MIN_VALUE ? Float.NaN : t / 10.f;
        final int plugged = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0);
        final String source = plugged == BatteryManager.BATTERY_PLUGGED_AC ? "ac"
            : plugged == BatteryManager.BATTERY_PLUGGED_USB ? "usb"
            : plugged == BatteryManager.BATTERY_PLUGGED_WIRELESS ? "wireless"
            : plugged != 0 ? "dock" : "";
        final int h = intent.getIntExtra(BatteryManager.EXTRA_HEALTH, 0);
        final String health = h == BatteryManager.BATTERY_HEALTH_GOOD ? "good"
            : h == BatteryManager.BATTERY_HEALTH_OVERHEAT ? "overheat"
            : h == BatteryManager.BATTERY_HEALTH_DEAD ? "dead"
            : h == BatteryManager.BATTERY_HEALTH_OVER_VOLTAGE ? "overvoltage"
            : h == BatteryManager.BATTERY_HEALTH_COLD ? "cold"
            : h == BatteryManager.BATTERY_HEALTH_UNSPECIFIED_FAILURE ? "failure" : "";
        final boolean temp_moved = Float.isNaN(temp) != Float.isNaN(s_bat_temp)
            || (!Float.isNaN(temp) && Math.abs(temp - s_bat_temp) >= 0.5f);
        if (temp_moved || !source.equals(s_bat_source) || !health.equals(s_bat_health)) {
            s_bat_temp = temp;
            s_bat_source = source;
            s_bat_health = health;
            changed();
        }
    }

    private static synchronized void readClock() {
        final boolean h24 = DateFormat.is24HourFormat(s_ctx);
        if (h24 != s_24h) { s_24h = h24; changed(); }
    }

    private static synchronized void readDark(Configuration config) {
        final boolean dark = (config.uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
        if (dark != s_dark) { s_dark = dark; changed(); }
    }

    private static synchronized void readGamepads() {
        try {
            final JSONArray list = new JSONArray();
            for (int id : InputDevice.getDeviceIds()) {
                final InputDevice dev = InputDevice.getDevice(id);
                if (dev == null || dev.isVirtual())
                    continue;
                final int src = dev.getSources();
                final boolean pad = (src & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                    || (src & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
                if (!pad)
                    continue;
                final JSONObject o = new JSONObject();
                o.put("name", dev.getName());
                o.put("builtIn", Build.VERSION.SDK_INT >= 29 && !dev.isExternal());
                int level = -1;
                boolean charging = false;
                // Android 12+ (the app is built with an older SDK: by reflection)
                if (Build.VERSION.SDK_INT >= 31) {
                    try {
                        final Object b = InputDevice.class.getMethod("getBatteryState").invoke(dev);
                        if (b != null) {
                            final Class<?> bc = b.getClass();
                            final boolean present = (Boolean) bc.getMethod("isPresent").invoke(b);
                            final float capacity = (Float) bc.getMethod("getCapacity").invoke(b);
                            final int status = (Integer) bc.getMethod("getStatus").invoke(b);
                            if (present && !Float.isNaN(capacity)) {
                                level = Math.round(capacity * 100);
                                charging = status == 2;  // BatteryState.STATUS_CHARGING
                            }
                        }
                    } catch (Exception e) {
                        // not told
                    }
                }
                o.put("battery", level);
                o.put("charging", charging);
                list.put(o);
            }
            final String json = list.toString();
            if (!json.equals(s_gamepads)) { s_gamepads = json; changed(); }
        } catch (Exception e) {
            // unknown
        }
    }

    // -- for Pegasus X ---------------------------------------------------------------

    @SuppressWarnings("deprecation")
    public static synchronized String state() {
        try {
            final JSONObject o = new JSONObject();
            o.put("wifiOn", s_wifi_on);
            o.put("network", s_network);
            o.put("internet", s_internet);
            o.put("wifiRssi", s_wifi_rssi);
            o.put("wifiLevel", s_wifi_rssi <= -127 || !s_network.equals("wifi") ? 0 : WifiManager.calculateSignalLevel(s_wifi_rssi, 5));
            o.put("airplane", s_airplane);
            o.put("volume", s_volume);
            o.put("volumeMax", s_volume_max);
            o.put("muted", s_muted);
            o.put("output", s_output);
            o.put("batteryTemp", Float.isNaN(s_bat_temp) ? -1000.0 : (double) s_bat_temp);
            o.put("batterySource", s_bat_source);
            o.put("batteryHealth", s_bat_health);
            o.put("thermal", s_thermal);
            o.put("dark", s_dark);
            o.put("h24", s_24h);
            o.put("model", (Build.MANUFACTURER + " " + Build.MODEL).trim());
            o.put("gamepads", new JSONArray(s_gamepads));
            return o.toString();
        } catch (Exception e) {
            return "{}";
        }
    }
}
