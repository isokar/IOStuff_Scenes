#include <IOStuffNode.h>

#include "scene.h"

// Firmware image: Aquarium scene on the IOStuff node SDK.
// The hardware profile is selected in /system. This scene does not name it.
static IOStuffScene gScene = {
    "iostuff.aquarium",
    "0.1.0",
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
