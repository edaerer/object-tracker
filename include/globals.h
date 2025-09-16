#ifndef GLOBALS_H
#define GLOBALS_H

#include <GLES2/gl2.h>
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "cglm/cglm.h"
#include "stb_image.h"
#include "text/text.h"
#include "tiny_obj_loader.h"

// =========================================
// === World & Terrain Constants ===
// =========================================
#define EDGE_SIZE_OF_EACH_CHUNK 5120
#define MAX_PLANES              6

// =========================================
// === Plane Data Structures ===
// =========================================
typedef struct {
    vec3 fuselageColor;
    vec3 wingColor;
    vec3 cockpitColor;
    vec3 thrusterColor;
    vec3 outlineColor;
} StarfighterColors;

typedef struct {
    vec3 position;
    vec3 front;
    vec3 up;
    vec3 right;
    mat4 modelMatrix;
    GLfloat speed;
    StarfighterColors colors;
    bool   useTexture;
    vec3   overrideColor;
    vec3   shadeColor;
    float  shadeStrength;
} Plane;

// =========================================
// === Global Variables (defined in .c) ===
// =========================================
extern Plane planes[MAX_PLANES];

extern const int SCR_WIDTH;
extern const int SCR_HEIGHT;

extern bool isCrashed;
extern vec3 crashPosition;
extern float deltaTime;

// Camera / view
extern float fov;
extern const float originalFov;
extern float zoomingSpeed;
extern float maximumZoomDistance;

extern bool  isAutopilotOn;
extern bool  isTriangleViewMode;

extern vec3  cameraPos, cameraFront, cameraUp, cameraRight;
extern vec3  lightDirection;

// Flight state
extern float verticalSpeed;

// Terrain (heightmap)
extern int terrain_width, terrain_height;
extern const unsigned char *imageData;
extern GLuint heightMapTextureID;
extern const float HEIGHT_SCALE_FACTOR;

// Speed / autopilot
extern bool  isSpeedFixed;
extern float fixedValue;
extern float currentMovementSpeed;
extern float offset_behind;
extern float offset_above;
extern float offset_right;

// =========================================
// === GL Utility Functions (main.c) ===
// =========================================
GLuint compileShader(const char *source, GLenum type);
GLuint createShaderProgram(const char *vsSource, const char *fsSource);

#endif // GLOBALS_H
