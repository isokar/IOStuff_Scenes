#include "iostuff_actions.h"
#include "iostuff_config.h"
#include "iostuff_descriptor.h"
#include "iostuff_profile.h"
#include "scene_logic.h"

#include <stdio.h>
#include <string.h>

static int gFailed = 0;
static AqState gState;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);                   \
            gFailed++;                                                              \
        }                                                                           \
    } while (0)

static int near(float a, float b)
{
    float delta = a - b;
    if (delta < 0.0f) {
        delta = -delta;
    }
    return delta < 0.0001f;
}

static AqSchedule defaults()
{
    AqState state;
    aqStateDefaults(&state);
    return state.schedule;
}

static int testAction(const char *id, const char *value)
{
    return aqDispatch(&gState, id, value);
}

static int testState(const char *id, char *out, size_t cap)
{
    return aqWriteState(&gState, id, out, cap);
}

static void testCycle()
{
    AqSchedule schedule = defaults();
    int slot;
    int expect[8] = {0, 0, 1, 1, 1, 2, 2, 0};
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t cr;
    uint8_t cg;
    uint8_t cb;
    float mul;
    float cool;
    float amp;
    float desat;
    CHECK(near(aqDayIntensity(0, schedule), 0.0f));
    CHECK(near(aqDayIntensity(8 * 60, schedule), 0.0f));
    CHECK(near(aqDayIntensity(8 * 60 + 30, schedule), 0.5f));
    CHECK(near(aqDayIntensity(9 * 60, schedule), 0.98f));
    CHECK(near(aqDayIntensity(14 * 60, schedule), 1.0f));
    CHECK(near(aqDayIntensity(19 * 60, schedule), 1.0f));
    CHECK(near(aqDayIntensity(19 * 60 + 30, schedule), 0.5f));
    CHECK(near(aqDayIntensity(20 * 60, schedule), 0.0f));
    CHECK(strcmp(aqPhaseName(7 * 60, schedule), "Nuit") == 0);
    CHECK(strcmp(aqPhaseName(8 * 60 + 30, schedule), "Aube") == 0);
    CHECK(strcmp(aqPhaseName(12 * 60, schedule), "Jour") == 0);
    CHECK(strcmp(aqPhaseName(19 * 60 + 30, schedule), "Crepuscule") == 0);
    CHECK(strcmp(aqPhaseName(22 * 60, schedule), "Nuit") == 0);
    for (slot = 0; slot < 8; slot++) {
        CHECK(aqEffectiveWeather(AQ_WEATHER_AUTO, slot * 180) == expect[slot]);
        CHECK(aqEffectiveWeather(AQ_WEATHER_AUTO, slot * 180 + 179) == expect[slot]);
    }
    CHECK(aqEffectiveWeather(AQ_WEATHER_CLOUDY, 12 * 60) == AQ_WEATHER_CLOUDY);
    CHECK(strcmp(aqWeatherName(AQ_WEATHER_VEILED), "Voile") == 0);
    aqWeatherProfile(AQ_WEATHER_CLEAR, mul, cool, amp, desat);
    aqDaylightColor(14 * 60, schedule.dayStart, schedule.duskStart, mul, cool, desat, r, g, b);
    aqWeatherProfile(AQ_WEATHER_CLOUDY, mul, cool, amp, desat);
    aqDaylightColor(14 * 60, schedule.dayStart, schedule.duskStart, mul, cool, desat, cr, cg, cb);
    CHECK((int)cr + (int)cg + (int)cb < (int)r + (int)g + (int)b);
    aqFramePixel(3 * 60, 3 * 60, 0.0f, AQ_WEATHER_CLEAR, schedule, 0, 0, r, g, b);
    CHECK(r == 0 && g == 0 && b == 0);
    CHECK(aqLerpChannel(0, 255, AQ_LERP_STEP) == 2);
    CHECK(aqLerpChannel(10, 0, AQ_FADE_STEP) == 0);
    CHECK(aqLerpChannel(40, 0, AQ_FADE_STEP) == 30);
    CHECK(near(aqCloudFactor(1000, 0, 0.0f), 1.0f));
    CHECK(aqCloudFactor(1000, 0, 0.14f) != aqCloudFactor(1000, 97, 0.14f));
}

