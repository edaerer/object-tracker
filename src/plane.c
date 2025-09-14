#include "plane.h"
#include "globals.h"
#include "heightMap.h"
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>


typedef enum {
    FLY_STRAIGHT,
    TURN_LEFT,
    TURN_RIGHT,
    AUTO_PITCH_UP,
    AUTO_PITCH_DOWN,
    SET_SPEED,
    TOGGLE_SPEED_LOCK,
    SET_CAMERA,
    HOLD_ZOOM,
    TOGGLE_GRID_VIEW
} AutopilotAction;

typedef struct {
    AutopilotAction action;
    float duration;
    float value;
} AutopilotCommand;

static const float MAX_PITCH_ANGLE_DEGREES = 135.0f;
static const float PITCH_SMOOTHING_RANGE   = 30.0f;

static GLuint shaderProgram = 0;
static GLuint VBO = 0, NBO = 0, TBO = 0;
static GLuint planeTextureID = 0;
static GLsizei planeDrawCount = 0;

static tinyobj_attrib_t    planeAttrib;
static tinyobj_shape_t    *planeShapes = NULL;
static size_t              numPlaneShapes = 0;
static tinyobj_material_t *planeMaterials = NULL;
static size_t              numPlaneMaterials = 0;

static int   accelerationSpeeding = 2.0f;
static int   currentCommandIndex  = 0;
static float commandTimer         = 0.0f;

static const float minimumHeightFromTheGroundAutoPilot = 250.0f;
static const float maximumHeightFromTheGroundAutoPilot = 1500.0f;

// Preprocess OBJ to strip vertex colors on 'v ' lines (Blender can export v x y z r g b)
// tinyobj_loader_c doesn't accept extra RGB; keep only first 3 floats.
static char* preprocess_obj_strip_vertex_colors(const char* input, size_t in_len, size_t* out_len) {
    if (!input || in_len == 0) { *out_len = 0; return NULL; }
    char* out = (char*)malloc(in_len + 1);
    if (!out) { *out_len = 0; return NULL; }
    size_t o = 0;
    const char* p = input;
    const char* end = input + in_len;
    while (p < end) {
        const char* nl = memchr(p, '\n', (size_t)(end - p));
        size_t linelen = nl ? (size_t)(nl - p) : (size_t)(end - p);
        if (linelen >= 2 && p[0] == 'v' && p[1] == ' ') {
            float x,y,z;
            // Parse first three floats; ignore the rest of the line
            if (sscanf(p + 2, "%f %f %f", &x, &y, &z) == 3) {
                int n = snprintf(out + o, (in_len - o), "v %.6f %.6f %.6f\n", x, y, z);
                if (n < 0) { free(out); *out_len = 0; return NULL; }
                o += (size_t)n;
            } else {
                // Fallback: copy as-is
                memcpy(out + o, p, linelen);
                o += linelen;
                out[o++] = '\n';
            }
        } else {
            // Copy the line as-is (plus newline if present)
            memcpy(out + o, p, linelen);
            o += linelen;
            if (nl) out[o++] = '\n';
        }
        p = nl ? (nl + 1) : end;
    }
    out[o] = '\0';
    *out_len = o;
    return out;
}


