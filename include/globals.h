#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "stb_image.h"

#include <GLES2/gl2.h>
#include <GLFW/glfw3.h>

#include "cglm/cglm.h"
#include "text/text.h"

#include "tiny_obj_loader.h"

#define EDGE_SIZE_OF_EACH_CHUNK 5120

// --- Window and Application State ---
extern GLFWwindow *window;
extern const int SCR_WIDTH;
extern const int SCR_HEIGHT;
extern bool isCrashed;
extern vec3 crashPosition;

// --- Timing ---
extern float deltaTime;

// --- Flight & Camera Model ---
extern vec3 planePos;
extern vec3 planeFront;
extern vec3 planeUp;
extern vec3 planeRight;
extern mat4 planeModelMatrix;
extern float fov;
extern bool isAutopilotOn;
extern vec3 cameraPos;
// Second Plane
extern vec3 secondPlanePos;
extern vec3 secondPlaneFront;
extern vec3 secondPlaneUp;
extern vec3 secondPlaneRight;
extern mat4 secondPlaneModelMatrix;

extern float verticalSpeed;

// --- Terrain Data ---
extern int terrain_width, terrain_height;
extern const unsigned char *imageData;
extern GLuint heightMapTextureID;
extern const float HEIGHT_SCALE_FACTOR;
extern bool isTriangleViewMode;

// --- Lighting ---
extern vec3 lightDirection;

extern const float originalFov;
extern float zoomingSpeed;
extern float maximumZoomDistance;
extern const float accelerationSpeeding;
extern const float speedingSlowingRatio; // increasingSpeed = slowingSpeed * ratio
extern const float accelerationSlowing;
extern bool isSpeedFixed;
extern float fixedValue;
extern float currentMovementSpeed;
extern float offset_behind;
extern float offset_above;
extern float offset_right;

// --- Shared Utility Functions ---
GLuint compileShader(const char *source, GLenum type);
GLuint createShaderProgram(const char *vsSource, const char *fsSource);
void check_gl_error(const char *file, int line);
#define CHECK_GL_ERROR() check_gl_error(__FILE__, __LINE__)

#endif // GLOBALS_H