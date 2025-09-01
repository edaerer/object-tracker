#include "textShowing.h"

// --- Global State & Configuration Variables ---
static GLuint uiShaderProgram, markerShaderProgram;
static GLuint uiVbo, markerVBO;
static int textID[13];
static char textString[13][100];

// --- Shader Sources ---
static const GLchar *uiVertexSource = "#version 100\n attribute vec2 position; void main() { gl_Position = vec4(position.x, position.y, 0.0, 1.0); }";
static const GLchar *uiFragmentSource = "#version 100\n precision mediump float; uniform vec4 uiColor; void main() { gl_FragColor = uiColor; }";
static const GLchar *markerVertexSource = "#version 100\n attribute vec2 position; uniform mat4 model; uniform mat4 view; uniform mat4 proj; void main() { gl_Position = proj * view * model * vec4(position.x, 0.0, position.y, 1.0); }";
static const GLchar *markerFragmentSource = "#version 100\n precision mediump float; uniform vec4 markerColor; void main() { gl_FragColor = markerColor; }";

bool init_ui()
{
    uiShaderProgram = createShaderProgram(uiVertexSource, uiFragmentSource);
    if (uiShaderProgram == 0)
        return false;
    GLfloat vertices[] = {0.0f, 0.075f, -0.06f, -0.045f, 0.06f, -0.045f};
    glGenBuffers(1, &uiVbo);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    return true;
}

void init_osd()
{
    stbi_set_flip_vertically_on_load(false);
    init_text_rendering("../include/text/freemono.png", "../include/text/freemono.meta", SCR_WIDTH, SCR_HEIGHT);
    stbi_set_flip_vertically_on_load(true);
    float size = 25.0f;
    float r = 0.0f, g = 0.0f, b = 1.0f, a = 1.0f;
    textID[0] = add_text("", -0.98f, 0.95f, size, r, g, b, a);
    textID[1] = add_text("", -0.98f, 0.90f, size, r, g, b, a);
    textID[2] = add_text("", -0.98f, 0.85f, size, r, g, b, a);
    textID[3] = add_text("", -0.98f, 0.80f, size, r, g, b, a);
    textID[4] = add_text("", -0.98f, 0.75f, size, r, g, b, a);
    textID[5] = add_text("", -0.98f, 0.70f, size, r, g, b, a);
    textID[6] = add_text("", -0.98f, 0.65f, size, r, g, b, a);
    textID[7] = add_text("", -0.20f, 0.9f, 45.0f, 1.0f, 1.0f, 0.0f, 1.0f);
    textID[8] = add_text("", -0.98f, -0.75f, 40.0f, 0.6f, 0.7f, 0.2f, 1.0f);
    textID[9] = add_text("", -0.98f, -0.85f, 40.0f, 0.6f, 0.7f, 0.2f, 1.0f);
    textID[10] = add_text("", -0.98f, 0.55f, 60.0f, 0.2f, 0.4f, 0.6f, 1.0f);
    textID[11] = add_text("", -0.98f, 0.60f, 35.0f, 0.0f, 0.4f, 0.6f, 1.0f);
    textID[12] = add_text("", -0.98f, -0.50f, 55.0f, 0.0f, 1.0f, 0.6f, 1.0f);
}

void update_osd_texts(float currentMovementSpeed, bool isSpeedFixed, float altitude, float heightAboveGround, float roll, float pitch, float hSpeed, float vSpeed, double elapsedTime)
{
    if (isAutopilotOn)
        sprintf(textString[0], "SPEED: %.1f kph (AUTOPILOT MODE)", currentMovementSpeed);
    else
        sprintf(textString[0], "SPEED: %.1f kph (%s)", currentMovementSpeed, isSpeedFixed ? "FIXED" : "AUTO");
    sprintf(textString[1], "ALTITUDE: %.1f m", altitude);
    sprintf(textString[2], "TERRAIN CLEARANCE: %.1f m", heightAboveGround);
    sprintf(textString[3], "BANK ANGLE: %.1f deg", roll);
    sprintf(textString[4], "PITCH ANGLE: %.1f deg", pitch);
    sprintf(textString[5], "HORIZONTAL SPEED: %.1f m/s", hSpeed);
    sprintf(textString[6], "VERTICAL SPEED: %.1f m/s", vSpeed);
    //sprintf(textString[8], "Movement: Arrow Keys | A: Autopilot | Speed-Up: Shift | Zoom: Z | Fix Speed: S | Change View: T");
    //sprintf(textString[9], "Change Camera Position: 1 2 3 4 5 6 7 8");
    sprintf(textString[12], "TIME: %.1f s", elapsedTime);

    if (isTriangleViewMode)
        sprintf(textString[11], "VIEW MODE: GRID");
    else
        sprintf(textString[11], "VIEW MODE: FILLED TEXTURE");

    for (int i = 0; i < 13; ++i)
    {
        if (i != 7 || i != 8 || i != 9 || i != 10)
            update_text(textID[i], textString[i]);
    }
}

void update_status_text(const char *status, float r, float g, float b)
{
    change_text_colour(textID[7], r, g, b, 1.0f);
    update_text(textID[7], (char *)status);
}

void update_crash_text()
{
    sprintf(textString[7], "CRASHED! R:RESTART C:CONTINUE");
    change_text_colour(textID[7], 1.0f, 0.0f, 0.0f, 1.0f);
    update_text(textID[7], textString[7]);
}

void update_fps_text(float fps)
{
    sprintf(textString[10], "FPS: %.1f", fps);
    update_text(textID[10], textString[10]);
}

bool init_crash_marker()
{
    markerShaderProgram = createShaderProgram(markerVertexSource, markerFragmentSource);
    if (markerShaderProgram == 0)
        return false;
    GLfloat vertices[] = {-5.0f, -5.0f, 5.0f, 5.0f, -5.0f, 5.0f, 5.0f, -5.0f};
    glGenBuffers(1, &markerVBO);
    glBindBuffer(GL_ARRAY_BUFFER, markerVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    return true;
}

void draw_crash_marker(mat4 view, mat4 proj)
{
    glUseProgram(markerShaderProgram);
    mat4 model;
    glm_translate_make(model, crashPosition);
    glUniformMatrix4fv(glGetUniformLocation(markerShaderProgram, "proj"), 1, GL_FALSE, (GLfloat *)proj);
    glUniformMatrix4fv(glGetUniformLocation(markerShaderProgram, "view"), 1, GL_FALSE, (GLfloat *)view);
    glUniformMatrix4fv(glGetUniformLocation(markerShaderProgram, "model"), 1, GL_FALSE, (GLfloat *)model);
    vec4 markerColor = {1.0f, 0.0f, 0.0f, 1.0f};
    glUniform4fv(glGetUniformLocation(markerShaderProgram, "markerColor"), 1, markerColor);
    glBindBuffer(GL_ARRAY_BUFFER, markerVBO);
    GLint posAttrib = glGetAttribLocation(markerShaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glLineWidth(3.0f);
    glDrawArrays(GL_LINES, 0, 4);
    glDisableVertexAttribArray(posAttrib);
}

void cleanup_ui()
{
    glDeleteProgram(uiShaderProgram);
    glDeleteBuffers(1, &uiVbo);
}

void cleanup_osd()
{
    // Text library handles on it own Is array from decided in the beginning no dynamic
}

void cleanup_crash_marker()
{
    glDeleteProgram(markerShaderProgram);
    glDeleteBuffers(1, &markerVBO);
}