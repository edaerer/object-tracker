#ifndef TEXTSHOWING_H
#define TEXTSHOWING_H

#include "globals.h"

// --- Function Prototypes ---
bool init_ui();
void init_osd();
void update_osd_texts(float currentMovementSpeed, bool isSpeedFixed, float altitude, float heightAboveGround, float roll, float pitch, float hSpeed, float vSpeed, double elapsedTime);
void update_crash_text();
void update_status_text(const char *status, float r, float g, float b);

void update_fps_text(float fps);

bool init_crash_marker();
void draw_crash_marker(mat4 view, mat4 proj);

void cleanup_ui();
void cleanup_osd();
void cleanup_crash_marker();

#endif // TEXTSHOWING_H