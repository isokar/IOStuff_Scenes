#include "scene.h"

#include <IOStuffNode.h>
#include <iostuff_profile.h>

#include <Arduino.h>
#include <math.h>
#include <string.h>

// Option ids stay here. Core never sees the historical 0..4 indexes.
static const char *kModeIds[] = {"rise", "silhouette", "temperatures", "vitraux", "breath"};

static const IOStuffActionOption kSceneOptions[] = {
    {"rise", "Montée de la lumière"},
    {"silhouette", "Silhouette des tours"},
    {"temperatures", "Deux températures"},
    {"vitraux", "Vitraux"},
    {"breath", "Respiration"},
};

const IOStuffActionDef sceneActions[] = {
    {"scene", "Scène", IOSTUFF_ACTION_SELECT, 0, 0, 1, kSceneOptions, 5},
    {"launch", "Illumination", IOSTUFF_ACTION_TOGGLE, 0, 0, 1, 0, 0},
};

// Scenography of one physical strip. 18 tower pixels, then the facade.
// Extra pixels beyond 38 stay on the facade. A shorter strip clips the towers.
static const int kTowers = 18;
static const int kModeCount = 5;
static const int kPhaseSeconds[5] = {0, 0, 3, 30, 0};
static const unsigned long kTickMs = 45;
static const int kBreathPeriodMs = 5000;
static const int kShimmerPeriod = 2800;
static const int kLerpStep = 14;
static const uint8_t kWarmR = 245;
static const uint8_t kWarmG = 128;
static const uint8_t kWarmB = 39;

static int gLaunch = 0;
static int gLightMode = 0;
static bool gRunning = false;
static unsigned long gTimer = 0;
static unsigned long gLastTick = 0;
static bool gPixelsReady = false;
static float gVitrailPhase = 0.0f;
static uint8_t gR[IOSTUFF_RGB_PIXEL_MAX];
static uint8_t gG[IOSTUFF_RGB_PIXEL_MAX];
static uint8_t gB[IOSTUFF_RGB_PIXEL_MAX];

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

static uint8_t scaleChannel(uint8_t value, uint8_t scale)
{
    return (uint8_t)(((uint16_t)value * scale) / 255);
}

