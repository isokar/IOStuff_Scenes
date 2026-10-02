#include "scene.h"

#include <Arduino.h>
#include <string.h>

// Historical Artemis palette and timings, from the previous sketch.
static const uint8_t kPalette[][3] = {
    {245, 128, 39},
    {232, 110, 14},
    {252, 120, 8},
    {255, 140, 100},
    {247, 80, 40},
};
static const int kPaletteCount = 5;
static const int kIgnitionSeconds = 3;
static const int kFlightSeconds = 30;
static const unsigned long kPaintMs = 300;

static const IOStuffActionDef kLaunch = {"launch", "Lancement", IOSTUFF_ACTION_TOGGLE, 0, 0, 1, 0, 0};
const IOStuffActionDef sceneActions[] = {kLaunch};

static int gLaunch = 0;
static int gRunning = 0;
static unsigned long gTimer = 0;
static unsigned long gLastPaint = 0;

static int stripPixels()
{
    int count = IOStuff.count("strip");
    if (count < 0) {
        return 0;
    }
    return count;
}

static void paintOff()
{
    int count = stripPixels();
    int i;
    for (i = 0; i < count; i++) {
        IOStuff.setPixel("strip", i, 0, 0, 0);
    }
}

static void paintPalette()
{
    int count = stripPixels();
    int i;
    for (i = 0; i < count; i++) {
        int color = random(0, kPaletteCount);
        IOStuff.setPixel("strip", i, kPalette[color][0], kPalette[color][1], kPalette[color][2]);
    }
}

static void showStrip()
{
    IOStuff.show("strip");
    gLastPaint = millis();
}

static void enterRest()
{
    IOStuff.output("relay", 0, false);
    paintOff();
    showStrip();
    gRunning = 0;
    gLaunch = 0;
}

static int atRest()
{
    return gLaunch == 0 && !gRunning;
}

static void beginLaunch()
{
    if (!atRest()) {
        return;
    }
    gLaunch = 1;
}

static void abortLaunch()
{
    if (gLaunch == 0 && !gRunning) {
        return;
    }
    gLaunch = 4;
}

void sceneSetup()
{
    randomSeed(micros());
    enterRest();
}

void sceneLoop()
{
    unsigned long now = millis();
    switch (gLaunch) {
    case 1:
        gRunning = 1;
        gTimer = now;
        IOStuff.output("relay", 0, true);
        gLaunch = 2;
        break;
    case 2:
        break;
    case 3:
        if (now - gLastPaint >= kPaintMs) {
            paintPalette();
            showStrip();
        }
        break;
    case 4:
        enterRest();
        return;
    case 0:
    default:
        if (now - gLastPaint >= kPaintMs) {
            paintOff();
            showStrip();
        }
        break;
    }
    if (gRunning && gLaunch == 3 && now > gTimer + (1000UL * (unsigned long)kFlightSeconds)) {
        gLaunch = 4;
    }
    if (gRunning && gLaunch == 2 && now > gTimer + (1000UL * (unsigned long)kIgnitionSeconds)) {
        gLaunch = 3;
        gTimer = now;
        paintPalette();
        showStrip();
    }
}

static void printStatus()
{
    IOStuff.httpPrint("<div class=\"card ");
    if (gLaunch == 2) {
        IOStuff.httpPrint("status-ignite");
    } else if (gLaunch == 3) {
        IOStuff.httpPrint("status-launch");
    } else if (gLaunch == 4) {
        IOStuff.httpPrint("status-abort");
    } else {
        IOStuff.httpPrint("status-wait");
    }
    IOStuff.httpPrint("\"><div class=\"status-label\">Statut du lancement</div><div class=\"status-text\">");
    if (gLaunch == 4) {
        IOStuff.httpPrint("Abandon");
    } else if (gLaunch == 2) {
        IOStuff.httpPrint("Allumage");
    } else if (gLaunch == 3) {
        IOStuff.httpPrint("D&eacute;collage");
    } else {
        IOStuff.httpPrint("En attente");
    }
    IOStuff.httpPrint("</div><div class=\"status-detail\">");
    if (gLaunch == 4) {
        IOStuff.httpPrint("S&eacute;quence interrompue &mdash; syst&egrave;me en s&eacute;curit&eacute;");
    } else if (gLaunch == 2) {
        IOStuff.httpPrint("S&eacute;quence d&rsquo;allumage des moteurs&hellip;");
    } else if (gLaunch == 3) {
        IOStuff.httpPrint("Propulsion active &mdash; d&eacute;collage en cours");
    } else {
        IOStuff.httpPrint("Pr&ecirc;t au lancement");
    }
    IOStuff.httpPrint("</div>");
}

