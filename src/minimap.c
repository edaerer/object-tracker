#include "minimap.h"
#include "globals.h"

#define MINIMAP_TEXTURE_SIZE 256
#define MAX_STARFIGHTERS MAX_PLANES

static GLuint minimapShaderProgram, minimapVBO, minimapEBO;
static GLuint starfighterTextureIDs[MAX_STARFIGHTERS];
static unsigned char *starfighterPixelData[MAX_STARFIGHTERS];
static vec2 starfighterTranslationVectors[MAX_STARFIGHTERS];
static float starfighterIconAngles[MAX_STARFIGHTERS];
static float miniMapSize = 2.0f;
static float transparencyOfMiniMap = 0.8f;
static StarfighterColors *starfighterColors[MAX_STARFIGHTERS];
static vec3 *starfighterPositions[MAX_STARFIGHTERS];
static vec3 *starfighterFronts[MAX_STARFIGHTERS];

static void set_pixel_alpha_target(int x, int y, vec3 color, unsigned char alpha, unsigned char *targetData);
static bool is_inside_triangle(vec2 p, vec2 tri[3]);
static float signed_dist_to_triangle(vec2 p, vec2 tri1[3], vec2 tri2[3]);
static void init_starfighter_texture(int index);
static void update_starfighter_position(int index);
static void cleanup_starfighter(int index);
static char *generate_fragment_shader();

static const GLchar *minimapVertexSource =
        "#version 100\n"
        "attribute vec2 position; attribute vec2 texcoord; varying vec2 vTexcoord;"
        "void main() { vTexcoord = texcoord; gl_Position = vec4(position, 0.0, 1.0); }";

