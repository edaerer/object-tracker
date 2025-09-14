// =========================================
// ===       heightMap.c (Refactor)       ===
// =========================================
// • Clear sectioning and naming
// • Consistent comments and constants
// • Safer allocations and GL setup
// • Functionality preserved

#include "globals.h"
#include "heightMap.h"
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

/* ==========================
 *  Global Data (exposed)
 * ========================== */
// Raw heightmap data exposed (read-only) to other modules
const unsigned char* imageData = NULL; // owned by this module (g_heightData)

/* ==========================
 *  Module-Local State
 * ========================== */
static unsigned char* g_heightData = NULL; // owns imageData memory
static GLuint terrainShaderProgram = 0;
static GLuint rockTextureID        = 0;
static GLuint sharedChunkEBO       = 0; // shared index buffer for all chunks
static GLuint sharedChunkIndexCount = 0;

// Fog configuration
static const float FOG_START_DISTANCE = 1500.0f;
static const float FOG_END_DISTANCE   = 6250.0f;

/* ==========================
 *  Chunk Pool
 * ========================== */
typedef struct {
    GLuint vbo; // positions
    GLuint tbo; // texcoords
    bool   isActive;
    int    gridX, gridZ; // integer tile coordinates
} Chunk;

static Chunk chunks[TOTAL_CHUNKS];

/* ==========================
 *  Shaders
 * ========================== */
static const GLchar* kTerrainVS =
    "#version 100\n"
    "attribute vec3 position;\n"
    "attribute vec2 texcoord;\n"
    "varying vec3 vNormal;\n"
    "varying float vHeight;\n"
    "varying vec3 vWorldPos;\n"
    "varying vec2 vTexCoord;\n"
    "uniform mat4 model; uniform mat4 view; uniform mat4 proj;\n"
    "uniform sampler2D heightMap; uniform float heightScale;\n"
    "uniform float terrainWidth; uniform float terrainHeight;\n"
    "float getHeight(vec2 tex){ return texture2D(heightMap, tex).r; }\n"
    "void main(){\n"
    "  vTexCoord = texcoord;\n"
    "  float seaValue = 0.17;\n"
    "  float originalHeight = getHeight(texcoord);\n"
    "  vHeight = originalHeight;\n"
    "  float clampedHeight = max(originalHeight, seaValue);\n"
    "  vec3 pos = vec3(position.x, clampedHeight * heightScale, position.z);\n"
    "  vWorldPos = (model * vec4(pos, 1.0)).xyz;\n"
    "  float texelSizeX = 1.0 / terrainWidth;\n"
    "  float texelSizeY = 1.0 / terrainHeight;\n"
    "  float hL = max(getHeight(vec2(texcoord.x - texelSizeX, texcoord.y)), seaValue) * heightScale;\n"
    "  float hR = max(getHeight(vec2(texcoord.x + texelSizeX, texcoord.y)), seaValue) * heightScale;\n"
    "  float hD = max(getHeight(vec2(texcoord.x, texcoord.y - texelSizeY)), seaValue) * heightScale;\n"
    "  float hU = max(getHeight(vec2(texcoord.x, texcoord.y + texelSizeY)), seaValue) * heightScale;\n"
    "  vNormal = normalize(vec3(hL - hR, 2.0, hD - hU));\n"
    "  gl_Position = proj * view * vec4(vWorldPos, 1.0);\n"
    "}";

