#include "plane.h"
#include "globals.h"
#include "heightMap.h"
#include "models/plane_yavuzselim.h"

#define IMAGE_HEIGHT PLANE_YAVUZSELIM_HEIGHT
#define IMAGE_WIDTH PLANE_YAVUZSELIM_WIDTH
#define IMAGE_CHANNELS 4

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
static const float PITCH_SMOOTHING_RANGE = 30.0f;
static GLuint shaderProgram;
static GLuint VBO, NBO, TBO;
static GLuint planeTextureID;
static GLsizei planeDrawCount;
static tinyobj_attrib_t planeAttrib;
static tinyobj_shape_t *planeShapes = NULL;
static size_t numPlaneShapes;
static tinyobj_material_t *planeMaterials = NULL;
static size_t numPlaneMaterials;
static int currentCommandIndex = 0;
static float commandTimer = 0.0f;
static const float minimumHeightFromTheGroundAutoPilot = 250.0f;
static const float maximumHeightFromTheGroundAutoPilot = 1500.0f;

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
                    .speed = 300.0f,
                    .colors = {{0.7f, 0.75f, 0.8f},
                               {1.0f, 0.0f, 0.0f},
                               {0.2f, 0.9f, 1.0f},
                               {1.0f, 0.5f, 0.1f},
                               {0.1f, 0.1f, 0.15f}},
                    .useTexture = false,
                    .overrideColor = {0.58f, 0.776f, 0.941f},
                    .shadeColor = {0.0f, 0.0f, 0.0f},
                    .shadeStrength = 0.0f},
                   {.position = {500.0f, 500.0f, 50.0f},
                    .front = {0.0f, 0.0f, -1.0f},
                    .up = {0.0f, 1.0f, 0.0f},
                    .right = {1.0f, 0.0f, 0.0f},
                    .speed = 300.0f,
                    .colors = {{0.7f, 0.75f, 0.8f},
                               {1.0f, 0.0f, 0.0f},
                               {0.2f, 0.9f, 1.0f},
                               {1.0f, 0.5f, 0.1f},
                               {0.1f, 0.1f, 0.15f}},
                    .useTexture = false,
                    .overrideColor = {0.737f, 0.8f, 0.851f},
                    .shadeColor = {0.0f, 0.0f, 0.0f},
                    .shadeStrength = 0.0f},
                   {.position = {-500.0f, 500.0f, -1500.0f},
                    .front = {0.0f, 0.0f, -1.0f},
                    .up = {0.0f, 1.0f, 0.0f},
                    .right = {1.0f, 0.0f, 0.0f},
                    .speed = 300.0f,
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
                    .speed = 500.0f,
                    .colors = {{0.7f, 0.75f, 0.8f},
                               {1.0f, 0.0f, 0.0f},
                               {0.2f, 0.9f, 1.0f},
                               {1.0f, 0.5f, 0.1f},
                               {0.1f, 0.1f, 0.15f}},
                    .useTexture = true,
                    .overrideColor = {1.0f, 1.0f, 1.0f},
                    .shadeColor = {0.0f, 0.0f, 0.0f},
                    .shadeStrength = 0.6f},
                   {.position = {500.0f, 500.0f, -3000.0f},
                    .front = {0.0f, 0.0f, -1.0f},
                    .up = {0.0f, 1.0f, 0.0f},
                    .right = {1.0f, 0.0f, 0.0f},
                    .speed = 500.0f,
                    .colors = {{0.7f, 0.75f, 0.8f},
                               {1.0f, 0.0f, 0.0f},
                               {0.2f, 0.9f, 1.0f},
                               {1.0f, 0.5f, 0.1f},
                               {0.1f, 0.1f, 0.15f}},
                    .useTexture = true,
                    .overrideColor = {1.0f, 1.0f, 1.0f},
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
        {TOGGLE_GRID_VIEW, 0.0f, 0.0f}, {FLY_STRAIGHT, 2.0f, 0.0f},     {SET_CAMERA, 0.0f, 2.0f},
        {FLY_STRAIGHT, 3.0f, 0.0f},     {SET_CAMERA, 0.0f, 1.0f},       {AUTO_PITCH_DOWN, 1.0f, 0.0f},
        {FLY_STRAIGHT, 3.0f, 0.0f},     {SET_CAMERA, 0.0f, 8.0f},       {FLY_STRAIGHT, 2.0f, 0.0f},
        {TOGGLE_GRID_VIEW, 0.0f, 0.0f}, {FLY_STRAIGHT, 3.0f, 0.0f},     {SET_CAMERA, 0.0f, 7.0f},
        {FLY_STRAIGHT, 1.0f, 0.0f},     {TOGGLE_SPEED_LOCK, 0.0f, 0.0f}};
