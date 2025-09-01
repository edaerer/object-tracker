#ifndef PLANE_H
#define PLANE_H

#include "globals.h"

typedef enum
{
    FLY_STRAIGHT,
    TURN_LEFT,
    TURN_RIGHT,

    AUTO_PITCH_UP,
    AUTO_PITCH_DOWN,
    // New Actions that simulate speed changes
    SET_SPEED,         // SHIFT_KEY, Gradually change to a target speed over the duration
    TOGGLE_SPEED_LOCK, // S_KEY,     Simulates pressing 'S'
                       // New Actions that simulate camera/view changes
    SET_CAMERA,        // CAMERA_VIEW, Simulates pressing '1' through '8'
    HOLD_ZOOM,         // Z_KEY,     Simulates holding 'Z' down
    TOGGLE_GRID_VIEW   // T_KEY,     Simulates pressing 'T'
} AutopilotAction;
// New expanded version of struct now includes a 'value' for extra data
typedef struct
{
    AutopilotAction action;
    float duration;
    float value; // e.g., for SET_CAMERA, value is 1.0-8.0. For SET_SPEED, value is the target speed.
} AutopilotCommand;

// --- Function Prototypes ---
bool init_plane();
void init_flight_model();
void init_second_flight_model();
void update_second_plane();
void draw_plane(mat4 model, mat4 view, mat4 proj);
void autoPilotMode();
void applyUpPitch(float rotation_speed);
void applyDownPitch(float rotation_speed);
void applyLeftTurn(float rotation_speed, float roll_speed);
void applyRightTurn(float rotation_speed, float roll_speed);
void applyAutoLeveling();

void reset_autopilot_state();
void cleanup_plane();
void file_reader_callback_impl(void *ctx, const char *filename, int is_mtl, const char *obj_filename, char **buf, size_t *len);

#endif // PLANE_H