Plane planes[MAX_PLANES] = {
                   {.position = {0.0f, 500.0f, 50.0f},
                    .front = {0.0f, 0.0f, -1.0f},
                    .up = {0.0f, 1.0f, 0.0f},
                    .right = {1.0f, 0.0f, 0.0f},
                    .speed = 0.0f,
                    .colors = {{0.7f, 0.75f, 0.8f},
                               {0.0f, 1.0f, 0.0f},
                               {0.2f, 0.9f, 1.0f},
                               {1.0f, 0.5f, 0.1f},
                               {0.1f, 0.1f, 0.15f}},
                    .useTexture = true,
                    .overrideColor = {0.0f, 1.0f, 1.0f},
                    .shadeColor = {0.0f, 0.0f, 0.0f},
                    .shadeStrength = 0.0f},
                   {.position = {-500.0f, 500.0f, 50.0f},
                    .front = {0.0f, 0.0f, -1.0f},
                    .up = {0.0f, 1.0f, 0.0f},
                    .right = {1.0f, 0.0f, 0.0f},
                    .speed = 600.0f,
                    .colors = {{0.7f, 0.75f, 0.8f},
                               {1.0f, 0.0f, 0.0f},
                               {0.2f, 0.9f, 1.0f},
                               {1.0f, 0.5f, 0.1f},
                               {0.1f, 0.1f, 0.15f}},
                    .useTexture = true,
                    .overrideColor = {0.58f, 0.776f, 0.941f},
                    .shadeColor = {0.0f, 0.0f, 0.0f},
                    .shadeStrength = 0.0f},
                   {.position = {500.0f, 500.0f, 50.0f},
                    .front = {0.0f, 0.0f, -1.0f},
                    .up = {0.0f, 1.0f, 0.0f},
                    .right = {1.0f, 0.0f, 0.0f},
                    .speed = 600.0f,
                    .colors = {{0.7f, 0.75f, 0.8f},
                               {1.0f, 0.0f, 0.0f},
                               {0.2f, 0.9f, 1.0f},
                               {1.0f, 0.5f, 0.1f},
                               {0.1f, 0.1f, 0.15f}},
                    .useTexture = true,
                    .overrideColor = {0.737f, 0.8f, 0.851f},
                    .shadeColor = {0.0f, 0.0f, 0.0f},
                    .shadeStrength = 0.0f},
                   {.position = {-500.0f, 500.0f, -1500.0f},
                    .front = {0.0f, 0.0f, -1.0f},
                    .up = {0.0f, 1.0f, 0.0f},
                    .right = {1.0f, 0.0f, 0.0f},
                    .speed = 600.0f,
                    .colors = {{0.7f, 0.75f, 0.8f},
                               {1.0f, 0.0f, 0.0f},
                               {0.2f, 0.9f, 1.0f},
                               {1.0f, 0.5f, 0.1f},
                               {0.1f, 0.1f, 0.15f}},
                    .useTexture = true,
                    .overrideColor = {1.0f, 1.0f, 1.0f},
                    .shadeColor = {0.0f, 0.0f, 0.0f},
                    .shadeStrength = 0.6f},
                   {.position = {500.0f, 500.0f, -1500.0f},
                    .front = {0.0f, 0.0f, -1.0f},
                    .up = {0.0f, 1.0f, 0.0f},
                    .right = {1.0f, 0.0f, 0.0f},
                    .speed = 700.0f,
                    .colors = {{0.7f, 0.75f, 0.8f},
                               {1.0f, 0.0f, 0.0f},
                               {0.2f, 0.9f, 1.0f},
                               {1.0f, 0.5f, 0.1f},
                               {0.1f, 0.1f, 0.15f}},
                    .useTexture = false,
                    .overrideColor = {0.58f, 0.776f, 0.941f},
                    .shadeColor = {0.0f, 0.0f, 0.0f},
                    .shadeStrength = 0.6f},
                   {.position = {500.0f, 500.0f, -3000.0f},
                    .front = {0.0f, 0.0f, -1.0f},
                    .up = {0.0f, 1.0f, 0.0f},
                    .right = {1.0f, 0.0f, 0.0f},
                    .speed = 700.0f,
                    .colors = {{0.7f, 0.75f, 0.8f},
                               {1.0f, 0.0f, 0.0f},
                               {0.2f, 0.9f, 1.0f},
                               {1.0f, 0.5f, 0.1f},
                               {0.1f, 0.1f, 0.15f}},
                    .useTexture = false,
                    .overrideColor = {0.737f, 0.8f, 0.851f},
                    .shadeColor = {0.0f, 0.0f, 0.0f},
                    .shadeStrength = 0.6f}
};

