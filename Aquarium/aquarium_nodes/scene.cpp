#include "scene.h"

#include "scene_logic.h"

#include <IOStuffNode.h>
#include <iostuff_profile.h>

#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const unsigned long kTickMs = 45;
static const char *kPrefsNs = "aquarium";

struct TzOption {
    const char *label;
    const char *posix;
};

static const TzOption kTimezones[AQ_TZ_COUNT] = {
    {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"UTC", "UTC0"},
    {"US/Eastern", "EST5EDT,M3.2.0,M11.1.0"},
    {"US/Pacific", "PST8PDT,M3.2.0,M11.1.0"},
    {"Asia/Tokyo", "JST-9"},
};

const IOStuffActionDef sceneActions[kAqActionCount] = {
    kAqActions[0],
    kAqActions[1],
    kAqActions[2],
    kAqActions[3],
    kAqActions[4],
    kAqActions[5],
};

static_assert(kAqActionCount == SCENE_ACTION_COUNT, "Aquarium action count");
static_assert(sizeof(kAqActions) / sizeof(kAqActions[0]) == kAqActionCount, "Aquarium action table");

static AqState gState;
static uint8_t gR[IOSTUFF_RGB_PIXEL_MAX];
static uint8_t gG[IOSTUFF_RGB_PIXEL_MAX];
static uint8_t gB[IOSTUFF_RGB_PIXEL_MAX];
static bool gPixelsReady = false;
static bool gNtpStarted = false;
static unsigned long gLastTick = 0;

static int minuteOfDay();
static bool suffixMatches(const char *path, const char *prefix, int *out);

static int stripCount()
{
    int count = IOStuff.count("strip");
    if (count < 0) {
        return 0;
    }
    if (count > IOSTUFF_RGB_PIXEL_MAX) {
        return IOSTUFF_RGB_PIXEL_MAX;
    }
    return count;
}

static void initPixels()
{
    int i;
    int count;
    if (gPixelsReady) {
        return;
    }
    count = stripCount();
    for (i = 0; i < count; i++) {
        gR[i] = 0;
        gG[i] = 0;
        gB[i] = 0;
    }
    gPixelsReady = true;
}

static void resetAnim()
{
    gPixelsReady = false;
    initPixels();
}

static void showStrip()
{
    IOStuff.show("strip");
}

static void setPixelTarget(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t step)
{
    gR[index] = (uint8_t)aqLerpChannel(gR[index], r, step);
    gG[index] = (uint8_t)aqLerpChannel(gG[index], g, step);
    gB[index] = (uint8_t)aqLerpChannel(gB[index], b, step);
    IOStuff.setPixel("strip", index, gR[index], gG[index], gB[index]);
}

static bool fadeOff()
{
    int i;
    int count = stripCount();
    bool still = false;
    initPixels();
    for (i = 0; i < count; i++) {
        uint8_t r = (uint8_t)aqLerpChannel(gR[i], 0, AQ_FADE_STEP);
        uint8_t g = (uint8_t)aqLerpChannel(gG[i], 0, AQ_FADE_STEP);
        uint8_t b = (uint8_t)aqLerpChannel(gB[i], 0, AQ_FADE_STEP);
        if (r != 0 || g != 0 || b != 0) {
            still = true;
        }
        gR[i] = r;
        gG[i] = g;
        gB[i] = b;
        IOStuff.setPixel("strip", i, r, g, b);
    }
    showStrip();
    return still;
}

static void stopLeds()
{
    int i;
    int count = stripCount();
    for (i = 0; i < count; i++) {
        gR[i] = 0;
        gG[i] = 0;
        gB[i] = 0;
        IOStuff.setPixel("strip", i, 0, 0, 0);
    }
    showStrip();
}

static void updateDay(bool forceDay)
{
    int minute;
    int colorMinute;
    float base;
    int i;
    int count;
    initPixels();
    count = stripCount();
    minute = minuteOfDay();
    base = forceDay ? 1.0f : aqDayIntensity(minute, gState.schedule);
    colorMinute = forceDay ? (gState.schedule.dayStart + gState.schedule.duskStart) / 2 : minute;
    for (i = 0; i < count; i++) {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        aqFramePixel(minute, colorMinute, base, gState.weather, gState.schedule, i, millis(), r, g, b);
        setPixelTarget(i, r, g, b, AQ_LERP_STEP);
    }
    showStrip();
}

