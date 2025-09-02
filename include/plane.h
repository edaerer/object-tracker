#ifndef PLANE_H
#define PLANE_H

#include "globals.h"

bool init_plane();
void init_flight_model(Plane plane);
void update_enemy_plane(Plane *enemy);
void draw_plane(Plane *plane, mat4 view, mat4 proj);
void autoPilotMode();
void applyUpPitch(float rotation_speed);
void applyDownPitch(float rotation_speed);
void applyLeftTurn(float rotation_speed, float roll_speed);
void applyRightTurn(float rotation_speed, float roll_speed);
void applyAutoLeveling();
void reset_autopilot_state();
void cleanup_plane();

#endif