static unsigned long secondsLeft(int phase)
{
    unsigned long duration = (phase == 2) ? (unsigned long)kIgnitionSeconds : (unsigned long)kFlightSeconds;
    unsigned long deadline = gTimer + (1000UL * duration);
    unsigned long now = millis();
    if (now >= deadline) {
        return 0;
    }
    return (deadline - now) / 1000UL;
}

static void renderPage()
{
    char number[16];
    if (gLaunch == 4) {
        enterRest();
    }
    IOStuff.httpBegin(200, "text/html");
    IOStuff.httpPrint("<!DOCTYPE html><html lang=\"fr\"><head><meta charset=\"UTF-8\">");
    IOStuff.httpPrint("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    if (gRunning || gLaunch > 0) {
        IOStuff.httpPrint("<meta http-equiv=\"refresh\" content=\"5\">");
    }
    IOStuff.httpPrint("<title>Artemis Launcher</title><style>");
    IOStuff.httpPrint("*{box-sizing:border-box;margin:0;padding:0}");
    IOStuff.httpPrint("html,body{min-height:100vh;font-family:'Segoe UI',system-ui,sans-serif;color:#e8edf5;background:linear-gradient(180deg,#050810 0%,#0b1428 40%,#0d1a35 100%)}");
    IOStuff.httpPrint("body{display:flex;flex-direction:column;align-items:center;padding:20px 16px 32px;max-width:480px;margin:0 auto;text-align:center}");
    IOStuff.httpPrint("h1{font-size:1.6rem;font-weight:700;letter-spacing:.18em;text-transform:uppercase;color:#fff;margin-bottom:4px;text-shadow:0 0 30px rgba(232,99,26,.4)}");
    IOStuff.httpPrint(".sub{font-size:.8rem;color:#7aa2c8;letter-spacing:.1em;margin-bottom:24px;text-transform:uppercase}");
    IOStuff.httpPrint(".card{width:100%;background:rgba(10,18,36,.9);border:1px solid rgba(122,162,200,.2);border-radius:12px;padding:20px 18px;margin-bottom:20px;box-shadow:0 8px 32px rgba(0,0,0,.5)}");
    IOStuff.httpPrint(".status-label{font-size:.65rem;text-transform:uppercase;letter-spacing:.18em;color:#5a7a9a;margin-bottom:8px}");
    IOStuff.httpPrint(".status-text{font-size:1.3rem;font-weight:600;color:#fff;margin-bottom:4px}");
    IOStuff.httpPrint(".status-detail{font-size:.85rem;color:#7aa2c8}");
    IOStuff.httpPrint(".status-wait .status-text{color:#7aa2c8}");
    IOStuff.httpPrint(".status-ignite .status-text{color:#f0883e;text-shadow:0 0 16px rgba(240,136,62,.6)}");
    IOStuff.httpPrint(".status-launch .status-text{color:#e8631a;text-shadow:0 0 20px rgba(232,99,26,.7)}");
    IOStuff.httpPrint(".status-abort .status-text{color:#c44d5a}");
    IOStuff.httpPrint(".countdown{font-size:2.5rem;font-weight:700;color:#e8631a;margin-top:14px;font-variant-numeric:tabular-nums}");
    IOStuff.httpPrint(".countdown span{font-size:.75rem;font-weight:400;color:#5a7a9a;display:block;margin-top:4px;letter-spacing:.12em;text-transform:uppercase}");
    IOStuff.httpPrint(".actions{display:flex;flex-direction:column;gap:12px;width:100%;margin-bottom:24px}");
    IOStuff.httpPrint("a{text-decoration:none}");
    IOStuff.httpPrint(".btn{display:block;width:100%;padding:18px 24px;border:none;border-radius:10px;font-family:inherit;font-size:1rem;font-weight:700;letter-spacing:.12em;text-transform:uppercase;cursor:pointer}");
    IOStuff.httpPrint(".btn-launch{background:linear-gradient(180deg,#f0883e 0%,#e8631a 50%,#c44d10 100%);color:#fff}");
    IOStuff.httpPrint(".btn-abort{background:linear-gradient(180deg,#d44d5a 0%,#9b2d3a 100%);color:#fff}");
    IOStuff.httpPrint(".sys{color:#7aa2c8;font-size:.8rem;letter-spacing:.08em;text-transform:uppercase}");
    IOStuff.httpPrint(".footer{margin-top:auto;padding-top:20px;font-size:.6rem;color:#3a5068;letter-spacing:.08em;text-transform:uppercase}");
    IOStuff.httpPrint("</style></head><body><h1>Artemis Launcher</h1>");
    IOStuff.httpPrint("<p class=\"sub\">Mod&egrave;le LEGO &mdash; Lanceur spatial</p>");
    printStatus();
    if (gRunning && (gLaunch == 2 || gLaunch == 3)) {
        snprintf(number, sizeof(number), "%lu", secondsLeft(gLaunch));
        IOStuff.httpPrint("<div class=\"countdown\">T-");
        IOStuff.httpPrint(number);
        IOStuff.httpPrint("<span>secondes</span></div>");
    }
    IOStuff.httpPrint("</div><div class=\"actions\">");
    if (gRunning) {
        IOStuff.httpPrint("<a href=\"/Launch/off\"><button class=\"btn btn-abort\">Abandon</button></a>");
    } else {
        IOStuff.httpPrint("<a href=\"/Launch/on\"><button class=\"btn btn-launch\">Lancer</button></a>");
    }
    IOStuff.httpPrint("</div><p><a class=\"sys\" href=\"/system\">Configuration du node</a></p>");
    IOStuff.httpPrint("<p class=\"footer\">IO-Stuff &middot; Artemis Launcher</p></body></html>");
    IOStuff.httpEnd();
}