static int minuteOfDay()
{
    if (gState.simHour >= 0) {
        int hour = gState.simHour;
        int minute = gState.simMinute;
        if (hour > 23) {
            hour = 23;
        }
        if (minute < 0) {
            minute = 0;
        }
        if (minute > 59) {
            minute = 59;
        }
        return hour * 60 + minute;
    }
    time_t now = time(nullptr);
    struct tm localTm;
    localtime_r(&now, &localTm);
    int minutes = localTm.tm_hour * 60 + localTm.tm_min;
    minutes %= 1440;
    if (minutes < 0) {
        minutes += 1440;
    }
    return minutes;
}

static void tick()
{
    int minute;
    if (gState.mode == AQ_MODE_OFF) {
        if (!fadeOff()) {
            stopLeds();
        }
        return;
    }
    if (gState.mode == AQ_MODE_ON) {
        updateDay(true);
        return;
    }
    minute = minuteOfDay();
    if (minute >= gState.schedule.dawnStart && minute < gState.schedule.duskEnd) {
        updateDay(false);
        return;
    }
    if (!fadeOff()) {
        stopLeds();
    }
}

static void applyTimezone()
{
    int index = gState.tzIndex;
    if (index < 0 || index >= AQ_TZ_COUNT) {
        index = 0;
    }
    setenv("TZ", kTimezones[index].posix, 1);
    tzset();
    Serial.print("TZ: ");
    Serial.println(kTimezones[index].label);
}

static void startTimeSync()
{
    applyTimezone();
    if (gNtpStarted) {
        return;
    }
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    gNtpStarted = true;
    Serial.println("NTP: sync demandee");
}

static bool timeReady()
{
    if (gState.simHour >= 0) {
        return true;
    }
    return time(nullptr) > 1700000000;
}

static void readKey(Preferences &prefs, const char *key, int &dest)
{
    if (prefs.isKey(key)) {
        dest = prefs.getInt(key, dest);
    }
}

static void readState(Preferences &prefs, AqState *state)
{
    readKey(prefs, "lightRun", state->mode);
    readKey(prefs, "dawnStart", state->schedule.dawnStart);
    readKey(prefs, "dayStart", state->schedule.dayStart);
    readKey(prefs, "duskStart", state->schedule.duskStart);
    readKey(prefs, "duskEnd", state->schedule.duskEnd);
    readKey(prefs, "simHour", state->simHour);
    readKey(prefs, "simMin", state->simMinute);
    readKey(prefs, "tzIndex", state->tzIndex);
    readKey(prefs, "weather", state->weather);
}

static void saveState()
{
    Preferences prefs;
    if (!prefs.begin(kPrefsNs, false)) {
        return;
    }
    prefs.putInt("lightRun", gState.mode);
    prefs.putInt("dawnStart", gState.schedule.dawnStart);
    prefs.putInt("dayStart", gState.schedule.dayStart);
    prefs.putInt("duskStart", gState.schedule.duskStart);
    prefs.putInt("duskEnd", gState.schedule.duskEnd);
    prefs.putInt("simHour", gState.simHour);
    prefs.putInt("simMin", gState.simMinute);
    prefs.putInt("tzIndex", gState.tzIndex);
    prefs.putInt("weather", gState.weather);
    prefs.end();
}

static void loadState()
{
    Preferences own;
    bool migrated = false;
    aqStateDefaults(&gState);
    if (own.begin(kPrefsNs, true)) {
        if (own.isKey("lightRun")) {
            readState(own, &gState);
            migrated = true;
        }
        own.end();
    }
    if (!migrated) {
        Preferences legacy;
        if (legacy.begin("my-app", true)) {
            readState(legacy, &gState);
            legacy.end();
        }
        aqClampState(&gState);
        saveState();
        return;
    }
    aqClampState(&gState);
}

static void print2(int value)
{
    if (value < 0) {
        value = 0;
    }
    if (value < 10) {
        IOStuff.httpPrint("0");
    }
    IOStuff.httpPrint(value);
}

