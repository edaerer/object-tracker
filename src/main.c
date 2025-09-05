#define _POSIX_C_SOURCE 200809L
/* =============================================================
 *  Flight / Terrain Demo - Refactored main.c
 *  Key changes vs original:
 *    - Frame timing & deltaTime computed once per loop (at top)
 *    - All simulation & game logic moved into update_system()
 *    - DRAW_SYSTEM() now ONLY renders (no timing logic)
 *    - detect_planes() throttled (configurable)
 *    - Optional manual frame cap when VSync disabled
 *    - Cleaner FPS calculation (every 0.25 s)
 *
 *  Tunables (see CONFIGURATION section below)
 * ============================================================= */

#include "globals.h"
#include "heightMap.h"
#include "minimap.h"
#include "plane.h"
#include "skybox.h"
#include "textShowing.h"

#define DARKNET_INCLUDE_ORIGINAL_API
#include "darknet.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

/* =============================================================
 * CONFIGURATION
 * ============================================================= */
#define KEYBOARD_ENABLED 1

/* 1 = use monitor vsync pacing (glfwSwapInterval(1))
 * 0 = no vsync (glfwSwapInterval(0)) and you may enable TARGET_FPS below */
#define USE_VSYNC 1

/* Manual frame cap (Hz) used only if USE_VSYNC == 0 and > 0.0
 * Set to 0.0 for uncapped (not recommended) */
#define TARGET_FPS 0.0

/* Throttle object detection (YOLO) – seconds between runs
 * Example: 0.10 = every 100 ms (~10 Hz) */
#define DETECTION_INTERVAL 0.10

/* Clamp for a single frame delta (to avoid spiral of death after stalls) */
#define MAX_FRAME_DELTA 0.25

/* =============================================================
 * GLOBAL STATE (original + required)
 * ============================================================= */
GLFWwindow *window;
const int SCR_WIDTH  = 1920;
const int SCR_HEIGHT = 1080;

bool  isCrashed               = false;
mat4  g_view, g_proj;
vec3  lightDirection          = {0.0f, 1.0f, 0.0f};
vec3  crashPosition;
bool  hasSetCrashView         = false;
float crashedScenarioCameraHeight = 450.0f;
bool  automaticCameraMovement = true;
float gravitySpeed            = 0.0f;
bool  isSpeedFixed            = false;
float fixedValue              = 0.0f;
float accelerationGoingFarPerSecond = 0.5f;
const float accelerationSpeeding     = 200.0f;
const float speedingSlowingRatio     = 2.5f;
const float accelerationSlowing      = (accelerationSpeeding / speedingSlowingRatio);
const float speedBoostMultiplier     = 12.5f;
float currentMovementSpeed;
float initialMovementSpeed     = 100.0f;
float initalAutoPilotModeSpeed = 100.0f;
bool  startingAutoPilotMode    = true;
float fov                      = 45.0f;
const float originalFov        = 45.0f;
float zoomingSpeed             = 20.0f;
float maximumZoomDistance      = 10.0f;
float bow_rate                 = 30.0f;
float autoLevelSpeed           = 1.0f;
float verticalSpeed            = 0.0f;
static float lastAltitude      = 0.0f;

/* REFACTORED: global deltaTime is now set once per frame in main loop */
float deltaTime = 0.0f;

/* Timing accumulators */
static double g_start_time     = 0.0;
static double g_last_time      = 0.0;
static double fps_accum        = 0.0;
static int    fps_frames       = 0;

vec3 cameraPos, cameraFront, cameraUp, cameraRight;
bool  isAutopilotOn            = true;
float offset_behind            = 400.0f;
float offset_above             = 170.0f;
float offset_right             = 0.0f;

int terrain_width, terrain_height;
const unsigned char *heightMapImageData = NULL;
GLuint heightMapTextureID;
const float HEIGHT_SCALE_FACTOR = 500.0f;
bool  isTriangleViewMode = false;

