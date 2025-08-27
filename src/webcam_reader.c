#ifdef WEBCAM_READER

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <tracker/frame_utils.h>
#include <tracker/image_reader.h>
#include <unistd.h>

#define WIDTH 640
#define HEIGHT 480

static int fd;
static int buf_len;
static int req_type;
static void *ptr;

void init_reader(const char *path) {
    fd = open(path, O_RDWR);

    struct v4l2_format fmt = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .fmt.pix.width = WIDTH,
            .fmt.pix.height = HEIGHT,
            .fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV,
            .fmt.pix.field = V4L2_FIELD_NONE,
    };
    struct v4l2_requestbuffers req = {
            .count = 1,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
    };
    struct v4l2_buffer buf = {
            .index = 0,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
    };
    struct v4l2_buffer qbuf = {
            .index = 0,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
    };

    ioctl(fd, VIDIOC_S_FMT, &fmt);
    ioctl(fd, VIDIOC_REQBUFS, &req);
    ioctl(fd, VIDIOC_QUERYBUF, &buf);

    buf_len = buf.length;
    req_type = req.type;
    ptr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ioctl(fd, VIDIOC_QBUF, &qbuf);
    ioctl(fd, VIDIOC_STREAMON, &type);
}

void close_reader(const char *path) {
    ioctl(fd, VIDIOC_STREAMOFF, &req_type);
    munmap(ptr, buf_len);
    close(fd);
}

imgdat_s load_imgdat() {
    struct v4l2_buffer dqbuf = {0};
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dqbuf.memory = V4L2_MEMORY_MMAP;


    ioctl(fd, VIDIOC_DQBUF, &dqbuf);

    const imgdat_s data = {
            .w = WIDTH,
            .h = HEIGHT,
            .c = 3,
            .start = yuyv_to_rgb(ptr, WIDTH, HEIGHT),
    };

    ioctl(fd, VIDIOC_QBUF, &dqbuf);
    return data;
}

#endif // WEBCAM_READER
