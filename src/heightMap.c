// =========================================
// ===         heightMap.c File          ===
// =========================================

#include "globals.h"
#include "heightMap.h"
#include "heightMapImage.h"
#define HEIGHTMAP_HEIGHT HEIGHTMAPIMAGE_HEIGHT
#define HEIGHTMAP_WIDTH HEIGHTMAPIMAGE_WIDTH
const unsigned char *imageData;

#include "rock2.h"
#define ROCK_HEIGHT RCK_3_HEIGHT
#define ROCK_WIDTH RCK_3_WIDTH

// --- Global State & Configuration Variables ---
static GLuint terrainShaderProgram;
static GLuint rockTextureID;

// MODIFICATION: Add a shared EBO for all chunks
static GLuint sharedChunkEBO;
static GLuint sharedChunkIndexCount;

// 4000.0f chosen because 1 chunk is 5120x5120 and the chunk grid is 3 chunks by 3 chunks
// so 15360x15360 accepting the we are in the middle of the chunk fog end distance can be maximum 7680 for
// hiding the texture creation
// Defines the distance from the camera where fog begins to appear.
static const float FOG_START_DISTANCE = 1500.0f;
// Defines the distance at which the fog reaches 100% density.
static const float FOG_END_DISTANCE = 6250.0f;

// isTriangleViewMode -> this is actually grid view mode

typedef struct
{
    GLuint vbo, tbo; // EBO is now shared
    bool isActive;
    int gridX, gridZ;
} Chunk;

static Chunk chunks[TOTAL_CHUNKS];

// fogColor = (0.95, 0.96, 0.97)

// --- Shader Sources ---

static const GLchar *terrainVertexSource =
    "#version 100\n"
    "attribute vec3 position; attribute vec2 texcoord;"
    "varying vec3 vNormal;\n"
    "varying float vHeight;\n"
    "varying vec3 vWorldPos; // We MUST pass the world position for distance fog\n"
    "varying vec2 vTexCoord; // MODIFICATION: Added to pass UVs to fragment shader\n"

    "uniform mat4 model; uniform mat4 view; uniform mat4 proj;\n"
    "uniform sampler2D heightMap; uniform float heightScale;\n"
    "uniform float terrainWidth; uniform float terrainHeight;\n"

    "float getHeight(vec2 tex) { return texture2D(heightMap, tex).r; }\n"

    "void main() {\n"
    "   vTexCoord = texcoord; // MODIFICATION: Pass texcoord to fragment shader\n"
    "   float seaValue = 0.17;\n"
    "   // We pass the ORIGINAL height (before clamping) to the fragment shader for accurate coloring of the water.\n"
    "   float originalHeight = getHeight(texcoord);\n"
    "   vHeight = originalHeight;\n"

    "   // But we clamp the GEOMETRY's height to create a flat sea floor.\n"
    "   float clampedHeight = max(originalHeight, seaValue);\n"
    "   vec3 pos = vec3(position.x, clampedHeight * heightScale, position.z);\n"
    "   vWorldPos = (model * vec4(pos, 1.0)).xyz;\n"

    "   // Normal calculation also needs to use clamped heights to be physically accurate for lighting.\n"
    "   float texelSizeX = 1.0 / terrainWidth;\n"
    "   float texelSizeY = 1.0 / terrainHeight;\n"
    "   float hL = max(getHeight(vec2(texcoord.x - texelSizeX, texcoord.y)), seaValue) * heightScale;\n"
    "   float hR = max(getHeight(vec2(texcoord.x + texelSizeX, texcoord.y)), seaValue) * heightScale;\n"
    "   float hD = max(getHeight(vec2(texcoord.x, texcoord.y - texelSizeY)), seaValue) * heightScale;\n"
    "   float hU = max(getHeight(vec2(texcoord.x, texcoord.y + texelSizeY)), seaValue) * heightScale;\n"
    "   vNormal = normalize(vec3(hL - hR, 2.0, hD - hU));\n"

    "   gl_Position = proj * view * vec4(vWorldPos, 1.0);\n"
    "}";