static int   screenshot_index      = 0;
static GLuint box_vbo              = 0;
static GLuint box_shader_program   = 0;
static network *g_net              = NULL;
static int   g_classes             = 0;
static float g_thresh              = 0.25f;
static float g_nms                 = 0.45f;

static double last_detection_time  = 0.0;

/* Forward declarations (some from original) */
void INIT_SYSTEM();
void DRAW_SYSTEM(); /* rendering only now */
void CLEANUP_SYSTEM();
void update_camera_and_view_matrix(mat4 view);
void processInput(); /* still uses global deltaTime for now */
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void whereItCrashed();
void detect_planes();
void init_darknet(const char *cfg, const char *weights);
void init_box_drawing();
GLuint createShaderProgram(const char *vsSource, const char *fsSource);

/* =============================================================
 * NEW: Central per-frame simulation function
 * ============================================================= */
static void update_system(double dt);

/* =============================================================
 * Utilities
 * ============================================================= */
static double get_current_time_seconds() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

/* Image conversion for detection */
static image make_image_from_gl_rgb(const unsigned char *rgb, int w, int h, int flip_y){
    image im = make_image(w, h, 4); /* CHW */
    for (int y = 0; y < h; ++y) {
        int src_y = flip_y ? (h - 1 - y) : y;
        const unsigned char *row = rgb + (size_t)src_y * w * 4;
        for (int x = 0; x < w; ++x) {
            int idx = x * 4;
            im.data[0 * w * h + y * w + x] = row[idx + 0] / 255.0f;
            im.data[1 * w * h + y * w + x] = row[idx + 1] / 255.0f;
            im.data[2 * w * h + y * w + x] = row[idx + 2] / 255.0f;
        }
    }
    return im;
}

static void build_plane_model_matrix(const Plane *p, mat4 model) {
    mat4 rot, trans;
    glm_mat4_identity(rot);
    glm_vec3_copy(p->right, rot[0]);
    glm_vec3_copy(p->up,    rot[1]);
    vec3 nf; glm_vec3_negate_to(p->front, nf);
    glm_vec3_copy(nf,       rot[2]); /* OpenGL forward -Z */
    glm_translate_make(trans, p->position);
    glm_mat4_mul(trans, rot, model);
}

static int project_screen_from_clip(const vec4 clip, int sw, int sh, float *sx, float *sy) {
    if (clip[3] <= 0.0f) return 0;
    float ndc_x = clip[0] / clip[3];
    float ndc_y = clip[1] / clip[3];
    *sx = (ndc_x * 0.5f + 0.5f) * (float)sw;
    *sy = (1.0f - (ndc_y * 0.5f + 0.5f)) * (float)sh;
    return 1;
}

/* =============================================================
 * MAIN
 * ============================================================= */
