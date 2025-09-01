#include "globals.h"
#include "plane.h"
#include "heightMap.h"
#include "models/plane_yavuzselim.h"

#define IMAGE_HEIGHT PLANE_YAVUZSELIM_HEIGHT
#define IMAGE_WIDTH PLANE_YAVUZSELIM_WIDTH
#define IMAGE_CHANNELS 4 // RGBA

static const float MAX_PITCH_ANGLE_DEGREES = 135.0f;
static const float PITCH_SMOOTHING_RANGE = 30.0f;

// --- Global State & Configuration Variables ---
static GLuint planeShaderProgram;
static GLuint planeVBO, planeNBO, planeTBO;
static GLuint planeTextureID;
static GLsizei planeDrawCount;

static tinyobj_attrib_t planeAttrib;
static tinyobj_shape_t *planeShapes = NULL;
static size_t numPlaneShapes;
static tinyobj_material_t *planeMaterials = NULL;
static size_t numPlaneMaterials;

static vec3 planeStartingCoordinates = {0.0f, 500.0f, 50.0f};

// --- Autopilot state ---
static int currentCommandIndex = 0;
static float commandTimer = 0.0f;
// ===================================================================
// === MODIFICATION: New global for safe altitude                  ===
// ===================================================================
static const float minimumHeightFromTheGroundAutoPilot = 250.0f;
static const float maximumHeightFromTheGroundAutoPilot = 1500.0f;

// acceleration is half of the isAutoPilotMode = false case's
// --- Autopilot Flight Plan ---
static AutopilotCommand flight_plan[] = {
    {FLY_STRAIGHT, 2.0f, 0.0f},
    {TURN_LEFT, 3.0f, 0.0f},
    {SET_CAMERA, 0.0f, 5.0f},
    {FLY_STRAIGHT, 3.0f, 0.0f},
    {SET_CAMERA, 1.0f, 1.0f},
    {SET_SPEED, 2.0f, 300.0f},
    {FLY_STRAIGHT, 2.0f, 0.0f},
    {TURN_LEFT, 8.0f, 0.0f},
    {FLY_STRAIGHT, 2.0f, 0.0f},
    {SET_SPEED, 2.0f, 500.0f},
    {SET_CAMERA, 0.0f, 2.0f},
    {FLY_STRAIGHT, 1.0f, 0.0f},
    {TURN_RIGHT, 1.0f, 0.0f},
    {FLY_STRAIGHT, 1.5f, 0.0f},
    {HOLD_ZOOM, 2.0f, 0.0f}, // Zoom while speed is changing
    {FLY_STRAIGHT, 3.0f, 0.0f},
    {SET_CAMERA, 0.0f, 1.0f},
    {TURN_RIGHT, 2.0f, 0.0f},
    {FLY_STRAIGHT, 1.0f, 0.0f},
    {SET_CAMERA, 0.0f, 1.0f},
    {FLY_STRAIGHT, 1.0f, 0.0f},
    {FLY_STRAIGHT, 6.0f, 0.0f},
    {AUTO_PITCH_UP, 1.0f, 0.0f},
    {FLY_STRAIGHT, 1.0f, 0.0f},
    {TOGGLE_GRID_VIEW, 0.0f, 0.0f},
    {FLY_STRAIGHT, 2.0f, 0.0f},
    {SET_CAMERA, 0.0f, 2.0f},
    {FLY_STRAIGHT, 3.0f, 0.0f},
    {SET_CAMERA, 0.0f, 1.0f},
    {AUTO_PITCH_DOWN, 1.0f, 0.0f},
    {FLY_STRAIGHT, 3.0f, 0.0f},
    {SET_CAMERA, 0.0f, 8.0f},
    {FLY_STRAIGHT, 2.0f, 0.0f},
    {TOGGLE_GRID_VIEW, 0.0f, 0.0f},
    {FLY_STRAIGHT, 3.0f, 0.0f},
    {SET_CAMERA, 0.0f, 7.0f},
    {FLY_STRAIGHT, 1.0f, 0.0f},
    {TOGGLE_SPEED_LOCK, 0.0f, 0.0f}};
