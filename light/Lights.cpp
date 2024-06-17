/*
 * SPDX-FileCopyrightText: 2018-2024 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "LightService"

#include "Lights.h"

#include <android-base/logging.h>

#include <fstream>

#define LEDS            "/sys/class/leds/"

#define LCD_LED         LEDS "lcd-backlight/"
#define RED_LED         LEDS "red/"
#define GREEN_LED       LEDS "green/"
#define BLUE_LED        LEDS "blue/"
#define RGB_LED         LEDS "rgb/"

#define BLINK           "blink"
#define BRIGHTNESS      "brightness"
#define DUTY_PCTS       "duty_pcts"
#define PAUSE_HI        "pause_hi"
#define PAUSE_LO        "pause_lo"
#define RAMP_STEP_MS    "ramp_step_ms"
#define RGB_BLINK       "rgb_blink"
#define START_IDX       "start_idx"


/*
 * 8 duty percent steps.
 */
#define RAMP_STEPS 8
/*
 * Each step will stay on for 50ms by default.
 */
#define RAMP_STEP_DURATION 50
/*
 * Each value represents a duty percent (0 - 100) for the led pwm.
 */
static int32_t BRIGHTNESS_RAMP[RAMP_STEPS] = {0, 12, 25, 37, 50, 72, 85, 100};

namespace {
/*
 * Write value to path and close file.
 */
static void set(std::string path, std::string value) {
    std::ofstream file(path);

    if (!file.is_open()) {
        LOG(WARNING) << "failed to write " << value.c_str() << " to " << path.c_str();
        return;
    }

    file << value;
}

static void set(std::string path, int value) {
    set(path, std::to_string(value));
}

static void handleBacklight(const HwLightState& state) {
    uint32_t brightness = state.color & 0xFF;
    set(LCD_LED BRIGHTNESS, brightness);
}

/*
 * Scale each value of the brightness ramp according to the
 * brightness of the color.
 */
static std::string getScaledRamp(uint32_t brightness) {
    std::string ramp, pad;

    for (auto const& step : BRIGHTNESS_RAMP) {
        ramp += pad + std::to_string(step * brightness / 0xFF);
        pad = ",";
    }

    return ramp;
}

static void handleNotification(const HwLightState& state) {
    uint32_t redBrightness, greenBrightness, blueBrightness, brightness;

    /*
     * Extract brightness from AARRGGBB.
     */
    redBrightness = (state.color >> 16) & 0xFF;
    greenBrightness = (state.color >> 8) & 0xFF;
    blueBrightness = state.color & 0xFF;

    brightness = (state.color >> 24) & 0xFF;

    /*
     * Scale RGB brightness if the Alpha brightness is within the proper range (0-255).
     */
    if (brightness > 0 && brightness < 255) {
        redBrightness = (redBrightness * brightness) / 0xFF;
        greenBrightness = (greenBrightness * brightness) / 0xFF;
        blueBrightness = (blueBrightness * brightness) / 0xFF;
    }

    /* Disable blinking. */
    set(RGB_LED RGB_BLINK, 0);

    if (state.flashMode == FlashMode::TIMED) {
        /*
         * If the flashOnMs duration is not long enough to fit ramping up
         * and down at the default step duration, step duration is modified
         * to fit.
         */
        int32_t stepDuration = RAMP_STEP_DURATION;
        int32_t pauseHi = state.flashOnMs - (stepDuration * RAMP_STEPS * 2);
        int32_t pauseLo = state.flashOffMs;

        if (pauseHi < 0) {
            stepDuration = state.flashOnMs / (RAMP_STEPS * 2);
            pauseHi = 0;
        }

        /* Red */
        set(RED_LED START_IDX, 0 * RAMP_STEPS);
        set(RED_LED DUTY_PCTS, getScaledRamp(redBrightness));
        set(RED_LED PAUSE_LO, pauseLo);
        set(RED_LED PAUSE_HI, pauseHi);
        set(RED_LED RAMP_STEP_MS, stepDuration);

        /* Green */
        set(GREEN_LED START_IDX, 1 * RAMP_STEPS);
        set(GREEN_LED DUTY_PCTS, getScaledRamp(greenBrightness));
        set(GREEN_LED PAUSE_LO, pauseLo);
        set(GREEN_LED PAUSE_HI, pauseHi);
        set(GREEN_LED RAMP_STEP_MS, stepDuration);

        /* Blue */
        set(BLUE_LED START_IDX, 2 * RAMP_STEPS);
        set(BLUE_LED DUTY_PCTS, getScaledRamp(blueBrightness));
        set(BLUE_LED PAUSE_LO, pauseLo);
        set(BLUE_LED PAUSE_HI, pauseHi);
        set(BLUE_LED RAMP_STEP_MS, stepDuration);

        /* Enable blinking. */
        set(RGB_LED RGB_BLINK, 1);
    } else {
        set(RED_LED BRIGHTNESS, redBrightness);
        set(GREEN_LED BRIGHTNESS, greenBrightness);
        set(BLUE_LED BRIGHTNESS, blueBrightness);
    }
}

static inline bool isLit(const HwLightState& state) {
    return state.color & 0x00ffffff;
}

/* Keep sorted in the order of importance. */
static std::vector<LightBackend> backends = {
    { LightType::ATTENTION, handleNotification },
    { LightType::NOTIFICATIONS, handleNotification },
    { LightType::BATTERY, handleNotification },
    { LightType::BACKLIGHT, handleBacklight },
};

}  // anonymous namespace

namespace aidl {
namespace android {
namespace hardware {
namespace light {

ndk::ScopedAStatus Lights::setLightState(int id, const HwLightState& state) {
    LightStateHandler handler = nullptr;
    LightType type = static_cast<LightType>(id);

    /* Lock global mutex until light state is updated. */
    std::lock_guard<std::mutex> lock(globalLock);

    /* Update the cached state value for the current type. */
    for (LightBackend& backend : backends) {
        if (backend.type == type) {
            backend.state = state;
            handler = backend.handler;
        }
    }

    /* If no handler has been found, then the type is not supported. */
    if (!handler) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    /* Light up the type with the highest priority that matches the current handler. */
    for (LightBackend& backend : backends) {
        if (handler == backend.handler && isLit(backend.state)) {
            handler(backend.state);
            return ndk::ScopedAStatus::ok();
        }
    }

    /* If no type has been lit up, then turn off the hardware. */
    handler(state);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Lights::getLights(std::vector<HwLight>* lights) {
    int i = 0;

    for (const LightBackend& backend : backends) {
        HwLight hwLight;
        hwLight.id = (int) backend.type;
        hwLight.type = backend.type;
        hwLight.ordinal = i;
        lights->push_back(hwLight);
        i++;
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace light
}  // namespace hardware
}  // namespace android
}  // namespace aidl
