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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import java.io.File;


// Pegasus X: Android ran the new build after an update started from Pegasus X
// (Check for updates): Pegasus X is opened again. Android sends this only to
// the app that was updated, and only once the new build is in.
public class UpdateReceiver extends BroadcastReceiver {
    static final String MARKER = "pegasusx-update-restart";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null || !Intent.ACTION_MY_PACKAGE_REPLACED.equals(intent.getAction()))
            return;
        // only an update Pegasus X started itself (not one from a file manager or adb)
        final File marker = new File(context.getFilesDir(), MARKER);
        if (!marker.exists())
            return;
        marker.delete();
        try {
            Intent open = new Intent(context, MainActivity.class);
            open.setAction(Intent.ACTION_MAIN);
            open.addCategory(Intent.CATEGORY_LAUNCHER);
            open.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            context.startActivity(open);
        }
        catch (Exception e) {
            // Android didn't allow it (newer versions limit apps starting
            // themselves from the background): the installer's Open button,
            // or the home screen when Pegasus X is the home app, brings it back
        }
    }
}