static void testSchedule()
{
    AqSchedule schedule = defaults();
    AqSchedule moved = aqApplySunrise(schedule, 6 * 60);
    CHECK(moved.dawnStart == 6 * 60);
    CHECK(moved.dayStart == 7 * 60);
    CHECK(moved.duskStart == 19 * 60);
    CHECK(moved.duskEnd == 20 * 60);
    moved = aqApplySunset(schedule, 23 * 60 + 30);
    CHECK(moved.duskEnd == 23 * 60 + 30);
    CHECK(moved.duskStart == 22 * 60 + 30);
    CHECK(moved.dawnStart == 8 * 60);
    schedule.dawnStart = 480;
    schedule.dayStart = 490;
    schedule.duskStart = 1140;
    schedule.duskEnd = 1200;
    moved = aqApplySunrise(schedule, 360);
    CHECK(moved.dawnStart == 360);
    CHECK(moved.dayStart == 420);
    schedule = defaults();
    schedule.duskStart = 680;
    schedule.duskEnd = 740;
    moved = aqApplySunrise(schedule, 630);
    CHECK(moved.dayStart == 679);
    CHECK(moved.dawnStart == 619);
}

static void testDispatch()
{
    aqStateDefaults(&gState);
    CHECK(aqDispatch(&gState, "mode", "on") == 1);
    CHECK(gState.mode == AQ_MODE_ON);
    CHECK(aqDispatch(&gState, "mode", "forced") == 0);
    CHECK(aqDispatch(&gState, "weather", "cloudy") == 1);
    CHECK(gState.weather == AQ_WEATHER_CLOUDY);
    CHECK(aqDispatch(&gState, "timezone", "tokyo") == 1);
    CHECK(gState.tzIndex == 5);
    CHECK(aqDispatch(&gState, "sunrise", "390") == 1);
    CHECK(gState.schedule.dawnStart == 390);
    CHECK(gState.schedule.dayStart == 450);
    CHECK(aqDispatch(&gState, "sunset", "1380") == 1);
    CHECK(gState.schedule.duskEnd == 1380);
    CHECK(gState.schedule.duskStart == 1320);
    CHECK(aqDispatch(&gState, "clock", "8") == 1);
    CHECK(gState.simHour == 8 && gState.simMinute == 30);
    CHECK(aqDispatch(&gState, "clock", "-1") == 1);
    CHECK(gState.simHour == -1 && gState.simMinute == 0);
    CHECK(aqDispatch(&gState, "clock", "24") == 0);
    CHECK(aqDispatch(&gState, "relay", "1") == 0);
}

