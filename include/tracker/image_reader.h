#ifndef IMAGE_READER_H
#define IMAGE_READER_H

#include <stdlib.h>

typedef struct {
    int   w, h, c;
    void* data;
} imgframe;

typedef struct {
    int   fd;
    int   w, h, c;
    void* params;

    struct {
        int   size;
        void* data;
    } buffer;
} imgreader;


imgreader* imgreader_init(const char* path, int w, int h);
int        imgreader_close(imgreader* reader);
int        imgdata_load(imgreader* reader, imgframe* frame);
int        imgdata_free(imgframe* frame);

#endif  // IMAGE_READER_H
