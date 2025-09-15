#define _POSIX_C_SOURCE 200809L
#include "globals.h"
#include "heightMap.h"
#include "minimap.h"
#include "plane.h"
#include "skybox.h"
#include "textShowing.h"
#include "onnx.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

/* ---- Runtime / build options ---- */
#define KEYBOARD_ENABLED 1
#define USE_VSYNC 1
#define TARGET_FPS 0.0
#define MAX_FRAME_DELTA 0.25

/* ---- Window ---- */
GLFWwindow *window;
const int SCR_WIDTH  = 1920;
const int SCR_HEIGHT = 1080;

/* ---- Globals (shared across modules) ---- */
bool  isCrashed = false;
mat4  g_view, g_proj;
vec3  lightDirection = {0.0f, 1.0f, 0.0f};

vec3  crashPosition;
bool  hasSetCrashView = false;
float crashedScenarioCameraHeight = 450.0f;
bool  automaticCameraMovement = true;
float gravitySpeed = 0.0f;

bool  isSpeedFixed = false;
float fixedValue = 0.0f;
float accelerationGoingFarPerSecond = 0.5f;
const float accelerationSpeeding = 200.0f;
const float speedingSlowingRatio = 2.5f;
const float accelerationSlowing  = (accelerationSpeeding / speedingSlowingRatio);
const float speedBoostMultiplier = 12.5f;

float currentMovementSpeed;
float initialMovementSpeed      = 300.0f;
float initalAutoPilotModeSpeed  = 650.0f;
bool  startingAutoPilotMode     = true;

float fov               = 45.0f;
const float originalFov = 45.0f;
float zoomingSpeed      = 20.0f;
float maximumZoomDistance = 10.0f;
float bow_rate          = 30.0f;
float autoLevelSpeed    = 1.0f;
float verticalSpeed     = 0.0f;
static float lastAltitude = 0.0f;

float  deltaTime = 0.0f;
static double g_start_time = 0.0;
static double g_last_time  = 0.0;
static double fps_accum    = 0.0;
static int    fps_frames   = 0;

vec3 cameraPos, cameraFront, cameraUp, cameraRight;
bool  isAutopilotOn   = true;
float offset_behind   = 400.0f;
float offset_above    = 170.0f;
float offset_right    = 0.0f;

int terrain_width, terrain_height;
const unsigned char *heightMapImageData = NULL;
GLuint heightMapTextureID;
const float HEIGHT_SCALE_FACTOR = 500.0f;
bool  isTriangleViewMode = false;

/* ---- Simple shader for 2D box overlay ---- */
static GLuint box_vbo = 0;
static GLuint box_shader_program = 0;

/* ---- ONNX detector ---- */
#define DETECTION_MODEL_PATH "./models/model.onnx"
static OnnxDetector g_detector;
static int          g_detector_ready = 0;
static float        g_thresh = 0.6f;
static float        g_nms    = 0.45f;
static double       g_last_det_ms = 0.0;

/* ---- Detector readback buffers ---- */
static unsigned char *g_rgba_buffer = NULL;
static unsigned char *g_rgb_buffer  = NULL;
static size_t         g_frame_buf_capacity = 0;

/* ---- 608x608 detector FBO (letterboxed) ---- */
#define DET_W 608
#define DET_H 608
static GLuint g_det_fbo       = 0;
static GLuint g_det_color_tex = 0;
static GLuint g_det_depth_rbo = 0;

/* ---- Forward decls (public) ---- */
void INIT_SYSTEM(void);
void DRAW_SYSTEM(void);
void detect_planes(void);
void processInput(void);
void whereItCrashed(void);
bool init_detection_fbo(void);
static void RENDER_FOR_DET(int w, int h);
static inline void flip_rgb_inplace(unsigned char* data, int W, int H);
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void CLEANUP_SYSTEM(void);

/* ---- Helpers (internal) ---- */
typedef struct { GLint x, y, w, h; } ViewRect;

static double get_current_time_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}
static double get_current_time_millis(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec * 1000.0 + (double)now.tv_nsec / 1e6;
}

