#include <IOStuffNode.h>

#include "scene.h"

// Firmware image: Cathedral scene on the IOStuff node SDK.
// Hardware profile comes from system config, not from this scene.
static IOStuffScene gScene = {
    "iostuff.cathedral",
    "0.1.0",
    "Cathedral",
    "0x0A",
    "0x03",
    sceneSetup,
    sceneLoop,
    sceneHttp,
    sceneActions,
    SCENE_ACTION_COUNT,
    sceneActionState,
    sceneOnAction,
    "Cathedral",
};

void setup()
{
    IOStuff.begin(gScene);
}

void loop()
{
    IOStuff.loop();
}