static const int numCommands = sizeof(flight_plan) / sizeof(AutopilotCommand);

// --- Shader Sources ---
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
    "void main() {\n"
    "   vec3 texColor = texture2D(u_texture, vTexCoord).rgb;\n"
    "   vec3 lightDir = normalize(u_lightDirection);\n"
    "   vec3 normal = normalize(vNormal);\n"
    "   float diffuse = max(dot(normal, lightDir), 0.25);\n"
    "   gl_FragColor = vec4(texColor * diffuse, 1.0);\n"
    "}\n";

void file_reader_callback_impl(void *ctx, const char *filename, int is_mtl, const char *obj_filename, char **buf, size_t *len)
{
    (void)ctx;
    (void)is_mtl;
    (void)obj_filename;
    long file_size;
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        *buf = NULL;
        *len = 0;
        return;
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    *buf = (char *)malloc(file_size + 1);
    fread(*buf, 1, file_size, fp);
    (*buf)[file_size] = '\0';
    *len = file_size;
    fclose(fp);
}

bool init_plane()
{
    planeShaderProgram = createShaderProgram(planeVertexSource, planeFragmentSource);
    if (planeShaderProgram == 0)
        return false;

    const char *plane_obj_path = "../include/models/plane.obj";
    int ret = tinyobj_parse_obj(&planeAttrib, &planeShapes, &numPlaneShapes,
                                &planeMaterials, &numPlaneMaterials, plane_obj_path,
                                file_reader_callback_impl, NULL, TINYOBJ_FLAG_TRIANGULATE);

    if (ret != TINYOBJ_SUCCESS || planeAttrib.num_vertices == 0)
    {
        fprintf(stderr, "TINYOBJ ERROR: Failed to parse plane object file.\n");
        return false;
    }

    const int width = IMAGE_WIDTH;
    const int height = IMAGE_HEIGHT;
    const int channels = IMAGE_CHANNELS;
    const unsigned char *original_data = plane_yavuzselim;

    printf("Loading and flipping plane texture from embedded data...\n");

    if (!original_data)
    {
        fprintf(stderr, "ERROR: Embedded plane texture data is missing.\n");
        return false;
    }

    // 1. Allocate a new buffer in memory for the flipped image data.
    const size_t image_size = width * height * channels;
    unsigned char *flipped_data = (unsigned char *)malloc(image_size);
    if (!flipped_data)
    {
        fprintf(stderr, "ERROR: Could not allocate memory for flipped plane texture.\n");
        return false;
    }

    // 2. Copy the data from the original buffer to the new one, flipping the rows.
    for (int i = 0; i < height; i++)
    {
        const unsigned char *src_row = original_data + ((height - 1 - i) * width * channels);
        unsigned char *dest_row = flipped_data + (i * width * channels);
        memcpy(dest_row, src_row, width * channels);
    }

    // 3. Generate and upload the NEW, flipped data to OpenGL.
    glGenTextures(1, &planeTextureID);
    glBindTexture(GL_TEXTURE_2D, planeTextureID);

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, flipped_data);

    // 4. Free the temporary buffer that we created.
    free(flipped_data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    printf("Successfully loaded plane texture from header.\n");

    planeDrawCount = planeAttrib.num_faces;
    GLfloat *v_buffer = (GLfloat *)malloc(planeDrawCount * 3 * sizeof(GLfloat));
    GLfloat *n_buffer = (GLfloat *)malloc(planeDrawCount * 3 * sizeof(GLfloat));
    GLfloat *t_buffer = (GLfloat *)malloc(planeDrawCount * 2 * sizeof(GLfloat));

    for (GLsizei i = 0; i < planeDrawCount; ++i)
    {
        tinyobj_vertex_index_t idx = planeAttrib.faces[i];
        int v_idx = idx.v_idx, n_idx = idx.vn_idx, t_idx = idx.vt_idx;
        memcpy(v_buffer + i * 3, planeAttrib.vertices + v_idx * 3, 3 * sizeof(GLfloat));
        if (n_idx >= 0)
            memcpy(n_buffer + i * 3, planeAttrib.normals + n_idx * 3, 3 * sizeof(GLfloat));
        if (t_idx >= 0)
            memcpy(t_buffer + i * 2, planeAttrib.texcoords + t_idx * 2, 2 * sizeof(GLfloat));
    }

    glGenBuffers(1, &planeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, planeDrawCount * 3 * sizeof(GLfloat), v_buffer, GL_STATIC_DRAW);
    glGenBuffers(1, &planeNBO);
    glBindBuffer(GL_ARRAY_BUFFER, planeNBO);
    glBufferData(GL_ARRAY_BUFFER, planeDrawCount * 3 * sizeof(GLfloat), n_buffer, GL_STATIC_DRAW);
    glGenBuffers(1, &planeTBO);
    glBindBuffer(GL_ARRAY_BUFFER, planeTBO);
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

void init_flight_model()
{
    glm_vec3_copy(planeStartingCoordinates, planePos);
    vec3 initialFront = {0.0f, 0.0f, -1.0f};
    vec3 worldUp = {0.0f, 1.0f, 0.0f};
    mat4 initialYawMatrix;
    glm_rotate_make(initialYawMatrix, glm_rad(-90.0f), worldUp);
    glm_mat4_mulv3(initialYawMatrix, initialFront, 1.0f, planeFront);
    glm_vec3_cross(planeFront, worldUp, planeRight);
    glm_normalize(planeRight);
    glm_vec3_cross(planeRight, planeFront, planeUp);
    glm_normalize(planeUp);
}

void init_second_flight_model()
{
    planeStartingCoordinates[0] = 0.0f;
    planeStartingCoordinates[1] = 500.0f;
    planeStartingCoordinates[2] = 50.0f;
    glm_vec3_copy(planeStartingCoordinates, secondPlanePos);
    vec3 initialFront = {0.0f, 0.0f, -1.0f};
    vec3 worldUp = {0.0f, 1.0f, 0.0f};
    mat4 initialYawMatrix;
    glm_rotate_make(initialYawMatrix, glm_rad(-90.0f), worldUp);
    glm_mat4_mulv3(initialYawMatrix, initialFront, 1.0f, secondPlaneFront);
    glm_vec3_cross(secondPlaneFront, worldUp, secondPlaneRight);
    glm_normalize(secondPlaneRight);
    glm_vec3_cross(secondPlaneRight, secondPlaneFront, secondPlaneUp);
    glm_normalize(secondPlaneUp);
}

void update_second_plane()
{
    float currentTime = glfwGetTime();
    float speed = 20.0f;
    
    // İkinci uçak önde gider (lider)
    secondPlanePos[0] = 100.0f + (currentTime * speed);  // Birinci uçaktan önde başlar
    secondPlanePos[2] = 50.0f;
    secondPlanePos[1] = 500.0f + 20.0f * sin(currentTime * 0.5f);
    
    // İkinci uçağın front vektörünü birinci uçakla aynı yöne ayarla
    glm_vec3_copy(planeFront, secondPlaneFront);
    glm_vec3_copy(planeRight, secondPlaneRight);
    glm_vec3_copy(planeUp, secondPlaneUp);
    
    // Model matrix'i güncelle
    glm_mat4_identity(secondPlaneModelMatrix);
    glm_translate(secondPlaneModelMatrix, secondPlanePos);
    
    // Birinci uçakla aynı rotasyonu uygula
    float yawAngle = atan2(-planeFront[0], -planeFront[2]);
    glm_rotate(secondPlaneModelMatrix, yawAngle, (vec3){0.0f, 1.0f, 0.0f});
}

void draw_plane(mat4 model, mat4 view, mat4 proj)
{
    glUseProgram(planeShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(planeShaderProgram, "model"), 1, GL_FALSE, (GLfloat *)model);
    glUniformMatrix4fv(glGetUniformLocation(planeShaderProgram, "view"), 1, GL_FALSE, (GLfloat *)view);
    glUniformMatrix4fv(glGetUniformLocation(planeShaderProgram, "proj"), 1, GL_FALSE, (GLfloat *)proj);
    glUniform3fv(glGetUniformLocation(planeShaderProgram, "u_lightDirection"), 1, lightDirection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, planeTextureID);
    glUniform1i(glGetUniformLocation(planeShaderProgram, "u_texture"), 0);

    GLint posAttrib = glGetAttribLocation(planeShaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);

    GLint normAttrib = glGetAttribLocation(planeShaderProgram, "normal");
    glEnableVertexAttribArray(normAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, planeNBO);
    glVertexAttribPointer(normAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);

    GLint texAttrib = glGetAttribLocation(planeShaderProgram, "texcoord");
    glEnableVertexAttribArray(texAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, planeTBO);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLES, 0, planeDrawCount);

    glDisableVertexAttribArray(posAttrib);
    glDisableVertexAttribArray(normAttrib);
    glDisableVertexAttribArray(texAttrib);
}

