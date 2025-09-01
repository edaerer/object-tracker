// =========================================================================================
// ===         UPDATED minimap.c with Fixed Const Qualifier Issues                     ===
// =========================================================================================

#include "globals.h"
#include "minimap.h"

// --- Minimap-Specific Global State ---
#define MINIMAP_TEXTURE_SIZE 256

static GLuint minimapShaderProgram, minimapVBO, minimapEBO, trailTextureID;
static unsigned char *trailPixelData = NULL;

// The primary state variables updated each frame
static vec2 translationVector;
static float playerIconAngle = 0.0f;

// Second starfighter globals
static GLuint secondStarfighterTextureID;
static unsigned char *secondStarfighterPixelData = NULL;
static vec2 secondTranslationVector;
static float secondPlayerIconAngle = 0.0f;

// --- Minimap Settings ---
static float miniMapSize = 2.0f;
static float transparencyOfMiniMap = 0.8f;

// --- Color Schemes ---
typedef struct
{
    vec3 fuselageColor;
    vec3 wingColor;
    vec3 cockpitColor;
    vec3 thrusterColor;
    vec3 outlineColor;
} StarfighterColors;

// Remove const to avoid qualifier issues
static StarfighterColors PRIMARY_COLORS = {
    {0.7f, 0.75f, 0.8f}, // Grey fuselage
    {0.0f, 0.8f, 0.3f},  // Green wings
    {0.2f, 0.9f, 1.0f},  // Cyan cockpit
    {1.0f, 0.5f, 0.1f},  // Orange thruster
    {0.1f, 0.1f, 0.15f}  // Dark grey outline
};

static StarfighterColors SECONDARY_COLORS = {
    {0.8f, 0.3f, 0.2f}, // Red fuselage
    {0.9f, 0.4f, 0.1f}, // Orange wings
    {1.0f, 0.8f, 0.2f}, // Yellow cockpit
    {1.0f, 0.2f, 0.4f}, // Pink thruster
    {0.3f, 0.1f, 0.1f}  // Dark red outline
};

// --- Forward Declarations for Helper Functions ---
static void set_pixel_alpha_target(int x, int y, vec3 color, unsigned char alpha, unsigned char *targetData);
static bool is_inside_triangle(vec2 p, vec2 tri[3]);
static float signed_dist_to_triangle(vec2 p, vec2 tri1[3], vec2 tri2[3]);

// --- Shader Sources ---
static const GLchar *minimapVertexSource =
    "#version 100\n"
    "attribute vec2 position; attribute vec2 texcoord; varying vec2 vTexcoord;"
    "void main() { vTexcoord = texcoord; gl_Position = vec4(position, 0.0, 1.0); }";

static const GLchar *minimapFragmentSource =
    "#version 100\n"
    "precision mediump float; varying vec2 vTexcoord;\n"
    "uniform sampler2D mapTexture; uniform sampler2D trailTexture; uniform sampler2D secondTrailTexture;\n"
    "uniform float alpha; uniform vec2 u_mapOffset;\n"
    "uniform float u_rotationAngle; uniform float u_secondRotationAngle;\n"
    "uniform vec2 u_secondMapOffset;\n"
    "uniform float u_primaryDepth; uniform float u_secondaryDepth;\n"
    "void main() {\n"
    "    vec4 mapColor = texture2D(mapTexture, vTexcoord + u_mapOffset);\n"

    // First starfighter (center reference)
    "    vec2 centered_tc = vTexcoord - 0.5;\n"
    "    float cos_a = cos(u_rotationAngle);\n"
    "    float sin_a = sin(u_rotationAngle);\n"
    "    vec2 rotated_tc;\n"
    "    rotated_tc.x = centered_tc.x * cos_a - centered_tc.y * sin_a;\n"
    "    rotated_tc.y = centered_tc.x * sin_a + centered_tc.y * cos_a;\n"
    "    vec4 trailColor = texture2D(trailTexture, rotated_tc + 0.5);\n"

    // Second starfighter - FIX: İkinci uçak için doğru koordinat hesaplama
    "    vec2 second_screen_coord = vTexcoord + u_mapOffset - u_secondMapOffset;\n"
    "    vec2 second_centered = second_screen_coord - 0.5;\n"
    "    float cos_b = cos(u_secondRotationAngle);\n"
    "    float sin_b = sin(u_secondRotationAngle);\n"
    "    vec2 second_rotated;\n"
    "    second_rotated.x = second_centered.x * cos_b - second_centered.y * sin_b;\n"
    "    second_rotated.y = second_centered.x * sin_b + second_centered.y * cos_b;\n"
    "    vec4 secondTrailColor = texture2D(secondTrailTexture, second_rotated + 0.5);\n"

    // Depth-based blending
    "    vec3 tempColor = mapColor.rgb;\n"
    "    if (u_primaryDepth > u_secondaryDepth) {\n"
    "        tempColor = mix(tempColor, secondTrailColor.rgb, secondTrailColor.a);\n"
    "        tempColor = mix(tempColor, trailColor.rgb, trailColor.a);\n"
    "    } else {\n"
    "        tempColor = mix(tempColor, trailColor.rgb, trailColor.a);\n"
    "        tempColor = mix(tempColor, secondTrailColor.rgb, secondTrailColor.a);\n"
    "    }\n"
    "    gl_FragColor = vec4(tempColor, alpha);\n"
    "}\n";
