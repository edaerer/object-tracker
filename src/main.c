// MODIFICATION: Define
#define _POSIX_C_SOURCE 200809L // POSIX standard to guarantee availability of clock_gettime

// ===============================================================
// ===         Final 3D Plane Simulator -- Version 1.6         ===
// ===============================================================

#include "globals.h"
#include "plane.h"
#include "heightMap.h"
#include "skybox.h"
#include "minimap.h"
#include "textShowing.h"

#include <time.h>

#define KEYBOARD_ENABLED 1

// --- Global Variable Definitions ---
GLFWwindow *window;
const int SCR_WIDTH = 1920;
const int SCR_HEIGHT = 1080;

// Application State
bool isCrashed = false;
vec3 lightDirection = {0.0f, 1.0f, 0.0f};
vec3 crashPosition;
bool hasSetCrashView = false;
float crashedScenarioCameraHeight = 450.0f;
bool automaticCameraMovement = true;
float gravitySpeed = 0.0f; // gravity ca be set if needed
bool isSpeedFixed = false;
float fixedValue = 0.0f;
float accelerationGoingFarPerSecond = 0.5f; // going far, going slower effect
const float accelerationSpeeding = 200.0f;
const float speedingSlowingRatio = 2.5f; // increasingSpeed = slowingSpeed * ratio
const float accelerationSlowing = (accelerationSpeeding / speedingSlowingRatio);
const float speedBoostMultiplier = 12.5f; // for determine max speed
float currentMovementSpeed;
float initialMovementSpeed = 100.0f;
float initalAutoPilotModeSpeed = 100.0f;
bool startingAutoPilotMode = true; // for inital autopilot
float fov = 45.0f;
const float originalFov = 45.0f;
float zoomingSpeed = 20.0f;
float maximumZoomDistance = 10.0f; // zooming angle Initial is 45 so making 10 makes texture close
float bow_rate = 30.0f;            // for turning left and right making bowing angle
float autoLevelSpeed = 1.0f;

float verticalSpeed = 0.0f;       // The calculated vertical speed in m/s
static float lastAltitude = 0.0f; // Stores the previous frame's altitude for calculation

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

static double startTime = 0.0;

// variables for the new FPS calculation logic
static int frame_count = 0;
static struct timespec last_time; // Stores the time of the last FPS update

// Plane & Camera
vec3 planePos, planeFront, planeUp, planeRight;
mat4 planeModelMatrix;
vec3 cameraPos, cameraFront, cameraUp, cameraRight;
bool isAutopilotOn = true;
// Second Plane
vec3 secondPlanePos, secondPlaneFront, secondPlaneUp, secondPlaneRight;
mat4 secondPlaneModelMatrix;
// planeStartingCoordinates this is in plane.c

// for camera angles - camera view modes
float offset_behind = 400.0f;
float offset_above = 170.0f;
float offset_right = 0.0f;

// Terrain
int terrain_width, terrain_height;
const unsigned char *heightMapImageData = NULL;
GLuint heightMapTextureID;
const float HEIGHT_SCALE_FACTOR = 500.0f;
bool isTriangleViewMode = false;

// --- Forward Declarations ---
void INIT_SYSTEM();
void DRAW_SYSTEM();
void CLEANUP_SYSTEM();
void update_camera_and_view_matrix(mat4 view);
void processInput();
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void whereItCrashed();
GLuint createShaderProgram(const char *vsSource, const char *fsSource);

// --- Helper Functions ---
static double get_current_time_seconds()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

