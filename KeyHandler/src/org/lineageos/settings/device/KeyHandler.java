/*
 * Copyright (C) 2018 The LineageOS Project
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

import android.content.Context;
import android.media.AudioManager;
import android.os.Vibrator;
import android.util.Log;
import android.view.KeyEvent;

import java.util.Locale;

import com.android.internal.os.DeviceKeyHandler;

public class KeyHandler implements DeviceKeyHandler {
    private static final String TAG = KeyHandler.class.getSimpleName();

    // Slider key codes
    private static final int MODE_NORMAL = 601;
    private static final int MODE_VIBRATION = 602;
    private static final int MODE_SILENCE = 603;

    private static final String BLOCK_CALIBRATION_PATH = "/sys/bus/platform/devices/soc:tri_state_key/hall_data_calib";
    private static final String VENDOR_PERSIST_CALIBRATION_PATH = "/mnt/vendor/persist/engineermode/tri_state_hall_data";

    private final Context mContext;
    private final AudioManager mAudioManager;
    private final Vibrator mVibrator;

    public KeyHandler(Context context) {
        mContext = context;

        mAudioManager = mContext.getSystemService(AudioManager.class);
        mVibrator = mContext.getSystemService(Vibrator.class);

        String hallData = Utils.readLine(VENDOR_PERSIST_CALIBRATION_PATH);
        if (hallData != null) {
            try {
                String[] calibData = hallData.split(",|;");
                if (calibData != null) {
                    if (calibData.length == 6) {
                        String newCalibrationData = String.format(Locale.US, "%s,%s,%s,%s,%s,%s", new Object[]{calibData[0], calibData[1], calibData[2], calibData[3], calibData[4], calibData[5]});
                        if (newCalibrationData != null) {
                            Utils.writeValue(BLOCK_CALIBRATION_PATH, newCalibrationData);
                        }
                    }
                }
            } catch (Exception e) {
                Log.e(TAG, "failed to init hall data: " + e.getMessage());
            }
        }
    }

    public KeyEvent handleKeyEvent(KeyEvent event) {
        int scanCode = event.getScanCode();

        switch (scanCode) {
            case MODE_NORMAL:
                mAudioManager.setRingerModeInternal(AudioManager.RINGER_MODE_NORMAL);
                break;
            case MODE_VIBRATION:
                mAudioManager.setRingerModeInternal(AudioManager.RINGER_MODE_VIBRATE);
                break;
            case MODE_SILENCE:
                mAudioManager.setRingerModeInternal(AudioManager.RINGER_MODE_SILENT);
                break;
            default:
                return event;
        }
        doHapticFeedback();

        return null;
    }

    private void doHapticFeedback() {
        if (mVibrator == null || !mVibrator.hasVibrator()) {
            return;
        }

        mVibrator.vibrate(50);
    }
}
