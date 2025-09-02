#ifndef GLOBALS_H
#define GLOBALS_H

#include <GLES2/gl2.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cglm/cglm.h"
#include "stb_image.h"
#include "text/text.h"
#include "tiny_obj_loader.h"

#define EDGE_SIZE_OF_EACH_CHUNK 5120
#define MAX_PLANES 6

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
    bool useTexture;
    vec3 overrideColor;
    vec3 shadeColor;
    float shadeStrength;
} Plane;

extern Plane planes[MAX_PLANES];
extern const int SCR_WIDTH;
extern const int SCR_HEIGHT;
extern bool isCrashed;
extern vec3 crashPosition;
extern float deltaTime;
extern float fov;
extern bool isAutopilotOn;
extern vec3 cameraPos;
extern float verticalSpeed;
extern int terrain_width, terrain_height;
extern const unsigned char *imageData;
extern GLuint heightMapTextureID;
extern const float HEIGHT_SCALE_FACTOR;
extern bool isTriangleViewMode;
extern vec3 lightDirection;
extern const float originalFov;
extern float zoomingSpeed;
extern float maximumZoomDistance;
extern const float accelerationSpeeding;
extern const float accelerationSlowing;
extern bool isSpeedFixed;
extern float fixedValue;
extern float currentMovementSpeed;
extern float offset_behind;
extern float offset_above;
extern float offset_right;

GLuint compileShader(const char *source, GLenum type);
GLuint createShaderProgram(const char *vsSource, const char *fsSource);

#endif