static const GLchar *terrainFragmentSource =
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
    "    vec3 finalColor;\n"
    "    float seaLevel = 0.17;\n"

    "    if (vHeight <= seaLevel) {\n"
    "        finalColor = vec3(0.1, 0.3, 0.8); // Deep blue water\n"
    "    } else {\n"
    "        // 1. --- DEFINE BASE COLORS AND TEXTURES --- \n"
    "        vec3 grassColor = vec3(0.15, 0.35, 0.1); // Lush green for lowlands\n"
    "        // Sample the rock texture. The 125.0 tiles it across the terrain.\n"
    "        vec3 rockColor = texture2D(u_rockTexture, vTexCoord * 125.0).rgb;\n"

    "        // 2. --- CALCULATE BLEND FACTOR --- \n"
    "        // Calculate slope: 1.0 for flat ground, 0.0 for vertical cliffs.\n"
    "        float slope = max(0.0, dot(normalize(vNormal), vec3(0.0, 1.0, 0.0)));\n"

    "        // Calculate a weight for how 'rocky' a surface should be.\n"
    "        float rockiness = 0.0;\n"
    "        // Factor 1: Altitude. Start turning to rock at height 0.2, fully rock at 0.6\n"
    "        rockiness += smoothstep(0.2, 0.6, vHeight);\n"
    "        // Factor 2: Slope. Start turning to rock on gentle slopes, fully rock on cliffs.\n"
    "        rockiness += 1.0 - pow(slope, 2.0);\n"
    "        // Clamp the final factor between 0.0 and 1.0\n"
    "        rockiness = clamp(rockiness, 0.0, 1.0);\n"

    "        // 3. --- BLEND BETWEEN GRASS AND ROCK --- \n"
    "        vec3 blendedColor = mix(grassColor, rockColor, rockiness);\n"

    "        // 4. --- APPLY LIGHTING --- \n"
    "        vec3 lightDir = normalize(u_lightDirection);\n"
    "        vec3 normal = normalize(vNormal);\n"
    "        float diffuse = max(dot(normal, lightDir), 0.3);\n"
    "        finalColor = blendedColor * diffuse;\n"
    "    }\n"

    "    // 5. --- FOG (Unchanged) --- \n"
    "    vec3 fogColor = vec3(0.95, 0.96, 0.97);\n"
    "    float distanceToCamera = length(vWorldPos - u_cameraPosition);\n"
    "    float fogFactor = smoothstep(u_fogStartDist, u_fogEndDist, distanceToCamera);\n"
    "    finalColor = mix(finalColor, fogColor, fogFactor);\n"

    "    gl_FragColor = vec4(finalColor, 1.0);\n"
    "}";

bool init_terrain_shader()
{
    terrainShaderProgram = createShaderProgram(terrainVertexSource, terrainFragmentSource);
    if (terrainShaderProgram == 0)
        return false;
    glEnable(GL_DEPTH_TEST);
    return true;
}

bool load_terrain_data()
{
    imageData = heightMapImage;
    terrain_height = HEIGHTMAP_HEIGHT;
    terrain_width = HEIGHTMAP_WIDTH;

    if (!imageData)
    {
        fprintf(stderr, "ERROR: Failed to load heightmap image.\n");
        return false;
    }
    printf("Loaded heightmap: %dx%d\n", terrain_width, terrain_height);
    return true;
}