static void printCss()
{
    IOStuff.httpPrint("*{box-sizing:border-box;margin:0;padding:0}");
    IOStuff.httpPrint("html,body{min-height:100vh;font-family:'Segoe UI',Tahoma,sans-serif;color:#d8f0ea;background:linear-gradient(165deg,#06141a 0%,#0a2430 45%,#0d2a28 100%)}");
    IOStuff.httpPrint("body{display:flex;flex-direction:column;align-items:center;padding:20px 16px 32px;max-width:480px;margin:0 auto;text-align:center}");
    IOStuff.httpPrint("h1{font-size:1.55rem;font-weight:500;letter-spacing:.1em;text-transform:uppercase;color:#5ec8b0;margin-bottom:4px}");
    IOStuff.httpPrint(".sub{font-size:.85rem;color:#7aa8a0;letter-spacing:.04em;margin-bottom:24px}");
    IOStuff.httpPrint(".card{width:100%;background:rgba(8,28,34,.88);border:1px solid rgba(94,200,176,.28);border-radius:12px;padding:20px 18px;margin-bottom:20px;box-shadow:0 8px 32px rgba(0,0,0,.35)}");
    IOStuff.httpPrint(".status-label{font-size:.7rem;text-transform:uppercase;letter-spacing:.15em;color:#6a9088;margin-bottom:8px}");
    IOStuff.httpPrint(".status-text{font-size:1.25rem;color:#e8fff8;margin-bottom:4px}");
    IOStuff.httpPrint(".status-detail{font-size:.9rem;color:#9bc4ba}");
    IOStuff.httpPrint(".status-wait .status-text{color:#7aa8a0}");
    IOStuff.httpPrint(".status-active .status-text{color:#5ec8b0}");
    IOStuff.httpPrint(".status-glow .status-text{color:#f0c878}");
    IOStuff.httpPrint(".status-night .status-text{color:#6a8590}");
    IOStuff.httpPrint(".clock{font-size:1.6rem;font-variant-numeric:tabular-nums;color:#a8ddd0;margin-top:12px}");
    IOStuff.httpPrint(".clock span{font-size:.75rem;color:#6a9088;display:block;margin-top:2px}");
    IOStuff.httpPrint(".actions{display:flex;flex-direction:column;gap:10px;width:100%;margin-bottom:20px}");
    IOStuff.httpPrint("a{text-decoration:none}");
    IOStuff.httpPrint(".btn{display:block;width:100%;padding:16px 20px;border:none;border-radius:10px;font-size:1rem;letter-spacing:.06em;cursor:pointer}");
    IOStuff.httpPrint(".btn-muted{background:rgba(40,70,72,.8);color:#8ab0a8}");
    IOStuff.httpPrint("details{width:100%;background:rgba(6,22,28,.7);border:1px solid rgba(106,144,136,.25);border-radius:10px;padding:14px 16px;text-align:left;margin-bottom:12px}");
    IOStuff.httpPrint("summary{font-size:.8rem;text-transform:uppercase;letter-spacing:.1em;color:#6a9088;cursor:pointer;list-style:none}");
    IOStuff.httpPrint("summary::-webkit-details-marker{display:none}");
    IOStuff.httpPrint(".modes{width:100%;margin-bottom:16px;text-align:left}");
    IOStuff.httpPrint(".modes-title{font-size:.7rem;text-transform:uppercase;letter-spacing:.12em;color:#6a9088;margin-bottom:10px}");
    IOStuff.httpPrint(".mode-list{display:flex;flex-direction:column;gap:8px}");
    IOStuff.httpPrint(".mode-opt{display:block;padding:12px 14px;border-radius:8px;border:1px solid rgba(106,144,136,.25);background:rgba(6,22,28,.55);color:#9bc4ba;font-size:.85rem;line-height:1.35}");
    IOStuff.httpPrint(".mode-opt small{display:block;font-size:.75rem;color:#5a7870;margin-top:3px}");
    IOStuff.httpPrint(".mode-active{border-color:rgba(94,200,176,.55);background:rgba(94,200,176,.12);color:#5ec8b0}");
    IOStuff.httpPrint(".hour-grid{display:grid;grid-template-columns:repeat(6,1fr);gap:6px;margin-top:10px}");
    IOStuff.httpPrint(".hour-btn{display:block;padding:8px 0;border-radius:6px;border:1px solid rgba(106,144,136,.25);background:rgba(6,22,28,.55);color:#9bc4ba;font-size:.75rem;text-align:center}");
    IOStuff.httpPrint(".hour-active{border-color:rgba(240,200,120,.5);background:rgba(240,200,120,.12);color:#f0c878}");
    IOStuff.httpPrint(".hint{font-size:.75rem;color:#6a9088;margin:8px 0 4px;line-height:1.4}");
    IOStuff.httpPrint(".sys{display:inline-block;margin-top:8px;color:#5ec8b0;font-size:.8rem;letter-spacing:.06em}");
    IOStuff.httpPrint(".footer{margin-top:auto;padding-top:20px;font-size:.65rem;color:#3a5850;letter-spacing:.05em}");
}