static AutopilotCommand flight_plan[] = {
    {FLY_STRAIGHT, 2.0f, 0.0f},     {TURN_LEFT, 3.0f, 0.0f},        {SET_CAMERA, 0.0f, 5.0f},
    {FLY_STRAIGHT, 3.0f, 0.0f},     {SET_CAMERA, 1.0f, 1.0f},       {SET_SPEED, 2.0f, 300.0f},
    {FLY_STRAIGHT, 2.0f, 0.0f},     {TURN_LEFT, 8.0f, 0.0f},        {FLY_STRAIGHT, 2.0f, 0.0f},
    {SET_SPEED, 2.0f, 500.0f},      {SET_CAMERA, 0.0f, 2.0f},       {FLY_STRAIGHT, 1.0f, 0.0f},
    {TURN_RIGHT, 1.0f, 0.0f},       {FLY_STRAIGHT, 1.5f, 0.0f},     {HOLD_ZOOM, 2.0f, 0.0f},
    {FLY_STRAIGHT, 3.0f, 0.0f},     {SET_CAMERA, 0.0f, 1.0f},       {TURN_RIGHT, 2.0f, 0.0f},
    {FLY_STRAIGHT, 1.0f, 0.0f},     {SET_CAMERA, 0.0f, 1.0f},       {FLY_STRAIGHT, 1.0f, 0.0f},
    {FLY_STRAIGHT, 6.0f, 0.0f},     {AUTO_PITCH_UP, 1.0f, 0.0f},    {FLY_STRAIGHT, 1.0f, 0.0f},
    {FLY_STRAIGHT, 2.0f, 0.0f},     {SET_CAMERA, 0.0f, 2.0f},
    {FLY_STRAIGHT, 3.0f, 0.0f},     {SET_CAMERA, 0.0f, 1.0f},       {AUTO_PITCH_DOWN, 1.0f, 0.0f},
    {FLY_STRAIGHT, 3.0f, 0.0f},     {SET_CAMERA, 0.0f, 8.0f},       {FLY_STRAIGHT, 2.0f, 0.0f},
    {FLY_STRAIGHT, 3.0f, 0.0f},     {SET_CAMERA, 0.0f, 7.0f},
    {FLY_STRAIGHT, 1.0f, 0.0f},     {TOGGLE_SPEED_LOCK, 0.0f, 0.0f}
};
static const int numCommands = (int)(sizeof(flight_plan) / sizeof(AutopilotCommand));

static const GLchar *planeVertexSource =
    "#version 100\n"
    "attribute vec3 position;\n"
    "attribute vec3 normal;\n"
    "attribute vec2 texcoord;\n"
    "varying vec3 vNormal;\n"
    "varying vec2 vTexCoord;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 proj;\n"
    "void main() {\n"
    "   gl_Position = proj * view * model * vec4(position, 1.0);\n"
    "   vNormal = mat3(model) * normal;\n"
    "   vTexCoord = texcoord;\n"
    "}\n";

static const GLchar *planeFragmentSource =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec3 vNormal;\n"
    "varying vec2 vTexCoord;\n"
    "uniform vec3 u_lightDirection;\n"
    "uniform sampler2D u_texture;\n"
    "uniform bool u_useTexture;\n"
    "uniform vec3 u_overrideColor;\n"
    "uniform vec3 u_shadeColor;\n"
    "uniform float u_shadeStrength;\n"
    "void main() {\n"
    "   vec3 finalColor;\n"
    "   if (u_useTexture) {\n"
    "       vec3 texColor = texture2D(u_texture, vTexCoord).rgb;\n"
    "       finalColor = mix(texColor, u_shadeColor, u_shadeStrength);\n"
    "   } else {\n"
    "       finalColor = u_overrideColor;\n"
    "   }\n"
    "   vec3 lightDir = normalize(u_lightDirection);\n"
    "   vec3 normal = normalize(vNormal);\n"
    "   float diffuse = max(dot(normal, lightDir), 0.25);\n"
    "   gl_FragColor = vec4(finalColor * diffuse, 1.0);\n"
    "}\n";

