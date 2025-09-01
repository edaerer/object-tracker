#ifndef SKYBOX_H
#define SKYBOX_H

#include "globals.h"

bool init_skybox();
void draw_skybox(mat4 view, mat4 proj);
void cleanup_skybox();

#endif // SKYBOX_H