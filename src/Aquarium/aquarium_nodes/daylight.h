#pragma once

// Daylight cycle taken from the historical Aquarium firmware (6_scenes.ino, 4_prefs.ino).
// Times are minutes from midnight. No Arduino, no pixels, no storage.

#include <math.h>
#include <stdint.h>

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

enum { AQ_LERP_STEP = 2 };
enum { AQ_FADE_STEP = 10 };
enum { AQ_CLOUD_PERIOD_MS = 36000 };

enum { AQ_MODE_AUTO = 0, AQ_MODE_ON = 1, AQ_MODE_OFF = 2, AQ_MODE_COUNT = 3 };
enum { AQ_WEATHER_CLEAR = 0, AQ_WEATHER_VEILED = 1, AQ_WEATHER_CLOUDY = 2, AQ_WEATHER_AUTO = 3, AQ_WEATHER_COUNT = 4 };

struct AqSchedule {
    int dawnStart;
    int dayStart;
    int duskStart;
    int duskEnd;
};

inline uint8_t aqClampU8(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

inline uint8_t aqScaleChannel(uint8_t value, uint8_t scale)
{
    return (uint8_t)((uint16_t)value * scale / 255);
}

inline int aqLerpChannel(uint8_t current, uint8_t target, uint8_t step)
{
    int delta = (int)target - (int)current;
    if (delta > step) {
        return current + step;
    }
    if (delta < -step) {
        return current - step;
    }
    return target;
}

inline int aqClampMinute(int minuteOfDay)
{
    if (minuteOfDay < 0) {
        return 0;
    }
    if (minuteOfDay > 1439) {
        return 1439;
    }
    return minuteOfDay;
}

inline float aqSmoothstep01(float t)
{
    if (t <= 0.0f) {
        return 0.0f;
    }
    if (t >= 1.0f) {
        return 1.0f;
    }
    return t * t * (3.0f - 2.0f * t);
}

inline int aqPositiveSpan(int span)
{
    if (span < 1) {
        return 1;
    }
    return span;
}

inline float aqDayIntensity(int minuteOfDay, const AqSchedule &schedule)
{
    if (minuteOfDay < schedule.dawnStart || minuteOfDay >= schedule.duskEnd) {
        return 0.0f;
    }
    if (minuteOfDay < schedule.dayStart) {
        float t = (float)(minuteOfDay - schedule.dawnStart) / (float)aqPositiveSpan(schedule.dayStart - schedule.dawnStart);
        return aqSmoothstep01(t);
    }
    if (minuteOfDay >= schedule.duskStart) {
        float t = (float)(minuteOfDay - schedule.duskStart) / (float)aqPositiveSpan(schedule.duskEnd - schedule.duskStart);
        return 1.0f - aqSmoothstep01(t);
    }
    int mid = (schedule.dayStart + schedule.duskStart) / 2;
    float dist = fabsf((float)(minuteOfDay - mid)) / (float)aqPositiveSpan(schedule.duskStart - schedule.dayStart);
    return 0.96f + 0.04f * (1.0f - dist);
}

inline int aqEffectiveWeather(int weatherMode, int minuteOfDay)
{
    if (weatherMode >= 0 && weatherMode <= AQ_WEATHER_CLOUDY) {
        return weatherMode;
    }
    int slot = minuteOfDay / 180;
    unsigned int h = (unsigned int)(slot * 2654435761u + 0x9E3779B9u);
    return (int)(h % 3);
}

inline const char *aqWeatherName(int weather)
{
    switch (weather) {
    case AQ_WEATHER_VEILED:
        return "Voile";
    case AQ_WEATHER_CLOUDY:
        return "Nuageux";
    case AQ_WEATHER_CLEAR:
    default:
        return "Clair";
    }
}

inline const char *aqPhaseName(int minuteOfDay, const AqSchedule &schedule)
{
    if (minuteOfDay < schedule.dawnStart || minuteOfDay >= schedule.duskEnd) {
        return "Nuit";
    }
    if (minuteOfDay < schedule.dayStart) {
        return "Aube";
    }
    if (minuteOfDay < schedule.duskStart) {
        return "Jour";
    }
    return "Crepuscule";
}

inline void aqWeatherProfile(int weather, float &intensityMul, float &coolBias, float &cloudAmp, float &desat)
{
    switch (weather) {
    case AQ_WEATHER_VEILED:
        intensityMul = 0.78f;
        coolBias = 0.12f;
        cloudAmp = 0.10f;
        desat = 0.12f;
        break;
    case AQ_WEATHER_CLOUDY:
        intensityMul = 0.58f;
        coolBias = 0.22f;
        cloudAmp = 0.22f;
        desat = 0.22f;
        break;
    case AQ_WEATHER_CLEAR:
    default:
        intensityMul = 1.00f;
        coolBias = 0.0f;
        cloudAmp = 0.14f;
        desat = 0.0f;
        break;
    }
}

inline void aqDaylightColor(
    int minuteOfDay,
    int dayStartMin,
    int duskStartMin,
    float intensity,
    float coolBias,
    float desat,
    uint8_t &r,
    uint8_t &g,
    uint8_t &b)
{
    float warmR = 255.0f, warmG = 165.0f, warmB = 85.0f;
    float coolR = 240.0f, coolG = 250.0f, coolB = 235.0f;
    float coolMix = 0.0f;
    if (minuteOfDay >= dayStartMin && minuteOfDay < duskStartMin) {
        int mid = (dayStartMin + duskStartMin) / 2;
        float span = (float)(duskStartMin - dayStartMin) * 0.5f;
        coolMix = 1.0f - fabsf((float)(minuteOfDay - mid)) / span;
        coolMix = aqSmoothstep01(coolMix);
        coolMix *= 0.85f;
    } else if (intensity > 0.0f) {
        coolMix = 0.10f;
    }
    coolMix = coolMix + coolBias * (1.0f - coolMix);
    if (coolMix > 1.0f) {
        coolMix = 1.0f;
    }
    float rr = warmR + (coolR - warmR) * coolMix;
    float gg = warmG + (coolG - warmG) * coolMix;
    float bb = warmB + (coolB - warmB) * coolMix;
    if (desat > 0.001f) {
        float grey = 0.30f * rr + 0.55f * gg + 0.15f * bb;
        float greyR = grey * 0.96f;
        float greyG = grey * 0.98f;
        float greyB = grey * 1.02f;
        rr = rr + (greyR - rr) * desat;
        gg = gg + (greyG - gg) * desat;
        bb = bb + (greyB - bb) * desat;
    }
    uint8_t level = aqClampU8((int)(intensity * 255.0f));
    r = aqScaleChannel(aqClampU8((int)rr), level);
    g = aqScaleChannel(aqClampU8((int)gg), level);
    b = aqScaleChannel(aqClampU8((int)bb), level);
}

inline float aqCloudFactor(uint32_t nowMs, int pixelOffset, float amp)
{
    float t = (float)((nowMs + (uint32_t)pixelOffset) % (uint32_t)AQ_CLOUD_PERIOD_MS);
    float phase = t * 2.0f * PI / (float)AQ_CLOUD_PERIOD_MS;
    float slow = (float)((nowMs / 2u + (uint32_t)pixelOffset * 3u) % (uint32_t)(AQ_CLOUD_PERIOD_MS * 2));
    float phase2 = slow * 2.0f * PI / (float)(AQ_CLOUD_PERIOD_MS * 2);
    float wave = 0.55f * sinf(phase) + 0.45f * sinf(phase2);
    float half = amp * 0.5f;
    return (1.0f - half) + amp * (0.5f + 0.5f * wave);
}

// One pixel of UpdateDayCycle, before the temporal lerp.
inline void aqFramePixel(
    int minuteOfDay,
    int colorMinute,
    float baseIntensity,
    int weatherMode,
    const AqSchedule &schedule,
    int pixelIndex,
    uint32_t nowMs,
    uint8_t &r,
    uint8_t &g,
    uint8_t &b)
{
    float intensityMul;
    float coolBias;
    float cloudAmp;
    float desat;
    uint8_t baseR;
    uint8_t baseG;
    uint8_t baseB;
    float cloud;
    int weather;
    if (baseIntensity <= 0.001f) {
        r = 0;
        g = 0;
        b = 0;
        return;
    }
    weather = aqEffectiveWeather(weatherMode, minuteOfDay);
    aqWeatherProfile(weather, intensityMul, coolBias, cloudAmp, desat);
    aqDaylightColor(colorMinute, schedule.dayStart, schedule.duskStart, baseIntensity * intensityMul, coolBias, desat, baseR, baseG, baseB);
    cloud = aqCloudFactor(nowMs, pixelIndex * 97, cloudAmp);
    r = aqClampU8((int)(baseR * cloud));
    g = aqClampU8((int)(baseG * cloud));
    b = aqClampU8((int)(baseB * cloud));
}

inline AqSchedule aqApplySunrise(AqSchedule schedule, int sunriseMin)
{
    int rampUpMin = schedule.dayStart - schedule.dawnStart;
    if (rampUpMin < 15 || rampUpMin > 240) {
        rampUpMin = 60;
    }
    sunriseMin = aqClampMinute(sunriseMin);
    schedule.dawnStart = sunriseMin;
    schedule.dayStart = schedule.dawnStart + rampUpMin;
    if (schedule.dayStart >= schedule.duskStart) {
        schedule.dayStart = schedule.duskStart - 1;
        schedule.dawnStart = schedule.dayStart - rampUpMin;
        if (schedule.dawnStart < 0) {
            schedule.dawnStart = 0;
            schedule.dayStart = schedule.dawnStart + rampUpMin;
            if (schedule.dayStart >= schedule.duskStart) {
                schedule.dayStart = schedule.duskStart - 1;
            }
        }
    }
    return schedule;
}

inline AqSchedule aqApplySunset(AqSchedule schedule, int sunsetEndMin)
{
    int rampDownMin = schedule.duskEnd - schedule.duskStart;
    if (rampDownMin < 15 || rampDownMin > 240) {
        rampDownMin = 60;
    }
    sunsetEndMin = aqClampMinute(sunsetEndMin);
    schedule.duskEnd = sunsetEndMin;
    schedule.duskStart = schedule.duskEnd - rampDownMin;
    if (schedule.duskStart <= schedule.dayStart) {
        schedule.duskStart = schedule.dayStart + 1;
        schedule.duskEnd = schedule.duskStart + rampDownMin;
        if (schedule.duskEnd > 1439) {
            schedule.duskEnd = 1439;
            schedule.duskStart = schedule.duskEnd - rampDownMin;
            if (schedule.duskStart <= schedule.dayStart) {
                schedule.duskStart = schedule.dayStart + 1;
            }
        }
    }
    return schedule;
}
