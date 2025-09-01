#include "skybox.h"

#include "skybox/headerFiles/front2.h"
#include "skybox/headerFiles/back2.h"
#include "skybox/headerFiles/up2.h"
#include "skybox/headerFiles/down2.h"
#include "skybox/headerFiles/right2.h"
#include "skybox/headerFiles/left2.h"

#define SKYBOX_WIDTH FRONT2_WIDTH
#define SKYBOX_HEIGHT FRONT2_HEIGHT

// --- Global State & Configuration Variables ---
static GLuint skyboxShaderProgram, skyboxVBO, skyboxCubemapTexture;

// --- Shader Sources ---
static const GLchar *skyboxVertexSource =
    "#version 100\n"
    "attribute vec3 position;\n"
    "varying vec3 vTexCoord;\n"
    "uniform mat4 view;\n"
    "uniform mat4 proj;\n"
    "void main() {\n"
    "    vTexCoord = position;\n"
    "    vec4 pos = proj * view * vec4(position, 1.0);\n"
    "    gl_Position = pos.xyww;\n"
    "}\n";

// FOGGY LOGIC -> FOG_HORIZON_START and FOG_HORIZON_END in the shader
static const GLchar *skyboxFragmentSource =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec3 vTexCoord;\n"
    "uniform samplerCube skybox;\n"
    "\n"
    "void main() {\n"
    "    // Define the fog color to match the terrain fog.\n"
    "    const vec3 fogColor = vec3(0.95, 0.96, 0.97);\n"
    "\n"
    "    // Define the vertical range for the fog effect.\n"
    "    //    -0.1 means the fog is fully solid slightly below the horizon.\n"
    "    //    0.70 means the fog will have completely faded out as you look up.\n"
    "    const float FOG_HORIZON_START = -0.01;\n"
    "    const float FOG_HORIZON_END = 0.70;\n"
    "\n"
    "    // Get the original color from the skybox texture.\n"
    "    vec4 skyColor = textureCube(skybox, vTexCoord);\n"
    "\n"
    "    // Calculate the fog blend factor based on the vertical look direction (vTexCoord.y).\n"
    "    //    We invert the result of smoothstep so the fog is strong at the bottom (y=-1) and weak at the top (y=1).\n"
    "    float fogFactor = 1.0 - smoothstep(FOG_HORIZON_START, FOG_HORIZON_END, vTexCoord.y);\n"
    "\n"
    "    // Mix the original sky color with the fog color using the calculated factor.\n"
    "    vec3 finalColor = mix(skyColor.rgb, fogColor, fogFactor);\n"
    "\n"
    "    gl_FragColor = vec4(finalColor, 1.0);\n"
    "}\n";

static GLuint loadCubemap(const unsigned char *face_data[6], int face_width, int face_height)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    for (unsigned int i = 0; i < 6; i++)
    {
        if (face_data[i])
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, face_width, face_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, face_data[i]);
        }
        else
        {
            fprintf(stderr, "Cubemap texture data is missing for face index: %d\n", i);
            glDeleteTextures(1, &textureID);
            return 0;
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return textureID;
}

bool init_skybox()
{
    skyboxShaderProgram = createShaderProgram(skyboxVertexSource, skyboxFragmentSource);
    if (skyboxShaderProgram == 0)
    {
        fprintf(stderr, "Failed to create skybox shader program.\n");
        return false;
    }

    const unsigned char *skybox_faces[6] = {
        front2, back2,
        up2, down2,
        right2, left2};

    skyboxCubemapTexture = loadCubemap(skybox_faces, SKYBOX_WIDTH, SKYBOX_HEIGHT);

    if (skyboxCubemapTexture == 0)
    {
        fprintf(stderr, "FATAL: Failed to load cubemap texture from files.\n");
        return false;
    }

    // The rest of the function (creating VBO, etc.) remains the same.
    GLfloat skyboxVertices[] = {
        // Positions
        // Right face (+X)
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,

        // Left face (-X)
        -1.0f, -1.0f, 1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,

        // Top face (+Y)
        -1.0f, 1.0f, -1.0f,
        1.0f, 1.0f, -1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, -1.0f,

        // Bottom face (-Y)
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, 1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, 1.0f,
        1.0f, -1.0f, 1.0f,

        // Back face (+Z)
        -1.0f, -1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, -1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,

        // Front face (-Z)
        -1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, 1.0f, -1.0f,
        1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f};

    glGenBuffers(1, &skyboxVBO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);

    return true;
}

void draw_skybox(mat4 view, mat4 proj)
{
    glDepthFunc(GL_LEQUAL);
    glUseProgram(skyboxShaderProgram);

    mat4 view_no_translation;
    glm_mat4_copy(view, view_no_translation);
    view_no_translation[3][0] = 0;
    view_no_translation[3][1] = 0;
    view_no_translation[3][2] = 0;

    glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "view"), 1, GL_FALSE, (GLfloat *)view_no_translation);
    glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "proj"), 1, GL_FALSE, (GLfloat *)proj);

    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    GLint posAttrib = glGetAttribLocation(skyboxShaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void *)0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemapTexture);
    glUniform1i(glGetUniformLocation(skyboxShaderProgram, "skybox"), 0);

    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDisableVertexAttribArray(posAttrib);
    glDepthFunc(GL_LESS);
}

void cleanup_skybox()
{
    glDeleteProgram(skyboxShaderProgram);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteTextures(1, &skyboxCubemapTexture);
}