static const int numCommands = sizeof(flight_plan) / sizeof(AutopilotCommand);

static const GLchar *planeVertexSource = "#version 100\n"
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
static const GLchar *planeFragmentSource = "#version 100\n"
                                           "precision mediump float;\n"
                                           "varying vec3 vNormal;\n"
                                           "varying vec2 vTexCoord;\n"
                                           "uniform vec3 u_lightDirection;\n"
                                           "uniform sampler2D u_texture;\n"
                                           "uniform bool u_useTexture;\n"        // Texture kullanılacak mı?
                                           "uniform vec3 u_overrideColor;\n"     // Override rengi
                                           "uniform vec3 u_shadeColor;\n"        // Shade rengi
                                           "uniform float u_shadeStrength;\n"    // Shade gücü (0-1)
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

void file_reader_callback_impl(void *ctx, const char *filename, int is_mtl, const char *obj_filename, char **buf,size_t *len) {
    (void) ctx;
    (void) is_mtl;
    (void) obj_filename;
    long file_size;
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        *buf = NULL;
        *len = 0;
        return;
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    *buf = (char *) malloc(file_size + 1);
    fread(*buf, 1, file_size, fp);
    (*buf)[file_size] = '\0';
    *len = file_size;
    fclose(fp);
}

bool init_plane() {
    shaderProgram = createShaderProgram(planeVertexSource, planeFragmentSource);
    if (shaderProgram == 0)
        return false;

    const char *plane_obj_path = "../include/models/plane.obj";
    int ret = tinyobj_parse_obj(&planeAttrib, &planeShapes, &numPlaneShapes, &planeMaterials, &numPlaneMaterials,
                                plane_obj_path, file_reader_callback_impl, NULL, TINYOBJ_FLAG_TRIANGULATE);

    if (ret != TINYOBJ_SUCCESS || planeAttrib.num_vertices == 0) {
        fprintf(stderr, "TINYOBJ ERROR: Failed to parse plane object file.\n");
        return false;
    }

    const int width = IMAGE_WIDTH;
    const int height = IMAGE_HEIGHT;
    const int channels = IMAGE_CHANNELS;
    const unsigned char *original_data = plane_yavuzselim;

    printf("Loading and flipping plane texture from embedded data...\n");

    if (!original_data) {
        fprintf(stderr, "ERROR: Embedded plane texture data is missing.\n");
        return false;
    }

    const size_t image_size = width * height * channels;
    unsigned char *flipped_data = (unsigned char *) malloc(image_size);
    if (!flipped_data) {
        fprintf(stderr, "ERROR: Could not allocate memory for flipped plane texture.\n");
        return false;
    }

    for (int i = 0; i < height; i++) {
        const unsigned char *src_row = original_data + ((height - 1 - i) * width * channels);
        unsigned char *dest_row = flipped_data + (i * width * channels);
        memcpy(dest_row, src_row, width * channels);
    }

    glGenTextures(1, &planeTextureID);
    glBindTexture(GL_TEXTURE_2D, planeTextureID);

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, flipped_data);

    free(flipped_data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    printf("Successfully loaded plane texture from header.\n");

    planeDrawCount = planeAttrib.num_faces;
    GLfloat *v_buffer = (GLfloat *) malloc(planeDrawCount * 3 * sizeof(GLfloat));
    GLfloat *n_buffer = (GLfloat *) malloc(planeDrawCount * 3 * sizeof(GLfloat));
    GLfloat *t_buffer = (GLfloat *) malloc(planeDrawCount * 2 * sizeof(GLfloat));

    for (GLsizei i = 0; i < planeDrawCount; ++i) {
        tinyobj_vertex_index_t idx = planeAttrib.faces[i];
        int v_idx = idx.v_idx, n_idx = idx.vn_idx, t_idx = idx.vt_idx;
        memcpy(v_buffer + i * 3, planeAttrib.vertices + v_idx * 3, 3 * sizeof(GLfloat));
        if (n_idx >= 0)
            memcpy(n_buffer + i * 3, planeAttrib.normals + n_idx * 3, 3 * sizeof(GLfloat));
        if (t_idx >= 0)
            memcpy(t_buffer + i * 2, planeAttrib.texcoords + t_idx * 2, 2 * sizeof(GLfloat));
    }

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, planeDrawCount * 3 * sizeof(GLfloat), v_buffer, GL_STATIC_DRAW);
    glGenBuffers(1, &NBO);
    glBindBuffer(GL_ARRAY_BUFFER, NBO);
    glBufferData(GL_ARRAY_BUFFER, planeDrawCount * 3 * sizeof(GLfloat), n_buffer, GL_STATIC_DRAW);
    glGenBuffers(1, &TBO);
    glBindBuffer(GL_ARRAY_BUFFER, TBO);
    glBufferData(GL_ARRAY_BUFFER, planeDrawCount * 2 * sizeof(GLfloat), t_buffer, GL_STATIC_DRAW);

    free(v_buffer);
    free(n_buffer);
    free(t_buffer);
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

    float yawAngle = atan2(-enemy->front[0], -enemy->front[2]);
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
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);

    GLint normAttrib = glGetAttribLocation(shaderProgram, "normal");
    glEnableVertexAttribArray(normAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, NBO);
    glVertexAttribPointer(normAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);

    GLint texAttrib = glGetAttribLocation(shaderProgram, "texcoord");
    glEnableVertexAttribArray(texAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, TBO);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLES, 0, planeDrawCount);

    glDisableVertexAttribArray(posAttrib);
    glDisableVertexAttribArray(normAttrib);
    glDisableVertexAttribArray(texAttrib);
}

