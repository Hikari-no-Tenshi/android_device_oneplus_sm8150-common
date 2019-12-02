/*
 * Copyright (C) 2019 The LineageOS Project
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

#define LOG_TAG "FingerprintInscreenService"

#include "FingerprintInscreen.h"
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <hidl/HidlTransportSupport.h>
#include <fstream>
#include <vector>
#include <stdlib.h>

#define FINGERPRINT_ACQUIRED_VENDOR 6
#define FINGERPRINT_ERROR_VENDOR 8

#define OP_ENABLE_FP_LONGPRESS 3
#define OP_DISABLE_FP_LONGPRESS 4
#define OP_RESUME_FP_ENROLL 8
#define OP_FINISH_FP_ENROLL 10

#define OP_DISPLAY_AOD_MODE 8
#define OP_DISPLAY_NOTIFY_PRESS 9
#define OP_DISPLAY_SET_DIM 10

// This is not a typo by me. It's by OnePlus.
#define HBM_ENABLE_PATH "/sys/class/drm/card0-DSI-1/op_friginer_print_hbm"
#define DIM_AMOUNT_PATH "/sys/class/drm/card0-DSI-1/dim_alpha"

namespace vendor {
namespace lineage {
namespace biometrics {
namespace fingerprint {
namespace inscreen {
namespace V1_0 {
namespace implementation {

bool isOnePlus7;

const std::vector<std::vector<int>> BRIGHTNESS_ALPHA_ARRAY = {
    std::vector<int>{0, 255},
    std::vector<int>{1, 241},
    std::vector<int>{2, 236},
    std::vector<int>{4, 235},
    std::vector<int>{5, 234},
    std::vector<int>{6, 232},
    std::vector<int>{10, 228},
    std::vector<int>{20, 220},
    std::vector<int>{30, 212},
    std::vector<int>{45, 204},
    std::vector<int>{70, 190},
    std::vector<int>{100, 179},
    std::vector<int>{150, 166},
    std::vector<int>{227, 144},
    std::vector<int>{300, 131},
    std::vector<int>{400, 112},
    std::vector<int>{500, 96},
    std::vector<int>{600, 83},
    std::vector<int>{800, 60},
    std::vector<int>{1023, 34},
    std::vector<int>{2000, 131}
};

using android::base::GetProperty;

/*
 * Write value to path and close file.
 */
template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    file << value;
}

template <typename T>
static T get(const std::string& path, const T& def) {
    std::ifstream file(path);
    T result;

    file >> result;
    return file.fail() ? def : result;
}

FingerprintInscreen::FingerprintInscreen() {
    this->mVendorFpService = IVendorFingerprintExtensions::getService();
    this->mVendorDisplayService = IOneplusDisplay::getService();
    std::string device = android::base::GetProperty("ro.product.device", "");
    isOnePlus7 = device == "OnePlus7";
}

Return<void> FingerprintInscreen::onStartEnroll() {
    this->mVendorFpService->updateStatus(OP_DISABLE_FP_LONGPRESS);
    this->mVendorFpService->updateStatus(OP_RESUME_FP_ENROLL);

    return Void();
}

Return<void> FingerprintInscreen::onFinishEnroll() {
    this->mVendorFpService->updateStatus(OP_FINISH_FP_ENROLL);

    return Void();
}

Return<void> FingerprintInscreen::onPress() {
    if (!isOnePlus7) {
        this->mVendorDisplayService->setMode(OP_DISPLAY_AOD_MODE, 2);
    }
    this->mVendorDisplayService->setMode(OP_DISPLAY_SET_DIM, 1);
    if (!isOnePlus7) {
        set(HBM_ENABLE_PATH, 1);
    }
    this->mVendorDisplayService->setMode(OP_DISPLAY_NOTIFY_PRESS, 1);

    return Void();
}

Return<void> FingerprintInscreen::onRelease() {
    if (!isOnePlus7) {
        this->mVendorDisplayService->setMode(OP_DISPLAY_AOD_MODE, 0);
    }
    this->mVendorDisplayService->setMode(OP_DISPLAY_SET_DIM, 0);
    if (!isOnePlus7) {
        set(HBM_ENABLE_PATH, 0);
    }
    this->mVendorDisplayService->setMode(OP_DISPLAY_NOTIFY_PRESS, 0);

    return Void();
}

Return<void> FingerprintInscreen::onShowFODView() {
    return Void();
}

