#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H

#include "globals.h"

// --- CHUNK LOGIC ---
#define CHUNK_GRID_DIM 3
// #define EDGE_SIZE_OF_EACH_CHUNK 1024 -- defined in globals.h
#define CHUNK_VERTEX_DIM 65
#define TOTAL_CHUNKS (CHUNK_GRID_DIM * CHUNK_GRID_DIM)

// --- Function Prototypes ---
bool load_terrain_data();
void init_heightmap_texture();
void init_chunks();
void update_chunks();
void draw_chunks(mat4 view, mat4 proj);
float get_terrain_height(float x, float z);
void cleanup_heightmap();

#endif // HEIGHTMAP_H