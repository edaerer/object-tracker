#include "skybox.h"
#include "stb_image.h"

#include <stdio.h>
#include <stdbool.h>

// --- Global State & Configuration Variables ---
static GLuint skyboxShaderProgram = 0;
static GLuint skyboxVBO = 0;
static GLuint skyboxCubemapTexture = 0;

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
    "void main() {\n"
    "    const vec3 fogColor = vec3(0.95, 0.96, 0.97);\n"
    "    const float FOG_HORIZON_START = -0.01;\n"
    "    const float FOG_HORIZON_END   = 0.70;\n"
    "    vec4 skyColor = textureCube(skybox, vTexCoord);\n"
    "    float fogFactor = 1.0 - smoothstep(FOG_HORIZON_START, FOG_HORIZON_END, vTexCoord.y);\n"
    "    vec3 finalColor = mix(skyColor.rgb, fogColor, fogFactor);\n"
    "    gl_FragColor = vec4(finalColor, 1.0);\n"
    "}\n";

static GLuint loadCubemapFromFiles(const char* faces[6])
{
    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    stbi_set_flip_vertically_on_load(0);
    for (unsigned int i = 0; i < 6; i++)
    {
        int w, h, n;
        unsigned char* data = stbi_load(faces[i], &w, &h, &n, 0);
        if (!data) {
            fprintf(stderr, "Cubemap face missing: %s\n", faces[i]);
            glDeleteTextures(1, &textureID);
            return 0;
        }
        GLenum fmt = (n == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef GL_TEXTURE_WRAP_R
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
#endif
    return textureID;
}

bool init_skybox(void)
{
    skyboxShaderProgram = createShaderProgram(skyboxVertexSource, skyboxFragmentSource);
    if (skyboxShaderProgram == 0)
    {
        fprintf(stderr, "Failed to create skybox shader program.\n");
        return false;
    }

    const char* face_files[6] = {
        "./images/right2.jpg",
        "./images/left2.jpg",
        "./images/up2.jpg",
        "./images/down2.jpg",
        "./images/back2.jpg",
        "./images/front2.jpg"
    };
    skyboxCubemapTexture = loadCubemapFromFiles(face_files);
    if (skyboxCubemapTexture == 0)
    {
        fprintf(stderr, "FATAL: Failed to load cubemap texture from files.\n");
        return false;
    }

    // A unit cube (36 vertices)
    const GLfloat skyboxVertices[] = {
        // +X
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        // -X
       -1.0f, -1.0f,  1.0f,
       -1.0f, -1.0f, -1.0f,
       -1.0f,  1.0f, -1.0f,
       -1.0f,  1.0f, -1.0f,
       -1.0f,  1.0f,  1.0f,
       -1.0f, -1.0f,  1.0f,
        // +Y
       -1.0f,  1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
       -1.0f,  1.0f,  1.0f,
       -1.0f,  1.0f, -1.0f,
        // -Y
       -1.0f, -1.0f, -1.0f,
       -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
       -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f,  1.0f,
        // +Z
       -1.0f, -1.0f,  1.0f,
       -1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f, -1.0f,  1.0f,
       -1.0f, -1.0f,  1.0f,
        // -Z
       -1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
       -1.0f,  1.0f, -1.0f,
       -1.0f, -1.0f, -1.0f
    };

    glGenBuffers(1, &skyboxVBO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

    return true;
}

void draw_skybox(mat4 view, mat4 proj)
{
    glDepthFunc(GL_LEQUAL);
    glUseProgram(skyboxShaderProgram);

    mat4 view_no_translation;
    glm_mat4_copy(view, view_no_translation);
    // remove translation (column-major: col 3 is translation)
    view_no_translation[3][0] = 0.0f;
    view_no_translation[3][1] = 0.0f;
    view_no_translation[3][2] = 0.0f;

    glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "view"), 1, GL_FALSE, (GLfloat *)view_no_translation);
    glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "proj"), 1, GL_FALSE, (GLfloat *)proj);

    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    GLint posAttrib = glGetAttribLocation(skyboxShaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (const GLvoid*)0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemapTexture);
    glUniform1i(glGetUniformLocation(skyboxShaderProgram, "skybox"), 0);

    glDrawArrays(GL_TRIANGLES, 0, 36);

    glDisableVertexAttribArray(posAttrib);
    glDepthFunc(GL_LESS);
}

void cleanup_skybox(void)
{
    if (skyboxShaderProgram) glDeleteProgram(skyboxShaderProgram);
    if (skyboxVBO) glDeleteBuffers(1, &skyboxVBO);
    if (skyboxCubemapTexture) glDeleteTextures(1, &skyboxCubemapTexture);
}