static void modeLink(const char *href, bool active, const char *title, const char *detail)
{
    IOStuff.httpPrint("<a href=\"");
    IOStuff.httpPrint(href);
    IOStuff.httpPrint("\" class=\"mode-opt");
    if (active) {
        IOStuff.httpPrint(" mode-active");
    }
    IOStuff.httpPrint("\">");
    IOStuff.httpPrint(title);
    IOStuff.httpPrint("<small>");
    IOStuff.httpPrint(detail);
    IOStuff.httpPrint("</small></a>");
}

static void hourSlot(const char *prefix, int minute, bool active)
{
    int hour = minute / 60;
    int mm = minute % 60;
    IOStuff.httpPrint("<a class=\"hour-btn");
    if (active) {
        IOStuff.httpPrint(" hour-active");
    }
    IOStuff.httpPrint("\" href=\"");
    IOStuff.httpPrint(prefix);
    IOStuff.httpPrint(minute);
    IOStuff.httpPrint("\">");
    print2(hour);
    IOStuff.httpPrint(":");
    print2(mm);
    IOStuff.httpPrint("</a>");
}

static void renderPage()
{
    int minute = minuteOfDay();
    int hour = minute / 60;
    int mm = minute % 60;
    bool night = minute < gState.schedule.dawnStart || minute >= gState.schedule.duskEnd;
    int weather = aqEffectiveWeather(gState.weather, minute);
    int slotHour;
    int slotHalf;
    IOStuff.httpBegin(200, "text/html; charset=utf-8");
    IOStuff.httpPrint("<!DOCTYPE html><html lang=\"fr\"><head><meta charset=\"UTF-8\">");
    IOStuff.httpPrint("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    IOStuff.httpPrint("<meta http-equiv=\"refresh\" content=\"8\">");
    IOStuff.httpPrint("<link rel=\"icon\" href=\"data:,\"><title>Aquarium</title><style>");
    printCss();
    IOStuff.httpPrint("</style></head><body><h1>Aquarium</h1>");
    IOStuff.httpPrint("<p class=\"sub\">&Eacute;clairage circadien</p><div class=\"card ");
    if (gState.mode == AQ_MODE_OFF) {
        IOStuff.httpPrint("status-wait");
    } else if (gState.mode == AQ_MODE_ON) {
        IOStuff.httpPrint("status-glow");
    } else if (night) {
        IOStuff.httpPrint("status-night");
    } else {
        IOStuff.httpPrint("status-active");
    }
    IOStuff.httpPrint("\"><div class=\"status-label\">&Eacute;tat</div><div class=\"status-text\">");
    if (gState.mode == AQ_MODE_OFF) {
        IOStuff.httpPrint("&Eacute;teint");
    } else if (gState.mode == AQ_MODE_ON) {
        IOStuff.httpPrint("Jour forc&eacute;");
    } else {
        IOStuff.httpPrint(aqPhaseName(minute, gState.schedule));
    }
    IOStuff.httpPrint("</div><div class=\"status-detail\">");
    if (gState.mode == AQ_MODE_OFF) {
        IOStuff.httpPrint("LEDs coup&eacute;es manuellement");
    } else if (gState.mode == AQ_MODE_ON) {
        IOStuff.httpPrint("Pleine lumi&egrave;re, hors cycle");
    } else if (night) {
        IOStuff.httpPrint("Nuit &mdash; LEDs &eacute;teintes");
    } else {
        IOStuff.httpPrint("Cycle auto &middot; ");
        IOStuff.httpPrint(aqWeatherName(weather));
        if (gState.weather == AQ_WEATHER_AUTO) {
            IOStuff.httpPrint(" (auto)");
        }
    }
    IOStuff.httpPrint("</div><div class=\"clock\">");
    print2(hour);
    IOStuff.httpPrint(":");
    print2(mm);
    IOStuff.httpPrint("<span>");
    if (gState.simHour >= 0) {
        IOStuff.httpPrint("heure simul&eacute;e");
    } else if (timeReady()) {
        IOStuff.httpPrint("NTP &middot; ");
        IOStuff.httpPrint(kTimezones[gState.tzIndex].label);
    } else {
        IOStuff.httpPrint("attente sync NTP&hellip;");
    }
    IOStuff.httpPrint("</span></div></div>");
    IOStuff.httpPrint("<div class=\"modes\"><div class=\"modes-title\">Mode</div><div class=\"mode-list\">");
    modeLink("/mode/0", gState.mode == AQ_MODE_AUTO, "Auto", "Aube &rarr; jour &rarr; cr&eacute;puscule &rarr; nuit off");
    modeLink("/mode/1", gState.mode == AQ_MODE_ON, "Allumer", "Jour forc&eacute; maintenant");
    modeLink("/mode/2", gState.mode == AQ_MODE_OFF, "&Eacute;teindre", "Coupure manuelle");
    IOStuff.httpPrint("</div></div><div class=\"modes\"><div class=\"modes-title\">M&eacute;t&eacute;o</div><div class=\"mode-list\">");
    modeLink("/weather/0", gState.weather == AQ_WEATHER_CLEAR, "Clair", "Plein soleil, variations l&eacute;g&egrave;res");
    modeLink("/weather/1", gState.weather == AQ_WEATHER_VEILED, "Voil&eacute;", "Un peu moins lumineux, ciel gris&acirc;tre");
    modeLink("/weather/2", gState.weather == AQ_WEATHER_CLOUDY, "Nuageux", "Plus sombre, passages de nuages");
    modeLink("/weather/3", gState.weather == AQ_WEATHER_AUTO, "Auto", "Change lentement dans la journ&eacute;e");
    IOStuff.httpPrint("</div></div><details><summary>Fuseau horaire</summary><div class=\"mode-list\" style=\"margin-top:10px\">");
    for (slotHour = 0; slotHour < AQ_TZ_COUNT; slotHour++) {
        IOStuff.httpPrint("<a href=\"/tz/");
        IOStuff.httpPrint(slotHour);
        IOStuff.httpPrint("\" class=\"mode-opt");
        if (gState.tzIndex == slotHour) {
            IOStuff.httpPrint(" mode-active");
        }
        IOStuff.httpPrint("\">");
        IOStuff.httpPrint(kTimezones[slotHour].label);
        IOStuff.httpPrint("</a>");
    }
    IOStuff.httpPrint("</div></details><details><summary>Lever &amp; Coucher</summary><div class=\"mode-list\" style=\"margin-top:10px\">");
    IOStuff.httpPrint("<div class=\"hint\">Lever : ");
    print2(gState.schedule.dawnStart / 60);
    IOStuff.httpPrint(":");
    print2(gState.schedule.dawnStart % 60);
    IOStuff.httpPrint(" &mdash; Coucher : ");
    print2(gState.schedule.duskEnd / 60);
    IOStuff.httpPrint(":");
    print2(gState.schedule.duskEnd % 60);
    IOStuff.httpPrint("</div><div class=\"modes-title\">Lever</div><div class=\"hour-grid\">");
    for (slotHour = 6; slotHour <= 10; slotHour++) {
        for (slotHalf = 0; slotHalf <= 1; slotHalf++) {
            int slot = slotHour * 60 + (slotHalf == 0 ? 0 : 30);
            hourSlot("/sunrise/", slot, gState.schedule.dawnStart == slot);
        }
    }
    IOStuff.httpPrint("</div><div class=\"modes-title\">Coucher</div><div class=\"hour-grid\">");
    for (slotHour = 18; slotHour <= 23; slotHour++) {
        for (slotHalf = 0; slotHalf <= 1; slotHalf++) {
            int slot = slotHour * 60 + (slotHalf == 0 ? 0 : 30);
            hourSlot("/sunset/", slot, gState.schedule.duskEnd == slot);
        }
    }
    IOStuff.httpPrint("</div></div></details><details open><summary>Tester une heure</summary>");
    IOStuff.httpPrint("<p class=\"hint\">Cycle bas&eacute; sur Lever/Coucher (d&eacute;but aube = Lever, LEDs off = Coucher). Les boutons tests utilisent XX:30 pour bien voir aube/cr&eacute;puscule.</p>");
    IOStuff.httpPrint("<div class=\"mode-list\">");
    modeLink("/phase/dawn", false, "Aube", "08:30");
    modeLink("/phase/day", false, "Jour", "12:00");
    modeLink("/phase/dusk", false, "Cr&eacute;puscule", "19:30");
    modeLink("/phase/night", false, "Nuit", "22:00");
    IOStuff.httpPrint("</div><div class=\"hour-grid\">");
    for (slotHour = 0; slotHour < 24; slotHour++) {
        IOStuff.httpPrint("<a class=\"hour-btn");
        if (gState.simHour == slotHour) {
            IOStuff.httpPrint(" hour-active");
        }
        IOStuff.httpPrint("\" href=\"/sim/");
        IOStuff.httpPrint(slotHour);
        IOStuff.httpPrint("\">");
        print2(slotHour);
        IOStuff.httpPrint("h</a>");
    }
    IOStuff.httpPrint("</div><div class=\"actions\" style=\"margin-top:12px;margin-bottom:0\">");
    IOStuff.httpPrint("<a href=\"/sim/clear\"><button class=\"btn btn-muted\">Revenir &agrave; l&rsquo;heure r&eacute;elle</button></a>");
    IOStuff.httpPrint("</div></details><p><a class=\"sys\" href=\"/system\">Configuration du node</a></p>");
    IOStuff.httpPrint("<p class=\"footer\">IO-Stuff &middot; Aquarium</p></body></html>");
    IOStuff.httpEnd();
}

