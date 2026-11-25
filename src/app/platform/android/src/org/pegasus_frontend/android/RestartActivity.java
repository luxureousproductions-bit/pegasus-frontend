// Pegasus X
// Copyright (C) 2026  Esteban (MisterE88)
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

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;

import java.io.File;


// Pegasus X: starts Pegasus X again (a new App Data Folder, Start over).
// It runs in a process of its own (":restart" in the manifest), opened while
// Pegasus X is still on screen, so Android lets it open Pegasus X again once
// the old process is gone: it waits for that (up to 3 s, then ends it), opens
// Pegasus X, and closes.
public class RestartActivity extends Activity {
    static final String EXTRA_PID = "pid";

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        final int pid = getIntent().getIntExtra(EXTRA_PID, -1);
        if (pid > 0 && pid != android.os.Process.myPid()) {
            final long until = SystemClock.uptimeMillis() + 3000;
            while (new File("/proc/" + pid).exists() && SystemClock.uptimeMillis() < until)
                SystemClock.sleep(100);
            if (new File("/proc/" + pid).exists())
                android.os.Process.killProcess(pid);
        }
        try {
            Intent open = new Intent(this, MainActivity.class);
            open.setAction(Intent.ACTION_MAIN);
            open.addCategory(Intent.CATEGORY_LAUNCHER);
            open.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
            startActivity(open);
        }
        catch (Exception e) {
            // nothing else to try: Pegasus X is opened by hand
        }
        finish();
        Runtime.getRuntime().exit(0);
    }
}
