#define _POSIX_C_SOURCE 200809L // POSIX standard to guarantee availability of clock_gettime
#include "globals.h"
#include "heightMap.h"
#include "minimap.h"
#include "plane.h"
#include "skybox.h"
#include "textShowing.h"

#include <time.h>

#define KEYBOARD_ENABLED 1

GLFWwindow *window;
const int SCR_WIDTH = 1920;
const int SCR_HEIGHT = 1080;
bool isCrashed = false;
vec3 lightDirection = {0.0f, 1.0f, 0.0f};
vec3 crashPosition;
bool hasSetCrashView = false;
float crashedScenarioCameraHeight = 450.0f;
bool automaticCameraMovement = true;
float gravitySpeed = 0.0f;
bool isSpeedFixed = false;
float fixedValue = 0.0f;
float accelerationGoingFarPerSecond = 0.5f;
const float accelerationSpeeding = 200.0f;
const float speedingSlowingRatio = 2.5f;
const float accelerationSlowing = (accelerationSpeeding / speedingSlowingRatio);
const float speedBoostMultiplier = 12.5f;
float currentMovementSpeed;
float initialMovementSpeed = 100.0f;
float initalAutoPilotModeSpeed = 100.0f;
bool startingAutoPilotMode = true;
float fov = 45.0f;
const float originalFov = 45.0f;
float zoomingSpeed = 20.0f;
float maximumZoomDistance = 10.0f;
float bow_rate = 30.0f;
float autoLevelSpeed = 1.0f;
float verticalSpeed = 0.0f;
static float lastAltitude = 0.0f;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
static double startTime = 0.0;
static int frame_count = 0;
static struct timespec last_time;
vec3 cameraPos, cameraFront, cameraUp, cameraRight;
bool isAutopilotOn = true;
float offset_behind = 400.0f;
float offset_above = 170.0f;
float offset_right = 0.0f;
int terrain_width, terrain_height;
const unsigned char *heightMapImageData = NULL;
GLuint heightMapTextureID;
const float HEIGHT_SCALE_FACTOR = 500.0f;
bool isTriangleViewMode = false;

void INIT_SYSTEM();
void DRAW_SYSTEM();
void CLEANUP_SYSTEM();
void update_camera_and_view_matrix(mat4 view);
void processInput();
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void whereItCrashed();
GLuint createShaderProgram(const char *vsSource, const char *fsSource);

static double get_current_time_seconds() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double) now.tv_sec + (double) now.tv_nsec / 1000000000.0;
}