static bool applyPage(const char *path)
{
    int value = 0;
    int previousTz = gState.tzIndex;
    if (strcmp(path, "/sim/clear") == 0) {
        gState.simHour = -1;
        gState.simMinute = 0;
    } else if (strcmp(path, "/phase/dawn") == 0) {
        gState.simHour = 8;
        gState.simMinute = 30;
    } else if (strcmp(path, "/phase/day") == 0) {
        gState.simHour = 12;
        gState.simMinute = 0;
    } else if (strcmp(path, "/phase/dusk") == 0) {
        gState.simHour = 19;
        gState.simMinute = 30;
    } else if (strcmp(path, "/phase/night") == 0) {
        gState.simHour = 22;
        gState.simMinute = 0;
    } else if (suffixMatches(path, "/mode/", &value)) {
        if (value < 0 || value >= AQ_MODE_COUNT) {
            return false;
        }
        gState.mode = value;
    } else if (suffixMatches(path, "/weather/", &value)) {
        if (value < 0 || value >= AQ_WEATHER_COUNT) {
            return false;
        }
        gState.weather = value;
    } else if (suffixMatches(path, "/tz/", &value)) {
        if (value < 0 || value >= AQ_TZ_COUNT) {
            return false;
        }
        gState.tzIndex = value;
    } else if (suffixMatches(path, "/sunrise/", &value)) {
        gState.schedule = aqApplySunrise(gState.schedule, value);
    } else if (suffixMatches(path, "/sunset/", &value)) {
        gState.schedule = aqApplySunset(gState.schedule, value);
    } else if (suffixMatches(path, "/sim/", &value)) {
        if (value < 0 || value > 23) {
            return false;
        }
        gState.simHour = value;
        gState.simMinute = 30;
    } else {
        return false;
    }
    aqClampState(&gState);
    saveState();
    if (gState.tzIndex != previousTz) {
        applyTimezone();
    }
    return true;
}

