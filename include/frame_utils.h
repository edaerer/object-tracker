#ifndef FRAME_UTILS_H
#define FRAME_UTILS_H

#include <stdlib.h>

double clamp(const double v) {
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

float *yuyv_to_rgb(const unsigned char *yuyv, int w, int h) {
    float *data = malloc(w * h * 3 * sizeof(float));

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; x += 2) {
            const int idx = (y * w + x) * 2;

            const double Y0 = yuyv[idx + 0];
            const double U = yuyv[idx + 1];
            const double Y1 = yuyv[idx + 2];
            const double V = yuyv[idx + 3];

            double r = clamp(Y0 + 1.402 * (V - 128));
            double g = clamp(Y0 - 0.344136 * (U - 128) - 0.714136 * (V - 128));
            double b = clamp(Y0 + 1.772 * (U - 128));
            data[0 * w * h + y * w + x] = (float) (r / 255.0);
            data[1 * w * h + y * w + x] = (float) (g / 255.0);
            data[2 * w * h + y * w + x] = (float) (b / 255.0);

            r = clamp(Y1 + 1.402 * (V - 128));
            g = clamp(Y1 - 0.344136 * (U - 128) - 0.714136 * (V - 128));
            b = clamp(Y1 + 1.772 * (U - 128));
            data[0 * w * h + y * w + x + 1] = (float) (r / 255.0);
            data[1 * w * h + y * w + x + 1] = (float) (g / 255.0);
            data[2 * w * h + y * w + x + 1] = (float) (b / 255.0);
        }
    }

    return data;
}

#endif