int sceneActionState(const char *id, char *out, size_t cap)
{
    const char *value;
    if (id == 0 || out == 0 || cap == 0 || strcmp(id, "launch") != 0) {
        return 0;
    }
    value = (gLaunch >= 1 && gLaunch <= 3) ? "true" : "false";
    if (strlen(value) + 1 > cap) {
        return 0;
    }
    memcpy(out, value, strlen(value) + 1);
    return 1;
}

int sceneOnAction(const char *id, const char *value)
{
    if (id == 0 || strcmp(id, "launch") != 0 || value == 0) {
        return 0;
    }
    if (strcmp(value, "true") == 0) {
        beginLaunch();
        return 1;
    }
    if (strcmp(value, "false") == 0) {
        abortLaunch();
        return 1;
    }
    return 0;
}

bool sceneHttp(const char *method, const char *path)
{
    if (method == 0 || path == 0 || strcmp(method, "GET") != 0) {
        return false;
    }
    if (strcmp(path, "/Launch/on") == 0) {
        beginLaunch();
        renderPage();
        return true;
    }
    if (strcmp(path, "/Launch/off") == 0) {
        abortLaunch();
        renderPage();
        return true;
    }
    if (strcmp(path, "/") == 0) {
        renderPage();
        return true;
    }
    return false;
}