static bool suffixMatches(const char *path, const char *prefix, int *out)
{
    size_t n = strlen(prefix);
    if (strncmp(path, prefix, n) != 0) {
        return false;
    }
    return aqParseInt(path + n, out) == 1;
}

void sceneSetup()
{
    loadState();
    IOStuff.setBrightness("strip", 220);
    resetAnim();
    startTimeSync();
    Serial.print("Light mode:");
    Serial.println(gState.mode);
    Serial.println("Aquarium IO ready");
}

void sceneLoop()
{
    unsigned long now = millis();
    if (now - gLastTick < kTickMs) {
        return;
    }
    gLastTick = now;
    tick();
}

int sceneActionState(const char *id, char *out, size_t cap)
{
    return aqWriteState(&gState, id, out, cap);
}

int sceneOnAction(const char *id, const char *value)
{
    int previousTz = gState.tzIndex;
    if (!aqDispatch(&gState, id, value)) {
        return 0;
    }
    aqClampState(&gState);
    saveState();
    if (strcmp(id, "timezone") == 0 && gState.tzIndex != previousTz) {
        applyTimezone();
    }
    return 1;
}

bool sceneHttp(const char *method, const char *path)
{
    if (method == 0 || path == 0 || strcmp(method, "GET") != 0) {
        return false;
    }
    if (strcmp(path, "/") == 0) {
        renderPage();
        return true;
    }
    if (applyPage(path)) {
        renderPage();
        return true;
    }
    return false;
}