int main()
{
    // *****_____ WINDOW OPERATIONS ______*****
    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Height Map Terrain Flight", NULL, NULL);
    if (!window)
    {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glfwSwapInterval(0); // for removing fps limit

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    INIT_SYSTEM();

    while (!glfwWindowShouldClose(window))
    {
        if (KEYBOARD_ENABLED)
        {
            processInput(); // Includes window operations
        }
        else
        {
            if (startingAutoPilotMode) 
            {
                currentMovementSpeed = 100.0f;
                startingAutoPilotMode = false;
            }
            autoPilotMode();
        }

        update_second_plane();
        DRAW_SYSTEM();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    CLEANUP_SYSTEM(); // all cleanup functions called here There is no glfw function here
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

void INIT_SYSTEM()
{
    printf("Initializing system...\n");
    init_flight_model();
    init_second_flight_model();

    if (!init_skybox())
        exit(-1);
    if (!init_ui())
        exit(-1);
    if (!init_crash_marker())
        exit(-1);
    if (!init_plane())
        exit(-1);

    init_heightmap_texture();
    init_chunks();
    init_osd();
    init_minimap();

    // Initialize the per-frame delta time
    lastFrame = get_current_time_seconds();

    startTime = lastFrame;

    lastAltitude = planePos[1];

    // Initialize the FPS counter's start time
    clock_gettime(CLOCK_MONOTONIC, &last_time);
    frame_count = 0;

    printf("Initialization successful.\n");
}

void DRAW_SYSTEM()
{
    // --- Per-frame delta time for physics and movement ---
    float currentFrame = get_current_time_seconds();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    double elapsedTime = currentFrame - startTime; // to display time

    // ===================================================================
    // === MODIFICATION: Calculate vertical speed every frame          ===
    // ===================================================================
    if (deltaTime > 0.0f)
    {
        verticalSpeed = (planePos[1] - lastAltitude) / deltaTime;
    }
    lastAltitude = planePos[1];

    // --- FPS calculation logic ---
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    frame_count++;

    // Calculate the time difference in seconds since the last FPS update
    double time_diff = (current_time.tv_sec - last_time.tv_sec) +
                       (current_time.tv_nsec - last_time.tv_nsec) / 1000000000.0;

    // Update the FPS display only every quarter of a second (or more) for stability
    if (time_diff >= 0.25)
    {
        float current_fps = frame_count / time_diff;

        update_fps_text(current_fps);

        // Reset for the next measurement interval
        frame_count = 0;
        last_time = current_time;
    }

    if (!isCrashed)
    {
        float groundHeight = get_terrain_height(planePos[0], planePos[2]);
        float heightAboveGround = planePos[1] - groundHeight;

        if (heightAboveGround < 0.5f)
        {
            isCrashed = true;
            hasSetCrashView = false;
            glm_vec3_copy(planePos, crashPosition);
            crashPosition[1] = groundHeight;
        }

        if (automaticCameraMovement)
        {
            vec3 move_vector;
            glm_vec3_scale(planeFront, currentMovementSpeed * deltaTime, move_vector);
            glm_vec3_add(planePos, move_vector, planePos);
            planePos[1] -= gravitySpeed * deltaTime;
        }

        vec3 worldUp = {0.0f, 1.0f, 0.0f};
        vec3 cross_product;
        glm_vec3_cross(worldUp, planeUp, cross_product);
        float roll_rad = acosf(glm_vec3_dot(worldUp, planeUp));
        if (glm_vec3_dot(cross_product, planeFront) < 0)
            roll_rad = -roll_rad;

        vec3 velocity;
        glm_vec3_scale(planeFront, currentMovementSpeed, velocity);
        float hSpeed = sqrtf(velocity[0] * velocity[0] + velocity[2] * velocity[2]);
        float vSpeed = velocity[1];

        update_osd_texts(currentMovementSpeed, isSpeedFixed, planePos[1], heightAboveGround, glm_deg(roll_rad), glm_deg(asinf(planeFront[1])), hSpeed, vSpeed, elapsedTime);

        if (heightAboveGround < 30.0f)
        {
            update_status_text("WARNING: PULL UP", 1.0f, 0.0f, 0.0f);
        }
        else if (isAutopilotOn)
        {
            update_status_text("AUTOPILOT ENGAGED", 0.0f, 1.0f, 0.0f);
        }
        else
        {
            update_status_text("FLIGHT STATUS: SAFE", 0.0f, 1.0f, 1.0f);
        }
    }
    else
    {
        if (!hasSetCrashView)
        {
            whereItCrashed();
            hasSetCrashView = true;
        }
        update_crash_text();
    }

    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4 proj, view;
    glm_perspective(glm_rad(fov), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 20000.0f, proj);

    if (!isCrashed)
    {
        update_camera_and_view_matrix(view);
    }
    else
    {
        vec3 center;
        glm_vec3_add(cameraPos, cameraFront, center);
        glm_lookat(cameraPos, center, cameraUp, view);
    }

    draw_skybox(view, proj);
    update_chunks();
    draw_chunks(view, proj);

    update_minimap_dot();

    if (!isCrashed)
    {
        draw_plane(planeModelMatrix, view, proj);
        draw_plane(secondPlaneModelMatrix, view, proj);
    }

    glDisable(GL_DEPTH_TEST);
    if (isCrashed)
    {
        draw_crash_marker(view, proj);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_texts();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_minimap();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void update_camera_and_view_matrix(mat4 view)
{
    if (isCrashed)
    {
        vec3 center;
        glm_vec3_add(cameraPos, cameraFront, center);
        glm_lookat(cameraPos, center, cameraUp, view);
        return;
    }
    mat4 trans, rot;
    glm_translate_make(trans, planePos);
    mat4 plane_orientation;
    glm_mat4_identity(plane_orientation);
    glm_vec3_copy(planeRight, plane_orientation[0]);
    glm_vec3_copy(planeUp, plane_orientation[1]);
    vec3 negatedFront;
    glm_vec3_negate_to(planeFront, negatedFront);
    glm_vec3_copy(negatedFront, plane_orientation[2]);
    glm_mat4_copy(plane_orientation, rot);
    glm_mat4_mul(trans, rot, planeModelMatrix);
    vec3 behindVec, aboveVec, rightVec;
    glm_vec3_scale(planeFront, offset_behind, behindVec);
    glm_vec3_scale(planeUp, offset_above, aboveVec);
    glm_vec3_scale(planeRight, offset_right, rightVec);
    glm_vec3_sub(planePos, behindVec, cameraPos);
    glm_vec3_add(cameraPos, aboveVec, cameraPos);
    glm_vec3_add(cameraPos, rightVec, cameraPos);
    vec3 lookAtTarget;
    if (offset_behind <= 0.0f && offset_behind >= -70.0f)
    {
        glm_vec3_add(cameraPos, planeFront, lookAtTarget);
    }
    else
    {
        glm_vec3_add(planePos, planeFront, lookAtTarget);
    }
    glm_lookat(cameraPos, lookAtTarget, planeUp, view);
}
void processInput()
{
    if (!isCrashed)
    {
        if (glfwGetKey(window, GLFW_KEY_1))
        {
            offset_behind = 400.0f;
            offset_above = 170.0f;
            offset_right = 0.0f;
        }
        else if (glfwGetKey(window, GLFW_KEY_2))
        {
            offset_behind = -50.0f;
            offset_above = 30.0f;
            offset_right = 0.0f;
        }
        else if (glfwGetKey(window, GLFW_KEY_3))
        {
            offset_behind = 400.0f;
            offset_above = 500.0f;
            offset_right = 0.0f;
        }
        else if (glfwGetKey(window, GLFW_KEY_4))
        {
            offset_behind = -300.0f;
            offset_above = 100.0f;
            offset_right = 0.0f;
        }
        else if (glfwGetKey(window, GLFW_KEY_5))
        {
            offset_behind = -200.0f;
            offset_above = 150.0f;
            offset_right = 250.0f;
        }
        else if (glfwGetKey(window, GLFW_KEY_6))
        {
            offset_behind = -200.0f;
            offset_above = 150.0f;
            offset_right = -250.0f;
        }
        else if (glfwGetKey(window, GLFW_KEY_7))
        {
            offset_behind = -300.0f;
            offset_above = -100.0f;
            offset_right = 0.0f;
        }
        else if (glfwGetKey(window, GLFW_KEY_8))
        {
            offset_behind = -75.0f;
            offset_above = 200.0f;
            offset_right = 550.0f;
        }
    }
    static bool a_key_pressed_last_frame = false;
    bool a_key_is_pressed_now = (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
    if (a_key_is_pressed_now && !a_key_pressed_last_frame)
    {
        isAutopilotOn = !isAutopilotOn;
        fixedValue = currentMovementSpeed;
        if (isAutopilotOn)
        {
            printf("SCRIPTED AUTOPILOT ENGAGED\n");
            reset_autopilot_state();
            isSpeedFixed = true;
            currentMovementSpeed = fixedValue;
        }
        else
        {
            printf("AUTOPILOT DISENGAGED\n");
            isSpeedFixed = false;
        }
    }
    a_key_pressed_last_frame = a_key_is_pressed_now;
    static bool t_key_pressed_last_frame = false;
    bool t_key_is_pressed_now = (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS);
    if (t_key_is_pressed_now && !t_key_pressed_last_frame)
    {
        isTriangleViewMode = !isTriangleViewMode;
        if (isTriangleViewMode)
        {
            printf("GRID VIEW MODE: ON\n");
        }
        else
        {
            printf("GRID VIEW MODE: OFF\n");
        }
    }
    t_key_pressed_last_frame = t_key_is_pressed_now;
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, 1);
    if (isCrashed)
    {
        isSpeedFixed = false;
        isAutopilotOn = false;
        currentMovementSpeed = initialMovementSpeed;
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        {
            isCrashed = false;
            hasSetCrashView = true;
            init_flight_model();
            init_second_flight_model();
            reset_minimap_for_restart();

            // ===================================================================
            // === MODIFICATION: Reset lastAltitude on restart               ===
            // ===================================================================
            lastAltitude = planePos[1];
        }
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
        {
            // Set the plane's position to the crash site.
            glm_vec3_copy(crashPosition, planePos);

            //    Elevate the plane to a safe altitude above the crash point.
            planePos[1] += 250.0f; // Gives the player 50 units of clearance.

            // Give the plane a safe, level flying orientation.
            //    (This logic is borrowed from init_flight_model but avoids resetting position)
            vec3 worldUp = {0.0f, 1.0f, 0.0f};
            glm_vec3_copy((vec3){0.0f, 0.0f, -1.0f}, planeFront); // Point forward
            glm_vec3_copy(worldUp, planeUp);                      // Level wings
            glm_vec3_cross(planeFront, planeUp, planeRight);      // Recalculate right vector
            glm_normalize(planeRight);

            // eset the game state to flying.
            isCrashed = false;
            hasSetCrashView = false; // Set to false so the camera re-engages correctly
        }
        return;
    }
    if (isAutopilotOn)
    {
        autoPilotMode();
        if (startingAutoPilotMode)
        {
            currentMovementSpeed = initalAutoPilotModeSpeed;
            fixedValue = currentMovementSpeed;
            startingAutoPilotMode = false;
        }
        else
        {
            currentMovementSpeed = fixedValue;
        }

        return;
    }
    static bool s_key_pressed_last_frame = false;
    bool s_key_is_pressed_now = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);
    if (s_key_is_pressed_now && !s_key_pressed_last_frame)
    {
        isSpeedFixed = !isSpeedFixed;
        if (isSpeedFixed)
            fixedValue = currentMovementSpeed;
    }
    s_key_pressed_last_frame = s_key_is_pressed_now;
    if (isSpeedFixed)
    {
        currentMovementSpeed = fixedValue;
    }
    else
    {
        float maxSpeed = initialMovementSpeed * speedBoostMultiplier;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
        {
            currentMovementSpeed += accelerationSpeeding * deltaTime;
            if (currentMovementSpeed > maxSpeed)
                currentMovementSpeed = maxSpeed;
            else
                offset_behind += accelerationGoingFarPerSecond;
        }
        else
        {
            currentMovementSpeed -= accelerationSlowing * deltaTime * 2.0f;
            if (currentMovementSpeed < initialMovementSpeed)
                currentMovementSpeed = initialMovementSpeed;
            else
                offset_behind -= accelerationGoingFarPerSecond / speedingSlowingRatio;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
    {
        fov -= zoomingSpeed * deltaTime;
        if (fov < maximumZoomDistance)
            fov = maximumZoomDistance;
    }
    else
    {
        fov += zoomingSpeed * deltaTime;
        if (fov > originalFov)
            fov = originalFov;
    }
    float rotation_speed = 45.0f * deltaTime;
    float roll_speed = bow_rate * deltaTime;
    bool isTurning = false;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        applyUpPitch(rotation_speed * 2);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        applyDownPitch(rotation_speed * 2);
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
    {
        isTurning = true;
        applyLeftTurn(rotation_speed, roll_speed);
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
    {
        isTurning = true;
        applyRightTurn(rotation_speed, roll_speed);
    }
    if (!isTurning)
        applyAutoLeveling();
    glm_normalize(planeFront);
    glm_vec3_cross(planeFront, planeUp, planeRight);
    glm_normalize(planeRight);
    glm_vec3_cross(planeRight, planeFront, planeUp);
    glm_normalize(planeUp);
}
void CLEANUP_SYSTEM()
{
    cleanup_plane();
    cleanup_heightmap();
    cleanup_skybox();
    cleanup_minimap();
    cleanup_ui();
    cleanup_crash_marker();
    cleanup_osd();
}
void whereItCrashed()
{
    cameraPos[1] = crashPosition[1] + crashedScenarioCameraHeight;
    cameraPos[2] = crashPosition[2] + crashedScenarioCameraHeight;
    cameraPos[0] = crashPosition[0];
    glm_vec3_sub(crashPosition, cameraPos, cameraFront);
    glm_normalize(cameraFront);
    vec3 worldUp = {0.0f, 1.0f, 0.0f};
    glm_vec3_cross(cameraFront, worldUp, cameraRight);
    glm_normalize(cameraRight);
    glm_vec3_cross(cameraRight, cameraFront, cameraUp);
    glm_normalize(cameraUp);
}
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    (void)window;
    glViewport(0, 0, width, height);
}
GLuint compileShader(const char *source, GLenum type)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        char *info = (char *)malloc(length);
        glGetShaderInfoLog(shader, length, NULL, info);
        fprintf(stderr, "Error compiling shader: %s\n", info);
        free(info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
GLuint createShaderProgram(const char *vsSource, const char *fsSource)
{
    GLuint vertexShader = compileShader(vsSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fsSource, GL_FRAGMENT_SHADER);
    if (vertexShader == 0 || fragmentShader == 0)
        return 0;
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        char *info = (char *)malloc(length);
        glGetProgramInfoLog(program, length, NULL, info);
        fprintf(stderr, "Error linking program: %s\n", info);
        free(info);
        glDeleteProgram(program);
        return 0;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}
void check_gl_error(const char *file, int line)
{
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
    {
        fprintf(stderr, "OpenGL Error at %s:%d\n", file, line);
    }
}