static const GLchar* kTerrainFS =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec3 vNormal;\n"
    "varying float vHeight;\n"
    "varying vec3 vWorldPos;\n"
    "varying vec2 vTexCoord;\n"
    "uniform vec3 u_lightDirection;\n"
    "uniform vec3 u_cameraPosition;\n"
    "uniform float u_fogStartDist;\n"
    "uniform float u_fogEndDist;\n"
    "uniform sampler2D u_rockTexture;\n"
    "void main(){\n"
    "  vec3 finalColor;\n"
    "  float seaLevel = 0.17;\n"
    "  if (vHeight <= seaLevel){\n"
    "    finalColor = vec3(0.1, 0.3, 0.8);\n"
    "  } else {\n"
    "    vec3 grassColor = vec3(0.15, 0.35, 0.1);\n"
    "    vec3 rockColor  = texture2D(u_rockTexture, vTexCoord * 125.0).rgb;\n"
    "    float slope     = max(0.0, dot(normalize(vNormal), vec3(0.0,1.0,0.0)));\n"
    "    float rockiness = 0.0;\n"
    "    rockiness += smoothstep(0.2, 0.6, vHeight);\n"
    "    rockiness += 1.0 - pow(slope, 2.0);\n"
    "    rockiness  = clamp(rockiness, 0.0, 1.0);\n"
    "    vec3 blendedColor = mix(grassColor, rockColor, rockiness);\n"
    "    vec3 lightDir = normalize(u_lightDirection);\n"
    "    vec3 normal   = normalize(vNormal);\n"
    "    float diffuse = max(dot(normal, lightDir), 0.3);\n"
    "    finalColor = blendedColor * diffuse;\n"
    "  }\n"
    "  vec3 fogColor = vec3(0.95, 0.96, 0.97);\n"
    "  float distanceToCamera = length(vWorldPos - u_cameraPosition);\n"
    "  float fogFactor = smoothstep(u_fogStartDist, u_fogEndDist, distanceToCamera);\n"
    "  finalColor = mix(finalColor, fogColor, fogFactor);\n"
    "  gl_FragColor = vec4(finalColor, 1.0);\n"
    "}";

/* ==========================
 *  Shader Setup
 * ========================== */
extern GLuint createShaderProgram(const char* vs, const char* fs); // provided elsewhere

static bool init_terrain_shader(void) {
    terrainShaderProgram = createShaderProgram(kTerrainVS, kTerrainFS);
    if (terrainShaderProgram == 0) return false;
    glEnable(GL_DEPTH_TEST);
    return true;
}

/* ==========================
 *  Heightmap Load & Textures
 * ========================== */
static bool load_terrain_data(void) {
    if (g_heightData) { stbi_image_free(g_heightData); g_heightData = NULL; }
    int comp = 0;
    stbi_set_flip_vertically_on_load(0);
    g_heightData = stbi_load("./images/heightMapImage.png", &terrain_width, &terrain_height, &comp, 1);
    imageData = g_heightData;

    if (!imageData) {
        fprintf(stderr, "ERROR: Failed to load heightmap image: ./images/heightMapImage.png\n");
        return false;
    }
    printf("Loaded heightmap: %dx%d from file.\n", terrain_width, terrain_height);
    return true;
}

