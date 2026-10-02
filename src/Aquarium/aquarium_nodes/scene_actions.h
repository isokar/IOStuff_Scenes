#pragma once

#include <IOStuffNode.h>

static const IOStuffActionOption kAqModeOptions[] = {
    {"auto", "Auto"},
    {"on", "Allumer"},
    {"off", "Éteindre"},
};

static const IOStuffActionOption kAqWeatherOptions[] = {
    {"clear", "Clair"},
    {"veiled", "Voilé"},
    {"cloudy", "Nuageux"},
    {"auto", "Auto"},
};

static const IOStuffActionOption kAqTimezoneOptions[] = {
    {"paris", "Europe/Paris"},
    {"london", "Europe/London"},
    {"utc", "UTC"},
    {"eastern", "US/Eastern"},
    {"pacific", "US/Pacific"},
    {"tokyo", "Asia/Tokyo"},
};

enum { AQ_TZ_COUNT = 6 };

static const IOStuffActionDef kAqActions[] = {
    {"mode", "Mode", IOSTUFF_ACTION_SELECT, 0, 0, 1, kAqModeOptions, 3},
    {"weather", "Météo", IOSTUFF_ACTION_SELECT, 0, 0, 1, kAqWeatherOptions, 4},
    {"timezone", "Fuseau", IOSTUFF_ACTION_SELECT, 0, 0, 1, kAqTimezoneOptions, AQ_TZ_COUNT},
    {"sunrise", "Lever", IOSTUFF_ACTION_RANGE, 360, 630, 30, 0, 0},
    {"sunset", "Coucher", IOSTUFF_ACTION_RANGE, 1080, 1410, 30, 0, 0},
    {"clock", "Heure simulée", IOSTUFF_ACTION_RANGE, -1, 23, 1, 0, 0},
};

enum { kAqActionCount = 6 };