// --- Functions For Auto Pilot Mode

void applyUpPitch(float rotation_speed)
{
    // --- SMOOTH PITCH LIMIT LOGIC ---
    float currentPitchDegrees = glm_deg(asinf(planeFront[1]));

    // 1. Calculate how much "room" we have left before hitting the limit.
    float pitchHeadroom = MAX_PITCH_ANGLE_DEGREES - currentPitchDegrees;

    // Exit immediately if we are already at or past the limit.
    if (pitchHeadroom <= 0.0f)
    {
        return;
    }

    // 2. Create a smoothing factor based on the remaining headroom.
    //    This will be 1.0 when the plane is level (far from the limit) and
    //    will smoothly decrease to 0.0 as it approaches the limit.
    float smoothFactor = fmin(0.5f, pitchHeadroom / PITCH_SMOOTHING_RANGE);

    // To create a more "parabolic" ease-out, we can square the factor.
    // This makes the slowdown much more noticeable as it nears the end.
    smoothFactor = smoothFactor * smoothFactor;

    // 3. Calculate the final, smoothed rotation speed to apply this frame.
    float finalRotationSpeed = rotation_speed * smoothFactor;

    // --- Original Rotation Logic ---
    mat4 rotation;
    glm_rotate_make(rotation, glm_rad(finalRotationSpeed), planeRight);
    glm_mat4_mulv3(rotation, planeFront, 1.0f, planeFront);
    glm_mat4_mulv3(rotation, planeUp, 1.0f, planeUp);
}