void file_reader_callback_impl(void *ctx, const char *filename, int is_mtl,
                               const char *obj_filename, char **buf, size_t *len) {
    (void)ctx; (void)is_mtl;
    char full[512];
    // ESKİ: filename[0] != '/'  → ./images/plane.obj gibi yolları da yanlışça birleştiriyordu
    if (obj_filename && filename && strchr(filename, '/') == NULL) {
        // SADECE "plane.mtl" gibi düz isimse OBJ'in klasörüyle birleştir
        const char* slash = strrchr(obj_filename, '/');
        if (!slash) snprintf(full, sizeof(full), "%s", filename);
        else {
            size_t dir_len = (size_t)(slash - obj_filename);
            snprintf(full, sizeof(full), "%.*s/%s", (int)dir_len, obj_filename, filename);
        }
    } else {
        // Zaten yolu içeriyorsa (./images/plane.obj gibi) doğrudan kullan
        snprintf(full, sizeof(full), "%s", filename ? filename : "");
    }
    FILE *fp = fopen(full, "rb");
    if (!fp) { *buf = NULL; *len = 0; return; }
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char* raw = (char*) malloc((size_t)file_size + 1);
    if (!raw) { fclose(fp); *len = 0; return; }
    fread(raw, 1, (size_t)file_size, fp);
    raw[file_size] = '\0';
    fclose(fp);
    // If this is an OBJ, strip vertex colors (v x y z r g b -> v x y z)
    size_t out_len = 0;
    const char* dot = strrchr(full, '.');
    if (dot && strcmp(dot, ".obj") == 0) {
        char* cleaned = preprocess_obj_strip_vertex_colors(raw, (size_t)file_size, &out_len);
        free(raw);
        if (!cleaned) { *buf = NULL; *len = 0; return; }
        *buf = cleaned;
        *len = out_len;
    } else {
        *buf = raw;
        *len = (size_t)file_size;
    }
}

bool init_plane(void) {
    shaderProgram = createShaderProgram(planeVertexSource, planeFragmentSource);
    if (shaderProgram == 0)
        return false;

    const char *plane_obj_path = "./images/plane.obj";
    int ret = tinyobj_parse_obj(&planeAttrib, &planeShapes, &numPlaneShapes,
                                &planeMaterials, &numPlaneMaterials,
                                plane_obj_path, file_reader_callback_impl, NULL,
                                TINYOBJ_FLAG_TRIANGULATE);
    if (ret != TINYOBJ_SUCCESS || planeAttrib.num_vertices == 0) {
        fprintf(stderr, "TINYOBJ ERROR: Failed to parse plane object file.\n");
        return false;
    }

    // --- Load texture from PNG using stb_image (no embedded header) ---
    int w, h, n;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load("./images/plane.png", &w, &h, &n, 0);
    if (!data) {
        fprintf(stderr, "ERROR: plane texture not found at ./images/plane_yavuzselim.png\n");
        return false;
    }

    glGenTextures(1, &planeTextureID);
    glBindTexture(GL_TEXTURE_2D, planeTextureID);
    GLenum fmt = (n == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);

    // Build interleaved buffers from tinyobj attrib
    planeDrawCount = (GLsizei)planeAttrib.num_faces;

    GLfloat *v_buffer = (GLfloat *) malloc((size_t)planeDrawCount * 3 * sizeof(GLfloat));
    GLfloat *n_buffer = (GLfloat *) malloc((size_t)planeDrawCount * 3 * sizeof(GLfloat));
    GLfloat *t_buffer = (GLfloat *) malloc((size_t)planeDrawCount * 2 * sizeof(GLfloat));
    if (!v_buffer || !n_buffer || !t_buffer) {
        fprintf(stderr, "ERROR: Could not allocate plane buffers.\n");
        free(v_buffer); free(n_buffer); free(t_buffer);
        return false;
    }

    for (GLsizei i = 0; i < planeDrawCount; ++i) {
        tinyobj_vertex_index_t idx = planeAttrib.faces[i];
        int v_idx = idx.v_idx, n_idx = idx.vn_idx, t_idx = idx.vt_idx;
        memcpy(v_buffer + i * 3, planeAttrib.vertices + v_idx * 3, 3 * sizeof(GLfloat));
        if (n_idx >= 0) memcpy(n_buffer + i * 3, planeAttrib.normals  + n_idx * 3, 3 * sizeof(GLfloat));
        else             memset(n_buffer + i * 3, 0, 3 * sizeof(GLfloat));
        if (t_idx >= 0) memcpy(t_buffer + i * 2, planeAttrib.texcoords + t_idx * 2, 2 * sizeof(GLfloat));
        else             memset(t_buffer + i * 2, 0, 2 * sizeof(GLfloat));
    }

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)planeDrawCount * 3 * sizeof(GLfloat), v_buffer, GL_STATIC_DRAW);

    glGenBuffers(1, &NBO);
    glBindBuffer(GL_ARRAY_BUFFER, NBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)planeDrawCount * 3 * sizeof(GLfloat), n_buffer, GL_STATIC_DRAW);

    glGenBuffers(1, &TBO);
    glBindBuffer(GL_ARRAY_BUFFER, TBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)planeDrawCount * 2 * sizeof(GLfloat), t_buffer, GL_STATIC_DRAW);

    free(v_buffer); free(n_buffer); free(t_buffer);
    tinyobj_attrib_free(&planeAttrib);
    tinyobj_shapes_free(planeShapes, numPlaneShapes);
    tinyobj_materials_free(planeMaterials, numPlaneMaterials);
    planeShapes = NULL;
    planeMaterials = NULL;

    return true;
}