void applyUpPitch(float rotation_speed) {
    float currentPitchDegrees = glm_deg(asinf(planes[0].front[1]));

    float pitchHeadroom = MAX_PITCH_ANGLE_DEGREES - currentPitchDegrees;

    if (pitchHeadroom <= 0.0f) {
        return;
    }

    float smoothFactor = fmin(0.5f, pitchHeadroom / PITCH_SMOOTHING_RANGE);

    smoothFactor = smoothFactor * smoothFactor;

    float finalRotationSpeed = rotation_speed * smoothFactor;

    mat4 rotation;
    glm_rotate_make(rotation, glm_rad(finalRotationSpeed), planes[0].right);
    glm_mat4_mulv3(rotation, planes[0].front, 1.0f, planes[0].front);
    glm_mat4_mulv3(rotation, planes[0].up, 1.0f, planes[0].up);
}

void applyDownPitch(float rotation_speed) {
    float currentPitchDegrees = glm_deg(asinf(planes[0].front[1]));

    float pitchHeadroom = currentPitchDegrees - (-MAX_PITCH_ANGLE_DEGREES);

    if (pitchHeadroom <= 0.0f) {
        return;
    }

    float smoothFactor = fmin(0.5f, pitchHeadroom / PITCH_SMOOTHING_RANGE);
    smoothFactor = smoothFactor * smoothFactor;

    float finalRotationSpeed = rotation_speed * smoothFactor;

    mat4 rotation;
    glm_rotate_make(rotation, glm_rad(-finalRotationSpeed), planes[0].right);
    glm_mat4_mulv3(rotation, planes[0].front, 1.0f, planes[0].front);
    glm_mat4_mulv3(rotation, planes[0].up, 1.0f, planes[0].up);
}

void applyLeftTurn(float rotation_speed, float roll_speed) {
    mat4 rotation;
    vec3 worldUp = {0.0f, 1.0f, 0.0f};
    glm_rotate_make(rotation, glm_rad(rotation_speed), worldUp);
    glm_mat4_mulv3(rotation, planes[0].front, 1.0f, planes[0].front);
    glm_mat4_mulv3(rotation, planes[0].right, 1.0f, planes[0].right);
    glm_mat4_mulv3(rotation, planes[0].up, 1.0f, planes[0].up);
    glm_rotate_make(rotation, glm_rad(-roll_speed), planes[0].front);
    glm_mat4_mulv3(rotation, planes[0].up, 1.0f, planes[0].up);
    glm_mat4_mulv3(rotation, planes[0].right, 1.0f, planes[0].right);
}