void applyDownPitch(float rotation_speed)
{
    // --- SMOOTH PITCH LIMIT LOGIC ---
    float currentPitchDegrees = glm_deg(asinf(planeFront[1]));

    // 1. Calculate headroom for downward pitch. Note the signs are flipped.
    float pitchHeadroom = currentPitchDegrees - (-MAX_PITCH_ANGLE_DEGREES);

    // Exit if we are already past the limit.
    if (pitchHeadroom <= 0.0f)
    {
        return;
    }

    // 2. Create the same smoothing factor.
    float smoothFactor = fmin(0.5f, pitchHeadroom / PITCH_SMOOTHING_RANGE);
    smoothFactor = smoothFactor * smoothFactor; // Parabolic ease-out

    // 3. Calculate the final, smoothed rotation speed.
    float finalRotationSpeed = rotation_speed * smoothFactor;

    // --- Original Rotation Logic ---
    mat4 rotation;
    // Remember to use a negative rotation for pitching down.
    glm_rotate_make(rotation, glm_rad(-finalRotationSpeed), planeRight);
    glm_mat4_mulv3(rotation, planeFront, 1.0f, planeFront);
    glm_mat4_mulv3(rotation, planeUp, 1.0f, planeUp);
}

void applyLeftTurn(float rotation_speed, float roll_speed)
{
    mat4 rotation;
    vec3 worldUp = {0.0f, 1.0f, 0.0f};
    glm_rotate_make(rotation, glm_rad(rotation_speed), worldUp);
    glm_mat4_mulv3(rotation, planeFront, 1.0f, planeFront);
    glm_mat4_mulv3(rotation, planeRight, 1.0f, planeRight);
    glm_mat4_mulv3(rotation, planeUp, 1.0f, planeUp);
    glm_rotate_make(rotation, glm_rad(-roll_speed), planeFront);
    glm_mat4_mulv3(rotation, planeUp, 1.0f, planeUp);
    glm_mat4_mulv3(rotation, planeRight, 1.0f, planeRight);
}