void init_flight_model(Plane plane) {
    vec3 initialFront = {0.0f, 0.0f, -1.0f};
    vec3 worldUp = {0.0f, 1.0f, 0.0f};
    mat4 initialYawMatrix;
    glm_rotate_make(initialYawMatrix, glm_rad(-90.0f), worldUp);
    glm_mat4_mulv3(initialYawMatrix, initialFront, 1.0f, plane.front);
    glm_vec3_cross(plane.front, worldUp, plane.right);
    glm_normalize(plane.right);
    glm_vec3_cross(plane.right, plane.front, plane.up);
    glm_normalize(plane.up);
}

void update_enemy_plane(Plane *enemy) {
    glm_vec3_copy(planes[0].front, enemy->front);
    glm_vec3_copy(planes[0].right, enemy->right);
    glm_vec3_copy(planes[0].up, enemy->up);

    vec3 movement;
    glm_vec3_scale(enemy->front, enemy->speed * deltaTime, movement);
    glm_vec3_add(enemy->position, movement, enemy->position);

    glm_mat4_identity(enemy->modelMatrix);
    glm_translate(enemy->modelMatrix, enemy->position);

    float yawAngle = atan2f(-enemy->front[0], -enemy->front[2]);
    glm_rotate(enemy->modelMatrix, yawAngle, (vec3) {0.0f, 1.0f, 0.0f});
}

void draw_plane(Plane *plane, mat4 view, mat4 proj) {
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, (GLfloat *) plane->modelMatrix);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, (GLfloat *) view);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "proj"), 1, GL_FALSE, (GLfloat *) proj);
    glUniform3fv(glGetUniformLocation(shaderProgram, "u_lightDirection"), 1, lightDirection);
    glUniform1i(glGetUniformLocation(shaderProgram, "u_useTexture"), plane->useTexture ? 1 : 0);
    glUniform3fv(glGetUniformLocation(shaderProgram, "u_overrideColor"), 1, plane->overrideColor);
    glUniform3fv(glGetUniformLocation(shaderProgram, "u_shadeColor"), 1, plane->shadeColor);
    glUniform1f(glGetUniformLocation(shaderProgram, "u_shadeStrength"), plane->shadeStrength);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, planeTextureID);
    glUniform1i(glGetUniformLocation(shaderProgram, "u_texture"), 0);

    GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)0);

    GLint normAttrib = glGetAttribLocation(shaderProgram, "normal");
    glEnableVertexAttribArray(normAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, NBO);
    glVertexAttribPointer(normAttrib, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)0);

    GLint texAttrib = glGetAttribLocation(shaderProgram, "texcoord");
    glEnableVertexAttribArray(texAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, TBO);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)0);

    glDrawArrays(GL_TRIANGLES, 0, planeDrawCount);

    glDisableVertexAttribArray(posAttrib);
    glDisableVertexAttribArray(normAttrib);
    glDisableVertexAttribArray(texAttrib);
}

void applyUpPitch(float rotation_speed) {
    float currentPitchDegrees = glm_deg(asinf(planes[0].front[1]));
    float pitchHeadroom = MAX_PITCH_ANGLE_DEGREES - currentPitchDegrees;
    if (pitchHeadroom <= 0.0f) return;

    float smoothFactor = fminf(0.5f, pitchHeadroom / PITCH_SMOOTHING_RANGE);
    smoothFactor = smoothFactor * smoothFactor;
    float finalRotationSpeed = rotation_speed * smoothFactor;

    mat4 rotation;
    glm_rotate_make(rotation, glm_rad(finalRotationSpeed), planes[0].right);
    glm_mat4_mulv3(rotation, planes[0].front, 1.0f, planes[0].front);
    glm_mat4_mulv3(rotation, planes[0].up,    1.0f, planes[0].up);
}

