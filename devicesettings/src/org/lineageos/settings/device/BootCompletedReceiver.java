/*
 * Copyright (C) 2018-2019 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