static void testContract()
{
    char json[IOSTUFF_SCENE_JSON_MAX];
    char device[IOSTUFF_V2_JSON_MAX];
    char legacy[64];
    char error[32];
    int status = 0;
    const IostuffProfile *rgb = iostuff_profile_find("iostuff.board.rgb");
    static IOStuffScene scene = {
        "iostuff.aquarium",
        "0.1.0",
        "Aquarium",
        "0x0B",
        "1.0",
        0,
        0,
        0,
        kAqActions,
        kAqActionCount,
        testState,
        testAction,
        "Aquarium",
    };
    aqStateDefaults(&gState);
    CHECK(kAqActionCount == 6);
    CHECK(strcmp(kAqActions[0].id, "mode") == 0);
    CHECK(strcmp(kAqActions[1].id, "weather") == 0);
    CHECK(strcmp(kAqActions[2].id, "timezone") == 0);
    CHECK(strcmp(kAqActions[3].id, "sunrise") == 0);
    CHECK(kAqActions[3].rangeMin == 360 && kAqActions[3].rangeMax == 630 && kAqActions[3].rangeStep == 30);
    CHECK(strcmp(kAqActions[4].id, "sunset") == 0);
    CHECK(kAqActions[4].rangeMin == 1080 && kAqActions[4].rangeMax == 1410 && kAqActions[4].rangeStep == 30);
    CHECK(strcmp(kAqActions[5].id, "clock") == 0);
    CHECK(kAqActions[5].rangeMin == -1 && kAqActions[5].rangeMax == 23 && kAqActions[5].rangeStep == 1);
    CHECK(iostuff_build_runtime_scene(json, sizeof(json), &scene) == 1);
    CHECK(strlen(json) < IOSTUFF_SCENE_JSON_MAX);
    CHECK(strstr(json, "\"id\":\"iostuff.aquarium\"") != 0);
    CHECK(strstr(json, "\"name\":\"Aquarium\"") != 0);
    CHECK(strstr(json, "\"id\":\"mode\"") != 0);
    CHECK(strstr(json, "\"id\":\"clock\"") != 0);
    CHECK(strstr(json, "\"min\":-1") != 0);
    CHECK(strstr(json, "\"state\":{") != 0);
    CHECK(strstr(json, "relay") == 0);
    CHECK(strstr(json, "digital-output") == 0);
    printf("RUNTIME %s\n", json);
    CHECK(iostuff_execute_action(&scene, "mode", "{\"value\":\"off\"}", &status, error, sizeof(error)) == 1);
    CHECK(status == 200 && gState.mode == AQ_MODE_OFF);
    CHECK(iostuff_execute_action(&scene, "sunrise", "{\"value\":361}", &status, error, sizeof(error)) == 1);
    CHECK(status == 400);
    CHECK(iostuff_execute_action(&scene, "sunrise", "{\"value\":360}", &status, error, sizeof(error)) == 1);
    CHECK(status == 200 && gState.schedule.dawnStart == 360);
    CHECK(iostuff_execute_action(&scene, "clock", "{\"value\":12}", &status, error, sizeof(error)) == 1);
    CHECK(status == 200 && gState.simHour == 12 && gState.simMinute == 30);
    CHECK(iostuff_execute_action(&scene, "missing", "{}", &status, error, sizeof(error)) == 1);
    CHECK(status == 404);
    CHECK(rgb != 0 && rgb->rgbStream.present == 1 && rgb->eth.present == 1);
    CHECK(iostuff_profile_cap(rgb, "relay") == 0);
    CHECK(iostuff_build_json(device, sizeof(device), "esp32-aabbccddeeff", "Aquarium", "0.1.0", "ignored",
                             rgb, 14, "iostuff.aquarium", "0.1.0") == 1);
    CHECK(strstr(device, "\"model\":\"iostuff.board.rgb\"") != 0);
    CHECK(strstr(device, "\"id\":\"strip\"") != 0);
    CHECK(strstr(device, "\"type\":\"led-strip\"") != 0);
    CHECK(strstr(device, "\"pin\":16") != 0);
    CHECK(strstr(device, "\"colorOrder\":\"GRB\"") != 0);
    CHECK(strstr(device, "\"protocol\":\"WS2812\"") != 0);
    CHECK(strstr(device, "\"id\":\"stream\"") != 0);
    CHECK(strstr(device, "\"type\":\"rgb-stream\"") != 0);
    CHECK(strstr(device, "\"format\":\"RGB888\"") != 0);
    CHECK(strstr(device, "\"port\":7777") != 0);
    CHECK(strstr(device, "\"pixelCount\":14") != 0);
    CHECK(strstr(device, "\"id\":\"iostuff.aquarium\"") != 0);
    CHECK(strstr(device, "digital-output") == 0);
    CHECK(strstr(device, "\"pin\":18") == 0);
    printf("DEVICE %s\n", device);
    CHECK(iostuff_legacy_reply(legacy, sizeof(legacy), 4660, "0x0B", "1.0") == 1);
    CHECK(strcmp(legacy, "4/4660/0x0B/1.0/77") == 0);
}

int main()
{
    testCycle();
    testSchedule();
    testDispatch();
    testContract();
    if (gFailed != 0) {
        printf("%d failed\n", gFailed);
        return 1;
    }
    printf("ok\n");
    return 0;
}