void applyDownPitch(float rotation_speed) {
    float currentPitchDegrees = glm_deg(asinf(planes[0].front[1]));
    float pitchHeadroom = currentPitchDegrees - (-MAX_PITCH_ANGLE_DEGREES);
    if (pitchHeadroom <= 0.0f) return;

    float smoothFactor = fminf(0.5f, pitchHeadroom / PITCH_SMOOTHING_RANGE);
    smoothFactor = smoothFactor * smoothFactor;
    float finalRotationSpeed = rotation_speed * smoothFactor;

    mat4 rotation;
    glm_rotate_make(rotation, glm_rad(-finalRotationSpeed), planes[0].right);
    glm_mat4_mulv3(rotation, planes[0].front, 1.0f, planes[0].front);
    glm_mat4_mulv3(rotation, planes[0].up,    1.0f, planes[0].up);
}

void applyLeftTurn(float rotation_speed, float roll_speed) {
    mat4 rotation;
    vec3 worldUp = {0.0f, 1.0f, 0.0f};
    glm_rotate_make(rotation, glm_rad(rotation_speed), worldUp);
    glm_mat4_mulv3(rotation, planes[0].front, 1.0f, planes[0].front);
    glm_mat4_mulv3(rotation, planes[0].right, 1.0f, planes[0].right);
    glm_mat4_mulv3(rotation, planes[0].up,    1.0f, planes[0].up);
    glm_rotate_make(rotation, glm_rad(-roll_speed), planes[0].front);
    glm_mat4_mulv3(rotation, planes[0].up,    1.0f, planes[0].up);
    glm_mat4_mulv3(rotation, planes[0].right, 1.0f, planes[0].right);
}

void applyRightTurn(float rotation_speed, float roll_speed) {
    mat4 rotation;
    vec3 worldUp = {0.0f, 1.0f, 0.0f};
    glm_rotate_make(rotation, glm_rad(-rotation_speed), worldUp);
    glm_mat4_mulv3(rotation, planes[0].front, 1.0f, planes[0].front);
    glm_mat4_mulv3(rotation, planes[0].right, 1.0f, planes[0].right);
    glm_mat4_mulv3(rotation, planes[0].up,    1.0f, planes[0].up);
    glm_rotate_make(rotation, glm_rad(roll_speed), planes[0].front);
    glm_mat4_mulv3(rotation, planes[0].up,    1.0f, planes[0].up);
    glm_mat4_mulv3(rotation, planes[0].right, 1.0f, planes[0].right);
}

void applyAutoLeveling(void) {
    vec3 worldUp = {0.0f, 1.0f, 0.0f};
    vec3 frontHorizontal;
    glm_vec3_copy(planes[0].front, frontHorizontal);
    frontHorizontal[1] = 0;
    if (glm_vec3_norm(frontHorizontal) > 0.001f) {
        glm_normalize(frontHorizontal);
        vec3 idealRight;
        glm_vec3_cross(frontHorizontal, worldUp, idealRight);
        glm_normalize(idealRight);
        vec3 idealUp;
        glm_vec3_cross(idealRight, planes[0].front, idealUp);
        glm_normalize(idealUp);
        glm_vec3_lerp(planes[0].up, idealUp, deltaTime * 1.0f, planes[0].up);
    }
}

void reset_autopilot_state(void) {
    currentCommandIndex = 0;
    commandTimer = 0.0f;
}