static void update_system(double dt) {
    double elapsedTime = g_last_time - g_start_time;

    if (!isCrashed && automaticCameraMovement) {
        vec3 move_vector;
        glm_vec3_scale(planes[0].front, currentMovementSpeed * (float)dt, move_vector);
        glm_vec3_add(planes[0].position, move_vector, planes[0].position);
        planes[0].position[1] -= gravitySpeed * (float)dt;
    }

    for (int i = 1; i < MAX_PLANES; i++)
        update_enemy_plane(&planes[i]);

    if (!isCrashed) {
        if (dt > 0.0)
            verticalSpeed = (planes[0].position[1] - lastAltitude) / (float)dt;
        lastAltitude = planes[0].position[1];
    }

    if (!isCrashed) {
        float groundHeight      = get_terrain_height(planes[0].position[0], planes[0].position[2]);
        float heightAboveGround = planes[0].position[1] - groundHeight;

        if (heightAboveGround < 0.5f) {
            isCrashed = true;
            hasSetCrashView = false;
            glm_vec3_copy(planes[0].position, crashPosition);
            crashPosition[1] = groundHeight;
        }

        vec3 worldUp = {0.0f, 1.0f, 0.0f};
        vec3 cross_product;
        glm_vec3_cross(worldUp, planes[0].up, cross_product);
        float roll_rad = acosf(glm_vec3_dot(worldUp, planes[0].up));
        if (glm_vec3_dot(cross_product, planes[0].front) < 0) roll_rad = -roll_rad;

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

    fps_accum  += dt;
    fps_frames += 1;
    if (fps_accum >= 0.25) {
        float fps = (float)(fps_frames / fps_accum);
        update_fps_text(fps);
        fps_accum  = 0.0;
        fps_frames = 0;
    }
}

/* 608x608 içine, ekranın aspect'ini koruyan letterbox viewport */
static inline ViewRect det_letterbox_rect(int target, float aspect) {
    ViewRect r;
    if (aspect >= 1.f) {
        r.w = target;
        r.h = (int)floorf(target / aspect + 0.5f);
        r.x = 0;
        r.y = (target - r.h) / 2;
    } else {
        r.h = target;
        r.w = (int)floorf(target * aspect + 0.5f);
        r.y = 0;
        r.x = (target - r.w) / 2;
    }
    return r;
}

// 0: benim uçak, 1: yakın enemy, 2: uzak enemy
static inline void class_to_color(int cls, float *r, float *g, float *b) {
    switch (cls) {
        case 0: // benim uçak
            *r = 0.15f; *g = 0.85f; *b = 1.00f;  // camgöbeği
            break;
        case 1: // yakın enemy
            *r = 1.00f; *g = 0.25f; *b = 0.25f;  // kırmızımsı
            break;
        case 2: // uzak enemy
            *r = 1.00f; *g = 0.85f; *b = 0.20f;  // sarı
            break;
        default:
            *r = 1.00f; *g = 1.00f; *b = 1.00f;  // beyaz (bilinmiyorsa)
            break;
    }
}

/* RGBA → RGB (no alpha), no flip */
/* RGBA -> 3-kanal gri (R=G=B=Y). BT.601: Y = 0.299R + 0.587G + 0.114B */
static inline void rgba_to_gray3(const unsigned char* src, unsigned char* dst, int pixels) {
    for (int i = 0; i < pixels; ++i) {
        unsigned int r = src[4*i + 0];
        unsigned int g = src[4*i + 1];
        unsigned int b = src[4*i + 2];

        unsigned int y = (77*r + 150*g + 29*b + 128) >> 8;

        dst[3*i + 0] = (unsigned char)y;
        dst[3*i + 1] = (unsigned char)y;
        dst[3*i + 2] = (unsigned char)y;
    }
}


/* Vertical flip (in-place) */
static inline void flip_inplace(unsigned char* data, int W, int H, int C) {
    size_t stride = (size_t)W * (size_t)C;
    unsigned char* tmp = (unsigned char*)malloc(stride);
    if (!tmp) return;
    for (int y = 0; y < H/2; ++y) {
        unsigned char* a = data + (size_t)y * stride;
        unsigned char* b = data + (size_t)(H-1-y) * stride;
        memcpy(tmp, a, stride);
        memcpy(a, b, stride);
        memcpy(b, tmp, stride);
    }
    free(tmp);
}

/* Minimal shader util */
GLuint compileShader(const char *source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        char *log = (char*)malloc((size_t)len);
        glGetShaderInfoLog(shader, len, NULL, log);
        fprintf(stderr, "Error compiling shader: %s\n", log);
        free(log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint createShaderProgram(const char *vs, const char *fs) {
    GLuint v = compileShader(vs, GL_VERTEX_SHADER);
    GLuint f = compileShader(fs, GL_FRAGMENT_SHADER);
    if (!v || !f) { if (v) glDeleteShader(v); if (f) glDeleteShader(f); return 0; }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        char *log = (char*)malloc((size_t)len);
        glGetProgramInfoLog(p, len, NULL, log);
        fprintf(stderr, "Error linking program: %s\n", log);
        free(log);
        glDeleteProgram(p);
        p = 0;
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

/* 2D red box overlay (NDC) */
static void init_box_drawing(void) {
    const char *vs =
        "attribute vec2 position;\n"
        "void main(){ gl_Position = vec4(position,0.0,1.0); }\n";
    const char *fs =
        "precision mediump float;\n"
        "uniform vec4 color;\n"
        "void main(){ gl_FragColor = color; }\n";

    box_shader_program = createShaderProgram(vs, fs);
    glGenBuffers(1, &box_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, box_vbo);
    // 48 float = 4 kenar * (2 üçgen * 3 tepe * 2 bileşen)
    glBufferData(GL_ARRAY_BUFFER, 48 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
static void drawBoundingBoxColored(int L, int T, int R, int B,
                                   float thickness_px,
                                   float r, float g, float b, float a) {
    // Ekran -> NDC
    float nl = 2.0f * L / SCR_WIDTH  - 1.0f;
    float nr = 2.0f * R / SCR_WIDTH  - 1.0f;
    float nt = 1.0f - 2.0f * T / SCR_HEIGHT;
    float nb = 1.0f - 2.0f * B / SCR_HEIGHT;

    // Piksel kalınlığını NDC’ye çevir
    float dx = 2.0f * thickness_px / SCR_WIDTH;
    float dy = 2.0f * thickness_px / SCR_HEIGHT;

    // 4 kenar = 4 ince dikdörtgen = 8 üçgen = 24 vertex
    float v[48] = {
        // Sol
        nl,     nt,   nl+dx, nt,   nl+dx, nb,
        nl,     nt,   nl+dx, nb,   nl,     nb,

        // Üst
        nl,     nt,   nr,    nt,   nr,    nt - dy,
        nl,     nt,   nr,    nt - dy,  nl, nt - dy,

        // Sağ
        nr - dx, nt,  nr,    nt,   nr,    nb,
        nr - dx, nt,  nr,    nb,   nr - dx, nb,

        // Alt
        nl,     nb + dy,  nr, nb + dy,  nr, nb,
        nl,     nb + dy,  nr, nb,       nl, nb
    };

    glUseProgram(box_shader_program);
    glUniform4f(glGetUniformLocation(box_shader_program, "color"), r, g, b, a);

    glBindBuffer(GL_ARRAY_BUFFER, box_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);

    GLint pos = glGetAttribLocation(box_shader_program, "position");
    glEnableVertexAttribArray(pos);
    glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glDisable(GL_DEPTH_TEST);           // overlay olarak üstte kalsın
    glDrawArrays(GL_TRIANGLES, 0, 24);  // 24 vertex
    glEnable(GL_DEPTH_TEST);

    glDisableVertexAttribArray(pos);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


/* 608x608 RGB FBO for detector */
bool init_detection_fbo(void) {
    glGenFramebuffers(1, &g_det_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_det_fbo);

    glGenTextures(1, &g_det_color_tex);
    glBindTexture(GL_TEXTURE_2D, g_det_color_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    /* store as RGBA8888; we’ll drop alpha after readback */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, DET_W, DET_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_det_color_tex, 0);

    glGenRenderbuffers(1, &g_det_depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, g_det_depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, DET_W, DET_H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_det_depth_rbo);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return status == GL_FRAMEBUFFER_COMPLETE;
}

/* Scene render into detector FBO with letterbox */
static void render_for_detection(ViewRect vp, mat4 det_view, mat4 det_proj) {
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, DET_W, DET_H);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(vp.x, vp.y, vp.w, vp.h);
    draw_skybox(det_view, det_proj);
    update_chunks();
    draw_chunks(det_view, det_proj);
    for (int i = 0; i < MAX_PLANES; ++i)
        draw_plane(&planes[i], det_view, det_proj);
}

/* ---- App ---- */
int main(void) {
    if (!glfwInit()) { fprintf(stderr, "Failed to initialize GLFW\n"); return -1; }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Height Map Terrain Flight", NULL, NULL);
    if (!window) { fprintf(stderr, "Failed to create GLFW window\n"); glfwTerminate(); return -1; }
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

    /* ONNX */
    {
        OnnxConfig cfg = onnx_default_config();
        cfg.verbose = 1;
        cfg.score_thresh = g_thresh;
        cfg.nms_iou_thresh = g_nms;
        if (onnx_load_model(&g_detector, DETECTION_MODEL_PATH, &cfg) == ONNX_OK) {
            g_detector_ready = 1;
            printf("[ONNX] Model yüklendi: %s\n", DETECTION_MODEL_PATH);
        } else {
            fprintf(stderr, "[ONNX] Model yükleme başarısız (%s)\n", DETECTION_MODEL_PATH);
        }
    }

    /* Detector FBO */
    if (!init_detection_fbo())
        fprintf(stderr, "[DET-FBO] init failed; default framebuffer fallback will be used.\n");

    g_last_time  = get_current_time_seconds();
    g_start_time = g_last_time;

    while (!glfwWindowShouldClose(window)) {
        double now = get_current_time_seconds();
        double dt  = now - g_last_time;
        if (dt > MAX_FRAME_DELTA) dt = MAX_FRAME_DELTA;
        g_last_time = now;
        deltaTime = (float)dt;

        if (KEYBOARD_ENABLED) {
            processInput();
        } else {
            if (startingAutoPilotMode) {
                currentMovementSpeed = 650.0f;
                startingAutoPilotMode = false;
            }
            autoPilotMode();
        }

        update_system(dt);
        DRAW_SYSTEM();
        detect_planes();

        glfwSwapBuffers(window);
        glfwPollEvents();

#if !USE_VSYNC
        if (TARGET_FPS > 0.0) {
            double frame_end = get_current_time_seconds();
            double remaining = (1.0 / TARGET_FPS) - (frame_end - now);
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

/* ---- Engine ---- */
void INIT_SYSTEM(void) {
    printf("Initializing system...\n");
    for (int i = 0; i < MAX_PLANES; i++) init_flight_model(planes[i]);

    if (!init_skybox())       exit(-1);
    if (!init_ui())           exit(-1);
    if (!init_crash_marker()) exit(-1);
    if (!init_plane())        exit(-1);

    init_heightmap_texture();
    init_chunks();
    init_osd();
    init_minimap();

    lastAltitude = planes[0].position[1];
    fps_accum = 0.0;
    fps_frames = 0;

    printf("Initialization successful.\n");
}

void DRAW_SYSTEM(void) {
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm_perspective(glm_rad(fov), (float)SCR_WIDTH / (float)SCR_HEIGHT, 5.0f, 5000.0f, g_proj);

    if (!isCrashed) {
        mat4 trans, rot;
        glm_translate_make(trans, planes[0].position);

        mat4 plane_orientation = GLM_MAT4_IDENTITY_INIT;
        glm_vec3_copy(planes[0].right, plane_orientation[0]);
        glm_vec3_copy(planes[0].up,    plane_orientation[1]);
        vec3 negFront; glm_vec3_negate_to(planes[0].front, negFront);
        glm_vec3_copy(negFront,         plane_orientation[2]);

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
        if (offset_behind <= 0.0f && offset_behind >= -70.0f)
            glm_vec3_add(cameraPos, planes[0].front, lookAtTarget);
        else
            glm_vec3_add(planes[0].position, planes[0].front, lookAtTarget);

        glm_lookat(cameraPos, lookAtTarget, planes[0].up, g_view);
    } else {
        vec3 center; glm_vec3_add(cameraPos, cameraFront, center);
        glm_lookat(cameraPos, center, cameraUp, g_view);
    }

    draw_skybox(g_view, g_proj);
    update_chunks();
    draw_chunks(g_view, g_proj);
    update_minimap_dot();

    if (!isCrashed) {
        for (int i = 0; i < MAX_PLANES; i++)
            draw_plane(&planes[i], g_view, g_proj);
    }

    glDisable(GL_DEPTH_TEST);
    if (isCrashed) draw_crash_marker(g_view, g_proj);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_texts();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_minimap();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void detect_planes(void) {
    if (!g_detector_ready) return;

    const int W = DET_W, H = DET_H;
    const size_t need_rgba = (size_t)W * H * 4;
    const size_t need_rgb  = (size_t)W * H * 3;

    double t_realloc0 = get_current_time_millis();
    if (g_frame_buf_capacity < need_rgba) {
        unsigned char* a = (unsigned char*)realloc(g_rgba_buffer, need_rgba);
        unsigned char* b = (unsigned char*)realloc(g_rgb_buffer,  need_rgb);
        if (!a || !b) { free(a); free(b); return; }
        g_rgba_buffer = a;
        g_rgb_buffer  = b;
        g_frame_buf_capacity = need_rgba;
    }
    double t_realloc1 = get_current_time_millis();

    float screen_aspect = (float)SCR_WIDTH / (float)SCR_HEIGHT;
    ViewRect lb = det_letterbox_rect(DET_W, screen_aspect);

    mat4 det_view, det_proj;
    glm_mat4_copy(g_view, det_view);
    glm_perspective(glm_rad(fov), screen_aspect, 5.0f, 5000.0f, det_proj);

    GLint prev_fbo = 0; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    GLint prev_vp[4];   glGetIntegerv(GL_VIEWPORT, prev_vp);

    double t_render0 = get_current_time_millis();
    glBindFramebuffer(GL_FRAMEBUFFER, g_det_fbo);  /* if 0, fallback to default */
    render_for_detection(lb, det_view, det_proj);
    glFinish();
    double t_render1 = get_current_time_millis();

    double t_read0 = get_current_time_millis();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, g_rgba_buffer);
    double t_read1 = get_current_time_millis();

    double t_convert0 = get_current_time_millis();
    rgba_to_gray3(g_rgba_buffer, g_rgb_buffer, W*H);
    flip_inplace(g_rgb_buffer, W, H, 3);
    double t_convert1 = get_current_time_millis();

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
    glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);

    OnnxDet* dets = NULL; int det_count = 0;
    double t_infer0 = get_current_time_millis();
    int rc = onnx_predict(&g_detector, g_rgb_buffer, W, H, &dets, &det_count);
    double t_infer1 = get_current_time_millis();
    if (rc != ONNX_OK) { return; }
    g_last_det_ms = (t_infer1 - t_infer0);

    float sx = (float)SCR_WIDTH  / (float)lb.w;
    float sy = (float)SCR_HEIGHT / (float)lb.h;

    double t_draw0 = get_current_time_millis();
    glDisable(GL_DEPTH_TEST);
    for (int i = 0; i < det_count; ++i) {
        float x1 = (dets[i].x1 - (float)lb.x) * sx;
        float y1 = (dets[i].y1 - (float)lb.y) * sy;
        float x2 = (dets[i].x2 - (float)lb.x) * sx;
        float y2 = (dets[i].y2 - (float)lb.y) * sy;

        int L = (int)floorf(x1), T = (int)floorf(y1);
        int R = (int)ceilf (x2), B = (int)ceilf (y2);
        if (L < 0) L = 0; if (T < 0) T = 0;
        if (R >= SCR_WIDTH)  R = SCR_WIDTH  - 1;
        if (B >= SCR_HEIGHT) B = SCR_HEIGHT - 1;

        if (R > L && B > T) {
            int cls = dets[i].cls;
            float rr, gg, bb;
            class_to_color(cls, &rr, &gg, &bb);

            // Kalınlık: 4 px örneği
            drawBoundingBoxColored(L, T, R, B, 4.0f, rr, gg, bb, 1.0f);
        }
    }
    glEnable(GL_DEPTH_TEST);
    double t_draw1 = get_current_time_millis();

    onnx_free_detections(dets);

    // --- Log timings ---
    printf("[DET] realloc=%.2fms render=%.2fms read=%.2fms convert=%.2fms infer=%.2fms draw=%.2fms total=%.2fms\n",
           (t_realloc1 - t_realloc0),
           (t_render1 - t_render0),
           (t_read1   - t_read0),
           (t_convert1- t_convert0),
           (t_infer1  - t_infer0),
           (t_draw1   - t_draw0),
           (t_draw1   - t_realloc0));
}


/* ---- Input / camera ---- */
void processInput(void) {
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

    static bool a_last = false;
    bool a_now = (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
    if (a_now && !a_last) {
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
    a_last = a_now;

    static bool t_last = false;
    bool t_now = (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS);
    if (t_now && !t_last) {
        isTriangleViewMode = !isTriangleViewMode;
        printf("GRID VIEW MODE: %s\n", isTriangleViewMode ? "ON" : "OFF");
    }
    t_last = t_now;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, 1);
        return;
    }

    if (isCrashed) {
        isSpeedFixed  = false;
        isAutopilotOn = false;
        currentMovementSpeed = initialMovementSpeed;

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            isCrashed = false;
            hasSetCrashView = true;
            for (int i=0; i<MAX_PLANES; i++) init_flight_model(planes[i]);
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

    static bool s_last = false;
    bool s_now = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);
    if (s_now && !s_last) {
        isSpeedFixed = !isSpeedFixed;
        if (isSpeedFixed) fixedValue = currentMovementSpeed;
    }
    s_last = s_now;

    if (isSpeedFixed) {
        currentMovementSpeed = fixedValue;
    } else {
        float maxSpeed = initialMovementSpeed * speedBoostMultiplier;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
            currentMovementSpeed += accelerationSpeeding * deltaTime;
            if (currentMovementSpeed > maxSpeed) currentMovementSpeed = maxSpeed;
            else offset_behind += accelerationGoingFarPerSecond;
        } else {
            currentMovementSpeed -= accelerationSlowing * deltaTime * 2.0f;
            if (currentMovementSpeed < initialMovementSpeed) currentMovementSpeed = initialMovementSpeed;
            else offset_behind -= accelerationGoingFarPerSecond / speedingSlowingRatio;
        }
    }

    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
        fov -= zoomingSpeed * deltaTime;
        if (fov < maximumZoomDistance) fov = maximumZoomDistance;
    } else {
        fov += zoomingSpeed * deltaTime;
        if (fov > originalFov) fov = originalFov;
    }

    float rotation_speed = 45.0f * deltaTime;
    float roll_speed     = bow_rate * deltaTime;
    bool isTurning = false;

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)   applyUpPitch(rotation_speed * 2);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) applyDownPitch(rotation_speed * 2);
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) { isTurning = true;  applyLeftTurn(rotation_speed, roll_speed); }
    if (glfwGetKey(window, GLFW_KEY_RIGHT)== GLFW_PRESS) { isTurning = true;  applyRightTurn(rotation_speed, roll_speed); }
    if (!isTurning) applyAutoLeveling();

    glm_normalize(planes[0].front);
    glm_vec3_cross(planes[0].front, planes[0].up, planes[0].right);
    glm_normalize(planes[0].right);
    glm_vec3_cross(planes[0].right, planes[0].front, planes[0].up);
    glm_normalize(planes[0].up);
}

void whereItCrashed(void) {
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

void framebuffer_size_callback(GLFWwindow *window_, int width, int height) {
    (void)window_;
    glViewport(0, 0, width, height);
}

void CLEANUP_SYSTEM(void) {
    cleanup_plane();
    cleanup_heightmap();
    cleanup_skybox();
    cleanup_minimap();
    cleanup_ui();
    cleanup_crash_marker();
    cleanup_osd();

    if (box_vbo)            glDeleteBuffers(1, &box_vbo);
    if (box_shader_program) glDeleteProgram(box_shader_program);

    if (g_detector_ready) { onnx_destroy(&g_detector); g_detector_ready = 0; }

    if (g_rgba_buffer) { free(g_rgba_buffer); g_rgba_buffer = NULL; }
    if (g_rgb_buffer)  { free(g_rgb_buffer);  g_rgb_buffer  = NULL; }
    g_frame_buf_capacity = 0;

    if (g_det_depth_rbo)  { glDeleteRenderbuffers(1, &g_det_depth_rbo);  g_det_depth_rbo  = 0; }
    if (g_det_color_tex)  { glDeleteTextures(1, &g_det_color_tex);       g_det_color_tex  = 0; }
    if (g_det_fbo)        { glDeleteFramebuffers(1, &g_det_fbo);         g_det_fbo        = 0; }
}