void applyRightTurn(float rotation_speed, float roll_speed)
{
    mat4 rotation;
    vec3 worldUp = {0.0f, 1.0f, 0.0f};
    glm_rotate_make(rotation, glm_rad(-rotation_speed), worldUp);
    glm_mat4_mulv3(rotation, planeFront, 1.0f, planeFront);
    glm_mat4_mulv3(rotation, planeRight, 1.0f, planeRight);
    glm_mat4_mulv3(rotation, planeUp, 1.0f, planeUp);
    glm_rotate_make(rotation, glm_rad(roll_speed), planeFront);
    glm_mat4_mulv3(rotation, planeUp, 1.0f, planeUp);
    glm_mat4_mulv3(rotation, planeRight, 1.0f, planeRight);
}

void applyAutoLeveling()
{
    vec3 worldUp = {0.0f, 1.0f, 0.0f};
    vec3 frontHorizontal;
    glm_vec3_copy(planeFront, frontHorizontal);
    frontHorizontal[1] = 0;
    if (glm_vec3_norm(frontHorizontal) > 0.001f)
    {
        glm_normalize(frontHorizontal);
        vec3 idealRight;
        glm_vec3_cross(frontHorizontal, worldUp, idealRight);
        glm_normalize(idealRight);
        vec3 idealUp;
        glm_vec3_cross(idealRight, planeFront, idealUp);
        glm_normalize(idealUp);
        glm_vec3_lerp(planeUp, idealUp, deltaTime * 1.0f, planeUp);
    }
}

void reset_autopilot_state()
{
    // This function can see and reset the static variables
    currentCommandIndex = 0;
    commandTimer = 0.0f;
}

