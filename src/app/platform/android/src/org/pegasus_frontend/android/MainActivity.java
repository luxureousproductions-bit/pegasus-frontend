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


package org.pegasus_frontend.android;

import android.app.ActivityManager;
import android.content.Context;
import android.app.Activity;
import android.content.Intent;
import android.content.ComponentName;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.UriPermission;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Environment;
import android.os.SystemClock;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.provider.Settings;
import androidx.core.content.FileProvider;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;


public class MainActivity extends org.qtproject.qt5.android.bindings.QtActivity {
    private static Activity m_self;
    private static PackageManager m_pm;
    private static int m_icon_density;


    @Override
    protected void onStart() {
        super.onStart();
        m_self = this;
        m_pm = getPackageManager();

        ActivityManager am = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
        m_icon_density = am.getLauncherLargeIconDensity();

        // Pegasus X: keep the screen away from accessibility services. Many
        // handhelds run one for button mapping, and Android then draws its
        // focus frame around a text on screen and leaves it there (the "blue
        // box"); Pegasus has no screen reader support to lose.
        final android.view.View content = getWindow().getDecorView();
        content.setImportantForAccessibility(android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        if (Build.VERSION.SDK_INT >= 26 && !m_focus_listener_added) {
            m_focus_listener_added = true;
            noFocusHighlight(content);

            // Qt puts an invisible text box over a text field being typed in
            // (for the on-screen keyboard), and Android draws its focus
            // highlight on it, which stays after the field is gone: the
            // "blue box". No view of Pegasus gets that highlight.
            content.getViewTreeObserver().addOnGlobalFocusChangeListener(
                new android.view.ViewTreeObserver.OnGlobalFocusChangeListener() {
                    @Override
                    public void onGlobalFocusChanged(android.view.View oldFocus, android.view.View newFocus) {
                        noFocusHighlight(newFocus);
                    }
                });
        }
    }

    private boolean m_focus_listener_added = false;

    // Pegasus X: turns off Android's focus highlight on a view and its children
    private static void noFocusHighlight(android.view.View view) {
        if (Build.VERSION.SDK_INT < 26 || view == null)
            return;
        view.setDefaultFocusHighlightEnabled(false);
        if (view instanceof android.view.ViewGroup) {
            final android.view.ViewGroup group = (android.view.ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++)
                noFocusHighlight(group.getChildAt(i));
        }
    }


    public static App[] appList() {
        Intent intent = new Intent(Intent.ACTION_MAIN, null);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        List<ResolveInfo> activities = m_pm.queryIntentActivities(intent, 0);

        App[] entries = new App[activities.size()];
        for (int i = 0; i < activities.size(); i++)
            entries[i] = new App(m_pm, activities.get(i));

        return entries;
    }


    public static byte[] appIcon(String packageName) {
        Drawable drawable = null;
        try {
            // NOTE: while there is m_pm.getApplicationInfo(), unfortunately
            //       that returns low density images for most apps
            ApplicationInfo appinfo = m_pm.getApplicationInfo(packageName, 0);
            Resources resources = m_pm.getResourcesForApplication(appinfo);
            Intent launch_intent = m_pm.getLaunchIntentForPackage(packageName);
            ResolveInfo resolveinfo = m_pm.resolveActivity(launch_intent, 0);
            // NOTE: getDrawableForDensity() has changed in API 21-22
            drawable = resources.getDrawableForDensity(resolveinfo.activityInfo.getIconResource(), m_icon_density);
        }
        catch (Exception ex) { }
        if (drawable == null)
            drawable = m_pm.getDefaultActivityIcon();

        Bitmap bitmap = drawableToBitmap(drawable);
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);
        return stream.toByteArray();
    }


