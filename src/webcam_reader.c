#ifdef WEBCAM_READER

#include <fcntl.h>
#include <frame_utils.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <image_reader.h>

#define WIDTH   640
#define HEIGHT  480

static int fd;
static void *ptr;
static struct v4l2_format fmt = {0};
static struct v4l2_buffer buf = {0};
static struct v4l2_buffer qbuf = {0};
static struct v4l2_requestbuffers req = {0};

void init_reader(const char *path) {
    fd = open(path, O_RDWR);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    ioctl(fd, VIDIOC_S_FMT, &fmt);

    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ioctl(fd, VIDIOC_REQBUFS, &req);

    buf.index = 0;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    ioctl(fd, VIDIOC_QUERYBUF, &buf);

    ptr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
               MAP_SHARED, fd, buf.m.offset);

    qbuf.index = 0;
    qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    qbuf.memory = V4L2_MEMORY_MMAP;
    ioctl(fd, VIDIOC_QBUF, &qbuf);

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMON, &type);
}

void close_reader(const char *path) {
    ioctl(fd, VIDIOC_STREAMOFF, &req.type);
    munmap(ptr, buf.length);
    close(fd);
}

imgdat_s load_data() {
    struct v4l2_buffer dqbuf = {0};
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dqbuf.memory = V4L2_MEMORY_MMAP;


    ioctl(fd, VIDIOC_DQBUF, &dqbuf);

    const imgdat_s data = {
        .w = WIDTH,
        .h = HEIGHT,
        .c = 3,
        .start = yuyv_to_rgb(ptr, WIDTH, HEIGHT),
        .length = WIDTH * HEIGHT * 3 * sizeof(float),
    };

    ioctl(fd, VIDIOC_QBUF, &dqbuf);
    return data;
}

#endif // WEBCAM_READER