void init_heightmap_texture(void) {
    if (!load_terrain_data()) return;

    // Heightmap texture (L8)
    glGenTextures(1, &heightMapTextureID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, heightMapTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, terrain_width, terrain_height, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, imageData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Rock texture (RGB/RGBA + mipmaps)
    int rw, rh, rn;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* rdata = stbi_load("./images/rock2.png", &rw, &rh, &rn, 0);
    if (!rdata) {
        fprintf(stderr, "ERROR: Failed to load rock texture: ./images/rock2.png\n");
    } else {
        glGenTextures(1, &rockTextureID);
        glBindTexture(GL_TEXTURE_2D, rockTextureID);
        const GLenum rfmt = (rn == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, rfmt, rw, rh, 0, rfmt, GL_UNSIGNED_BYTE, rdata);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        stbi_image_free(rdata);
    }
}

/* ==========================
 *  Chunk Mesh Builders
 * ========================== */
static void generate_chunk_vertex_data(Chunk* chunk) {
    const int vertexCount = CHUNK_VERTEX_DIM * CHUNK_VERTEX_DIM;
    GLfloat* vertices  = (GLfloat*)malloc((size_t)vertexCount * 3 * sizeof(GLfloat));
    GLfloat* texcoords = (GLfloat*)malloc((size_t)vertexCount * 2 * sizeof(GLfloat));
    if (!vertices || !texcoords) {
        fprintf(stderr, "ERROR: Failed to allocate memory for chunk mesh.\n");
        free(vertices); free(texcoords);
        return;
    }

    const float world_start_x = chunk->gridX * EDGE_SIZE_OF_EACH_CHUNK;
    const float world_start_z = chunk->gridZ * EDGE_SIZE_OF_EACH_CHUNK;
    const float terrain_total_width  = (float)terrain_width;
    const float terrain_total_height = (float)terrain_height;

    int i = 0;
    for (int z = 0; z < CHUNK_VERTEX_DIM; ++z) {
        for (int x = 0; x < CHUNK_VERTEX_DIM; ++x) {
            const float vx = world_start_x + ((float)x / (CHUNK_VERTEX_DIM - 1)) * EDGE_SIZE_OF_EACH_CHUNK;
            const float vz = world_start_z + ((float)z / (CHUNK_VERTEX_DIM - 1)) * EDGE_SIZE_OF_EACH_CHUNK;
            vertices[i*3 + 0] = vx;
            vertices[i*3 + 1] = 0.0f;
            vertices[i*3 + 2] = vz;
            texcoords[i*2 + 0] = (vx + terrain_total_width  / 2.0f) / terrain_total_width;
            texcoords[i*2 + 1] = (vz + terrain_total_height / 2.0f) / terrain_total_height;
            ++i;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, chunk->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)vertexCount * 3 * sizeof(GLfloat), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, chunk->tbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)vertexCount * 2 * sizeof(GLfloat), texcoords);

    free(vertices); free(texcoords);
}

static void build_shared_index_buffer(void) {
    const int numQuadsX = CHUNK_VERTEX_DIM - 1;
    const int numQuadsZ = CHUNK_VERTEX_DIM - 1;
    sharedChunkIndexCount = (GLuint)(numQuadsX * numQuadsZ * 6);

    GLuint* indices = (GLuint*)malloc((size_t)sharedChunkIndexCount * sizeof(GLuint));
    if (!indices) {
        fprintf(stderr, "ERROR: Failed to allocate memory for shared chunk indices.\n");
        return;
    }

    int i = 0;
    for (int z = 0; z < numQuadsZ; ++z) {
        for (int x = 0; x < numQuadsX; ++x) {
            const GLuint topLeft     = (GLuint)(z * CHUNK_VERTEX_DIM + x);
            const GLuint topRight    = topLeft + 1;
            const GLuint bottomLeft  = (GLuint)((z + 1) * CHUNK_VERTEX_DIM + x);
            const GLuint bottomRight = bottomLeft + 1;
            indices[i++] = topLeft;    indices[i++] = bottomLeft; indices[i++] = topRight;
            indices[i++] = topRight;   indices[i++] = bottomLeft; indices[i++] = bottomRight;
        }
    }

    glGenBuffers(1, &sharedChunkEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sharedChunkEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)sharedChunkIndexCount * sizeof(GLuint), indices, GL_STATIC_DRAW);
    free(indices);
}

