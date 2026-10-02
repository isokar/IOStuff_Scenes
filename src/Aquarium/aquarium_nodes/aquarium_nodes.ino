#include <IOStuffNode.h>

#include "scene.h"

#ifndef IOSTUFF_AQUARIUM_VERSION
#define IOSTUFF_AQUARIUM_VERSION "0.1.0"
#endif

// Firmware image: Aquarium scene on the IOStuff node SDK.
// The hardware profile is selected in /system. This scene does not name it.
static IOStuffScene gScene = {
    "iostuff.aquarium",
    IOSTUFF_AQUARIUM_VERSION,
    "Aquarium",
    "0x0B",
    "1.0",
    sceneSetup,
    sceneLoop,
    sceneHttp,
    sceneActions,
    SCENE_ACTION_COUNT,
    sceneActionState,
    sceneOnAction,
    "Aquarium",
};

void setup()
{
    IOStuff.begin(gScene);
}

void loop()
{
    IOStuff.loop();
}
