#ifndef IMAGE_READER_H
#define IMAGE_READER_H

#include <stdlib.h>

typedef struct {
    int w, h, c;
    int length;
    void *start;
} imgdat_s;


static void free_imgdat(const imgdat_s data) {
    free(data.start);
}

void init_reader(const char *path);

void close_reader(const char *path);

imgdat_s load_imgdat();

#endif // IMAGE_READER_H