Return<void> FingerprintInscreen::onHideFODView() {
    if (!isOnePlus7) {
        this->mVendorDisplayService->setMode(OP_DISPLAY_AOD_MODE, 0);
    }
    this->mVendorDisplayService->setMode(OP_DISPLAY_SET_DIM, 0);
    if (!isOnePlus7) {
        set(HBM_ENABLE_PATH, 0);
    }
    this->mVendorDisplayService->setMode(OP_DISPLAY_NOTIFY_PRESS, 0);

    return Void();
}

Return<bool> FingerprintInscreen::handleAcquired(int32_t acquiredInfo, int32_t vendorCode) {
    std::lock_guard<std::mutex> _lock(mCallbackLock);
    if (mCallback == nullptr) {
        return false;
    }

    if (acquiredInfo == FINGERPRINT_ACQUIRED_VENDOR) {
        if (vendorCode == 0) {
            Return<void> ret = mCallback->onFingerDown();
            if (!ret.isOk()) {
                LOG(ERROR) << "FingerDown() error: " << ret.description();
            }
            return true;
        }

        if (vendorCode == 1) {
            Return<void> ret = mCallback->onFingerUp();
            if (!ret.isOk()) {
                LOG(ERROR) << "FingerUp() error: " << ret.description();
            }
            return true;
        }
    }

    return false;
}

Return<bool> FingerprintInscreen::handleError(int32_t error, int32_t vendorCode) {
    return error == FINGERPRINT_ERROR_VENDOR && vendorCode == 6;
}

Return<void> FingerprintInscreen::setLongPressEnabled(bool enabled) {
    this->mVendorFpService->updateStatus(
            enabled ? OP_ENABLE_FP_LONGPRESS : OP_DISABLE_FP_LONGPRESS);

    return Void();
}

int interpolate(int x, int xa, int xb, int ya, int yb) {
    int sub = 0;
    int bf = (((yb - ya) * 2) * (x - xa)) / (xb - xa);
    int factor = bf / 2;
    int plus = bf % 2;
    if (!(xa - xb == 0 || yb - ya == 0)) {
        sub = (((2 * (x - xa)) * (x - xb)) / (yb - ya)) / (xa - xb);
    }
    return ya + factor + plus + sub;
}

int getDimAlpha(int brightness) {
    int level = BRIGHTNESS_ALPHA_ARRAY.size();
    int i = 0;
    while (i < level && BRIGHTNESS_ALPHA_ARRAY[i][0] < brightness) {
        i++;
    }
    if (i == 0) {
        return BRIGHTNESS_ALPHA_ARRAY[0][1];
    }
    if (i == level) {
        return BRIGHTNESS_ALPHA_ARRAY[level - 1][1];
    }
    return interpolate(brightness,
            BRIGHTNESS_ALPHA_ARRAY[i - 1][0],
            BRIGHTNESS_ALPHA_ARRAY[i][0],
            BRIGHTNESS_ALPHA_ARRAY[i - 1][1],
            BRIGHTNESS_ALPHA_ARRAY[i][1]);
}

Return<int32_t> FingerprintInscreen::getDimAmount(int32_t brightness) {
    int dimAmount = get(DIM_AMOUNT_PATH, 0);
    if (isOnePlus7) {
        int curBrightness = brightness * 4.011765;
        int val = getDimAlpha(curBrightness);
        float alpha = ((float) val) / 255.0f;
        float ratio = ((float) stof(android::base::GetProperty("persist.vendor.sys.fod.icon.dim", "90"))) / 100.0f;
        dimAmount = (alpha * ratio) * 255.0f;
    }
    LOG(INFO) << "dimAmount = " << dimAmount;

    return dimAmount;
}

Return<bool> FingerprintInscreen::shouldBoostBrightness() {
    if (!isOnePlus7) {
        return false;
    } else {
        return true;
    }
}

Return<void> FingerprintInscreen::setCallback(const sp<IFingerprintInscreenCallback>& callback) {
    {
        std::lock_guard<std::mutex> _lock(mCallbackLock);
        mCallback = callback;
    }

    return Void();
}

Return<int32_t> FingerprintInscreen::getPositionX() {
    return FOD_POS_X;
}

Return<int32_t> FingerprintInscreen::getPositionY() {
    return FOD_POS_Y;
}

Return<int32_t> FingerprintInscreen::getSize() {
    return FOD_SIZE;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace inscreen
}  // namespace fingerprint
}  // namespace biometrics
}  // namespace lineage
}  // namespace vendor