void init_chunks(void) {
    if (!init_terrain_shader()) return;

    build_shared_index_buffer();

    // Initialize chunk pool GPU buffers
    const int vertexCount = CHUNK_VERTEX_DIM * CHUNK_VERTEX_DIM;
    for (int i = 0; i < TOTAL_CHUNKS; ++i) {
        chunks[i].isActive = false;
        chunks[i].gridX = chunks[i].gridZ = -9999;

        glGenBuffers(1, &chunks[i].vbo);
        glBindBuffer(GL_ARRAY_BUFFER, chunks[i].vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vertexCount * 3 * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &chunks[i].tbo);
        glBindBuffer(GL_ARRAY_BUFFER, chunks[i].tbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vertexCount * 2 * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
    }
}

void update_chunks(void) {
    if (isCrashed) return;

    const int camChunkX = (int)floorf(planes[0].position[0] / EDGE_SIZE_OF_EACH_CHUNK);
    const int camChunkZ = (int)floorf(planes[0].position[2] / EDGE_SIZE_OF_EACH_CHUNK);

    bool is_chunk_required[TOTAL_CHUNKS];
    for (int k = 0; k < TOTAL_CHUNKS; ++k) is_chunk_required[k] = false;

    for (int dz = 0; dz < CHUNK_GRID_DIM; ++dz) {
        for (int dx = 0; dx < CHUNK_GRID_DIM; ++dx) {
            const int required_gridX = camChunkX - CHUNK_GRID_DIM / 2 + dx;
            const int required_gridZ = camChunkZ - CHUNK_GRID_DIM / 2 + dz;

            bool found = false;
            for (int i = 0; i < TOTAL_CHUNKS; ++i) {
                if (chunks[i].isActive && chunks[i].gridX == required_gridX && chunks[i].gridZ == required_gridZ) {
                    is_chunk_required[i] = true; found = true; break;
                }
            }

            if (!found) {
                for (int i = 0; i < TOTAL_CHUNKS; ++i) {
                    if (!chunks[i].isActive) {
                        chunks[i].isActive = true;
                        chunks[i].gridX = required_gridX;
                        chunks[i].gridZ = required_gridZ;
                        generate_chunk_vertex_data(&chunks[i]);
                        is_chunk_required[i] = true;
                        break;
                    }
                }
            }
        }
    }

    for (int i = 0; i < TOTAL_CHUNKS; ++i) {
        if (!is_chunk_required[i]) chunks[i].isActive = false;
    }
}

void draw_chunks(mat4 view, mat4 proj) {
    glUseProgram(terrainShaderProgram);

    mat4 model; glm_mat4_identity(model);

    glUniformMatrix4fv(glGetUniformLocation(terrainShaderProgram, "proj"),  1, GL_FALSE, (GLfloat*)proj);
    glUniformMatrix4fv(glGetUniformLocation(terrainShaderProgram, "view"),  1, GL_FALSE, (GLfloat*)view);
    glUniformMatrix4fv(glGetUniformLocation(terrainShaderProgram, "model"), 1, GL_FALSE, (GLfloat*)model);

    glUniform1f(glGetUniformLocation(terrainShaderProgram, "heightScale"), HEIGHT_SCALE_FACTOR);
    glUniform1f(glGetUniformLocation(terrainShaderProgram, "terrainWidth"),  (float)terrain_width);
    glUniform1f(glGetUniformLocation(terrainShaderProgram, "terrainHeight"), (float)terrain_height);

    glUniform1f(glGetUniformLocation(terrainShaderProgram, "u_fogStartDist"), FOG_START_DISTANCE);
    glUniform1f(glGetUniformLocation(terrainShaderProgram, "u_fogEndDist"),   FOG_END_DISTANCE);

    glUniform3fv(glGetUniformLocation(terrainShaderProgram, "u_lightDirection"), 1, lightDirection);
    glUniform3fv(glGetUniformLocation(terrainShaderProgram, "u_cameraPosition"), 1, cameraPos);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, heightMapTextureID);
    glUniform1i(glGetUniformLocation(terrainShaderProgram, "heightMap"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, rockTextureID);
    glUniform1i(glGetUniformLocation(terrainShaderProgram, "u_rockTexture"), 1);

    const GLint posAttrib = glGetAttribLocation(terrainShaderProgram, "position");
    const GLint texAttrib = glGetAttribLocation(terrainShaderProgram, "texcoord");
    glEnableVertexAttribArray(posAttrib);
    glEnableVertexAttribArray(texAttrib);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sharedChunkEBO);

    for (int i = 0; i < TOTAL_CHUNKS; ++i) {
        if (!chunks[i].isActive) continue;

        glBindBuffer(GL_ARRAY_BUFFER, chunks[i].vbo);
        glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)0);

        glBindBuffer(GL_ARRAY_BUFFER, chunks[i].tbo);
        glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)0);

        if (isTriangleViewMode)
            glDrawElements(GL_LINES, (GLsizei)sharedChunkIndexCount, GL_UNSIGNED_INT, (const GLvoid*)0);
        else
            glDrawElements(GL_TRIANGLES, (GLsizei)sharedChunkIndexCount, GL_UNSIGNED_INT, (const GLvoid*)0);
    }

    glDisableVertexAttribArray(posAttrib);
    glDisableVertexAttribArray(texAttrib);
}

