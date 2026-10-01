#include <IOStuffNode.h>

#include "scene.h"

// Firmware image: Artemis scene on the IOStuff node SDK.
// The hardware profile is selected in /system. This scene does not name it.
static IOStuffScene gScene = {
    "iostuff.artemis",
    "0.1.0",
    "Artemis",
    "0x0A",
    "0x03",
    sceneSetup,
    sceneLoop,
    sceneHttp,
    sceneActions,
    SCENE_ACTION_COUNT,
    sceneActionState,
    sceneOnAction,
    "Artemis Launcher",
};

void setup()
{
    IOStuff.begin(gScene);
}

void loop()
{
    IOStuff.loop();
}
