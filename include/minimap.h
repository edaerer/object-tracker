#ifndef MINIMAP_H
#define MINIMAP_H

#include "globals.h"

void init_minimap();
void update_minimap_dot();
void draw_minimap();
void reset_minimap_for_restart();
void cleanup_minimap();

#endif