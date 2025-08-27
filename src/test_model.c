#ifdef TEST_MODEL

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <darknet/darknet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <tracker/image_reader.h>

typedef struct {
    GLFWwindow *window;
    GLuint prog;
    GLuint vao, vbo;
    GLuint tex;
    int tex_w, tex_h;
} GL33Ctx;

static GL33Ctx g = {0};

static void glfw_err(int code, const char *desc) { fprintf(stderr, "GLFW %d: %s\n", code, desc); }

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "shader compile error:\n%s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "program link error:\n%s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    glDetachShader(p, vs);
    glDetachShader(p, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

static int ensure_texture(GLint w, GLint h) {
    if (g.tex && g.tex_w == w && g.tex_h == h)
        return 1;
    if (!g.tex)
        glGenTextures(1, &g.tex);
    glBindTexture(GL_TEXTURE_2D, g.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    g.tex_w = w;
    g.tex_h = h;
    return 1;
}

int init_gl33_window(const char *title, int win_w, int win_h) {
    glfwSetErrorCallback(glfw_err);
    if (!glfwInit()) {
        fprintf(stderr, "glfwInit failed\n");
        return 0;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g.window = glfwCreateWindow(win_w, win_h, title, NULL, NULL);
    if (!g.window) {
        fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 0;
    }
    glfwMakeContextCurrent(g.window);
    glfwSwapInterval(1);

    gladLoadGL();

    // --- Shaders (v flip uygulanmış) ---
    const char *vsrc = "#version 330 core\n"
                       "layout (location=0) in vec2 in_pos;\n"
                       "layout (location=1) in vec2 in_uv;\n"
                       "out vec2 v_uv;\n"
                       "void main(){\n"
                       "  v_uv = vec2(in_uv.x, 1.0 - in_uv.y);\n" // y eksenini çevir
                       "  gl_Position = vec4(in_pos.x, -in_pos.y, 0.0, 1.0);\n"
                       "}\n";

    const char *fsrc = "#version 330 core\n"
                       "in vec2 v_uv;\n"
                       "out vec4 frag;\n"
                       "uniform sampler2D u_tex;\n"
                       "void main(){ frag = texture(u_tex, v_uv); }\n";

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vsrc);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fsrc);
    if (!vs || !fs)
        return 0;
    g.prog = link_program(vs, fs);
    if (!g.prog)
        return 0;

    // Fullscreen quad (triangle strip):  pos.xy, uv.xy
    float verts[] = {
            -1.f, -1.f, 0.f, 1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f, 0.f, 0.f, 1.f, 1.f, 1.f, 0.f,
    };
    glGenVertexArrays(1, &g.vao);
    glGenBuffers(1, &g.vbo);
    glBindVertexArray(g.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) (0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) (2 * sizeof(float)));
    glBindVertexArray(0);

    // İlk texture kabı (pencere boyutu kadar başlatabilir, sonra frame boyutuyla yeniden tanımlarız)
    ensure_texture(win_w, win_h);

    glUseProgram(g.prog);
    glUniform1i(glGetUniformLocation(g.prog, "u_tex"), 0);
    glUseProgram(0);

    return 1;
}

void shutdown_gl33(void) {
    if (g.vbo)
        glDeleteBuffers(1, &g.vbo);
    if (g.vao)
        glDeleteVertexArrays(1, &g.vao);
    if (g.prog)
        glDeleteProgram(g.prog);
    if (g.tex)
        glDeleteTextures(1, &g.tex);
    if (g.window) {
        glfwDestroyWindow(g.window);
        g.window = NULL;
    }
    glfwTerminate();
    memset(&g, 0, sizeof(g));
}

