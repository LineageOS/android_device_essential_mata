/*
 * Copyright (C) 2018-2019 The LineageOS Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * Also add information on how to contact you by electronic and paper mail.
 *
 */

package org.lineageos.settings.device;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.SystemProperties;
import android.preference.PreferenceManager;

import org.lineageos.settings.device.R;

public class BootCompletedReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        setTouchscreenGripRejection(context);
        setTouchscreenSmoothness(context);
    }

    private void setTouchscreenGripRejection(Context context) {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        boolean gripRejEnabled = prefs.getBoolean(Constants.GRIP_REJ_KEY, false);
        SystemProperties.set(Constants.GRIP_REJ_PROPERTY, gripRejEnabled ? "on" : "off");
    }

    private void setTouchscreenSmoothness(Context context) {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        int smoothness = prefs.getInt(Constants.SMOOTHNESS_KEY,
                context.getResources().getInteger(R.integer.smoothness_default));
        SystemProperties.set(Constants.SMOOTHNESS_PROPERTY, Integer.toString(smoothness));
    }

}