void autoPilotMode(void) {
    commandTimer += deltaTime;
    AutopilotCommand currentCommand = flight_plan[currentCommandIndex];

    if (commandTimer >= currentCommand.duration) {
        commandTimer = 0.0f;
        currentCommandIndex = (currentCommandIndex + 1) % numCommands;
        currentCommand = flight_plan[currentCommandIndex];
    }

    float rotation_speed = 45.0f * deltaTime;
    float roll_speed     = 30.0f * deltaTime;

    float groundHeight = get_terrain_height(planes[0].position[0], planes[0].position[2]);
    float heightAboveGround = planes[0].position[1] - groundHeight;
    float pitchChange = 0.0f;

    const float altitudeDeadZone = 10.0f;
    const float vsSensitivity = 0.5f;

    if (heightAboveGround < minimumHeightFromTheGroundAutoPilot - altitudeDeadZone) {
        float targetClimbSpeed = 50.0f;
        float vsError = targetClimbSpeed - verticalSpeed;
        pitchChange = vsError * vsSensitivity * deltaTime;
    } else if (heightAboveGround > maximumHeightFromTheGroundAutoPilot + altitudeDeadZone) {
        float targetDescendSpeed = -30.0f;
        float vsError = targetDescendSpeed - verticalSpeed;
        pitchChange = vsError * vsSensitivity * deltaTime;
    } else {
        float vsError = 0.0f - verticalSpeed;
        pitchChange = vsError * vsSensitivity * deltaTime;
    }

    switch (currentCommand.action) {
        case TURN_LEFT:
            applyLeftTurn(rotation_speed, roll_speed);
            applyAutoLeveling();
            break;
        case TURN_RIGHT:
            applyRightTurn(rotation_speed, roll_speed);
            applyAutoLeveling();
            break;
        case AUTO_PITCH_UP:
            applyUpPitch(rotation_speed);
            break;
        case AUTO_PITCH_DOWN:
            applyDownPitch(rotation_speed);
            break;
        case SET_SPEED:
            if (currentMovementSpeed < currentCommand.value) {
                currentMovementSpeed += (accelerationSpeeding / 2) * deltaTime;
                offset_behind += 0.5f;
                if (currentMovementSpeed > currentCommand.value)
                    currentMovementSpeed = currentCommand.value;
            } else {
                currentMovementSpeed -= (accelerationSpeeding / 2) * deltaTime;
                offset_behind -= 0.25f;
                if (currentMovementSpeed < currentCommand.value)
                    currentMovementSpeed = currentCommand.value;
            }
            fixedValue = currentMovementSpeed;
            break;
        case HOLD_ZOOM:
            fov -= zoomingSpeed * deltaTime;
            if (fov < maximumZoomDistance) fov = maximumZoomDistance;
            break;
        case SET_CAMERA:
            if (currentCommand.value == 1.0f) {
                offset_behind = 400.0f; offset_above = 170.0f; offset_right = 0.0f;
            } else if (currentCommand.value == 2.0f) {
                offset_behind = -50.0f; offset_above = 30.0f; offset_right = 0.0f;
            } else if (currentCommand.value == 3.0f) {
                offset_behind = 400.0f; offset_above = 500.0f; offset_right = 0.0f;
            } else if (currentCommand.value == 4.0f) {
                offset_behind = -300.0f; offset_above = 100.0f; offset_right = 0.0f;
            } else if (currentCommand.value == 5.0f) {
                offset_behind = -200.0f; offset_above = 150.0f; offset_right = 250.0f;
            } else if (currentCommand.value == 6.0f) {
                offset_behind = -200.0f; offset_above = 150.0f; offset_right = -250.0f;
            } else if (currentCommand.value == 7.0f) {
                offset_behind = -300.0f; offset_above = -100.0f; offset_right = 0.0f;
            } else if (currentCommand.value == 8.0f) {
                offset_behind = -75.0f; offset_above = 200.0f; offset_right = 550.0f;
            }
            break;
        case TOGGLE_SPEED_LOCK:
            if (commandTimer < deltaTime * 1.5f) {
                isSpeedFixed = !isSpeedFixed;
                if (isSpeedFixed) fixedValue = currentMovementSpeed;
            }
            break;
        case FLY_STRAIGHT:
        default:
            applyAutoLeveling();
            break;
    }

    if (currentCommand.action != AUTO_PITCH_UP && currentCommand.action != AUTO_PITCH_DOWN) {
        applyUpPitch(pitchChange);
    }

    if (currentCommand.action != HOLD_ZOOM && fov < originalFov) {
        fov += zoomingSpeed * deltaTime;
        if (fov > originalFov) fov = originalFov;
    }

    glm_normalize(planes[0].front);
    glm_vec3_cross(planes[0].front, planes[0].up, planes[0].right);
    glm_normalize(planes[0].right);
    glm_vec3_cross(planes[0].right, planes[0].front, planes[0].up);
    glm_normalize(planes[0].up);
}

void cleanup_plane(void) {
    if (shaderProgram) glDeleteProgram(shaderProgram);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (NBO) glDeleteBuffers(1, &NBO);
    if (TBO) glDeleteBuffers(1, &TBO);
    if (planeTextureID) glDeleteTextures(1, &planeTextureID);
}