static char *generate_fragment_shader() {
    static char shader_buffer[8192];

    strcpy(shader_buffer, "#version 100\n"
                          "precision mediump float; varying vec2 vTexcoord;\n"
                          "uniform sampler2D mapTexture;\n");

    for (int i = 0; i < MAX_STARFIGHTERS; i++) {
        char texture_uniform[64];
        snprintf(texture_uniform, sizeof(texture_uniform), "uniform sampler2D starfighterTexture%d;\n", i);
        strcat(shader_buffer, texture_uniform);
    }

    strcat(shader_buffer, "uniform float alpha; uniform vec2 u_mapOffset;\n");

    for (int i = 0; i < MAX_STARFIGHTERS; i++) {
        char rotation_uniform[64];
        snprintf(rotation_uniform, sizeof(rotation_uniform), "uniform float u_rotationAngle%d; ", i);
        strcat(shader_buffer, rotation_uniform);
    }
    strcat(shader_buffer, "\n");

    for (int i = 0; i < MAX_STARFIGHTERS; i++) {
        char offset_uniform[64];
        snprintf(offset_uniform, sizeof(offset_uniform), "uniform vec2 u_starfighterOffset%d; ", i);
        strcat(shader_buffer, offset_uniform);
    }
    strcat(shader_buffer, "\n");

    for (int i = 0; i < MAX_STARFIGHTERS; i++) {
        char depth_uniform[64];
        snprintf(depth_uniform, sizeof(depth_uniform), "uniform float u_starfighterDepth%d; ", i);
        strcat(shader_buffer, depth_uniform);
    }
    strcat(shader_buffer, "\n");

    strcat(shader_buffer, "void main() {\n"
                          "    vec4 mapColor = texture2D(mapTexture, vTexcoord + u_mapOffset);\n"
                          "    vec3 tempColor = mapColor.rgb;\n"
                          "    \n"
                          "    // Depth array oluştur\n");

    char depth_array[512];
    if (MAX_STARFIGHTERS == 1) {
        strcpy(depth_array, "    float depths[1];\n    depths[0] = u_starfighterDepth0;\n");
    } else if (MAX_STARFIGHTERS == 2) {
        strcpy(depth_array,
               "    float depths[2];\n    depths[0] = u_starfighterDepth0; depths[1] = u_starfighterDepth1;\n");
    } else if (MAX_STARFIGHTERS == 3) {
        strcpy(depth_array, "    float depths[3];\n    depths[0] = u_starfighterDepth0; depths[1] = "
                            "u_starfighterDepth1; depths[2] = u_starfighterDepth2;\n");
    } else {
        snprintf(depth_array, sizeof(depth_array), "    float depths[%d];\n", MAX_STARFIGHTERS);
        for (int i = 0; i < MAX_STARFIGHTERS; i++) {
            char depth_assign[64];
            snprintf(depth_assign, sizeof(depth_assign), "    depths[%d] = u_starfighterDepth%d;\n", i, i);
            strcat(depth_array, depth_assign);
        }
    }
    strcat(shader_buffer, depth_array);

    char rendering_loop[2048];
    snprintf(rendering_loop, sizeof(rendering_loop),
             "    \n"
             "    // Basit bubble sort ile depth sıralaması (back to front)\n"
             "    int sortedIndices[%d];\n"
             "    for(int i = 0; i < %d; i++) sortedIndices[i] = i;\n"
             "    \n"
             "    for(int i = 0; i < %d - 1; i++) {\n"
             "        for(int j = 0; j < %d - 1 - i; j++) {\n"
             "            if(depths[sortedIndices[j]] > depths[sortedIndices[j + 1]]) {\n"
             "                int temp = sortedIndices[j];\n"
             "                sortedIndices[j] = sortedIndices[j + 1];\n"
             "                sortedIndices[j + 1] = temp;\n"
             "            }\n"
             "        }\n"
             "    }\n"
             "    \n"
             "    // Render each starfighter in depth order\n"
             "    for(int i = 0; i < %d; i++) {\n"
             "        int renderIndex = sortedIndices[i];\n"
             "        vec2 offset, screen_coord, centered, rotated;\n"
             "        float angle, cos_angle, sin_angle;\n"
             "        vec4 starfighterColor;\n"
             "        \n",
             MAX_STARFIGHTERS, MAX_STARFIGHTERS, MAX_STARFIGHTERS, MAX_STARFIGHTERS, MAX_STARFIGHTERS);
    strcat(shader_buffer, rendering_loop);

    strcat(shader_buffer, "        // Select offset and angle based on renderIndex\n");
    for (int i = 0; i < MAX_STARFIGHTERS; i++) {
        char case_code[256];
        if (i == 0) {
            snprintf(case_code, sizeof(case_code),
                     "        if(renderIndex == %d) {\n"
                     "            offset = u_starfighterOffset%d;\n"
                     "            angle = u_rotationAngle%d;\n"
                     "        }",
                     i, i, i);
        } else {
            snprintf(case_code, sizeof(case_code),
                     " else if(renderIndex == %d) {\n"
                     "            offset = u_starfighterOffset%d;\n"
                     "            angle = u_rotationAngle%d;\n"
                     "        }",
                     i, i, i);
        }
        strcat(shader_buffer, case_code);
    }
    strcat(shader_buffer, "\n        \n");

    strcat(shader_buffer, "        screen_coord = vTexcoord + u_mapOffset - offset;\n"
                          "        centered = screen_coord - 0.5;\n"
                          "        cos_angle = cos(angle);\n"
                          "        sin_angle = sin(angle);\n"
                          "        rotated.x = centered.x * cos_angle - centered.y * sin_angle;\n"
                          "        rotated.y = centered.x * sin_angle + centered.y * cos_angle;\n"
                          "        \n");

    strcat(shader_buffer, "        // Sample appropriate texture\n");
    for (int i = 0; i < MAX_STARFIGHTERS; i++) {
        char texture_sample[256];
        if (i == 0) {
            snprintf(texture_sample, sizeof(texture_sample),
                     "        if(renderIndex == %d) {\n"
                     "            starfighterColor = texture2D(starfighterTexture%d, rotated + 0.5);\n"
                     "        }",
                     i, i);
        } else {
            snprintf(texture_sample, sizeof(texture_sample),
                     " else if(renderIndex == %d) {\n"
                     "            starfighterColor = texture2D(starfighterTexture%d, rotated + 0.5);\n"
                     "        }",
                     i, i);
        }
        strcat(shader_buffer, texture_sample);
    }
    strcat(shader_buffer, "\n        \n");

    strcat(shader_buffer, "        tempColor = mix(tempColor, starfighterColor.rgb, starfighterColor.a);\n"
                          "    }\n"
                          "    gl_FragColor = vec4(tempColor, alpha);\n"
                          "}\n");

    return shader_buffer;
}