static unsigned char *image_to_rgb8(const image im) {
    int wh = im.w * im.h;
    unsigned char *out = (unsigned char *) malloc(wh * 3);
    if (!out)
        return NULL;
    for (int i = 0; i < wh; ++i) {
        float r = im.data[0 * wh + i];
        float g = im.data[1 * wh + i];
        float b = im.data[2 * wh + i];
        if (r < 0)
            r = 0;
        if (r > 1)
            r = 1;
        if (g < 0)
            g = 0;
        if (g > 1)
            g = 1;
        if (b < 0)
            b = 0;
        if (b > 1)
            b = 1;
        out[3 * i + 0] = (unsigned char) (r * 255.0f + 0.5f);
        out[3 * i + 1] = (unsigned char) (g * 255.0f + 0.5f);
        out[3 * i + 2] = (unsigned char) (b * 255.0f + 0.5f);
    }
    return out;
}

// Her kare çağır: RGB8 veriyi yükle ve çiz
void draw_rgb8_to_window_33(const unsigned char *rgb, int w, int h) {
    if (!g.window)
        return;
    if (glfwWindowShouldClose(g.window))
        return;

    glActiveTexture(GL_TEXTURE0);
    ensure_texture(w, h);
    glBindTexture(GL_TEXTURE_2D, g.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, rgb);

    int fbw, fbh;
    glfwGetFramebufferSize(g.window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g.prog);
    glBindVertexArray(g.vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glUseProgram(0);

    glfwSwapBuffers(g.window);
    glfwPollEvents();
}

int window_should_close_33(void) { return g.window && glfwWindowShouldClose(g.window); }


long long timeInMilliseconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (((long long) tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}


// En yüksek tespitleri çiz
void draw_top_detections(image im, void *sorted_dets_ptr, int count) {
    typedef struct {
        int index;
        int class_id;
        float confidence;
        box bbox;
    } sorted_detection;

    sorted_detection *sorted_dets = sorted_dets_ptr;

    printf("Çizim başlıyor: %d tespit, resim %dx%dx%d\n", count, im.w, im.h, im.c);

    if (!im.data || im.w <= 0 || im.h <= 0 || im.c != 3) {
        printf("Geçersiz resim verisi!\n");
        return;
    }

    // Farklı renkler (RGB)
    float colors[10][3] = {
            {1.0, 0.0, 0.0}, // Kırmızı
            {0.0, 1.0, 0.0}, // Yeşil
            {0.0, 0.0, 1.0}, // Mavi
            {1.0, 1.0, 0.0}, // Sarı
            {1.0, 0.0, 1.0}, // Magenta
            {0.0, 1.0, 1.0}, // Cyan
            {1.0, 0.5, 0.0}, // Turuncu
            {0.5, 0.0, 1.0}, // Mor
            {0.0, 0.5, 0.0}, // Koyu yeşil
            {0.5, 0.5, 0.5} // Gri
    };

    for (int i = 0; i < count; i++) {
        box b = sorted_dets[i].bbox;

        // Bounding box koordinatları
        int left = (int) ((b.x - b.w / 2.) * im.w);
        int right = (int) ((b.x + b.w / 2.) * im.w);
        int top = (int) ((b.y - b.h / 2.) * im.h);
        int bot = (int) ((b.y + b.h / 2.) * im.h);

        // Güvenli sınır kontrolü
        if (left < 0)
            left = 0;
        if (right >= im.w)
            right = im.w - 1;
        if (top < 0)
            top = 0;
        if (bot >= im.h)
            bot = im.h - 1;

        if (left >= right || top >= bot)
            continue;

        printf("Kutu %d çiziliyor: Sınıf=%d, Güven=%.3f%%, Box=(%d,%d,%d,%d)\n", i + 1, sorted_dets[i].class_id,
               sorted_dets[i].confidence * 100, left, top, right, bot);

        // Renk seç
        float *color = colors[i % 10];
        int thickness = 3;

        // Üst çizgi
        for (int x = left; x <= right; x++) {
            for (int t = 0; t < thickness; t++) {
                int y = top + t;
                if (x >= 0 && x < im.w && y >= 0 && y < im.h) {
                    int idx = y * im.w + x;
                    if (idx >= 0 && idx < im.w * im.h) {
                        im.data[0 * im.h * im.w + idx] = color[0]; // R
                        im.data[1 * im.h * im.w + idx] = color[1]; // G
                        im.data[2 * im.h * im.w + idx] = color[2]; // B
                    }
                }
            }
        }

        // Alt çizgi
        for (int x = left; x <= right; x++) {
            for (int t = 0; t < thickness; t++) {
                int y = bot - t;
                if (x >= 0 && x < im.w && y >= 0 && y < im.h) {
                    int idx = y * im.w + x;
                    if (idx >= 0 && idx < im.w * im.h) {
                        im.data[0 * im.h * im.w + idx] = color[0];
                        im.data[1 * im.h * im.w + idx] = color[1];
                        im.data[2 * im.h * im.w + idx] = color[2];
                    }
                }
            }
        }

        // Sol çizgi
        for (int y = top; y <= bot; y++) {
            for (int t = 0; t < thickness; t++) {
                int x = left + t;
                if (x >= 0 && x < im.w && y >= 0 && y < im.h) {
                    int idx = y * im.w + x;
                    if (idx >= 0 && idx < im.w * im.h) {
                        im.data[0 * im.h * im.w + idx] = color[0];
                        im.data[1 * im.h * im.w + idx] = color[1];
                        im.data[2 * im.h * im.w + idx] = color[2];
                    }
                }
            }
        }

        // Sağ çizgi
        for (int y = top; y <= bot; y++) {
            for (int t = 0; t < thickness; t++) {
                int x = right - t;
                if (x >= 0 && x < im.w && y >= 0 && y < im.h) {
                    int idx = y * im.w + x;
                    if (idx >= 0 && idx < im.w * im.h) {
                        im.data[0 * im.h * im.w + idx] = color[0];
                        im.data[1 * im.h * im.w + idx] = color[1];
                        im.data[2 * im.h * im.w + idx] = color[2];
                    }
                }
            }
        }
    }

    printf("Toplam %d kutu çizildi\n", count);
}

static int last_yolo_classes(network *net) {
    for (int i = net->n - 1; i >= 0; --i)
        if (net->layers[i].type == YOLO)
            return net->layers[i].classes;
    return -1;
}

typedef struct {
    int index;
    int class_id;
    float confidence;
    box bbox;
} sorted_detection;

int main(int argc, char *argv[]) {
    // ---- YOLO/algoritma eşikleri ----
    const float thresh = 0.1f; // önceki kodundaki gibi çok düşük eşik
    const float hier = 0.5f;
    const float nms = 0.45f;
    const int TOPK = 10;

    // ---- Ağı yükle ----
    // Not: cfg/weights eşleşmeli. tiny kullanıyorsan ikisini de tiny seç.
    printf("%d\n", argc);
    const char *cfg = argc >= 2 ? argv[1] : "../config/yolov3-tiny.cfg";
    const char *weights = argc >= 3 ? argv[2] : "../weights/yolov3-tiny.weights";

    network *net = load_network((char *) cfg, (char *) weights, 0);
    if (!net) {
        fprintf(stderr, "load_network() basarisiz\n");
        return 1;
    }
    set_batch_network(net, 1);

    // ---- Kamera ----
    init_reader("/dev/video0");

    int window_initialized = 0;

    while (!window_should_close_33()) {
        long time1 = timeInMilliseconds();

        // 1) Kameradan kare al
        imgdat_s imgdat = load_imgdat();
        image im = {
            .w = imgdat.w, 
            .h = imgdat.h, 
            .c = imgdat.c, 
            .data = imgdat.start
        };

        // 2) İlk karede pencereyi aç (orijinal çözünürlükte)
        if (!window_initialized) {
            if (!init_gl33_window("YOLO (OpenGL 3.3)", im.w, im.h)) {
                fprintf(stderr, "OpenGL/GLFW init basarisiz\n");
                free_imgdat(imgdat);
                break;
            }
            window_initialized = 1;
        }

        // 3) Ağ girişi için letterbox
        image sz = letterbox_image(im, net->w, net->h);

        // 4) İleri yayılım
        network_predict_image(net, sz);

        // 5) Kutuları topla
        int nboxes = 0;
        detection *dets = get_network_boxes(net, im.w, im.h, thresh, hier, 0, 1, &nboxes);

        int classes = last_yolo_classes(net);
        if (classes <= 0) {
            fprintf(stderr, "YOLO katmani yok!\n");
            if (dets)
                free_detections(dets, nboxes);
            free_image(sz);
            free_imgdat(imgdat);
            break;
        }

        // 6) NMS
        if (nms > 0)
            do_nms_sort(dets, nboxes, classes, nms);

        // 7) Geçerli tespitleri skoruna göre topla
        int valid_cap = (nboxes > 0) ? nboxes : 1;
        sorted_detection *sorted_dets = (sorted_detection *) malloc(sizeof(sorted_detection) * valid_cap);
        int valid_count = 0;

        for (int i = 0; i < nboxes; ++i) {
            if (!dets[i].prob)
                continue;
            float max_prob = 0.0f;
            int best_class = -1;
            for (int c = 0; c < classes; ++c) {
                float p = dets[i].prob[c];
                if (p > max_prob) {
                    max_prob = p;
                    best_class = c;
                }
            }
            if (best_class >= 0 && max_prob >= thresh) {
                sorted_dets[valid_count].index = i;
                sorted_dets[valid_count].class_id = best_class;
                sorted_dets[valid_count].confidence = max_prob;
                sorted_dets[valid_count].bbox = dets[i].bbox;
                valid_count++;
            }
        }

        // 8) Azalan güvene göre kabarcık sıralama (valid_count küçük -> yeterli)
        for (int i = 0; i < valid_count - 1; ++i) {
            for (int j = 0; j < valid_count - i - 1; ++j) {
                if (sorted_dets[j].confidence < sorted_dets[j + 1].confidence) {
                    sorted_detection t = sorted_dets[j];
                    sorted_dets[j] = sorted_dets[j + 1];
                    sorted_dets[j + 1] = t;
                }
            }
        }

        // 9) Kutuları orijinal görüntü üzerinde çiz (en iyi TOPK)
        int draw_count = (valid_count < TOPK) ? valid_count : TOPK;
        if (draw_count > 0) {
            draw_top_detections(im, sorted_dets, draw_count);
        }

        // 10) im (CHW, float[0..1]) -> RGB8 (HWC)
        int wh = im.w * im.h;
        unsigned char *rgb8 = (unsigned char *) malloc(wh * 3);
        if (rgb8) {
            for (int i = 0; i < wh; ++i) {
                float r = im.data[0 * wh + i];
                float g = im.data[1 * wh + i];
                float b = im.data[2 * wh + i];
                if (r < 0)
                    r = 0;
                if (r > 1)
                    r = 1;
                if (g < 0)
                    g = 0;
                if (g > 1)
                    g = 1;
                if (b < 0)
                    b = 0;
                if (b > 1)
                    b = 1;
                rgb8[3 * i + 0] = (unsigned char) (r * 255.0f + 0.5f);
                rgb8[3 * i + 1] = (unsigned char) (g * 255.0f + 0.5f);
                rgb8[3 * i + 2] = (unsigned char) (b * 255.0f + 0.5f);
            }

            // 11) Pencereye çiz
            draw_rgb8_to_window_33(rgb8, im.w, im.h);
            free(rgb8);
        }

        // 12) Temizlik
        if (sorted_dets)
            free(sorted_dets);
        if (dets)
            free_detections(dets, nboxes);
        free_image(sz);
        free_imgdat(imgdat);

        // 13) Çıkış kontrolü
        if (window_should_close_33())
            break;

        long time2 = timeInMilliseconds();
        printf("%lu ms\n", time2 - time1);
    }

    // ---- Kapanış ----
    shutdown_gl33();
    free_network(net);
    return 0;
}

#endif // TEST_MODEL
