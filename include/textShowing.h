#ifndef TEXTSHOWING_H
#define TEXTSHOWING_H

#include "globals.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ===============================
// UI/Text initialisation & teardown
// ===============================
bool init_ui(void);      // minimal 2D UI (reserved)
void cleanup_ui(void);

// ===============================
// On-Screen-Display (OSD) text
// ===============================
void init_osd(void);
void update_osd_texts(
    float currentMovementSpeed,
    bool  isSpeedFixed,
    float altitude,
    float heightAboveGround,
    float roll_deg,
    float pitch_deg,
    float hSpeed,
    float vSpeed,
    double elapsedTime
);
void update_status_text(const char *status, float r, float g, float b);
void update_crash_text(void);
void update_fps_text(float fps);
void cleanup_osd(void);

// ===============================
// Crash marker (world-space)
// ===============================
bool init_crash_marker(void);
void draw_crash_marker(mat4 view, mat4 proj);
void cleanup_crash_marker(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // TEXTSHOWING_H
