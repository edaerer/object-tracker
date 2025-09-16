#ifndef MINIMAP_H
#define MINIMAP_H

#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize minimap renderer (shaders, VBOs, textures if any)
void init_minimap(void);

// Update player dot/marker position based on current plane position
void update_minimap_dot(void);

// Draw the minimap overlay (expects blending state managed by caller)
void draw_minimap(void);

// Reset state when restarting after a crash (recenters/clears trails)
void reset_minimap_for_restart(void);

// Release resources
void cleanup_minimap(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // MINIMAP_H