static void get_continuous_normalized_position(vec2 out_pos, const vec3 world_pos) {
    float total_width = (float) terrain_width;
    float total_height = (float) terrain_height;
    out_pos[0] = (world_pos[0] + total_width / 2.0f) / total_width;
    out_pos[1] = (world_pos[2] + total_height / 2.0f) / total_height;
}

static void draw_starfighter_icon(float angle_rad, StarfighterColors *colors, unsigned char *targetPixelData) {
    const float NOSE_POINT_SIZE = 3.5f;
    const float TAIL_POINT_SIZE = 4.0f;

    memset(targetPixelData, 0, MINIMAP_TEXTURE_SIZE * MINIMAP_TEXTURE_SIZE * 4);

    int centerX = MINIMAP_TEXTURE_SIZE / 2;
    int centerY = MINIMAP_TEXTURE_SIZE / 2;
    float size = 15.0f;

    const float feather_width = 1.0f;
    float cos_a = cos(angle_rad);
    float sin_a = sin(angle_rad);
    const float fuselage_width = size * 0.2f;

    vec2 vertices[8] = {
            {0.0f, 1.0f * size}, // nose
            {0.0f, -1.0f * size}, // tail
            {-fuselage_width, 0.4f * size}, // cockpit_left
            {fuselage_width, 0.4f * size}, // cockpit_right
            {-fuselage_width, -0.8f * size}, // tail_left
            {fuselage_width, -0.8f * size}, // tail_right
            {-1.5f * size, -0.75f * size}, // left_wingtip
            {1.5f * size, -0.75f * size} // right_wingtip
    };

    int triangles[5][3] = {
            {0, 2, 3}, // fuselage_front: nose, cockpit_left, cockpit_right
            {2, 4, 5}, // fuselage_back1: cockpit_left, tail_left, tail_right
            {2, 5, 3}, // fuselage_back2: cockpit_left, tail_right, cockpit_right
            {2, 6, 4}, // left_wing: cockpit_left, left_wingtip, tail_left
            {3, 7, 5} // right_wing: cockpit_right, right_wingtip, tail_right
    };

    float min_x = vertices[0][0], max_x = vertices[0][0];
    float min_y = vertices[0][1], max_y = vertices[0][1];

    for (int i = 1; i < 8; i++) {
        float rx = vertices[i][0] * cos_a - vertices[i][1] * sin_a;
        float ry = vertices[i][0] * sin_a + vertices[i][1] * cos_a;
        min_x = fmin(min_x, rx);
        max_x = fmax(max_x, rx);
        min_y = fmin(min_y, ry);
        max_y = fmax(max_y, ry);
    }

    float max_point_size = fmax(NOSE_POINT_SIZE, TAIL_POINT_SIZE);
    int minX = (int) floorf(min_x - max_point_size - feather_width);
    int maxX = (int) ceilf(max_x + max_point_size + feather_width);
    int minY = (int) floorf(min_y - max_point_size - feather_width);
    int maxY = (int) ceilf(max_y + max_point_size + feather_width);

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            float local_x = x * cos_a + y * sin_a;
            float local_y = y * cos_a - x * sin_a;
            vec2 local_point = {local_x, local_y};

            bool is_in_fuselage = false;
            bool is_in_wings = false;

            for (int t = 0; t < 5; t++) {
                vec2 tri[3];
                for (int v = 0; v < 3; v++) {
                    glm_vec2_copy(vertices[triangles[t][v]], tri[v]);
                }

                if (is_inside_triangle(local_point, tri)) {
                    if (t < 3) {
                        is_in_fuselage = true;
                    } else {
                        is_in_wings = true;
                    }
                }
            }

            float dist_to_nose = glm_vec2_distance(local_point, vertices[0]);
            float dist_to_tail = glm_vec2_distance(local_point, vertices[1]);
            bool is_in_nose_circle = dist_to_nose < NOSE_POINT_SIZE;
            bool is_in_tail_circle = dist_to_tail < TAIL_POINT_SIZE;

            if (is_in_fuselage || is_in_wings || is_in_nose_circle || is_in_tail_circle) {
                float dist = 999.0f;

                for (int t = 0; t < 5; t++) {
                    if ((t < 3 && is_in_fuselage) || (t >= 3 && is_in_wings)) {
                        vec2 tri[3];
                        for (int v = 0; v < 3; v++) {
                            glm_vec2_copy(vertices[triangles[t][v]], tri[v]);
                        }
                        dist = fmin(dist, signed_dist_to_triangle(local_point, tri, NULL));
                    }
                }

                if (is_in_nose_circle)
                    dist = fmin(dist, NOSE_POINT_SIZE - dist_to_nose);
                if (is_in_tail_circle)
                    dist = fmin(dist, TAIL_POINT_SIZE - dist_to_tail);

                float alpha = glm_smoothstep(0.0f, feather_width, dist);
                if (alpha <= 0.0f)
                    continue;

                vec3 final_color;
                if (is_in_nose_circle) {
                    glm_vec3_copy(colors->fuselageColor, final_color);
                    float cockpit_glow = 1.0f - glm_smoothstep(0.0, NOSE_POINT_SIZE, dist_to_nose);
                    glm_vec3_lerp(final_color, colors->cockpitColor, cockpit_glow * cockpit_glow, final_color);
                } else if (is_in_tail_circle) {
                    glm_vec3_copy(colors->fuselageColor, final_color);
                    float thruster_glow = 1.0f - glm_smoothstep(0.0, TAIL_POINT_SIZE, dist_to_tail);
                    glm_vec3_lerp(final_color, colors->thrusterColor, thruster_glow, final_color);
                } else if (is_in_wings) {
                    glm_vec3_copy(colors->wingColor, final_color);
                } else {
                    glm_vec3_copy(colors->fuselageColor, final_color);
                    float gradient_t = (local_y + size) / (2.0f * size);
                    glm_vec3_lerp(final_color, (vec3) {0.4f, 0.45f, 0.5f}, 1.0f - gradient_t, final_color);
                }

                glm_vec3_lerp(colors->outlineColor, final_color,
                              glm_smoothstep(feather_width * 0.5f, feather_width, dist), final_color);

                set_pixel_alpha_target(centerX + x, centerY - y, final_color, (unsigned char) (alpha * 255.0f),
                                       targetPixelData);
            }
        }
    }
}

