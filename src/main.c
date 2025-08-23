#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <image_reader.h>

int main(void) {
    init_reader("/dev/video0");

    while (1) {
        imgdat_s data = load_data();

        printf("Size: %d\n", data.length);
        printf("Width: %d\n", data.w);
        printf("Height: %d\n", data.h);

        free_imgdat(data);
    }

    close_reader(NULL);

    return 0;
}