// ===========================================================================================
// === FINAL, CORRECTED Version of autoPilotMode() with Vertical Speed Stabilizer        ===
// ===========================================================================================
void autoPilotMode()
{
    commandTimer += deltaTime;
    AutopilotCommand currentCommand = flight_plan[currentCommandIndex];

    if (commandTimer >= currentCommand.duration)
    {
        commandTimer = 0.0f;
        currentCommandIndex = (currentCommandIndex + 1) % numCommands;
        currentCommand = flight_plan[currentCommandIndex];
    }

    float rotation_speed = 45.0f * deltaTime;
    float roll_speed = 30.0f * deltaTime;

    // --- NEW SMOOTH ALTITUDE CONTROLLER LOGIC ---
    float groundHeight = get_terrain_height(planePos[0], planePos[2]);
    float heightAboveGround = planePos[1] - groundHeight;
    float pitchChange = 0.0f; // This will be the final pitch adjustment for this frame

    const float altitudeDeadZone = 10.0f;
    const float vsSensitivity = 0.5f;

    if (heightAboveGround < minimumHeightFromTheGroundAutoPilot - altitudeDeadZone)
    {
        float targetClimbSpeed = 50.0f;
        float vsError = targetClimbSpeed - verticalSpeed;
        pitchChange = vsError * vsSensitivity * deltaTime;
    }
    else if (heightAboveGround > maximumHeightFromTheGroundAutoPilot + altitudeDeadZone)
    {
        float targetDescendSpeed = -30.0f;
        float vsError = targetDescendSpeed - verticalSpeed;
        pitchChange = vsError * vsSensitivity * deltaTime;
    }
    else
    {
        float vsError = 0.0f - verticalSpeed;
        pitchChange = vsError * vsSensitivity * deltaTime;
    }

    // --- YOUR ORIGINAL UNIFIED ACTION SWITCH ---
    switch (currentCommand.action)
    {
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
        if (currentMovementSpeed < currentCommand.value)
        {
            currentMovementSpeed += (accelerationSpeeding / 2) * deltaTime;
            offset_behind += 0.5f;
            if (currentMovementSpeed > currentCommand.value)
                currentMovementSpeed = currentCommand.value;
        }
        else
        {
            currentMovementSpeed -= (accelerationSpeeding / 2) * deltaTime; // accelerationSlowing
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
        // This is an instantaneous action, but it will hold for the specified duration.
        if (currentCommand.value == 1.0f)
        {
            offset_behind = 400.0f;
            offset_above = 170.0f;
            offset_right = 0.0f;
        }
        else if (currentCommand.value == 2.0f)
        {
            offset_behind = -50.0f;
            offset_above = 30.0f;
            offset_right = 0.0f;
        }
        else if (currentCommand.value == 3.0f)
        {
            offset_behind = 400.0f;
            offset_above = 500.0f;
            offset_right = 0.0f;
        }
        else if (currentCommand.value == 4.0f)
        {
            offset_behind = -300.0f;
            offset_above = 100.0f;
            offset_right = 0.0f;
        }
        else if (currentCommand.value == 5.0f)
        {
            offset_behind = -200.0f;
            offset_above = 150.0f;
            offset_right = 250.0f;
        }
        else if (currentCommand.value == 6.0f)
        {
            offset_behind = -200.0f;
            offset_above = 150.0f;
            offset_right = -250.0f;
        }
        else if (currentCommand.value == 7.0f)
        {
            offset_behind = -300.0f;
            offset_above = -100.0f;
            offset_right = 0.0f;
        }
        else if (currentCommand.value == 8.0f)
        {
            offset_behind = -75.0f;
            offset_above = 200.0f;
            offset_right = 550.0f;
        }
        break;
    case TOGGLE_SPEED_LOCK:
        // This is complex. To make it work as a single command, it needs a state change
        // that is only triggered once. We'll handle this by assuming duration is 0.
        if (commandTimer < deltaTime * 1.5f)
        { // Only run on the first frame
            isSpeedFixed = !isSpeedFixed;
            if (isSpeedFixed)
                fixedValue = currentMovementSpeed;
        }
        break;
    case TOGGLE_GRID_VIEW:
        if (commandTimer < deltaTime * 1.5f)
        { // Only run on the first frame
            isTriangleViewMode = !isTriangleViewMode;
        }
        break;

    case FLY_STRAIGHT:
    default:
        applyAutoLeveling();
        break;
    }

    // --- APPLY ALTITUDE CORRECTION ---
    // The altitude controller runs in parallel with the flight plan,
    // unless the command is a manual pitch override.
    if (currentCommand.action != AUTO_PITCH_UP && currentCommand.action != AUTO_PITCH_DOWN)
    {
        applyUpPitch(pitchChange);
    }

    // --- HANDLE ZOOM RESET ---
    // If the current command is NOT hold zoom, smoothly return to original FOV.
    if (currentCommand.action != HOLD_ZOOM && fov < originalFov)
    {
        fov += zoomingSpeed * deltaTime;
        if (fov > originalFov)
            fov = originalFov;
    }

    // Final vector normalization (always do this)
    glm_normalize(planeFront);
    glm_vec3_cross(planeFront, planeUp, planeRight);
    glm_normalize(planeRight);
    glm_vec3_cross(planeRight, planeFront, planeUp);
    glm_normalize(planeUp);
}

void cleanup_plane()
{
    glDeleteProgram(planeShaderProgram);
    glDeleteBuffers(1, &planeVBO);
    glDeleteBuffers(1, &planeNBO);
    glDeleteBuffers(1, &planeTBO);
    glDeleteTextures(1, &planeTextureID);
}
