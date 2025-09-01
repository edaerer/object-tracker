// minimap.h (Arranged for single moving dot)

#ifndef MINIMAP_H
#define MINIMAP_H

#include "globals.h"

// Initializes the minimap's shaders, buffers, and textures.
void init_minimap();

// Clears the dot texture, calculates the player's new position,
// and redraws the dot. Call this every frame to move the dot.
void update_minimap_dot();

// STATIC FUNCTIONS THAT USED IN minimap.c
// static void get_mirrored_normalized_position(vec2 out_pos, const vec3 world_pos);
// static void get_wrapped_normalized_position(vec2 out_pos, const vec3 world_pos);

// Renders the minimap quad with the map and dot textures.
// Call this every frame within your main render loop.
void draw_minimap();

// Frees all resources used by the minimap.
// Call this once during application shutdown.
void cleanup_minimap();

// Resets the minimap state when the player restarts.
// Call this from your restart logic.
void reset_minimap_for_restart();

#endif // MINIMAP_H