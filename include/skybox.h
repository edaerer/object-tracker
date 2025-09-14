#ifndef SKYBOX_H
#define SKYBOX_H

#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif

// Create skybox resources (shaders, cube mesh, textures)
bool init_skybox(void);

// Draw skybox using provided view/projection (view without translation is recommended)
void draw_skybox(mat4 view, mat4 proj);

// Release skybox GPU resources
void cleanup_skybox(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // SKYBOX_H