static uint8_t clampU8(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

static int lerpChannel(uint8_t current, uint8_t target, uint8_t step)
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

static void resetAnim()
{
    int i;
    int count = stripCount();
    for (i = 0; i < count; i++) {
        gR[i] = 0;
        gG[i] = 0;
        gB[i] = 0;
    }
    gPixelsReady = true;
    gVitrailPhase = 0.0f;
}

static uint8_t breathSmooth(unsigned long period, int offsetMs)
{
    float t = (float)((millis() + (unsigned long)offsetMs) % period);
    float phase = t * 2.0f * 3.14159265f / (float)period;
    return (uint8_t)(118.0f + 117.0f * sinf(phase));
}

static uint8_t shimmerLevel(int pixelOffset)
{
    float t = (float)((millis() + (unsigned long)pixelOffset) % (unsigned long)kShimmerPeriod);
    float phase = t * 2.0f * 3.14159265f / (float)kShimmerPeriod;
    return (uint8_t)(215.0f + 40.0f * sinf(phase));
}

static void setPixelTarget(int index, uint8_t r, uint8_t g, uint8_t b)
{
    gR[index] = (uint8_t)lerpChannel(gR[index], r, kLerpStep);
    gG[index] = (uint8_t)lerpChannel(gG[index], g, kLerpStep);
    gB[index] = (uint8_t)lerpChannel(gB[index], b, kLerpStep);
    IOStuff.setPixel("strip", index, gR[index], gG[index], gB[index]);
}

static void fadeZone(int start, int end, uint8_t r, uint8_t g, uint8_t b)
{
    int i;
    for (i = start; i < end; i++) {
        setPixelTarget(i, r, g, b);
    }
}

static void warmZone(int start, int end, uint8_t r, uint8_t g, uint8_t b)
{
    int i;
    for (i = start; i < end; i++) {
        uint8_t level = shimmerLevel(i * 130);
        setPixelTarget(i, scaleChannel(r, level), scaleChannel(g, level), scaleChannel(b, level));
    }
}

static void skyZone(int start, int end)
{
    int i;
    uint8_t sky = (uint8_t)(breathSmooth(kBreathPeriodMs, 0) / 2 + 70);
    for (i = start; i < end; i++) {
        setPixelTarget(i, scaleChannel(20, sky), scaleChannel(30, sky), scaleChannel(80, sky));
    }
}

static void vitrailColor(float phase, uint8_t &r, uint8_t &g, uint8_t &b)
{
    float wave = 0.5f + 0.5f * sinf(phase);
    if (wave < 0.33f) {
        float t = wave / 0.33f;
        r = clampU8((int)(60 + t * 80));
        g = clampU8((int)(40 + t * 20));
        b = clampU8((int)(120 + t * 40));
    } else if (wave < 0.66f) {
        float t = (wave - 0.33f) / 0.33f;
        r = clampU8((int)(140 - t * 20));
        g = clampU8((int)(60 - t * 30));
        b = clampU8((int)(160 - t * 80));
    } else {
        float t = (wave - 0.66f) / 0.34f;
        r = clampU8((int)(120 + t * 60));
        g = clampU8((int)(30 + t * 20));
        b = clampU8((int)(80 + t * 60));
    }
}

static void vitrailZone(int start, int end, uint8_t warmMix)
{
    int i;
    gVitrailPhase += 0.04f;
    for (i = start; i < end; i++) {
        uint8_t vr;
        uint8_t vg;
        uint8_t vb;
        uint8_t wr = scaleChannel(kWarmR, warmMix);
        uint8_t wg = scaleChannel(kWarmG, warmMix);
        uint8_t wb = scaleChannel(kWarmB, warmMix);
        vitrailColor(gVitrailPhase + i * 0.45f, vr, vg, vb);
        setPixelTarget(i, clampU8((vr * 3 + wr) / 4), clampU8((vg * 3 + wg) / 4), clampU8((vb * 3 + wb) / 4));
    }
}

static void paint(int mode, int launch, int highEnd, int lowStart, int lowEnd)
{
    if (mode == 1) {
        if (launch == 2) {
            warmZone(0, highEnd, 255, 200, 130);
            fadeZone(lowStart, lowEnd, 0, 0, 0);
        } else {
            warmZone(0, highEnd, 255, 200, 130);
            warmZone(lowStart, lowEnd, kWarmR, kWarmG, kWarmB);
        }
    } else if (mode == 2) {
        if (launch == 2) {
            warmZone(lowStart, lowEnd, kWarmR, kWarmG, kWarmB);
            skyZone(0, highEnd);
        } else {
            warmZone(lowStart, lowEnd, kWarmR, kWarmG, kWarmB);
            warmZone(0, highEnd, 255, 200, 130);
        }
    } else if (mode == 3) {
        warmZone(lowStart, lowEnd, kWarmR, kWarmG, kWarmB);
        vitrailZone(0, highEnd, launch == 2 ? 160 : 200);
    } else if (mode == 4) {
        int i;
        uint8_t low = breathSmooth(kBreathPeriodMs, 0);
        for (i = lowStart; i < lowEnd; i++) {
            setPixelTarget(i, scaleChannel(kWarmR, low), scaleChannel(kWarmG, low), scaleChannel(kWarmB, low));
        }
        if (launch == 2) {
            fadeZone(0, highEnd, 0, 0, 0);
        } else {
            uint8_t high = breathSmooth(kBreathPeriodMs, 2500);
            for (i = 0; i < highEnd; i++) {
                setPixelTarget(i, scaleChannel(255, high), scaleChannel(200, high), scaleChannel(130, high));
            }
        }
    } else if (launch == 2) {
        warmZone(lowStart, lowEnd, kWarmR, kWarmG, kWarmB);
        fadeZone(0, highEnd, 0, 0, 0);
    } else {
        warmZone(lowStart, lowEnd, kWarmR, kWarmG, kWarmB);
        warmZone(0, highEnd, 255, 200, 130);
    }
    IOStuff.show("strip");
}

static bool fadeOff()
{
    int i;
    int count = stripCount();
    bool still = false;
    for (i = 0; i < count; i++) {
        uint8_t r = (uint8_t)lerpChannel(gR[i], 0, 10);
        uint8_t g = (uint8_t)lerpChannel(gG[i], 0, 10);
        uint8_t b = (uint8_t)lerpChannel(gB[i], 0, 10);
        if (r != 0 || g != 0 || b != 0) {
            still = true;
        }
        gR[i] = r;
        gG[i] = g;
        gB[i] = b;
        IOStuff.setPixel("strip", i, r, g, b);
    }
    IOStuff.show("strip");
    return still;
}

static void lightsOff()
{
    int i;
    int count = stripCount();
    for (i = 0; i < count; i++) {
        gR[i] = 0;
        gG[i] = 0;
        gB[i] = 0;
        IOStuff.setPixel("strip", i, 0, 0, 0);
    }
    IOStuff.show("strip");
}

static void tick()
{
    int count = stripCount();
    int highEnd = count < kTowers ? count : kTowers;
    int lowStart = highEnd;
    int lowEnd = count;
    if (!gPixelsReady) {
        resetAnim();
    }
    switch (gLaunch) {
    case 1:
        gRunning = true;
        gTimer = millis();
        resetAnim();
        gLaunch = 2;
        break;
    case 4:
        if (!fadeOff()) {
            lightsOff();
            gRunning = false;
            gLaunch = 0;
            resetAnim();
        }
        break;
    case 0:
    default:
        if (gLaunch != 2 && gLaunch != 3) {
            lightsOff();
        }
        break;
    }
    if (gRunning && gLaunch == 3 && millis() > gTimer + (1000UL * (unsigned long)kPhaseSeconds[3])) {
        gLaunch = 4;
    }
    if (gRunning && gLaunch == 2 && millis() > gTimer + (1000UL * (unsigned long)kPhaseSeconds[2])) {
        gLaunch = 3;
        gTimer = millis();
    }
    if (gLaunch == 2 || gLaunch == 3) {
        paint(gLightMode, gLaunch, highEnd, lowStart, lowEnd);
    }
}

void sceneSetup()
{
    int mode = IOStuff.sceneGetInt("lightMode", 0);
    if (mode < 0 || mode >= kModeCount) {
        mode = 0;
    }
    gLightMode = mode;
    IOStuff.setBrightness("strip", 220);
    resetAnim();
    Serial.print("Cathedral mode ");
    Serial.println(gLightMode);
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

static void saveMode(int mode)
{
    if (mode < 0 || mode >= kModeCount || gRunning) {
        return;
    }
    gLightMode = mode;
    IOStuff.scenePutInt("lightMode", mode);
}

static void printCss()
{
    IOStuff.httpPrint("html,body{min-height:100vh;font-family:Georgia,'Times New Roman',serif;color:#e8dcc8;");
    IOStuff.httpPrint("background:linear-gradient(165deg,#070b14 0%,#12182a 45%,#1a1428 100%)}");
    IOStuff.httpPrint("body{display:flex;flex-direction:column;align-items:center;padding:20px 16px 32px;max-width:480px;margin:0 auto;text-align:center}");
    IOStuff.httpPrint("h1{font-size:1.55rem;font-weight:400;letter-spacing:.12em;text-transform:uppercase;color:#d4af37;margin-bottom:4px;text-shadow:0 0 24px rgba(212,175,55,.35)}");
    IOStuff.httpPrint(".sub{font-size:.85rem;color:#9a8f7a;letter-spacing:.06em;margin-bottom:24px;font-style:italic}");
    IOStuff.httpPrint(".card{width:100%;background:rgba(20,24,38,.85);border:1px solid rgba(212,175,55,.25);border-radius:12px;padding:20px 18px;margin-bottom:20px;box-shadow:0 8px 32px rgba(0,0,0,.4)}");
    IOStuff.httpPrint(".status-label{font-size:.7rem;text-transform:uppercase;letter-spacing:.15em;color:#8a7f6a;margin-bottom:8px}");
    IOStuff.httpPrint(".status-text{font-size:1.25rem;color:#f5e6c8;margin-bottom:4px}");
    IOStuff.httpPrint(".status-detail{font-size:.9rem;color:#b8a88a}");
    IOStuff.httpPrint(".status-wait .status-text{color:#9a8f7a}");
    IOStuff.httpPrint(".status-active .status-text{color:#d4af37}");
    IOStuff.httpPrint(".status-glow .status-text{color:#f0c060;text-shadow:0 0 12px rgba(240,192,96,.5)}");
    IOStuff.httpPrint(".countdown{font-size:2rem;font-weight:400;color:#d4af37;margin-top:12px;font-variant-numeric:tabular-nums}");
    IOStuff.httpPrint(".countdown span{font-size:.85rem;color:#8a7f6a;display:block;margin-top:2px}");
    IOStuff.httpPrint(".actions{display:flex;flex-direction:column;gap:12px;width:100%;margin-bottom:24px}");
    IOStuff.httpPrint("a{text-decoration:none}");
    IOStuff.httpPrint(".btn{display:block;width:100%;padding:18px 24px;border:none;border-radius:10px;font-family:Georgia,serif;font-size:1.1rem;letter-spacing:.08em;cursor:pointer}");
    IOStuff.httpPrint(".btn-primary{background:linear-gradient(180deg,#d4af37 0%,#a8841f 100%);color:#1a1428;box-shadow:0 4px 20px rgba(212,175,55,.35)}");
    IOStuff.httpPrint(".btn-danger{background:linear-gradient(180deg,#9b2d3a 0%,#6b1f28 100%);color:#f5e6c8;box-shadow:0 4px 16px rgba(155,45,58,.4)}");
    IOStuff.httpPrint(".scenes{width:100%;margin-bottom:20px;text-align:left}");
    IOStuff.httpPrint(".scenes-title{font-size:.7rem;text-transform:uppercase;letter-spacing:.12em;color:#8a7f6a;margin-bottom:10px}");
    IOStuff.httpPrint(".scene-list{display:flex;flex-direction:column;gap:8px}");
    IOStuff.httpPrint(".scene-opt{display:block;padding:12px 14px;border-radius:8px;border:1px solid rgba(138,127,106,.25);background:rgba(15,18,28,.5);color:#b8a88a;font-size:.85rem;line-height:1.35}");
    IOStuff.httpPrint(".scene-opt small{display:block;font-size:.75rem;color:#7a7060;margin-top:3px}");
    IOStuff.httpPrint(".scene-active{border-color:rgba(212,175,55,.5);background:rgba(212,175,55,.1);color:#d4af37}");
    IOStuff.httpPrint(".scene-active small{color:#9a8a6a}");
    IOStuff.httpPrint(".scene-locked{opacity:.55;pointer-events:none}");
    IOStuff.httpPrint(".sys{margin-top:8px;font-size:.8rem;letter-spacing:.08em;text-transform:uppercase;color:#b8a88a}");
    IOStuff.httpPrint(".footer{margin-top:auto;padding-top:20px;font-size:.65rem;color:#5a5348;letter-spacing:.05em}");
}

static void sceneOption(int mode, const char *title, const char *detail)
{
    IOStuff.httpPrint("<a href=\"/scene/");
    IOStuff.httpPrint(mode);
    IOStuff.httpPrint("\" class=\"scene-opt");
    if (gLightMode == mode) {
        IOStuff.httpPrint(" scene-active");
    }
    IOStuff.httpPrint("\">");
    IOStuff.httpPrint(title);
    IOStuff.httpPrint("<small>");
    IOStuff.httpPrint(detail);
    IOStuff.httpPrint("</small></a>");
}

static void printStatusText()
{
    if (gLaunch == 4) {
        IOStuff.httpPrint("Extinction");
    } else if (gLaunch == 0) {
        IOStuff.httpPrint("En veille");
    } else if (gLaunch == 3) {
        IOStuff.httpPrint("Mise en lumi&egrave;re");
    } else if (gLightMode == 1) {
        IOStuff.httpPrint("Silhouette des tours");
    } else if (gLightMode == 2) {
        IOStuff.httpPrint("Deux temp&eacute;ratures");
    } else if (gLightMode == 3) {
        IOStuff.httpPrint("Vitraux");
    } else if (gLightMode == 4) {
        IOStuff.httpPrint("Respiration");
    } else {
        IOStuff.httpPrint("Mont&eacute;e de la lumi&egrave;re");
    }
}

static void printStatusDetail()
{
    if (gLaunch == 4) {
        IOStuff.httpPrint("Arr&ecirc;t du spectacle");
    } else if (gLaunch == 0) {
        IOStuff.httpPrint("Pr&ecirc;te pour un nouveau spectacle");
    } else if (gLaunch == 3) {
        IOStuff.httpPrint("La cath&eacute;drale brille dans sa splendeur");
    } else if (gLightMode == 1) {
        IOStuff.httpPrint("Les fl&egrave;ches s&rsquo;illuminent dans la nuit&hellip;");
    } else if (gLightMode == 2) {
        IOStuff.httpPrint("Pierre chaude en bas, ciel nocturne en haut");
    } else if (gLightMode == 3) {
        IOStuff.httpPrint("Les vitraux prennent vie sur les tours");
    } else if (gLightMode == 4) {
        IOStuff.httpPrint("La lumi&egrave;re pulse doucement");
    } else {
        IOStuff.httpPrint("La fa&ccedil;ade s&rsquo;&eacute;veille&hellip;");
    }
}

static void renderPage()
{
    IOStuff.httpBegin(200, "text/html; charset=utf-8");
    IOStuff.httpPrint("<!DOCTYPE html><html lang=\"fr\"><head><meta charset=\"UTF-8\">");
    IOStuff.httpPrint("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    IOStuff.httpPrint("<link rel=\"icon\" href=\"data:,\">");
    if (gRunning || gLaunch > 0) {
        IOStuff.httpPrint("<meta http-equiv=\"refresh\" content=\"2\">");
    }
    IOStuff.httpPrint("<title>Notre-Dame de Paris</title><style>*{box-sizing:border-box;margin:0;padding:0}");
    printCss();
    IOStuff.httpPrint("</style></head><body><h1>Notre-Dame de Paris</h1>");
    IOStuff.httpPrint("<p class=\"sub\">Mod&egrave;le LEGO &mdash; Contr&ocirc;le d&rsquo;&eacute;clairage</p>");
    IOStuff.httpPrint("<div class=\"card ");
    if (gLaunch == 0) {
        IOStuff.httpPrint("status-wait");
    } else if (gLaunch == 3) {
        IOStuff.httpPrint("status-glow");
    } else if (gLaunch > 0) {
        IOStuff.httpPrint("status-active");
    } else {
        IOStuff.httpPrint("status-wait");
    }
    IOStuff.httpPrint("\"><div class=\"status-label\">&Eacute;tat</div><div class=\"status-text\">");
    printStatusText();
    IOStuff.httpPrint("</div><div class=\"status-detail\">");
    printStatusDetail();
    IOStuff.httpPrint("</div>");
    if (gRunning && (gLaunch == 2 || gLaunch == 3)) {
        unsigned long deadline = gTimer + (1000UL * (unsigned long)kPhaseSeconds[gLaunch]);
        int remain = 0;
        if (millis() < deadline) {
            remain = (int)((deadline - millis()) / 1000UL);
        }
        IOStuff.httpPrint("<div class=\"countdown\">");
        IOStuff.httpPrint(remain);
        IOStuff.httpPrint("<span>secondes</span></div>");
    }
    IOStuff.httpPrint("</div><div class=\"scenes");
    if (gRunning) {
        IOStuff.httpPrint(" scene-locked");
    }
    IOStuff.httpPrint("\"><div class=\"scenes-title\">Sc&eacute;nario lumineux</div><div class=\"scene-list\">");
    sceneOption(0, "Mont&eacute;e de la lumi&egrave;re", "Fa&ccedil;ade puis tours");
    sceneOption(1, "Silhouette des tours", "Tours puis fa&ccedil;ade");
    sceneOption(2, "Deux temp&eacute;ratures", "Ambre en bas, ciel en haut");
    sceneOption(3, "Vitraux", "Couleurs sur les tours");
    sceneOption(4, "Respiration", "Pulsation douce et calme");
    IOStuff.httpPrint("</div></div><div class=\"actions\">");
    if (gRunning) {
        IOStuff.httpPrint("<a href=\"/Launch/off\"><button class=\"btn btn-danger\">&Eacute;teindre</button></a>");
    } else {
        IOStuff.httpPrint("<a href=\"/Launch/on\"><button class=\"btn btn-primary\">Illuminer la cath&eacute;drale</button></a>");
    }
    IOStuff.httpPrint("</div><p><a class=\"sys\" href=\"/system\">Configuration du node</a></p>");
    IOStuff.httpPrint("<p class=\"footer\">IO-Stuff &middot; Cath&eacute;drale Notre-Dame</p></body></html>");
    IOStuff.httpEnd();
}

static int modeFromId(const char *id)
{
    int i;
    if (id == 0) {
        return -1;
    }
    for (i = 0; i < kModeCount; i++) {
        if (strcmp(kModeIds[i], id) == 0) {
            return i;
        }
    }
    return -1;
}

int sceneActionState(const char *id, char *out, size_t cap)
{
    if (id == 0 || out == 0 || cap == 0) {
        return 0;
    }
    out[0] = 0;
    if (strcmp(id, "scene") == 0) {
        const char *mode = kModeIds[gLightMode];
        if (strlen(mode) + 1 > cap) {
            return 0;
        }
        memcpy(out, mode, strlen(mode) + 1);
        return 1;
    }
    if (strcmp(id, "launch") == 0) {
        const char *value = (gLaunch >= 1 && gLaunch <= 3) ? "true" : "false";
        if (strlen(value) + 1 > cap) {
            return 0;
        }
        memcpy(out, value, strlen(value) + 1);
        return 1;
    }
    return 0;
}

int sceneOnAction(const char *id, const char *value)
{
    if (id == 0) {
        return 0;
    }
    if (strcmp(id, "scene") == 0) {
        int mode = modeFromId(value);
        if (mode < 0 || gRunning) {
            return 0;
        }
        saveMode(mode);
        return 1;
    }
    if (strcmp(id, "launch") == 0) {
        if (value != 0 && strcmp(value, "true") == 0) {
            if (gLaunch == 0 || gLaunch == 4) {
                gLaunch = 1;
            }
            return 1;
        }
        if (value != 0 && strcmp(value, "false") == 0) {
            if (gLaunch != 0) {
                gLaunch = 4;
            }
            return 1;
        }
    }
    return 0;
}

bool sceneHttp(const char *method, const char *path)
{
    if (method == 0 || path == 0 || strcmp(method, "GET") != 0) {
        return false;
    }
    if (strcmp(path, "/Launch/on") == 0) {
        if (!gRunning) {
            gLaunch = 1;
        }
        renderPage();
        return true;
    }
    if (strcmp(path, "/Launch/off") == 0) {
        gLaunch = 4;
        renderPage();
        return true;
    }
    if (strncmp(path, "/scene/", 7) == 0 && path[7] >= '0' && path[7] <= '4' && path[8] == 0) {
        saveMode(path[7] - '0');
        renderPage();
        return true;
    }
    if (strcmp(path, "/") == 0) {
        renderPage();
        return true;
    }
    return false;
}