// --- Internal Helper & Drawing Functions ---

static void get_continuous_normalized_position(vec2 out_pos, const vec3 world_pos)
{
    float total_width = (float)terrain_width;
    float total_height = (float)terrain_height;
    out_pos[0] = (world_pos[0] + total_width / 2.0f) / total_width;
    out_pos[1] = (world_pos[2] + total_height / 2.0f) / total_height;
}

// --- SHARED DRAWING FUNCTION ---
static void draw_starfighter_icon(float angle_rad, StarfighterColors *colors, unsigned char *targetPixelData)
{
    const float NOSE_POINT_SIZE = 3.5f;
    const float TAIL_POINT_SIZE = 4.0f;

    int centerX = MINIMAP_TEXTURE_SIZE / 2;
    int centerY = MINIMAP_TEXTURE_SIZE / 2;
    float size = 15.0f;

    const float feather_width = 1.0f;
    float cos_a = cos(angle_rad);
    float sin_a = sin(angle_rad);
    const float fuselage_width = size * 0.2f;

    // Define vertices for starfighter shape
    vec2 p_nose = {0.0f, 1.0f * size};
    vec2 p_tail = {0.0f, -1.0f * size};
    vec2 p_cockpit_left = {-fuselage_width, 0.4f * size};
    vec2 p_cockpit_right = {fuselage_width, 0.4f * size};
    vec2 p_tail_left = {-fuselage_width, -0.8f * size};
    vec2 p_tail_right = {fuselage_width, -0.8f * size};
    vec2 p_left_wingtip = {-1.5f * size, -0.75f * size};
    vec2 p_right_wingtip = {1.5f * size, -0.75f * size};

    // Define triangles
    vec2 t_fuselage_front[3];
    glm_vec2_copy(p_nose, t_fuselage_front[0]);
    glm_vec2_copy(p_cockpit_left, t_fuselage_front[1]);
    glm_vec2_copy(p_cockpit_right, t_fuselage_front[2]);

    vec2 t_fuselage_back1[3];
    glm_vec2_copy(p_cockpit_left, t_fuselage_back1[0]);
    glm_vec2_copy(p_tail_left, t_fuselage_back1[1]);
    glm_vec2_copy(p_tail_right, t_fuselage_back1[2]);

    vec2 t_fuselage_back2[3];
    glm_vec2_copy(p_cockpit_left, t_fuselage_back2[0]);
    glm_vec2_copy(p_tail_right, t_fuselage_back2[1]);
    glm_vec2_copy(p_cockpit_right, t_fuselage_back2[2]);

    vec2 t_left_wing[3];
    glm_vec2_copy(p_cockpit_left, t_left_wing[0]);
    glm_vec2_copy(p_left_wingtip, t_left_wing[1]);
    glm_vec2_copy(p_tail_left, t_left_wing[2]);

    vec2 t_right_wing[3];
    glm_vec2_copy(p_cockpit_right, t_right_wing[0]);
    glm_vec2_copy(p_right_wingtip, t_right_wing[1]);
    glm_vec2_copy(p_tail_right, t_right_wing[2]);

    // Calculate bounding box
    float min_x = 0.0f, max_x = 0.0f, min_y = 0.0f, max_y = 0.0f;
    vec2 points[4];
    glm_vec2_copy(p_nose, points[0]);
    glm_vec2_copy(p_left_wingtip, points[1]);
    glm_vec2_copy(p_right_wingtip, points[2]);
    glm_vec2_copy(p_tail, points[3]);

    for (int i = 0; i < 4; ++i)
    {
        float rx = points[i][0] * cos_a - points[i][1] * sin_a;
        float ry = points[i][0] * sin_a + points[i][1] * cos_a;
        if (i == 0)
        {
            min_x = max_x = rx;
            min_y = max_y = ry;
        }
        else
        {
            min_x = fmin(min_x, rx);
            max_x = fmax(max_x, rx);
            min_y = fmin(min_y, ry);
            max_y = fmax(max_y, ry);
        }
    }

    int minX = (int)floorf(min_x - fmax(NOSE_POINT_SIZE, TAIL_POINT_SIZE) - feather_width);
    int maxX = (int)ceilf(max_x + fmax(NOSE_POINT_SIZE, TAIL_POINT_SIZE) + feather_width);
    int minY = (int)floorf(min_y - fmax(NOSE_POINT_SIZE, TAIL_POINT_SIZE) - feather_width);
    int maxY = (int)ceilf(max_y + fmax(NOSE_POINT_SIZE, TAIL_POINT_SIZE) + feather_width);

    // Rasterization
    for (int y = minY; y <= maxY; y++)
    {
        for (int x = minX; x <= maxX; x++)
        {
            float local_x = x * cos_a + y * sin_a;
            float local_y = y * cos_a - x * sin_a;

            // Check if pixel is inside ANY part of the composite shape
            bool is_in_fuselage = is_inside_triangle((vec2){local_x, local_y}, t_fuselage_front) ||
                                  is_inside_triangle((vec2){local_x, local_y}, t_fuselage_back1) ||
                                  is_inside_triangle((vec2){local_x, local_y}, t_fuselage_back2);
            bool is_in_left_wing = is_inside_triangle((vec2){local_x, local_y}, t_left_wing);
            bool is_in_right_wing = is_inside_triangle((vec2){local_x, local_y}, t_right_wing);

            float dist_to_nose = glm_vec2_distance((vec2){local_x, local_y}, p_nose);
            float dist_to_tail = glm_vec2_distance((vec2){local_x, local_y}, p_tail);
            bool is_in_nose_circle = dist_to_nose < NOSE_POINT_SIZE;
            bool is_in_tail_circle = dist_to_tail < TAIL_POINT_SIZE;

            if (is_in_fuselage || is_in_left_wing || is_in_right_wing || is_in_nose_circle || is_in_tail_circle)
            {
                // Calculate distance to the nearest edge for Anti-Aliasing
                float dist = 999.0f;
                if (is_in_fuselage)
                    dist = fmin(dist, signed_dist_to_triangle((vec2){local_x, local_y}, t_fuselage_front, t_fuselage_back1));
                if (is_in_left_wing)
                    dist = fmin(dist, signed_dist_to_triangle((vec2){local_x, local_y}, t_left_wing, NULL));
                if (is_in_right_wing)
                    dist = fmin(dist, signed_dist_to_triangle((vec2){local_x, local_y}, t_right_wing, NULL));
                if (is_in_nose_circle)
                    dist = fmin(dist, NOSE_POINT_SIZE - dist_to_nose);
                if (is_in_tail_circle)
                    dist = fmin(dist, TAIL_POINT_SIZE - dist_to_tail);

                float alpha = glm_smoothstep(0.0f, feather_width, dist);
                if (alpha <= 0.0f)
                    continue;

                vec3 final_color;
                // Set Color based on a priority system (points override body)
                if (is_in_nose_circle)
                {
                    glm_vec3_copy(colors->fuselageColor, final_color);
                    float cockpit_glow = 1.0f - glm_smoothstep(0.0, NOSE_POINT_SIZE, dist_to_nose);
                    glm_vec3_lerp(final_color, colors->cockpitColor, cockpit_glow * cockpit_glow, final_color);
                }
                else if (is_in_tail_circle)
                {
                    glm_vec3_copy(colors->fuselageColor, final_color);
                    float thruster_glow = 1.0f - glm_smoothstep(0.0, TAIL_POINT_SIZE, dist_to_tail);
                    glm_vec3_lerp(final_color, colors->thrusterColor, thruster_glow, final_color);
                }
                else if (is_in_left_wing || is_in_right_wing)
                {
                    glm_vec3_copy(colors->wingColor, final_color);
                }
                else
                { // In fuselage
                    glm_vec3_copy(colors->fuselageColor, final_color);
                    float gradient_t = (local_y + size) / (2.0f * size);
                    glm_vec3_lerp(final_color, (vec3){0.4f, 0.45f, 0.5f}, 1.0f - gradient_t, final_color);
                }

                glm_vec3_lerp(colors->outlineColor, final_color, glm_smoothstep(feather_width * 0.5f, feather_width, dist), final_color);

                set_pixel_alpha_target(centerX + x, centerY - y, final_color, (unsigned char)(alpha * 255.0f), targetPixelData);
            }
        }
    }
}

