#pragma once

#include <IOStuffNode.h>

void sceneSetup();
void sceneLoop();
bool sceneHttp(const char *method, const char *path);

extern const IOStuffActionDef sceneActions[];
enum { SCENE_ACTION_COUNT = 1 };

int sceneActionState(const char *id, char *out, size_t cap);
int sceneOnAction(const char *id, const char *value);
