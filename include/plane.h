#ifndef PLANE_H
#define PLANE_H

#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===============================
// Plane lifecycle & GPU resources
// ===============================
// Create plane meshes/materials & GPU resources.
bool init_plane(void);

// Reset one plane's physical orientation/transform (by value as used in main.c).
void init_flight_model(Plane plane);

// Simple AI tick for non-player planes.
void update_enemy_plane(Plane *enemy);

// Draw a single plane.
void draw_plane(Plane *plane, mat4 view, mat4 proj);

// ===============================
// Player controls / autopilot
// ===============================
void autoPilotMode(void);
void reset_autopilot_state(void);

void applyUpPitch(float rotation_speed);
void applyDownPitch(float rotation_speed);
void applyLeftTurn(float rotation_speed, float roll_speed);
void applyRightTurn(float rotation_speed, float roll_speed);
void applyAutoLeveling(void);

// ===============================
// Cleanup
// ===============================
void cleanup_plane(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // PLANE_H