// --- Main Minimap Interface Functions ---

void init_minimap()
{
    if (minimapShaderProgram == 0)
    {
        minimapShaderProgram = createShaderProgram(minimapVertexSource, minimapFragmentSource);

        const float anchorX = 0.95f, anchorY = 0.95f;
        const float baseWidth = 0.25f, baseHeight = 0.30f;
        float scaledWidth = baseWidth * miniMapSize;
        float scaledHeight = baseHeight * miniMapSize;

        GLfloat minimap_vertices[] = {
            anchorX - scaledWidth, anchorY, 0.0f, 0.0f,
            anchorX - scaledWidth, anchorY - scaledHeight, 0.0f, 1.0f,
            anchorX, anchorY - scaledHeight, 1.0f, 1.0f,
            anchorX, anchorY, 1.0f, 0.0f};
        GLuint minimap_indices[] = {0, 1, 2, 0, 2, 3};

        glGenBuffers(1, &minimapVBO);
        glGenBuffers(1, &minimapEBO);
        glBindBuffer(GL_ARRAY_BUFFER, minimapVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(minimap_vertices), minimap_vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, minimapEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(minimap_indices), minimap_indices, GL_STATIC_DRAW);

        // First starfighter
        trailPixelData = (unsigned char *)calloc(MINIMAP_TEXTURE_SIZE * MINIMAP_TEXTURE_SIZE * 4, sizeof(unsigned char));
        draw_starfighter_icon(0.0f, &PRIMARY_COLORS, trailPixelData);

        glGenTextures(1, &trailTextureID);
        glBindTexture(GL_TEXTURE_2D, trailTextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MINIMAP_TEXTURE_SIZE, MINIMAP_TEXTURE_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, trailPixelData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Second starfighter
        secondStarfighterPixelData = (unsigned char *)calloc(MINIMAP_TEXTURE_SIZE * MINIMAP_TEXTURE_SIZE * 4, sizeof(unsigned char));
        draw_starfighter_icon(0.0f, &SECONDARY_COLORS, secondStarfighterPixelData);

        glGenTextures(1, &secondStarfighterTextureID);
        glBindTexture(GL_TEXTURE_2D, secondStarfighterTextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MINIMAP_TEXTURE_SIZE, MINIMAP_TEXTURE_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, secondStarfighterPixelData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

void update_minimap_dot()
{
    if (isCrashed)
        return;

    // First starfighter (existing)
    vec2 continuousPos;
    get_continuous_normalized_position(continuousPos, planePos);
    translationVector[0] = continuousPos[0] - 0.5f;
    translationVector[1] = continuousPos[1] - 0.5f;
    playerIconAngle = atan2(-planeFront[0], -planeFront[2]);

    // Second starfighter - DÜZELTME: Doğru pozisyon hesaplama
    vec2 secondContinuousPos;
    get_continuous_normalized_position(secondContinuousPos, secondPlanePos);
    secondTranslationVector[0] = secondContinuousPos[0] - 0.5f;
    secondTranslationVector[1] = secondContinuousPos[1] - 0.5f;
    
    // İkinci uçağın açısını front vektöründen hesapla
    secondPlayerIconAngle = atan2(-secondPlaneFront[0], -secondPlaneFront[2]);

    // Debug output
    static int debug_counter = 0;
    if (debug_counter++ % 60 == 0)
    { 
        printf("=== MINIMAP DEBUG ===\n");
        printf("Primary Plane:\n");
        printf("  World Pos: (%.1f, %.1f, %.1f)\n", planePos[0], planePos[1], planePos[2]);
        printf("  Normalized: (%.3f, %.3f)\n", continuousPos[0], continuousPos[1]);
        printf("  Translation: (%.3f, %.3f)\n", translationVector[0], translationVector[1]);
        printf("  Angle: %.2f degrees\n", glm_deg(playerIconAngle));

        printf("Second Plane:\n");
        printf("  World Pos: (%.1f, %.1f, %.1f)\n", secondPlanePos[0], secondPlanePos[1], secondPlanePos[2]);
        printf("  Normalized: (%.3f, %.3f)\n", secondContinuousPos[0], secondContinuousPos[1]);
        printf("  Translation: (%.3f, %.3f)\n", secondTranslationVector[0], secondTranslationVector[1]);
        printf("  Angle: %.2f degrees\n", glm_deg(secondPlayerIconAngle));
        printf("====================\n\n");
    }
}

void draw_minimap()
{
    glUseProgram(minimapShaderProgram);

    // Existing uniforms
    glUniform2fv(glGetUniformLocation(minimapShaderProgram, "u_mapOffset"), 1, translationVector);
    glUniform1f(glGetUniformLocation(minimapShaderProgram, "u_rotationAngle"), playerIconAngle);

    // New uniforms for second starfighter
    glUniform2fv(glGetUniformLocation(minimapShaderProgram, "u_secondMapOffset"), 1, secondTranslationVector);
    glUniform1f(glGetUniformLocation(minimapShaderProgram, "u_secondRotationAngle"), secondPlayerIconAngle);

    // Bind all three textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, heightMapTextureID);
    glUniform1i(glGetUniformLocation(minimapShaderProgram, "mapTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, trailTextureID);
    glUniform1i(glGetUniformLocation(minimapShaderProgram, "trailTexture"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, secondStarfighterTextureID);
    glUniform1i(glGetUniformLocation(minimapShaderProgram, "secondTrailTexture"), 2);

    glUniform1f(glGetUniformLocation(minimapShaderProgram, "alpha"), transparencyOfMiniMap);

    glBindBuffer(GL_ARRAY_BUFFER, minimapVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, minimapEBO);

    GLint posAttrib = glGetAttribLocation(minimapShaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void *)0);

    GLint texAttrib = glGetAttribLocation(minimapShaderProgram, "texcoord");
    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void *)(2 * sizeof(GLfloat)));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glDisableVertexAttribArray(posAttrib);
    glDisableVertexAttribArray(texAttrib);

    glActiveTexture(GL_TEXTURE0);
}

void cleanup_minimap()
{
    glDeleteProgram(minimapShaderProgram);
    glDeleteBuffers(1, &minimapVBO);
    glDeleteBuffers(1, &minimapEBO);
    glDeleteTextures(1, &trailTextureID);
    glDeleteTextures(1, &secondStarfighterTextureID);

    if (trailPixelData)
    {
        free(trailPixelData);
        trailPixelData = NULL;
    }

    if (secondStarfighterPixelData)
    {
        free(secondStarfighterPixelData);
        secondStarfighterPixelData = NULL;
    }
}

// --- Helper Functions for Procedural Drawing ---

bool is_inside_triangle(vec2 p, vec2 tri[3])
{
    float d1 = (p[0] - tri[1][0]) * (tri[0][1] - tri[1][1]) - (tri[0][0] - tri[1][0]) * (p[1] - tri[1][1]);
    float d2 = (p[0] - tri[2][0]) * (tri[1][1] - tri[2][1]) - (tri[1][0] - tri[2][0]) * (p[1] - tri[2][1]);
    float d3 = (p[0] - tri[0][0]) * (tri[2][1] - tri[0][1]) - (tri[2][0] - tri[0][0]) * (p[1] - tri[0][1]);
    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

float signed_dist_to_edge(vec2 p, vec2 v0, vec2 v1)
{
    vec2 pv;
    pv[0] = p[0] - v0[0];
    pv[1] = p[1] - v0[1];

    vec2 vv;
    vv[0] = v1[0] - v0[0];
    vv[1] = v1[1] - v0[1];

    float h = glm_clamp(glm_vec2_dot(pv, vv) / glm_vec2_dot(vv, vv), 0.0, 1.0);

    vec2 proj_point;
    proj_point[0] = vv[0] * h;
    proj_point[1] = vv[1] * h;

    return glm_vec2_distance(pv, proj_point);
}

float signed_dist_to_triangle(vec2 p, vec2 tri1[3], vec2 tri2[3])
{
    float d = signed_dist_to_edge(p, tri1[0], tri1[1]);
    d = fmin(d, signed_dist_to_edge(p, tri1[1], tri1[2]));
    d = fmin(d, signed_dist_to_edge(p, tri1[2], tri1[0]));
    if (tri2 != NULL)
    {
        d = fmin(d, signed_dist_to_edge(p, tri2[0], tri2[1]));
        d = fmin(d, signed_dist_to_edge(p, tri2[1], tri2[2]));
        d = fmin(d, signed_dist_to_edge(p, tri2[2], tri2[0]));
    }
    return d;
}

static void set_pixel_alpha_target(int x, int y, vec3 color, unsigned char alpha, unsigned char *targetData)
{
    if (x < 0 || x >= MINIMAP_TEXTURE_SIZE || y < 0 || y >= MINIMAP_TEXTURE_SIZE || !targetData)
        return;

    int index = (y * MINIMAP_TEXTURE_SIZE + x) * 4;
    vec3 background_color = {
        (float)targetData[index + 0] / 255.0f,
        (float)targetData[index + 1] / 255.0f,
        (float)targetData[index + 2] / 255.0f};
    float bg_alpha = (float)targetData[index + 3] / 255.0f;

    float new_alpha_f = (float)alpha / 255.0f;
    float final_alpha = new_alpha_f + bg_alpha * (1.0f - new_alpha_f);
    if (final_alpha < 1e-5)
        return;

    vec3 blended_color;
    blended_color[0] = (color[0] * new_alpha_f + background_color[0] * bg_alpha * (1.0f - new_alpha_f)) / final_alpha;
    blended_color[1] = (color[1] * new_alpha_f + background_color[1] * bg_alpha * (1.0f - new_alpha_f)) / final_alpha;
    blended_color[2] = (color[2] * new_alpha_f + background_color[2] * bg_alpha * (1.0f - new_alpha_f)) / final_alpha;

    targetData[index + 0] = (unsigned char)(blended_color[0] * 255.0f);
    targetData[index + 1] = (unsigned char)(blended_color[1] * 255.0f);
    targetData[index + 2] = (unsigned char)(blended_color[2] * 255.0f);
    targetData[index + 3] = (unsigned char)(final_alpha * 255.0f);
}

void reset_minimap_for_restart()
{
    // With the new GPU approach, no specific reset logic is needed here,
    // as update_minimap_dot() will provide the correct state each frame.
}