/* ==========================
 *  Sampling Helpers
 * ========================== */
float get_terrain_height(float x, float z) {
    if (!imageData) return 0.0f;

    const float u = (x + (float)terrain_width  / 2.0f) / (float)terrain_width;
    const float v = (z + (float)terrain_height / 2.0f) / (float)terrain_height;

    int u_int = (int)floorf(u); float u_frac = u - (float)u_int; if (abs(u_int) % 2 == 1) u_frac = 1.0f - u_frac;
    int v_int = (int)floorf(v); float v_frac = v - (float)v_int; if (abs(v_int) % 2 == 1) v_frac = 1.0f - v_frac;

    const float finalX = u_frac * (float)(terrain_width  - 1);
    const float finalZ = v_frac * (float)(terrain_height - 1);

    const int gridX = (int)floorf(finalX);
    const int gridZ = (int)floorf(finalZ);
    const float fracX = finalX - (float)gridX;
    const float fracZ = finalZ - (float)gridZ;

    const int x1 = (gridX + 1) % terrain_width;
    const int z1 = (gridZ + 1) % terrain_height;

    const float h00 = (float)imageData[gridZ * terrain_width + gridX] / 255.0f;
    const float h10 = (float)imageData[gridZ * terrain_width + x1]   / 255.0f;
    const float h01 = (float)imageData[z1   * terrain_width + gridX] / 255.0f;
    const float h11 = (float)imageData[z1   * terrain_width + x1]    / 255.0f;

    const float h_interp_z0 = (h00 * (1.0f - fracX)) + (h10 * fracX);
    const float h_interp_z1 = (h01 * (1.0f - fracX)) + (h11 * fracX);
    const float normalized_height = (h_interp_z0 * (1.0f - fracZ)) + (h_interp_z1 * fracZ);

    const float seaValueNormalized = 0.17f;
    return fmaxf(normalized_height, seaValueNormalized) * HEIGHT_SCALE_FACTOR;
}

/* ==========================
 *  Cleanup
 * ========================== */
void cleanup_heightmap(void) {
    if (terrainShaderProgram) { glDeleteProgram(terrainShaderProgram); terrainShaderProgram = 0; }

    for (int i = 0; i < TOTAL_CHUNKS; ++i) {
        if (chunks[i].vbo) { glDeleteBuffers(1, &chunks[i].vbo); chunks[i].vbo = 0; }
        if (chunks[i].tbo) { glDeleteBuffers(1, &chunks[i].tbo); chunks[i].tbo = 0; }
        chunks[i].isActive = false; chunks[i].gridX = chunks[i].gridZ = 0;
    }

    if (sharedChunkEBO) { glDeleteBuffers(1, &sharedChunkEBO); sharedChunkEBO = 0; }
    if (heightMapTextureID) { glDeleteTextures(1, &heightMapTextureID); heightMapTextureID = 0; }
    if (rockTextureID)      { glDeleteTextures(1, &rockTextureID);      rockTextureID      = 0; }

    if (g_heightData) { stbi_image_free(g_heightData); g_heightData = NULL; }
    imageData = NULL;
}