void init_heightmap_texture()
{
    load_terrain_data();
    glGenTextures(1, &heightMapTextureID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, heightMapTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, terrain_width, terrain_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, imageData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &rockTextureID);
    glBindTexture(GL_TEXTURE_2D, rockTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ROCK_WIDTH, ROCK_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, rck_3);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

// MODIFICATION: Simplified to only generate vertex data
void generate_chunk_vertex_data(Chunk *chunk)
{
    int vertexCount = CHUNK_VERTEX_DIM * CHUNK_VERTEX_DIM;
    GLfloat *vertices = (GLfloat *)malloc(vertexCount * 3 * sizeof(GLfloat));
    GLfloat *texcoords = (GLfloat *)malloc(vertexCount * 2 * sizeof(GLfloat));
    if (!vertices || !texcoords)
    {
        fprintf(stderr, "ERROR: Failed to allocate memory for chunk mesh.\n");
        free(vertices);
        free(texcoords);
        return;
    }

    float world_start_x = chunk->gridX * EDGE_SIZE_OF_EACH_CHUNK;
    float world_start_z = chunk->gridZ * EDGE_SIZE_OF_EACH_CHUNK;
    float terrain_total_width = (float)terrain_width;
    float terrain_total_height = (float)terrain_height;

    int i = 0;
    for (int z = 0; z < CHUNK_VERTEX_DIM; z++)
    {
        for (int x = 0; x < CHUNK_VERTEX_DIM; x++)
        {
            float vx = world_start_x + ((float)x / (CHUNK_VERTEX_DIM - 1)) * EDGE_SIZE_OF_EACH_CHUNK;
            float vz = world_start_z + ((float)z / (CHUNK_VERTEX_DIM - 1)) * EDGE_SIZE_OF_EACH_CHUNK;
            vertices[i * 3 + 0] = vx;
            vertices[i * 3 + 1] = 0.0f;
            vertices[i * 3 + 2] = vz;
            texcoords[i * 2 + 0] = (vx + terrain_total_width / 2.0f) / terrain_total_width;
            texcoords[i * 2 + 1] = (vz + terrain_total_height / 2.0f) / terrain_total_height;
            i++;
        }
    }

    // Use glBufferSubData to update the existing buffers
    glBindBuffer(GL_ARRAY_BUFFER, chunk->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * 3 * sizeof(GLfloat), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, chunk->tbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * 2 * sizeof(GLfloat), texcoords);

    free(vertices);
    free(texcoords);
}

void init_chunks()
{
    init_terrain_shader();

    // MODIFICATION: Create a single, shared index buffer (EBO)
    int numQuadsX = CHUNK_VERTEX_DIM - 1;
    int numQuadsZ = CHUNK_VERTEX_DIM - 1;
    sharedChunkIndexCount = numQuadsX * numQuadsZ * 6;
    GLuint *indices = (GLuint *)malloc(sharedChunkIndexCount * sizeof(GLuint));
    if (!indices)
    {
        fprintf(stderr, "ERROR: Failed to allocate memory for shared chunk indices.\n");
        return;
    }
    int i = 0;
    for (int z = 0; z < numQuadsZ; z++)
    {
        for (int x = 0; x < numQuadsX; x++)
        {
            GLuint topLeft = (z * CHUNK_VERTEX_DIM) + x;
            GLuint topRight = topLeft + 1;
            GLuint bottomLeft = ((z + 1) * CHUNK_VERTEX_DIM) + x;
            GLuint bottomRight = bottomLeft + 1;
            indices[i++] = topLeft;
            indices[i++] = bottomLeft;
            indices[i++] = topRight;
            indices[i++] = topRight;
            indices[i++] = bottomLeft;
            indices[i++] = bottomRight;
        }
    }
    glGenBuffers(1, &sharedChunkEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sharedChunkEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sharedChunkIndexCount * sizeof(GLuint), indices, GL_STATIC_DRAW);
    free(indices);

    // --- Initialize the chunk pool ---
    int vertexCount = CHUNK_VERTEX_DIM * CHUNK_VERTEX_DIM;
    for (i = 0; i < TOTAL_CHUNKS; ++i)
    {
        chunks[i].isActive = false;
        chunks[i].gridX = -9999;
        chunks[i].gridZ = -9999;

        glGenBuffers(1, &chunks[i].vbo);
        glBindBuffer(GL_ARRAY_BUFFER, chunks[i].vbo);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &chunks[i].tbo);
        glBindBuffer(GL_ARRAY_BUFFER, chunks[i].tbo);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 2 * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
    }
}