void applyRightTurn(float rotation_speed, float roll_speed) {
    mat4 rotation;
    vec3 worldUp = {0.0f, 1.0f, 0.0f};
    glm_rotate_make(rotation, glm_rad(-rotation_speed), worldUp);
    glm_mat4_mulv3(rotation, planes[0].front, 1.0f, planes[0].front);
    glm_mat4_mulv3(rotation, planes[0].right, 1.0f, planes[0].right);
    glm_mat4_mulv3(rotation, planes[0].up, 1.0f, planes[0].up);
    glm_rotate_make(rotation, glm_rad(roll_speed), planes[0].front);
    glm_mat4_mulv3(rotation, planes[0].up, 1.0f, planes[0].up);
    glm_mat4_mulv3(rotation, planes[0].right, 1.0f, planes[0].right);
}

void applyAutoLeveling() {
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

void reset_autopilot_state() {
    currentCommandIndex = 0;
    commandTimer = 0.0f;
}

void autoPilotMode() {
    commandTimer += deltaTime;
    AutopilotCommand currentCommand = flight_plan[currentCommandIndex];

    if (commandTimer >= currentCommand.duration) {
        commandTimer = 0.0f;
        currentCommandIndex = (currentCommandIndex + 1) % numCommands;
        currentCommand = flight_plan[currentCommandIndex];
    }

    float rotation_speed = 45.0f * deltaTime;
    float roll_speed = 30.0f * deltaTime;

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
            if (fov < maximumZoomDistance)
                fov = maximumZoomDistance;
            break;

        case SET_CAMERA:
            if (currentCommand.value == 1.0f) {
                offset_behind = 400.0f;
                offset_above = 170.0f;
                offset_right = 0.0f;
            } else if (currentCommand.value == 2.0f) {
                offset_behind = -50.0f;
                offset_above = 30.0f;
                offset_right = 0.0f;
            } else if (currentCommand.value == 3.0f) {
                offset_behind = 400.0f;
                offset_above = 500.0f;
                offset_right = 0.0f;
            } else if (currentCommand.value == 4.0f) {
                offset_behind = -300.0f;
                offset_above = 100.0f;
                offset_right = 0.0f;
            } else if (currentCommand.value == 5.0f) {
                offset_behind = -200.0f;
                offset_above = 150.0f;
                offset_right = 250.0f;
            } else if (currentCommand.value == 6.0f) {
                offset_behind = -200.0f;
                offset_above = 150.0f;
                offset_right = -250.0f;
            } else if (currentCommand.value == 7.0f) {
                offset_behind = -300.0f;
                offset_above = -100.0f;
                offset_right = 0.0f;
            } else if (currentCommand.value == 8.0f) {
                offset_behind = -75.0f;
                offset_above = 200.0f;
                offset_right = 550.0f;
            }
            break;
        case TOGGLE_SPEED_LOCK:
            if (commandTimer < deltaTime * 1.5f) {
                isSpeedFixed = !isSpeedFixed;
                if (isSpeedFixed)
                    fixedValue = currentMovementSpeed;
            }
            break;
        case TOGGLE_GRID_VIEW:
            if (commandTimer < deltaTime * 1.5f) {
                isTriangleViewMode = !isTriangleViewMode;
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
        if (fov > originalFov)
            fov = originalFov;
    }

    glm_normalize(planes[0].front);
    glm_vec3_cross(planes[0].front, planes[0].up, planes[0].right);
    glm_normalize(planes[0].right);
    glm_vec3_cross(planes[0].right, planes[0].front, planes[0].up);
    glm_normalize(planes[0].up);
}

void cleanup_plane() {
    glDeleteProgram(shaderProgram);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &NBO);
    glDeleteBuffers(1, &TBO);
    glDeleteTextures(1, &planeTextureID);
}