int main() {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Height Map Terrain Flight", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    INIT_SYSTEM();

    while (!glfwWindowShouldClose(window)) {
        if (KEYBOARD_ENABLED) {
            processInput();
        } else {
            if (startingAutoPilotMode) {
                currentMovementSpeed = 100.0f;
                startingAutoPilotMode = false;
            }
            autoPilotMode();
        }

        DRAW_SYSTEM();

        for (int i=1; i<MAX_PLANES; i++) {
            update_enemy_plane(&planes[i]);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    CLEANUP_SYSTEM();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

void INIT_SYSTEM() {
    printf("Initializing system...\n");

    for (int i=0; i<MAX_PLANES; i++) {
        init_flight_model(planes[i]);
    }

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

    lastFrame = get_current_time_seconds();
    startTime = lastFrame;
    lastAltitude = planes[0].position[1];
    clock_gettime(CLOCK_MONOTONIC, &last_time);
    frame_count = 0;
    printf("Initialization successful.\n");
}

void DRAW_SYSTEM() {
    float currentFrame = get_current_time_seconds();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    double elapsedTime = currentFrame - startTime;
    if (deltaTime > 0.0f) {
        verticalSpeed = (planes[0].position[1] - lastAltitude) / deltaTime;
    }
    lastAltitude = planes[0].position[1];
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    frame_count++;
    double time_diff =
            (current_time.tv_sec - last_time.tv_sec) + (current_time.tv_nsec - last_time.tv_nsec) / 1000000000.0;
    if (time_diff >= 0.25) {
        float current_fps = frame_count / time_diff;
        update_fps_text(current_fps);
        frame_count = 0;
        last_time = current_time;
    }

    if (!isCrashed) {
        float groundHeight = get_terrain_height(planes[0].position[0], planes[0].position[2]);
        float heightAboveGround = planes[0].position[1] - groundHeight;

        if (heightAboveGround < 0.5f) {
            isCrashed = true;
            hasSetCrashView = false;
            glm_vec3_copy(planes[0].position, crashPosition);
            crashPosition[1] = groundHeight;
        }

        if (automaticCameraMovement) {
            vec3 move_vector;
            glm_vec3_scale(planes[0].front, currentMovementSpeed * deltaTime, move_vector);
            glm_vec3_add(planes[0].position, move_vector, planes[0].position);
            planes[0].position[1] -= gravitySpeed * deltaTime;
        }

        vec3 worldUp = {0.0f, 1.0f, 0.0f};
        vec3 cross_product;
        glm_vec3_cross(worldUp, planes[0].up, cross_product);
        float roll_rad = acosf(glm_vec3_dot(worldUp, planes[0].up));
        if (glm_vec3_dot(cross_product, planes[0].front) < 0)
            roll_rad = -roll_rad;

        vec3 velocity;
        glm_vec3_scale(planes[0].front, currentMovementSpeed, velocity);
        float hSpeed = sqrtf(velocity[0] * velocity[0] + velocity[2] * velocity[2]);
        float vSpeed = velocity[1];

        update_osd_texts(currentMovementSpeed, isSpeedFixed, planes[0].position[1], heightAboveGround, glm_deg(roll_rad),
                         glm_deg(asinf(planes[0].front[1])), hSpeed, vSpeed, elapsedTime);

        if (heightAboveGround < 30.0f) {
            update_status_text("WARNING: PULL UP", 1.0f, 0.0f, 0.0f);
        } else if (isAutopilotOn) {
            update_status_text("AUTOPILOT ENGAGED", 0.0f, 1.0f, 0.0f);
        } else {
            update_status_text("FLIGHT STATUS: SAFE", 0.0f, 1.0f, 1.0f);
        }
    } else {
        if (!hasSetCrashView) {
            whereItCrashed();
            hasSetCrashView = true;
        }
        update_crash_text();
    }

    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4 proj, view;
    glm_perspective(glm_rad(fov), (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 20000.0f, proj);

    if (!isCrashed) {
        update_camera_and_view_matrix(view);
    } else {
        vec3 center;
        glm_vec3_add(cameraPos, cameraFront, center);
        glm_lookat(cameraPos, center, cameraUp, view);
    }

    draw_skybox(view, proj);
    update_chunks();
    draw_chunks(view, proj);

    update_minimap_dot();

    if (!isCrashed) {
        for (int i=0; i<MAX_PLANES; i++) {
            draw_plane(&planes[i], view, proj);
        }
    }

    glDisable(GL_DEPTH_TEST);
    if (isCrashed) {
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

void update_camera_and_view_matrix(mat4 view) {
    if (isCrashed) {
        vec3 center;
        glm_vec3_add(cameraPos, cameraFront, center);
        glm_lookat(cameraPos, center, cameraUp, view);
        return;
    }
    mat4 trans, rot;
    glm_translate_make(trans, planes[0].position);
    mat4 plane_orientation;
    glm_mat4_identity(plane_orientation);
    glm_vec3_copy(planes[0].right, plane_orientation[0]);
    glm_vec3_copy(planes[0].up, plane_orientation[1]);
    vec3 negatedFront;
    glm_vec3_negate_to(planes[0].front, negatedFront);
    glm_vec3_copy(negatedFront, plane_orientation[2]);
    glm_mat4_copy(plane_orientation, rot);
    glm_mat4_mul(trans, rot, planes[0].modelMatrix);
    vec3 behindVec, aboveVec, rightVec;
    glm_vec3_scale(planes[0].front, offset_behind, behindVec);
    glm_vec3_scale(planes[0].up, offset_above, aboveVec);
    glm_vec3_scale(planes[0].right, offset_right, rightVec);
    glm_vec3_sub(planes[0].position, behindVec, cameraPos);
    glm_vec3_add(cameraPos, aboveVec, cameraPos);
    glm_vec3_add(cameraPos, rightVec, cameraPos);
    vec3 lookAtTarget;
    if (offset_behind <= 0.0f && offset_behind >= -70.0f) {
        glm_vec3_add(cameraPos, planes[0].front, lookAtTarget);
    } else {
        glm_vec3_add(planes[0].position, planes[0].front, lookAtTarget);
    }
    glm_lookat(cameraPos, lookAtTarget, planes[0].up, view);
}
void processInput() {
    if (!isCrashed) {
        if (glfwGetKey(window, GLFW_KEY_1)) {
            offset_behind = 400.0f;
            offset_above = 170.0f;
            offset_right = 0.0f;
        } else if (glfwGetKey(window, GLFW_KEY_2)) {
            offset_behind = -50.0f;
            offset_above = 30.0f;
            offset_right = 0.0f;
        } else if (glfwGetKey(window, GLFW_KEY_3)) {
            offset_behind = 400.0f;
            offset_above = 500.0f;
            offset_right = 0.0f;
        } else if (glfwGetKey(window, GLFW_KEY_4)) {
            offset_behind = -300.0f;
            offset_above = 100.0f;
            offset_right = 0.0f;
        } else if (glfwGetKey(window, GLFW_KEY_5)) {
            offset_behind = -200.0f;
            offset_above = 150.0f;
            offset_right = 250.0f;
        } else if (glfwGetKey(window, GLFW_KEY_6)) {
            offset_behind = -200.0f;
            offset_above = 150.0f;
            offset_right = -250.0f;
        } else if (glfwGetKey(window, GLFW_KEY_7)) {
            offset_behind = -300.0f;
            offset_above = -100.0f;
            offset_right = 0.0f;
        } else if (glfwGetKey(window, GLFW_KEY_8)) {
            offset_behind = -75.0f;
            offset_above = 200.0f;
            offset_right = 550.0f;
        }
    }
    static bool a_key_pressed_last_frame = false;
    bool a_key_is_pressed_now = (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
    if (a_key_is_pressed_now && !a_key_pressed_last_frame) {
        isAutopilotOn = !isAutopilotOn;
        fixedValue = currentMovementSpeed;
        if (isAutopilotOn) {
            printf("SCRIPTED AUTOPILOT ENGAGED\n");
            reset_autopilot_state();
            isSpeedFixed = true;
            currentMovementSpeed = fixedValue;
        } else {
            printf("AUTOPILOT DISENGAGED\n");
            isSpeedFixed = false;
        }
    }
    a_key_pressed_last_frame = a_key_is_pressed_now;
    static bool t_key_pressed_last_frame = false;
    bool t_key_is_pressed_now = (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS);
    if (t_key_is_pressed_now && !t_key_pressed_last_frame) {
        isTriangleViewMode = !isTriangleViewMode;
        if (isTriangleViewMode) {
            printf("GRID VIEW MODE: ON\n");
        } else {
            printf("GRID VIEW MODE: OFF\n");
        }
    }
    t_key_pressed_last_frame = t_key_is_pressed_now;
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, 1);
    if (isCrashed) {
        isSpeedFixed = false;
        isAutopilotOn = false;
        currentMovementSpeed = initialMovementSpeed;
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            isCrashed = false;
            hasSetCrashView = true;
            for (int i=0; i<MAX_PLANES; i++) {
                init_flight_model(planes[i]);
            }
            reset_minimap_for_restart();
            lastAltitude = planes[0].position[1];
        }
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
            glm_vec3_copy(crashPosition, planes[0].position);

            planes[0].position[1] += 250.0f;
            vec3 worldUp = {0.0f, 1.0f, 0.0f};
            glm_vec3_copy((vec3) {0.0f, 0.0f, -1.0f}, planes[0].front);
            glm_vec3_copy(worldUp, planes[0].up);
            glm_vec3_cross(planes[0].front, planes[0].up, planes[0].right);
            glm_normalize(planes[0].right);
            isCrashed = false;
            hasSetCrashView = false;
        }
        return;
    }
    if (isAutopilotOn) {
        autoPilotMode();
        if (startingAutoPilotMode) {
            currentMovementSpeed = initalAutoPilotModeSpeed;
            fixedValue = currentMovementSpeed;
            startingAutoPilotMode = false;
        } else {
            currentMovementSpeed = fixedValue;
        }

        return;
    }
    static bool s_key_pressed_last_frame = false;
    bool s_key_is_pressed_now = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);
    if (s_key_is_pressed_now && !s_key_pressed_last_frame) {
        isSpeedFixed = !isSpeedFixed;
        if (isSpeedFixed)
            fixedValue = currentMovementSpeed;
    }
    s_key_pressed_last_frame = s_key_is_pressed_now;
    if (isSpeedFixed) {
        currentMovementSpeed = fixedValue;
    } else {
        float maxSpeed = initialMovementSpeed * speedBoostMultiplier;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
            currentMovementSpeed += accelerationSpeeding * deltaTime;
            if (currentMovementSpeed > maxSpeed)
                currentMovementSpeed = maxSpeed;
            else
                offset_behind += accelerationGoingFarPerSecond;
        } else {
            currentMovementSpeed -= accelerationSlowing * deltaTime * 2.0f;
            if (currentMovementSpeed < initialMovementSpeed)
                currentMovementSpeed = initialMovementSpeed;
            else
                offset_behind -= accelerationGoingFarPerSecond / speedingSlowingRatio;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
        fov -= zoomingSpeed * deltaTime;
        if (fov < maximumZoomDistance)
            fov = maximumZoomDistance;
    } else {
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
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        isTurning = true;
        applyLeftTurn(rotation_speed, roll_speed);
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        isTurning = true;
        applyRightTurn(rotation_speed, roll_speed);
    }
    if (!isTurning)
        applyAutoLeveling();
    glm_normalize(planes[0].front);
    glm_vec3_cross(planes[0].front, planes[0].up, planes[0].right);
    glm_normalize(planes[0].right);
    glm_vec3_cross(planes[0].right, planes[0].front, planes[0].up);
    glm_normalize(planes[0].up);
}
void CLEANUP_SYSTEM() {
    cleanup_plane();
    cleanup_heightmap();
    cleanup_skybox();
    cleanup_minimap();
    cleanup_ui();
    cleanup_crash_marker();
    cleanup_osd();
}
void whereItCrashed() {
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
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    (void) window;
    glViewport(0, 0, width, height);
}
GLuint compileShader(const char *source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        char *info = (char *) malloc(length);
        glGetShaderInfoLog(shader, length, NULL, info);
        fprintf(stderr, "Error compiling shader: %s\n", info);
        free(info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
GLuint createShaderProgram(const char *vsSource, const char *fsSource) {
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
    if (status == GL_FALSE) {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        char *info = (char *) malloc(length);
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
