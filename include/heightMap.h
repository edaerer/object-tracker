#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H

#include "globals.h"

// Chunk grid layout and resolution
#define CHUNK_GRID_DIM   3
#define CHUNK_VERTEX_DIM 65
#define TOTAL_CHUNKS     (CHUNK_GRID_DIM * CHUNK_GRID_DIM)

#ifdef __cplusplus
extern "C" {
#endif

// Textures & shader init; loads ./images/heightMapImage.png
void init_heightmap_texture(void);

// Chunk pool (GPU buffers)
void init_chunks(void);
void update_chunks(void);
void draw_chunks(mat4 view, mat4 proj);

// Sample terrain height in world units
float get_terrain_height(float x, float z);

// Cleanup all heightmap-related state
void cleanup_heightmap(void);

#ifdef __cplusplus
}
#endif

#endif // HEIGHTMAP_H
