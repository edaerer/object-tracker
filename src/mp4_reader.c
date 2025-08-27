#ifdef MP4_READER

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <unistd.h>

#define WIDTH  1280
#define HEIGHT 720

imgreader* imgreader_init(const char* path, int w, int h) {
    return NULL;
}

int imgreader_close(imgreader* reader) {
    return 0;
}

int imgdata_load(imgreader* reader, imgdata* data) {
    return 0;
}

int imgdata_free(imgdata* data) {
    return 0;
}

#endif  // MP4_READER