    private static Bitmap drawableToBitmap(Drawable drawable) {
        if (drawable instanceof BitmapDrawable) {
            // TODO: handle null
            return ((BitmapDrawable) drawable).getBitmap();
        }

        int w = Math.max(1, drawable.getIntrinsicWidth());
        int h = Math.max(1, drawable.getIntrinsicHeight());
        Bitmap bitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);

        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }


    public static String primaryStoragePath() {
        final File storage = Environment.getExternalStorageDirectory();
        return storage.getAbsolutePath();
    }


    public static String[] sdcardPaths() {
        // Functions with high API level dependencies:
        // - https://developer.android.com/reference/android/os/storage/StorageManager#getStorageVolumes()
        // - https://developer.android.com/reference/android/os/storage/StorageVolume#getDirectory()

        final StorageManager storage_man = (StorageManager) m_self.getSystemService(Context.STORAGE_SERVICE);

        List<StorageVolume> storage_vols = null;
        if (Build.VERSION.SDK_INT >= 24) {
            storage_vols = storage_man.getStorageVolumes();
        }
        else {
            try {
                final Method volumelist_getter = StorageManager.class.getMethod("getVolumeList");
                final StorageVolume[] storage_vols_arr = (StorageVolume[]) volumelist_getter.invoke(storage_man);
                storage_vols = Arrays.asList(storage_vols_arr);
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            } catch (InvocationTargetException e) {
                e.printStackTrace();
            } catch (NoSuchMethodException e) {
                e.printStackTrace();
            }
        }

        List<File> mount_points = new ArrayList<File>();
        if (Build.VERSION.SDK_INT >= 30) {
            for (StorageVolume sv : storage_vols)
                mount_points.add(sv.getDirectory());
        }
        else {
            try {
                final Method dir_getter = StorageVolume.class.getMethod("getPathFile");
                for (StorageVolume sv : storage_vols) {
                    final File mount_point = (File) dir_getter.invoke(sv);
                    mount_points.add(mount_point);
                }
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            } catch (InvocationTargetException e) {
                e.printStackTrace();
            } catch (NoSuchMethodException e) {
                e.printStackTrace();
            }
        }

        List<String> paths = new ArrayList<String>();
        for (File mp : mount_points) {
            if (mp != null)
                paths.add(mp.getAbsolutePath());
        }
        paths.add("/"); // Always add the root
        return paths.toArray(new String[paths.size()]);
    }


    // Blocks until removable storage (SD card) has finished mounting, or the
    // timeout passes. When Pegasus is the home app it starts right after boot,
    // often before the SD card is mounted; the storage roots and config dirs are
    // read once at startup, so anything on the card would be missed until the
    // app is restarted. Called from main() before the backend starts.
    //
    // Waits while a removable volume reports "checking"/"unmounted". Right after
    // boot it also waits for a card to appear at all, but only if one was seen
    // on a previous run, so devices without an SD card never wait.
    public static String waitForStorage(Context ctx, int timeoutMs) {
        if (Build.VERSION.SDK_INT < 24)
            return "skipped (API < 24)";

        final StorageManager sm = (StorageManager) ctx.getSystemService(Context.STORAGE_SERVICE);
        final SharedPreferences prefs = ctx.getSharedPreferences("pegasus_storage", Context.MODE_PRIVATE);
        final boolean expectCard = prefs.getBoolean("expect_removable", false);
        final boolean freshBoot = SystemClock.elapsedRealtime() < 120000;

        final long start = SystemClock.elapsedRealtime();
        final long deadline = start + timeoutMs;
        int mounted = 0;
        int pending = 0;
        int seen = 0;

        while (true) {
            mounted = 0;
            pending = 0;
            seen = 0;
            for (StorageVolume sv : sm.getStorageVolumes()) {
                if (!sv.isRemovable())
                    continue;
                seen++;
                final String state = sv.getState();
                if (Environment.MEDIA_MOUNTED.equals(state)
                    || Environment.MEDIA_MOUNTED_READ_ONLY.equals(state)) {
                    boolean readable = true;
                    if (Build.VERSION.SDK_INT >= 30) {
                        final File dir = sv.getDirectory();
                        readable = dir != null && dir.canRead();
                    }
                    if (readable)
                        mounted++;
                    else
                        pending++;
                }
                else if (Environment.MEDIA_CHECKING.equals(state)
                    || Environment.MEDIA_UNMOUNTED.equals(state)
                    || Environment.MEDIA_UNKNOWN.equals(state)) {
                    pending++;
                }
            }

            final boolean waitForCard = expectCard && freshBoot && mounted == 0;
            if (pending == 0 && !waitForCard)
                break;
            if (SystemClock.elapsedRealtime() >= deadline)
                break;

            try {
                Thread.sleep(250);
            } catch (InterruptedException e) {
                break;
            }
        }

        // Remember whether a card is normally present. A card that was taken
        // out for good (none listed at all) stops future waits.
        if (mounted > 0)
            prefs.edit().putBoolean("expect_removable", true).apply();
        else if (seen == 0)
            prefs.edit().putBoolean("expect_removable", false).apply();

        return "waited " + (SystemClock.elapsedRealtime() - start) + "ms"
            + " (removable: " + seen + " seen, " + mounted + " mounted, " + pending + " pending;"
            + " expected card: " + expectCard + ", fresh boot: " + freshBoot + ")";
    }


    // Package names of all installed apps, used by the ES-DE data source
    // to pick which of an emulator's known package names is present
    public static String[] installedPackages() {
        final List<ApplicationInfo> apps = m_pm.getInstalledApplications(0);
        String[] out = new String[apps.size()];
        for (int i = 0; i < apps.size(); i++)
            out[i] = apps.get(i).packageName;
        return out;
    }


    public static String[] grantedPaths() {
        List<String> paths = new ArrayList();
        for (UriPermission uriperm : m_self.getContentResolver().getPersistedUriPermissions()) {
            final Uri uri = uriperm.getUri();
            paths.add(uri.getPath());
        }
        return paths.toArray(new String[paths.size()]);
    }


    public static void rememberGrantedPath(Uri uri) {
        m_self
            .getContentResolver()
            .takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
    }


    public static boolean getAllStorageAccess() {
        if (Build.VERSION.SDK_INT < 30)
            return true;

        if (Environment.isExternalStorageManager())
            return true;

        Intent intent = new Intent();
        intent.setAction(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
        intent.setData(Uri.parse("package:" + m_self.getPackageName()));
        m_self.startActivity(intent);
        return false;
    }

    public static BatteryInfo queryBattery() {
        final IntentFilter ifilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        final Intent batIntent = m_self.registerReceiver(null, ifilter);

        final int batStatus = batIntent.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
        if (batStatus == BatteryManager.BATTERY_STATUS_UNKNOWN)
            return null;

        final boolean hasBattery = batIntent.getBooleanExtra(BatteryManager.EXTRA_PRESENT, true);
        final boolean batPlugged = batIntent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1) > 0;
        final boolean batCharged = batStatus == BatteryManager.BATTERY_STATUS_FULL;

        final int batLevel = batIntent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        final int batScale = batIntent.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
        final float batPercent = (batLevel >= 0 && batScale > 0)
            ? batLevel / (float) batScale
            : Float.NaN;

        return new BatteryInfo(hasBattery, batPlugged, batCharged, batPercent);
    }

    public static String launchAmCommand(String[] args_arr) {
        final LinkedList<String> args = new LinkedList(Arrays.asList(args_arr));
        if (args.isEmpty())
            return "No arguments provided to 'am'";

        final String am_command = args.pop().toLowerCase();
        if (!am_command.equals("start"))
            return "For 'am', only the 'start' command is supported at the moment, '" + am_command + "' is not";

        try {
            Intent intent = IntentHelper.parseIntentCommand(args);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            try {
                m_self.startActivity(intent);
            }
            catch (SecurityException e) {
                // Pegasus X: Android refuses to pass on read access to a URI
                // Pegasus itself has no grant for, eg. a storage document URI
                // (as used for most standalone emulators); those emulators use
                // their own folder access instead, so launch without the flag,
                // the same way ES-DE does
                intent.setFlags(intent.getFlags() & ~Intent.FLAG_GRANT_READ_URI_PERMISSION);
                m_self.startActivity(intent);
            }
        }
        catch (Exception e) {
            return e.toString() + ": " + e.getMessage();
        }

        return null;
    }

    // Pegasus X: for each "package/activity" entry, the component to launch
    // on this device: the entry itself if it exists; if only the package is
    // installed (some builds of an emulator name their game screen
    // differently), the screen of that app that opens files; or "" if the
    // app isn't installed. The result has the same length as the input.
    public static String[] resolveComponents(String[] components) {
        String[] out = new String[components.length];
        for (int i = 0; i < components.length; i++) {
            out[i] = "";
            final ComponentName cn = ComponentName.unflattenFromString(components[i]);
            if (cn == null)
                continue;
            try {
                m_pm.getActivityInfo(cn, 0);
                out[i] = components[i];
            }
            catch (PackageManager.NameNotFoundException e) {
                out[i] = findGameActivity(cn.getPackageName());
            }
        }
        return out;
    }

    // Pegasus X: the screen of an installed app that opens game files: one
    // that handles viewing a file, or else the app's own start screen.
    // Returns "package/activity", or "" if the app isn't installed.
    public static String findGameActivity(String packageName) {
        final String[] mimeTypes = { "application/octet-stream", "*/*" };
        final String[] schemes = { "content", "file" };
        for (String scheme : schemes) {
            for (String mime : mimeTypes) {
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setDataAndType(Uri.parse(scheme + "://pegasus/game.iso"), mime);
                intent.setPackage(packageName);
                final List<ResolveInfo> found = m_pm.queryIntentActivities(intent, 0);
                if (found != null && !found.isEmpty() && found.get(0).activityInfo != null)
                    return packageName + "/" + found.get(0).activityInfo.name;
            }
        }

        final Intent launch = m_pm.getLaunchIntentForPackage(packageName);
        if (launch != null && launch.getComponent() != null)
            return launch.getComponent().flattenToString();
        return "";
    }

    public static String toContentUri(String path) {
        final Uri uri = FileProvider.getUriForFile(
            m_self,
            m_self.getPackageName() + ".files",
            new File(path));
        return uri.toString();
    }
}
