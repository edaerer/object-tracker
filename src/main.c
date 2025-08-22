#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <../libs/sod/sod.h>
#include <image_reader.h>

int main(void) {
    init_reader("/dev/video0");

    while (1) {
        imgdat_s data = load_data();

        sod_img img = sod_make_empty_image(
            data.w, data.h, data.c);
        img.data = data.start;
        sod_img_save_as_png(img, "frame.png");

        free_imgdat(data);
    }

    close_reader(NULL);

    return 0;
}