static void init_starfighter_texture(int index) {
    starfighterPixelData[index] =
            (unsigned char *) calloc(MINIMAP_TEXTURE_SIZE * MINIMAP_TEXTURE_SIZE * 4, sizeof(unsigned char));
    draw_starfighter_icon(0.0f, starfighterColors[index], starfighterPixelData[index]);

    glGenTextures(1, &starfighterTextureIDs[index]);
    glBindTexture(GL_TEXTURE_2D, starfighterTextureIDs[index]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MINIMAP_TEXTURE_SIZE, MINIMAP_TEXTURE_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 starfighterPixelData[index]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void update_starfighter_position(int index) {
    vec2 continuousPos;
    get_continuous_normalized_position(continuousPos, *starfighterPositions[index]);
    starfighterTranslationVectors[index][0] = continuousPos[0] - 0.5f;
    starfighterTranslationVectors[index][1] = continuousPos[1] - 0.5f;
    starfighterIconAngles[index] = atan2(-(*starfighterFronts[index])[0], -(*starfighterFronts[index])[2]);
}

static void cleanup_starfighter(int index) {
    if (starfighterTextureIDs[index]) {
        glDeleteTextures(1, &starfighterTextureIDs[index]);
        starfighterTextureIDs[index] = 0;
    }
    if (starfighterPixelData[index]) {
        free(starfighterPixelData[index]);
        starfighterPixelData[index] = NULL;
    }
}

void init_minimap() {
    if (minimapShaderProgram == 0) {
        char *fragmentShaderSource = generate_fragment_shader();
        minimapShaderProgram = createShaderProgram(minimapVertexSource, fragmentShaderSource);

        const float anchorX = 0.95f, anchorY = 0.95f;
        const float baseWidth = 0.25f, baseHeight = 0.30f;
        float scaledWidth = baseWidth * miniMapSize;
        float scaledHeight = baseHeight * miniMapSize;

        GLfloat minimap_vertices[] = {anchorX - scaledWidth,
                                      anchorY,
                                      0.0f,
                                      0.0f,
                                      anchorX - scaledWidth,
                                      anchorY - scaledHeight,
                                      0.0f,
                                      1.0f,
                                      anchorX,
                                      anchorY - scaledHeight,
                                      1.0f,
                                      1.0f,
                                      anchorX,
                                      anchorY,
                                      1.0f,
                                      0.0f};
        GLuint minimap_indices[] = {0, 1, 2, 0, 2, 3};

        glGenBuffers(1, &minimapVBO);
        glGenBuffers(1, &minimapEBO);
        glBindBuffer(GL_ARRAY_BUFFER, minimapVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(minimap_vertices), minimap_vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, minimapEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(minimap_indices), minimap_indices, GL_STATIC_DRAW);

        for (int i=0; i<MAX_STARFIGHTERS; i++) {
            starfighterColors[i] = &planes[i].colors;
            starfighterPositions[i] = &planes[i].position;
            starfighterFronts[i] = &planes[i].front;
            init_starfighter_texture(i);
        }
    }
}

void update_minimap_dot() {
    if (isCrashed)
        return;

    for (int i = 0; i < MAX_STARFIGHTERS; i++) {
        update_starfighter_position(i);
    }
}

void draw_minimap() {
    glUseProgram(minimapShaderProgram);

    glUniform2fv(glGetUniformLocation(minimapShaderProgram, "u_mapOffset"), 1, starfighterTranslationVectors[0]);

    for (int i = 0; i < MAX_STARFIGHTERS; i++) {
        char uniform_name[64];

        snprintf(uniform_name, sizeof(uniform_name), "u_starfighterOffset%d", i);
        glUniform2fv(glGetUniformLocation(minimapShaderProgram, uniform_name), 1, starfighterTranslationVectors[i]);

        snprintf(uniform_name, sizeof(uniform_name), "u_rotationAngle%d", i);
        glUniform1f(glGetUniformLocation(minimapShaderProgram, uniform_name), starfighterIconAngles[i]);

        float depth = (*starfighterPositions[i])[1];
        snprintf(uniform_name, sizeof(uniform_name), "u_starfighterDepth%d", i);
        glUniform1f(glGetUniformLocation(minimapShaderProgram, uniform_name), depth);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, heightMapTextureID);
    glUniform1i(glGetUniformLocation(minimapShaderProgram, "mapTexture"), 0);

    for (int i = 0; i < MAX_STARFIGHTERS; i++) {
        char uniform_name[64];
        glActiveTexture(GL_TEXTURE1 + i);
        glBindTexture(GL_TEXTURE_2D, starfighterTextureIDs[i]);
        snprintf(uniform_name, sizeof(uniform_name), "starfighterTexture%d", i);
        glUniform1i(glGetUniformLocation(minimapShaderProgram, uniform_name), 1 + i);
    }

    glUniform1f(glGetUniformLocation(minimapShaderProgram, "alpha"), transparencyOfMiniMap);

    glBindBuffer(GL_ARRAY_BUFFER, minimapVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, minimapEBO);

    GLint posAttrib = glGetAttribLocation(minimapShaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void *) 0);

    GLint texAttrib = glGetAttribLocation(minimapShaderProgram, "texcoord");
    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void *) (2 * sizeof(GLfloat)));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glDisableVertexAttribArray(posAttrib);
    glDisableVertexAttribArray(texAttrib);

    glActiveTexture(GL_TEXTURE0);
}

