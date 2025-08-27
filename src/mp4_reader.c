#ifdef MP4_READER

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <unistd.h>

#define WIDTH 1280
#define HEIGHT 720

static int fd;

void init_reader(const char *path) {}

void close_reader(const char *path) {}

imgdat_s load_data() { return NULL; }

#endif // MP4_READER
