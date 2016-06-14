/*
 * Copyright (C) 2015-2016 The CyanogenMod Project
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

package com.cyanogenmod.settings.device;

import android.app.ActionBar;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.SwitchPreference;
import android.provider.Settings;
import android.view.Menu;
import android.view.MenuItem;

import cyanogenmod.providers.CMSettings;

import org.cyanogenmod.internal.util.ScreenType;

public class TouchscreenGestureSettings extends PreferenceActivity {

    private static final String KEY_AMBIENT_DISPLAY_ENABLE = "ambient_display_enable";
    private static final String KEY_GESTURE_HAND_WAVE = "gesture_hand_wave";
    private static final String KEY_GESTURE_PICK_UP = "gesture_pick_up";
    private static final String KEY_GESTURE_POCKET = "gesture_pocket";
    private static final String KEY_HAPTIC_FEEDBACK = "touchscreen_gesture_haptic_feedback";

    private Context mContext;
    private Handler mGestureHandler = new Handler();

    private SwitchPreference mAmbientDisplayPreference;
    private SwitchPreference mHandwavePreference;
    private SwitchPreference mHapticFeedback;
    private SwitchPreference mPickupPreference;
    private SwitchPreference mPocketPreference;

    private SwitchPreference mCGesturePreference;
    private SwitchPreference mEGesturePreference;
    private SwitchPreference mSGesturePreference;
    private SwitchPreference mVGesturePreference;
    private SwitchPreference mWGesturePreference;
    private SwitchPreference mZGesturePreference;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.touchscreen_panel);
        boolean dozeEnabled = isDozeEnabled();
        mAmbientDisplayPreference = (SwitchPreference) findPreference(KEY_AMBIENT_DISPLAY_ENABLE);
        mAmbientDisplayPreference.setChecked(dozeEnabled);
        mAmbientDisplayPreference.setOnPreferenceChangeListener(mAmbientDisplayPrefListener);
        mHandwavePreference = (SwitchPreference) findPreference(KEY_GESTURE_HAND_WAVE);
        mHandwavePreference.setEnabled(dozeEnabled);
        mPickupPreference = (SwitchPreference) findPreference(KEY_GESTURE_PICK_UP);
        mPickupPreference.setEnabled(dozeEnabled);
        mPocketPreference = (SwitchPreference) findPreference(KEY_GESTURE_POCKET);
        mPocketPreference.setEnabled(dozeEnabled);
        mHapticFeedback = (SwitchPreference) findPreference(KEY_HAPTIC_FEEDBACK);
        mHapticFeedback.setOnPreferenceChangeListener(mHapticPrefListener);
        mCGesturePreference =
                (SwitchPreference) findPreference(CMActionsSettings.TOUCHSCREEN_C_GESTURE_KEY);
        mCGesturePreference.setOnPreferenceChangeListener(mGesturePrefListener);
        mEGesturePreference =
                (SwitchPreference) findPreference(CMActionsSettings.TOUCHSCREEN_E_GESTURE_KEY);
        mEGesturePreference.setOnPreferenceChangeListener(mGesturePrefListener);
        mSGesturePreference =
                (SwitchPreference) findPreference(CMActionsSettings.TOUCHSCREEN_S_GESTURE_KEY);
        mSGesturePreference.setOnPreferenceChangeListener(mGesturePrefListener);
        mVGesturePreference =
                (SwitchPreference) findPreference(CMActionsSettings.TOUCHSCREEN_V_GESTURE_KEY);
        mVGesturePreference.setOnPreferenceChangeListener(mGesturePrefListener);
        mWGesturePreference =
                (SwitchPreference) findPreference(CMActionsSettings.TOUCHSCREEN_W_GESTURE_KEY);
        mWGesturePreference.setOnPreferenceChangeListener(mGesturePrefListener);
        mZGesturePreference =
                (SwitchPreference) findPreference(CMActionsSettings.TOUCHSCREEN_Z_GESTURE_KEY);
        mZGesturePreference.setOnPreferenceChangeListener(mGesturePrefListener);

        final ActionBar actionBar = getActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);

        // If running on a phone, remove padding around the listview
        if (!ScreenType.isTablet(this)) {
            getListView().setPadding(0, 0, 0, 0);
        }

        mContext = getApplicationContext();
    }

    @Override
    protected void onResume() {
        super.onResume();
        mHapticFeedback.setChecked(CMSettings.System.getInt(getContentResolver(),
                CMSettings.System.TOUCHSCREEN_GESTURE_HAPTIC_FEEDBACK, 1) != 0);
        // If running on a phone, remove padding around the listview
        if (!ScreenType.isTablet(this)) {
            getListView().setPadding(0, 0, 0, 0);
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            onBackPressed();
            return true;
        }
        return false;
    }

    private boolean enableDoze(boolean enable) {
        return Settings.Secure.putInt(getContentResolver(),
                Settings.Secure.DOZE_ENABLED, enable ? 1 : 0);
    }

    private boolean isDozeEnabled() {
        return Settings.Secure.getInt(getContentResolver(),
                Settings.Secure.DOZE_ENABLED, 1) != 0;
    }

    private Preference.OnPreferenceChangeListener mAmbientDisplayPrefListener =
        new Preference.OnPreferenceChangeListener() {
        @Override
        public boolean onPreferenceChange(Preference preference, Object newValue) {
            boolean enable = (boolean) newValue;
            boolean ret = enableDoze(enable);
            if (ret) {
                mHandwavePreference.setEnabled(enable);
                mPickupPreference.setEnabled(enable);
                mPocketPreference.setEnabled(enable);
            }
            return ret;
        }
    };

    private Preference.OnPreferenceChangeListener mHapticPrefListener =
        new Preference.OnPreferenceChangeListener() {
        @Override
        public boolean onPreferenceChange(Preference preference, Object newValue) {
            final String key = preference.getKey();
            if (KEY_HAPTIC_FEEDBACK.equals(key)) {
                final boolean value = (Boolean) newValue;
                CMSettings.System.putInt(getContentResolver(),
                        CMSettings.System.TOUCHSCREEN_GESTURE_HAPTIC_FEEDBACK, value ? 1 : 0);
                return true;
            }
            return false;
        }
    };

    private Preference.OnPreferenceChangeListener mGesturePrefListener =
        new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                mGestureHandler.postDelayed(mUpdateGestures, 500);
                return true;
            }
        };

    private final Runnable mUpdateGestures = new Runnable() {
        public void run(){
            try {
                CMActionsSettings.updateGestureMode(mContext);
            }
            catch (Exception e) {
                // Can't do much anyway
            }
        }
    };
}