void update_chunks()
{
    if (isCrashed)
        return;

    int camChunkX = floorf(planePos[0] / EDGE_SIZE_OF_EACH_CHUNK);
    int camChunkZ = floorf(planePos[2] / EDGE_SIZE_OF_EACH_CHUNK);

    bool is_chunk_required[TOTAL_CHUNKS] = {false};
    for (int dz = 0; dz < CHUNK_GRID_DIM; dz++)
    {
        for (int dx = 0; dx < CHUNK_GRID_DIM; dx++)
        {
            int required_gridX = camChunkX - CHUNK_GRID_DIM / 2 + dx;
            int required_gridZ = camChunkZ - CHUNK_GRID_DIM / 2 + dz;

            bool found = false;
            for (int i = 0; i < TOTAL_CHUNKS; i++)
            {
                if (chunks[i].isActive && chunks[i].gridX == required_gridX && chunks[i].gridZ == required_gridZ)
                {
                    is_chunk_required[i] = true;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                for (int i = 0; i < TOTAL_CHUNKS; i++)
                {
                    if (!chunks[i].isActive)
                    {
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

    for (int i = 0; i < TOTAL_CHUNKS; i++)
    {
        if (!is_chunk_required[i])
        {
            chunks[i].isActive = false;
        }
    }
}

void draw_chunks(mat4 view, mat4 proj)
{
    glUseProgram(terrainShaderProgram);

    mat4 model;
    glm_mat4_identity(model);

    glUniformMatrix4fv(glGetUniformLocation(terrainShaderProgram, "proj"), 1, GL_FALSE, (GLfloat *)proj);
    glUniformMatrix4fv(glGetUniformLocation(terrainShaderProgram, "view"), 1, GL_FALSE, (GLfloat *)view);
    glUniformMatrix4fv(glGetUniformLocation(terrainShaderProgram, "model"), 1, GL_FALSE, (GLfloat *)model);
    glUniform1f(glGetUniformLocation(terrainShaderProgram, "heightScale"), HEIGHT_SCALE_FACTOR);
    glUniform3fv(glGetUniformLocation(terrainShaderProgram, "u_lightDirection"), 1, lightDirection);
    glUniform1f(glGetUniformLocation(terrainShaderProgram, "terrainWidth"), (float)terrain_width);
    glUniform1f(glGetUniformLocation(terrainShaderProgram, "terrainHeight"), (float)terrain_height);
    glUniform1f(glGetUniformLocation(terrainShaderProgram, "u_fogStartDist"), FOG_START_DISTANCE);
    glUniform1f(glGetUniformLocation(terrainShaderProgram, "u_fogEndDist"), FOG_END_DISTANCE);
    glUniform3fv(glGetUniformLocation(terrainShaderProgram, "u_cameraPosition"), 1, cameraPos);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, heightMapTextureID);
    glUniform1i(glGetUniformLocation(terrainShaderProgram, "heightMap"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, rockTextureID);
    glUniform1i(glGetUniformLocation(terrainShaderProgram, "u_rockTexture"), 1);

    GLint posAttrib = glGetAttribLocation(terrainShaderProgram, "position");
    GLint texAttrib = glGetAttribLocation(terrainShaderProgram, "texcoord");
    glEnableVertexAttribArray(posAttrib);
    glEnableVertexAttribArray(texAttrib);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sharedChunkEBO);

    for (int i = 0; i < TOTAL_CHUNKS; ++i)
    {
        if (!chunks[i].isActive)
            continue;

        glBindBuffer(GL_ARRAY_BUFFER, chunks[i].vbo);
        glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, chunks[i].tbo);
        glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

        if (isTriangleViewMode)
        {
            glDrawElements(GL_LINES, sharedChunkIndexCount, GL_UNSIGNED_INT, 0);
        }
        else
        {
            glDrawElements(GL_TRIANGLES, sharedChunkIndexCount, GL_UNSIGNED_INT, 0);
        }
    }

    glDisableVertexAttribArray(posAttrib);
    glDisableVertexAttribArray(texAttrib);
}

float get_terrain_height(float x, float z)
{
    if (!imageData)
        return 0.0f;

    float u = (x + (float)terrain_width / 2.0f) / (float)terrain_width;
    float v = (z + (float)terrain_height / 2.0f) / (float)terrain_height;

    int u_int = floorf(u);
    float u_frac = u - u_int;
    if (abs(u_int) % 2 == 1)
    {
        u_frac = 1.0f - u_frac;
    }
    int v_int = floorf(v);
    float v_frac = v - v_int;
    if (abs(v_int) % 2 == 1)
    {
        v_frac = 1.0f - v_frac;
    }

    float finalX = u_frac * (terrain_width - 1);
    float finalZ = v_frac * (terrain_height - 1);

    int gridX = (int)floorf(finalX);
    int gridZ = (int)floorf(finalZ);
    float fracX = finalX - gridX;
    float fracZ = finalZ - gridZ;

    int x1 = (gridX + 1) % terrain_width;
    int z1 = (gridZ + 1) % terrain_height;

    float h00 = (float)imageData[gridZ * terrain_width + gridX] / 255.0f;
    float h10 = (float)imageData[gridZ * terrain_width + x1] / 255.0f;
    float h01 = (float)imageData[z1 * terrain_width + gridX] / 255.0f;
    float h11 = (float)imageData[z1 * terrain_width + x1] / 255.0f;

    float h_interp_z0 = (h00 * (1.0f - fracX)) + (h10 * fracX);
    float h_interp_z1 = (h01 * (1.0f - fracX)) + (h11 * fracX);
    float normalized_height = (h_interp_z0 * (1.0f - fracZ)) + (h_interp_z1 * fracZ);

    float seaValueNormalized = 0.17;
    return fmax(normalized_height, seaValueNormalized) * HEIGHT_SCALE_FACTOR;
}

void cleanup_heightmap()
{
    glDeleteProgram(terrainShaderProgram);
    for (int i = 0; i < TOTAL_CHUNKS; ++i)
    {
        glDeleteBuffers(1, &chunks[i].vbo);
        glDeleteBuffers(1, &chunks[i].tbo);
    }
    glDeleteBuffers(1, &sharedChunkEBO);
    glDeleteTextures(1, &heightMapTextureID);
    glDeleteTextures(1, &rockTextureID);

    imageData = NULL;
}