void cleanup_minimap() {
    glDeleteProgram(minimapShaderProgram);
    glDeleteBuffers(1, &minimapVBO);
    glDeleteBuffers(1, &minimapEBO);

    for (int i = 0; i < MAX_STARFIGHTERS; i++) {
        cleanup_starfighter(i);
    }
}

void reset_minimap_for_restart() {}

bool is_inside_triangle(vec2 p, vec2 tri[3]) {
    float d1 = (p[0] - tri[1][0]) * (tri[0][1] - tri[1][1]) - (tri[0][0] - tri[1][0]) * (p[1] - tri[1][1]);
    float d2 = (p[0] - tri[2][0]) * (tri[1][1] - tri[2][1]) - (tri[1][0] - tri[2][0]) * (p[1] - tri[2][1]);
    float d3 = (p[0] - tri[0][0]) * (tri[2][1] - tri[0][1]) - (tri[2][0] - tri[0][0]) * (p[1] - tri[0][1]);
    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

float signed_dist_to_edge(vec2 p, vec2 v0, vec2 v1) {
    vec2 pv = {p[0] - v0[0], p[1] - v0[1]};
    vec2 vv = {v1[0] - v0[0], v1[1] - v0[1]};
    float h = glm_clamp(glm_vec2_dot(pv, vv) / glm_vec2_dot(vv, vv), 0.0, 1.0);
    vec2 proj_point = {vv[0] * h, vv[1] * h};
    return glm_vec2_distance(pv, proj_point);
}

float signed_dist_to_triangle(vec2 p, vec2 tri1[3], vec2 tri2[3]) {
    float d = signed_dist_to_edge(p, tri1[0], tri1[1]);
    d = fmin(d, signed_dist_to_edge(p, tri1[1], tri1[2]));
    d = fmin(d, signed_dist_to_edge(p, tri1[2], tri1[0]));
    if (tri2 != NULL) {
        d = fmin(d, signed_dist_to_edge(p, tri2[0], tri2[1]));
        d = fmin(d, signed_dist_to_edge(p, tri2[1], tri2[2]));
        d = fmin(d, signed_dist_to_edge(p, tri2[2], tri2[0]));
    }
    return d;
}

static void set_pixel_alpha_target(int x, int y, vec3 color, unsigned char alpha, unsigned char *targetData) {
    if (x < 0 || x >= MINIMAP_TEXTURE_SIZE || y < 0 || y >= MINIMAP_TEXTURE_SIZE || !targetData)
        return;

    int index = (y * MINIMAP_TEXTURE_SIZE + x) * 4;
    vec3 background_color = {(float) targetData[index + 0] / 255.0f, (float) targetData[index + 1] / 255.0f,
                             (float) targetData[index + 2] / 255.0f};
    float bg_alpha = (float) targetData[index + 3] / 255.0f;

    float new_alpha_f = (float) alpha / 255.0f;
    float final_alpha = new_alpha_f + bg_alpha * (1.0f - new_alpha_f);
    if (final_alpha < 1e-5)
        return;

    vec3 blended_color;
    blended_color[0] = (color[0] * new_alpha_f + background_color[0] * bg_alpha * (1.0f - new_alpha_f)) / final_alpha;
    blended_color[1] = (color[1] * new_alpha_f + background_color[1] * bg_alpha * (1.0f - new_alpha_f)) / final_alpha;
    blended_color[2] = (color[2] * new_alpha_f + background_color[2] * bg_alpha * (1.0f - new_alpha_f)) / final_alpha;

    targetData[index + 0] = (unsigned char) (blended_color[0] * 255.0f);
    targetData[index + 1] = (unsigned char) (blended_color[1] * 255.0f);
    targetData[index + 2] = (unsigned char) (blended_color[2] * 255.0f);
    targetData[index + 3] = (unsigned char) (final_alpha * 255.0f);
}
