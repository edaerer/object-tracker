#ifdef WEBCAM_READER

#include <fcntl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <tracker/frame_utils.h>
#include <tracker/image_reader.h>
#include <unistd.h>

imgreader* imgreader_init(const char* path, int w, int h) {
    struct v4l2_format fmt = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .fmt.pix.width = w,
        .fmt.pix.height = h,
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

    imgreader* reader = malloc(sizeof(imgreader));
    reader->fd = open(path, O_RDWR);

    ioctl(reader->fd, VIDIOC_S_FMT, &fmt);
    ioctl(reader->fd, VIDIOC_G_FMT, &fmt);
    ioctl(reader->fd, VIDIOC_REQBUFS, &req);
    ioctl(reader->fd, VIDIOC_QUERYBUF, &buf);

    reader->w = fmt.fmt.pix.width;
    reader->h = fmt.fmt.pix.height;
    reader->c = 3;
    reader->params = malloc(sizeof(int));
    reader->buffer.size = buf.length;
    reader->buffer.data = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                               MAP_SHARED, reader->fd, buf.m.offset);

    memcpy(reader->params, &req.type, sizeof(int));

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ioctl(reader->fd, VIDIOC_QBUF, &qbuf);
    ioctl(reader->fd, VIDIOC_STREAMON, &type);

    return reader;
}

int imgreader_close(imgreader* reader) {
    int reqtype;
    memcpy(&reqtype, reader->params, sizeof(int));

    ioctl(reader->fd, VIDIOC_STREAMOFF, &reqtype);
    munmap(reader->buffer.data, reader->buffer.size);
    close(reader->fd);

    free(reader->buffer.data);
    free(reader->params);
    free(reader);

    return 1;
}

int imgdata_load(imgreader* reader, imgframe* frame) {
    struct v4l2_buffer dqbuf = {0};
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dqbuf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(reader->fd, VIDIOC_DQBUF, &dqbuf) < 0) {
        return 0;
    }

    frame->w = reader->w;
    frame->h = reader->h;
    frame->c = reader->c;
    frame->data = yuyv_to_rgb(reader->buffer.data, reader->w, reader->h);

    if (ioctl(reader->fd, VIDIOC_QBUF, &dqbuf) < 0) {
        return 0;
    }

    return 1;
}

int imgdata_free(imgframe* frame) {
    free(frame);
    free(frame->data);
}

#endif  // WEBCAM_READER
