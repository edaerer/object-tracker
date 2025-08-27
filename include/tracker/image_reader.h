#ifndef IMAGE_READER_H
#define IMAGE_READER_H

#include <stdlib.h>

typedef struct {
    int   w;
    int   h;
    int   c;
    void* start;
} imgdata;

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

int        imgdata_load(imgreader* reader, imgdata* data);

int        imgdata_free(imgdata* data);

#endif  // IMAGE_READER_H
