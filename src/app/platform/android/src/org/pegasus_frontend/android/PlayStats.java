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

import android.app.AppOpsManager;
import android.app.usage.UsageEvents;
import android.app.usage.UsageStatsManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Process;
import android.provider.Settings;


// Pegasus X: how long an emulator was really in front, from Android's usage
// history (needs the "Usage access" permission, given in Android's settings;
// without it Pegasus X counts the time until it's back in front).
public class PlayStats {
    private static Context context() {
        return MainActivity.context();
    }

    public static boolean hasUsageAccess() {
        final Context ctx = context();
        if (ctx == null)
            return false;
        try {
            final AppOpsManager ops = (AppOpsManager) ctx.getSystemService(Context.APP_OPS_SERVICE);
            final int mode = Build.VERSION.SDK_INT >= 29
                ? ops.unsafeCheckOpNoThrow(AppOpsManager.OPSTR_GET_USAGE_STATS, Process.myUid(), ctx.getPackageName())
                : ops.checkOpNoThrow(AppOpsManager.OPSTR_GET_USAGE_STATS, Process.myUid(), ctx.getPackageName());
            return mode == AppOpsManager.MODE_ALLOWED;
        } catch (Exception e) {
            return false;
        }
    }

    public static void openUsageAccess() {
        final Context ctx = context();
        if (ctx == null)
            return;
        try {
            final Intent intent = new Intent(Settings.ACTION_USAGE_ACCESS_SETTINGS);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ctx.startActivity(intent);
        } catch (Exception e) {
            // no such screen on this device: nothing to open
        }
    }

    // milliseconds the package's activities were in front between `from`
    // and `to` (wall clock ms); -1 without usage access
    public static long foregroundMillis(String pkg, long from, long to) {
        final Context ctx = context();
        if (ctx == null || !hasUsageAccess())
            return -1;
        try {
            final UsageStatsManager usm = (UsageStatsManager) ctx.getSystemService(Context.USAGE_STATS_SERVICE);
            // a little before, so an activity resumed just before `from` counts from `from`
            final UsageEvents events = usm.queryEvents(from - 5 * 60 * 1000, to);
            final UsageEvents.Event event = new UsageEvents.Event();
            int inFront = 0;       // resumed activities of the package
            long since = -1;       // when it came to the front
            long total = 0;
            while (events.hasNextEvent()) {
                events.getNextEvent(event);
                if (!pkg.equals(event.getPackageName()))
                    continue;
                final int type = event.getEventType();
                final long at = event.getTimeStamp();
                // MOVE_TO_FOREGROUND / ACTIVITY_RESUMED = 1, MOVE_TO_BACKGROUND / ACTIVITY_PAUSED = 2
                if (type == 1) {
                    if (inFront == 0)
                        since = at;
                    inFront++;
                }
                else if (type == 2 || type == 23 /* ACTIVITY_STOPPED */) {
                    if (inFront > 0)
                        inFront--;
                    if (inFront == 0 && since >= 0) {
                        total += Math.max(0, Math.min(at, to) - Math.max(since, from));
                        since = -1;
                    }
                }
            }
            if (inFront > 0 && since >= 0)
                total += Math.max(0, to - Math.max(since, from));
            return total;
        } catch (Exception e) {
            return -1;
        }
    }
}
