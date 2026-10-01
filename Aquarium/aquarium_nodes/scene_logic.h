#pragma once

// Scene state transitions shared by the firmware and the host tests.
// Persistence and the timezone environment stay in scene.cpp.

#include "daylight.h"
#include "scene_actions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AqState {
    int mode;
    int weather;
    int tzIndex;
    int simHour;
    int simMinute;
    AqSchedule schedule;
};

inline void aqStateDefaults(AqState *state)
{
    state->mode = AQ_MODE_AUTO;
    state->weather = AQ_WEATHER_CLEAR;
    state->tzIndex = 0;
    state->simHour = -1;
    state->simMinute = 0;
    state->schedule.dawnStart = 8 * 60;
    state->schedule.dayStart = 9 * 60;
    state->schedule.duskStart = 19 * 60;
    state->schedule.duskEnd = 20 * 60;
}

inline void aqClampState(AqState *state)
{
    if (state->mode < 0 || state->mode >= AQ_MODE_COUNT) {
        state->mode = AQ_MODE_AUTO;
    }
    if (state->weather < 0 || state->weather >= AQ_WEATHER_COUNT) {
        state->weather = AQ_WEATHER_CLEAR;
    }
    if (state->tzIndex < 0 || state->tzIndex >= AQ_TZ_COUNT) {
        state->tzIndex = 0;
    }
    if (state->simHour < -1 || state->simHour > 23) {
        state->simHour = -1;
    }
    if (state->simMinute < 0) {
        state->simMinute = 0;
    }
    if (state->simMinute > 59) {
        state->simMinute = 59;
    }
    state->schedule.dawnStart = aqClampMinute(state->schedule.dawnStart);
    state->schedule.dayStart = aqClampMinute(state->schedule.dayStart);
    state->schedule.duskStart = aqClampMinute(state->schedule.duskStart);
    state->schedule.duskEnd = aqClampMinute(state->schedule.duskEnd);
}

inline int aqOptionIndex(const IOStuffActionOption *options, int count, const char *id)
{
    int i;
    if (id == 0) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        if (strcmp(options[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

inline int aqParseInt(const char *text, int *out)
{
    char *end = 0;
    long value;
    if (text == 0 || text[0] == 0) {
        return 0;
    }
    value = strtol(text, &end, 10);
    if (end == text || *end != 0) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

inline int aqDispatch(AqState *state, const char *id, const char *value)
{
    int parsed = 0;
    int index;
    if (state == 0 || id == 0) {
        return 0;
    }
    if (strcmp(id, "mode") == 0) {
        index = aqOptionIndex(kAqModeOptions, AQ_MODE_COUNT, value);
        if (index < 0) {
            return 0;
        }
        state->mode = index;
        return 1;
    }
    if (strcmp(id, "weather") == 0) {
        index = aqOptionIndex(kAqWeatherOptions, AQ_WEATHER_COUNT, value);
        if (index < 0) {
            return 0;
        }
        state->weather = index;
        return 1;
    }
    if (strcmp(id, "timezone") == 0) {
        index = aqOptionIndex(kAqTimezoneOptions, AQ_TZ_COUNT, value);
        if (index < 0) {
            return 0;
        }
        state->tzIndex = index;
        return 1;
    }
    if (strcmp(id, "sunrise") == 0) {
        if (!aqParseInt(value, &parsed)) {
            return 0;
        }
        state->schedule = aqApplySunrise(state->schedule, parsed);
        return 1;
    }
    if (strcmp(id, "sunset") == 0) {
        if (!aqParseInt(value, &parsed)) {
            return 0;
        }
        state->schedule = aqApplySunset(state->schedule, parsed);
        return 1;
    }
    if (strcmp(id, "clock") == 0) {
        if (!aqParseInt(value, &parsed)) {
            return 0;
        }
        if (parsed < -1 || parsed > 23) {
            return 0;
        }
        if (parsed < 0) {
            state->simHour = -1;
            state->simMinute = 0;
        } else {
            state->simHour = parsed;
            state->simMinute = 30;
        }
        return 1;
    }
    return 0;
}

inline int aqWriteText(char *out, size_t cap, const char *text)
{
    size_t n;
    if (out == 0 || cap == 0 || text == 0) {
        return 0;
    }
    n = strlen(text);
    if (n + 1 > cap) {
        return 0;
    }
    memcpy(out, text, n + 1);
    return 1;
}

inline int aqWriteState(const AqState *state, const char *id, char *out, size_t cap)
{
    char number[16];
    if (state == 0 || id == 0) {
        return 0;
    }
    if (strcmp(id, "mode") == 0) {
        return aqWriteText(out, cap, kAqModeOptions[state->mode].id);
    }
    if (strcmp(id, "weather") == 0) {
        return aqWriteText(out, cap, kAqWeatherOptions[state->weather].id);
    }
    if (strcmp(id, "timezone") == 0) {
        return aqWriteText(out, cap, kAqTimezoneOptions[state->tzIndex].id);
    }
    if (strcmp(id, "sunrise") == 0) {
        snprintf(number, sizeof(number), "%d", state->schedule.dawnStart);
        return aqWriteText(out, cap, number);
    }
    if (strcmp(id, "sunset") == 0) {
        snprintf(number, sizeof(number), "%d", state->schedule.duskEnd);
        return aqWriteText(out, cap, number);
    }
    if (strcmp(id, "clock") == 0) {
        snprintf(number, sizeof(number), "%d", state->simHour);
        return aqWriteText(out, cap, number);
    }
    return 0;
}