int main() {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                              "Height Map Terrain Flight", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

#if USE_VSYNC
    glfwSwapInterval(1);
#else
    glfwSwapInterval(0);
#endif

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    INIT_SYSTEM();
    init_box_drawing();
    init_darknet("yolov4-tiny.cfg", "yolov4-tiny-best.weights");

    g_last_time   = get_current_time_seconds();
    g_start_time  = g_last_time;
    last_detection_time = g_last_time;

    while (!glfwWindowShouldClose(window)) {
        /* 1. Timing */
        double now = get_current_time_seconds();
        double dt  = now - g_last_time;
        if (dt > MAX_FRAME_DELTA) dt = MAX_FRAME_DELTA;
        g_last_time = now;
        deltaTime = (float)dt; /* Keep legacy global usage */

        /* 2. Input */
        if (KEYBOARD_ENABLED) {
            processInput();  /* uses global deltaTime */
        } else {
            if (startingAutoPilotMode) {
                currentMovementSpeed = 300.0f;
                startingAutoPilotMode = false;
            }
            autoPilotMode(); /* existing function, assumed elsewhere */
        }

        /* 3. Simulation / Game logic */
        update_system(dt);

        /* 4. Rendering */
        DRAW_SYSTEM();

        /* 5. Throttled detection (expensive glReadPixels) */
        double detect_now = now;
        if (detect_now - last_detection_time >= DETECTION_INTERVAL) {
            detect_planes();
            last_detection_time = detect_now;
        }

        /* 6. Present & events */
        glfwSwapBuffers(window);
        glfwPollEvents();

#if !USE_VSYNC
        if (TARGET_FPS > 0.0) {
            double frame_end = get_current_time_seconds();
            double frame_time = frame_end - now;
            double target = 1.0 / TARGET_FPS;
            double remaining = target - frame_time;
            if (remaining > 0.0005) {
                struct timespec ts;
                ts.tv_sec  = (time_t)remaining;
                ts.tv_nsec = (long)((remaining - (double)ts.tv_sec) * 1e9);
                if (ts.tv_nsec < 0) ts.tv_nsec = 0;
                nanosleep(&ts, NULL);
            }
        }
#endif
    }

    CLEANUP_SYSTEM();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

/* =============================================================
 * SYSTEM INITIALIZATION
 * ============================================================= */
void INIT_SYSTEM() {
    printf("Initializing system...\n");
    for (int i = 0; i < MAX_PLANES; i++) {
        init_flight_model(planes[i]);
    }

    if (!init_skybox())          exit(-1);
    if (!init_ui())              exit(-1);
    if (!init_crash_marker())    exit(-1);
    if (!init_plane())           exit(-1);

    init_heightmap_texture();
    init_chunks();
    init_osd();
    init_minimap();

    lastAltitude = planes[0].position[1];

    fps_accum  = 0.0;
    fps_frames = 0;

    printf("Initialization successful.\n");
}

/* =============================================================
 * UPDATE (Simulation)
 * ============================================================= */
static void update_system(double dt) {
    double elapsedTime = g_last_time - g_start_time;

    /* Plane movement (player) – was in DRAW_SYSTEM before */
    if (!isCrashed && automaticCameraMovement) {
        vec3 move_vector;
        glm_vec3_scale(planes[0].front, currentMovementSpeed * (float)dt, move_vector);
        glm_vec3_add(planes[0].position, move_vector, planes[0].position);
        planes[0].position[1] -= gravitySpeed * (float)dt;
    }

    /* Enemy planes */
    for (int i = 1; i < MAX_PLANES; i++) {
        update_enemy_plane(&planes[i]); /* could pass dt if you refactor */
    }

    /* Vertical speed & altitude tracking */
    if (!isCrashed) {
        if (dt > 0.0) {
            verticalSpeed = (planes[0].position[1] - lastAltitude) / (float)dt;
        }
        lastAltitude = planes[0].position[1];
    }

    /* Crash detection & OSD updates */
    if (!isCrashed) {
        float groundHeight      = get_terrain_height(planes[0].position[0], planes[0].position[2]);
        float heightAboveGround = planes[0].position[1] - groundHeight;

        if (heightAboveGround < 0.5f) {
            isCrashed = true;
            hasSetCrashView = false;
            glm_vec3_copy(planes[0].position, crashPosition);
            crashPosition[1] = groundHeight;
        }

        /* Roll calculation */
        vec3 worldUp = {0.0f, 1.0f, 0.0f};
        vec3 cross_product;
        glm_vec3_cross(worldUp, planes[0].up, cross_product);
        float roll_rad = acosf(glm_vec3_dot(worldUp, planes[0].up));
        if (glm_vec3_dot(cross_product, planes[0].front) < 0)
            roll_rad = -roll_rad;

        vec3 velocity;
        glm_vec3_scale(planes[0].front, currentMovementSpeed, velocity);
        float hSpeed = sqrtf(velocity[0]*velocity[0] + velocity[2]*velocity[2]);
        float vSpeed = velocity[1];

        update_osd_texts(currentMovementSpeed, isSpeedFixed,
                         planes[0].position[1], heightAboveGround,
                         glm_deg(roll_rad), glm_deg(asinf(planes[0].front[1])),
                         hSpeed, vSpeed, elapsedTime);

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

    /* FPS tracking (every 0.25 s) */
    fps_accum  += dt;
    fps_frames += 1;
    if (fps_accum >= 0.25) {
        float fps = (float)(fps_frames / fps_accum);
        update_fps_text(fps);
        fps_accum  = 0.0;
        fps_frames = 0;
    }
}

/* =============================================================
 * RENDERING (DRAW_SYSTEM simplified)
 * ============================================================= */
void DRAW_SYSTEM() {
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm_perspective(glm_rad(fov),
                    (float)SCR_WIDTH / (float)SCR_HEIGHT,
                    5.0f, 5000.0f, g_proj);

    if (!isCrashed) {
        update_camera_and_view_matrix(g_view);
    } else {
        vec3 center;
        glm_vec3_add(cameraPos, cameraFront, center);
        glm_lookat(cameraPos, center, cameraUp, g_view);
    }

    draw_skybox(g_view, g_proj);
    update_chunks();
    draw_chunks(g_view, g_proj);
    update_minimap_dot();

    if (!isCrashed) {
        for (int i = 0; i < MAX_PLANES; i++) {
            draw_plane(&planes[i], g_view, g_proj);
        }
    }

    glDisable(GL_DEPTH_TEST);
    if (isCrashed) {
        draw_crash_marker(g_view, g_proj);
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

/* =============================================================
 * CAMERA
 * ============================================================= */
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
    glm_vec3_copy(planes[0].up,    plane_orientation[1]);
    vec3 negatedFront;
    glm_vec3_negate_to(planes[0].front, negatedFront);
    glm_vec3_copy(negatedFront, plane_orientation[2]);

    glm_mat4_copy(plane_orientation, rot);
    glm_mat4_mul(trans, rot, planes[0].modelMatrix);

    vec3 behindVec, aboveVec, rightVec;
    glm_vec3_scale(planes[0].front, offset_behind, behindVec);
    glm_vec3_scale(planes[0].up,    offset_above,  aboveVec);
    glm_vec3_scale(planes[0].right, offset_right,  rightVec);

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

void project_point(const vec3 world, const mat4 view, const mat4 proj,
                   int screen_w, int screen_h, float *out_x, float *out_y) {
    vec4 tmp = {world[0], world[1], world[2], 1.0f};
    vec4 clip;
    glm_mat4_mulv(view, tmp, tmp);
    glm_mat4_mulv(proj, tmp, clip);
    if (clip[3] != 0.0f) {
        clip[0] /= clip[3];
        clip[1] /= clip[3];
    }
    *out_x = (clip[0] * 0.5f + 0.5f) * screen_w;
    *out_y = (clip[1] * 0.5f + 0.5f) * screen_h;
}

/* =============================================================
 * DARKNET / YOLO INIT
 * ============================================================= */
void init_darknet(const char *cfg, const char *weights) {
    g_net = load_network((char*)cfg, (char*)weights, 0);
    set_batch_network(g_net, 1);
    fuse_conv_batchnorm(*g_net);

    layer l = g_net->layers[g_net->n - 1];
    g_classes = l.classes;
}

/* =============================================================
 * BOX RENDERING (2D overlay)
 * ============================================================= */
void init_box_drawing() {
    const char *vertex_shader =
        "attribute vec2 position;\n"
        "void main() {\n"
        "  gl_Position = vec4(position, 0.0, 1.0);\n"
        "}\n";

    const char *fragment_shader =
        "precision mediump float;\n"
        "uniform vec4 color;\n"
        "void main() {\n"
        "  gl_FragColor = color;\n"
        "}\n";

    box_shader_program = createShaderProgram(vertex_shader, fragment_shader);

    glGenBuffers(1, &box_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, box_vbo);
    glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void drawBoundingBox(int left, int top, int right, int bottom) {
    float normalized_left   = 2.0f * left  / SCR_WIDTH  - 1.0f;
    float normalized_right  = 2.0f * right / SCR_WIDTH  - 1.0f;
    float normalized_top    = 1.0f - 2.0f * top    / SCR_HEIGHT;
    float normalized_bottom = 1.0f - 2.0f * bottom / SCR_HEIGHT;

    float vertices[] = {
        normalized_left,  normalized_top,
        normalized_right, normalized_top,
        normalized_right, normalized_bottom,
        normalized_left,  normalized_bottom
    };

    glUseProgram(box_shader_program);
    GLint color_loc = glGetUniformLocation(box_shader_program, "color");
    glUniform4f(color_loc, 1.0f, 0.0f, 0.0f, 1.0f);

    glBindBuffer(GL_ARRAY_BUFFER, box_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    GLint position_attr = glGetAttribLocation(box_shader_program, "position");
    glEnableVertexAttribArray(position_attr);
    glVertexAttribPointer(position_attr, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glDrawArrays(GL_LINE_LOOP, 0, 4);

    glDisableVertexAttribArray(position_attr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* =============================================================
 * OBJECT DETECTION (Throttled)
 * ============================================================= */
void detect_planes(void) {
    if (!g_net) {
        fprintf(stderr, "Darknet network not loaded.\n");
        return;
    }

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);

    /* --- Static reusable pixel buffer to avoid malloc every call --- */
    static unsigned char *pixel_buffer = NULL;
    static size_t pixel_capacity = 0;
    size_t need = (size_t)fbw * (size_t)fbh * 4;
    if (need > pixel_capacity) {
        unsigned char *tmp = realloc(pixel_buffer, need);
        if (!tmp) {
            fprintf(stderr, "Failed to allocate %zu bytes for pixel buffer\n", need);
            return;
        }
        pixel_buffer = tmp;
        pixel_capacity = need;
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    /* glFlush() not needed; glReadPixels will sync as required */
    glReadPixels(0, 0, fbw, fbh, GL_RGBA, GL_UNSIGNED_BYTE, pixel_buffer);

    /* Build Darknet image (CHW, normalized) */
    image im = make_image_from_gl_rgb(pixel_buffer, fbw, fbh, 1);

    int netw = g_net->w;
    int neth = g_net->h;

    /* Letterbox to network dims (alloc each call; can optimize later) */
    image sized = letterbox_image(im, netw, neth);

    network_predict(*g_net, sized.data);

    int nboxes = 0;

    /* IMPORTANT:
       Pass letter=1 so Darknet internally accounts for letterbox padding
       OR pass letter=0 and call correct_yolo_boxes manually.
       Here we pass letter=1.
       Signature: get_network_boxes(net, orig_w, orig_h, thresh, hier, map, relative, &nboxes, letter)
    */
    detection *dets = get_network_boxes(g_net, im.w, im.h, g_thresh, 0.5f, 0, 1, &nboxes, 1);

    if (dets && nboxes > 0) {
        do_nms_sort(dets, nboxes, g_net->layers[g_net->n - 1].classes, g_nms);
    }

    for (int i = 0; i < nboxes; i++) {
        for (int c = 0; c < g_net->layers[g_net->n - 1].classes; c++) {
            float p = dets[i].prob ? dets[i].prob[c] : 0.0f;
            if (p > 0.25f) {
                box bbox = dets[i].bbox;

                /* bbox.* are still normalized (0–1) (because relative=1).
                   Convert to pixel coordinates of ORIGINAL framebuffer size (im.w, im.h). */
                float bx = bbox.x;
                float by = bbox.y;
                float bw = bbox.w;
                float bh = bbox.h;

                int left   = (int)((bx - bw / 2.0f) * im.w);
                int right  = (int)((bx + bw / 2.0f) * im.w);
                int top    = (int)((by - bh / 2.0f) * im.h);
                int bottom = (int)((by + bh / 2.0f) * im.h);

                if (left < 0) left = 0;
                if (right > im.w) right = im.w;
                if (top < 0) top = 0;
                if (bottom > im.h) bottom = im.h;

                if (right - left > 1 && bottom - top > 1) {
                    /* Draw in screen pixel space */
                    drawBoundingBox(left, top, right, bottom);
                }
            }
        }
    }

    if (dets) free_detections(dets, nboxes);
    free_image(sized);
    free_image(im);
}

/* =============================================================
 * INPUT HANDLING (unchanged except comments)
 * ============================================================= */
void processInput() {
    if (!isCrashed) {
        if (glfwGetKey(window, GLFW_KEY_1)) { offset_behind = 400.0f; offset_above = 170.0f; offset_right = 0.0f; }
        else if (glfwGetKey(window, GLFW_KEY_2)) { offset_behind = -50.0f; offset_above = 30.0f; offset_right = 0.0f; }
        else if (glfwGetKey(window, GLFW_KEY_3)) { offset_behind = 400.0f; offset_above = 500.0f; offset_right = 0.0f; }
        else if (glfwGetKey(window, GLFW_KEY_4)) { offset_behind = -300.0f; offset_above = 100.0f; offset_right = 0.0f; }
        else if (glfwGetKey(window, GLFW_KEY_5)) { offset_behind = -200.0f; offset_above = 150.0f; offset_right = 250.0f; }
        else if (glfwGetKey(window, GLFW_KEY_6)) { offset_behind = -200.0f; offset_above = 150.0f; offset_right = -250.0f; }
        else if (glfwGetKey(window, GLFW_KEY_7)) { offset_behind = -300.0f; offset_above = -100.0f; offset_right = 0.0f; }
        else if (glfwGetKey(window, GLFW_KEY_8)) { offset_behind = -75.0f;  offset_above = 200.0f; offset_right = 550.0f; }
    }

    static bool a_key_pressed_last_frame = false;
    bool a_key_now = (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
    if (a_key_now && !a_key_pressed_last_frame) {
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
    a_key_pressed_last_frame = a_key_now;

    static bool t_key_pressed_last_frame = false;
    bool t_key_now = (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS);
    if (t_key_now && !t_key_pressed_last_frame) {
        isTriangleViewMode = !isTriangleViewMode;
        printf("GRID VIEW MODE: %s\n", isTriangleViewMode ? "ON" : "OFF");
    }
    t_key_pressed_last_frame = t_key_now;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, 1);

    if (isCrashed) {
        isSpeedFixed   = false;
        isAutopilotOn  = false;
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
            glm_vec3_copy((vec3){0.0f, 0.0f, -1.0f}, planes[0].front);
            glm_vec3_copy(worldUp, planes[0].up);
            glm_vec3_cross(planes[0].front, planes[0].up, planes[0].right);
            glm_normalize(planes[0].right);
            isCrashed       = false;
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
    bool s_key_now = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);
    if (s_key_now && !s_key_pressed_last_frame) {
        isSpeedFixed = !isSpeedFixed;
        if (isSpeedFixed) fixedValue = currentMovementSpeed;
    }
    s_key_pressed_last_frame = s_key_now;

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
    float roll_speed     = bow_rate * deltaTime;
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

/* =============================================================
 * CRASH CAMERA
 * ============================================================= */
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

/* =============================================================
 * FRAMEBUFFER RESIZE
 * ============================================================= */
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
}

/* =============================================================
 * SHADERS
 * ============================================================= */
GLuint compileShader(const char *source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        char *info = (char*)malloc((size_t)length);
        glGetShaderInfoLog(shader, length, NULL, info);
        fprintf(stderr, "Error compiling shader: %s\n", info);
        free(info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint createShaderProgram(const char *vsSource, const char *fsSource) {
    GLuint vertexShader   = compileShader(vsSource, GL_VERTEX_SHADER);
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
        char *info = (char*)malloc((size_t)length);
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

/* =============================================================
 * CLEANUP
 * ============================================================= */
void CLEANUP_SYSTEM() {
    cleanup_plane();
    cleanup_heightmap();
    cleanup_skybox();
    cleanup_minimap();
    cleanup_ui();
    cleanup_crash_marker();
    cleanup_osd();

    if (box_vbo)            glDeleteBuffers(1, &box_vbo);
    if (box_shader_program) glDeleteProgram(box_shader_program);
}