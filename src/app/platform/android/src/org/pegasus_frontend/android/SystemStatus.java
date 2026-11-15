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

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
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

import java.util.LinkedHashMap;
import java.util.Map;


// Pegasus X: what Android says about the device, for themes (api.device):
// Wi-Fi and the network, volume and where the sound goes, the battery's
// temperature and source, how hot the device runs, dark mode, the clock's
// 12/24-hour setting, the controllers (and their batteries, Android 12+), and
// Bluetooth: on or off, and what's connected (name, kind, battery when told).
//
// Android tells us when any of it changes (broadcasts and callbacks, on the
// main thread); each change bumps `version`. Pegasus X reads version() now
// and then (a cheap call) and state() only when it moved. Nothing here needs
// a permission prompt (ACCESS_WIFI_STATE and ACCESS_NETWORK_STATE are normal
// ones; BLUETOOTH too, as the app targets Android 11: no "Nearby devices"
// prompt, even on Android 12+); the Wi-Fi network's name isn't read (that
// needs location).
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
    // Bluetooth: on/off, and the devices connected now, by address
    private static boolean s_bt_on = false;
    private static final Map<String, BluetoothDevice> s_bt_connected = new LinkedHashMap<>();
    private static final Map<String, Integer> s_bt_battery = new LinkedHashMap<>();
    private static String s_bt_devices = "[]";

    private static void changed() { s_version++; }

    // Pegasus X frees the theme's cached graphics and unused QML components
    // (FrontendLayer::releaseMemory); registered by FrontendLayer at startup
    private static native void nativeLowMemory();

    private static void lowMemory() {
        try {
            nativeLowMemory();
        } catch (UnsatisfiedLinkError e) {
            // not registered (yet): nothing to free
        }
    }

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
                    readBluetoothBatteries();
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

        // -- Bluetooth: on/off, devices connecting and leaving, their battery
        //    (that last one Android sends without documenting it; most do)
        final IntentFilter bt_filter = new IntentFilter();
        bt_filter.addAction(BluetoothAdapter.ACTION_STATE_CHANGED);
        bt_filter.addAction(BluetoothDevice.ACTION_ACL_CONNECTED);
        bt_filter.addAction(BluetoothDevice.ACTION_ACL_DISCONNECTED);
        bt_filter.addAction("android.bluetooth.device.action.BATTERY_LEVEL_CHANGED");
        try {
            ctx.registerReceiver(new BroadcastReceiver() {
                @Override
                public void onReceive(Context c, Intent intent) {
                    onBluetooth(intent);
                }
            }, bt_filter);
        } catch (Exception e) {
            // no Bluetooth here
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
            @Override public void onLowMemory() { lowMemory(); }
        });

        readWifi();
        readAirplane();
        readSound();
        readGamepads();
        readBluetoothStart();
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

    // -- Bluetooth -------------------------------------------------------------------

    private static synchronized void readBluetoothStart() {
        try {
            final BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
            if (adapter == null)
                return;
            s_bt_on = adapter.isEnabled();
            // what's connected already: the paired devices that say so
            // (BluetoothDevice.isConnected() isn't public: by reflection)
            if (s_bt_on) {
                for (BluetoothDevice d : adapter.getBondedDevices()) {
                    try {
                        final Object on = BluetoothDevice.class.getMethod("isConnected").invoke(d);
                        if (on instanceof Boolean && (Boolean) on)
                            s_bt_connected.put(d.getAddress(), d);
                    } catch (Exception e) {
                        // not told: it shows once it connects again
                    }
                }
            }
            readBluetoothBatteries();
            writeBluetooth();
        } catch (Exception e) {
            // no Bluetooth, or not allowed
        }
    }

    private static synchronized void onBluetooth(Intent intent) {
        try {
            final String a = intent.getAction();
            if (BluetoothAdapter.ACTION_STATE_CHANGED.equals(a)) {
                final int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_OFF);
                s_bt_on = state == BluetoothAdapter.STATE_ON;
                if (!s_bt_on) {
                    s_bt_connected.clear();
                    s_bt_battery.clear();
                }
            }
            else {
                final BluetoothDevice d = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                if (d == null)
                    return;
                if (BluetoothDevice.ACTION_ACL_CONNECTED.equals(a)) {
                    s_bt_connected.put(d.getAddress(), d);
                }
                else if (BluetoothDevice.ACTION_ACL_DISCONNECTED.equals(a)) {
                    s_bt_connected.remove(d.getAddress());
                    s_bt_battery.remove(d.getAddress());
                }
                else {
                    final int level = intent.getIntExtra("android.bluetooth.device.extra.BATTERY_LEVEL", -1);
                    s_bt_battery.put(d.getAddress(), level);
                }
            }
            writeBluetooth();
        } catch (Exception e) {
            // not allowed
        }
    }

    // a connected device's battery, when it tells (BluetoothDevice.getBatteryLevel() isn't public)
    private static synchronized void readBluetoothBatteries() {
        boolean moved = false;
        for (BluetoothDevice d : s_bt_connected.values()) {
            try {
                final Object level = BluetoothDevice.class.getMethod("getBatteryLevel").invoke(d);
                if (level instanceof Integer) {
                    final Integer old = s_bt_battery.put(d.getAddress(), (Integer) level);
                    if (old == null || !old.equals(level))
                        moved = true;
                }
            } catch (Exception e) {
                // not told
            }
        }
        if (moved)
            writeBluetooth();
    }

    private static String kindOf(BluetoothDevice d) {
        try {
            final BluetoothClass c = d.getBluetoothClass();
            if (c == null)
                return "other";
            switch (c.getMajorDeviceClass()) {
                case BluetoothClass.Device.Major.AUDIO_VIDEO: return "audio";
                case BluetoothClass.Device.Major.PHONE: return "phone";
                case BluetoothClass.Device.Major.COMPUTER: return "computer";
                case BluetoothClass.Device.Major.WEARABLE: return "wearable";
                case BluetoothClass.Device.Major.PERIPHERAL: {
                    final int dev = c.getDeviceClass();
                    if ((dev & 0x40) != 0) return "keyboard";   // keyboard bit
                    if ((dev & 0x80) != 0) return "mouse";      // pointing bit
                    return "controller";                        // joystick, gamepad, remote...
                }
                default: return "other";
            }
        } catch (Exception e) {
            return "other";
        }
    }

    private static synchronized void writeBluetooth() {
        try {
            final JSONArray list = new JSONArray();
            for (BluetoothDevice d : s_bt_connected.values()) {
                final JSONObject o = new JSONObject();
                String name = null;
                try { name = d.getName(); } catch (Exception e) { /* not allowed */ }
                o.put("name", name != null ? name : d.getAddress());
                o.put("kind", kindOf(d));
                final Integer level = s_bt_battery.get(d.getAddress());
                o.put("battery", level != null && level >= 0 && level <= 100 ? level : -1);
                list.put(o);
            }
            final String json = list.toString();
            if (!json.equals(s_bt_devices))
                s_bt_devices = json;
            changed();
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
            o.put("btOn", s_bt_on);
            o.put("btDevices", new JSONArray(s_bt_devices));
            return o.toString();
        } catch (Exception e) {
            return "{}";
        }
    }
}
