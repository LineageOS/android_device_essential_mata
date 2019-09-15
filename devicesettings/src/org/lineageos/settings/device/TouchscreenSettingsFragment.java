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

import android.app.ActionBar;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.SystemProperties;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.MenuItem;

import androidx.preference.PreferenceFragment;

import org.lineageos.settings.device.R;
import org.lineageos.settings.device.widget.SeekBarPreference;

public class TouchscreenSettingsFragment extends PreferenceFragment
        implements SharedPreferences.OnSharedPreferenceChangeListener {

    private static final String TAG = TouchscreenSettingsFragment.class.getSimpleName();

    private final boolean DEBUG = false;

    private SeekBarPreference mSmoothnessSeekBar;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        addPreferencesFromResource(R.xml.touchscreen_panel);
        final ActionBar actionBar = getActivity().getActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);

        mSmoothnessSeekBar = (SeekBarPreference) findPreference(Constants.SMOOTHNESS_KEY);

        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getContext());
        doUpdateGripRejPreference(prefs);
        doUpdateSmoothnessPreference(prefs);
    }

    @Override
    public void onResume() {
        super.onResume();
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getContext());
        prefs.registerOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onPause() {
        super.onPause();
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getContext());
        prefs.unregisterOnSharedPreferenceChangeListener(this);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            getActivity().onBackPressed();
            return true;
        }
        return false;
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPrefs, String key) {
        if (DEBUG) Log.d(TAG, "onSharedPreferenceChanged: " + key);

        if (Constants.GRIP_REJ_KEY.equals(key)) {
            doUpdateGripRejPreference(sharedPrefs);
        } else if (Constants.SMOOTHNESS_KEY.equals(key)) {
            doUpdateSmoothnessPreference(sharedPrefs);
        }
    }

    private void doUpdateGripRejPreference(SharedPreferences sharedPrefs) {
        boolean gripRejEnabled = sharedPrefs.getBoolean(Constants.GRIP_REJ_KEY, false);
        if (DEBUG) Log.d(TAG, "doUpdateGripRejPreference: " + (gripRejEnabled ? "on" : "off"));
        SystemProperties.set(Constants.GRIP_REJ_PROPERTY, gripRejEnabled ? "on" : "off");
    }

    private void doUpdateSmoothnessPreference(SharedPreferences sharedPrefs) {
        int smoothness = sharedPrefs.getInt(Constants.SMOOTHNESS_KEY, 0);
        if (DEBUG) Log.d(TAG, "doUpdateSmoothnessPreference: " + Integer.toString(smoothness));
        SystemProperties.set(Constants.SMOOTHNESS_PROPERTY, Integer.toString(smoothness));
        mSmoothnessSeekBar.setSummary(getString(R.string.touchscreen_smoothness_summary, smoothness));
    }
}
