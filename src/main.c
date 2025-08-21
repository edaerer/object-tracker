#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#define SOD_IMPLEMENTATION
#include "sod.h"

static inline unsigned char clamp(int v){
    if(v<0) return 0;
    if(v>255) return 255;
    return v;
}

int main() {
    int fd = open("/dev/video0", O_RDWR);
    if(fd < 0){
        perror("Can not open camera.");
        return -1;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if(ioctl(fd, VIDIOC_S_FMT, &fmt) < 0){
        perror("Format ayarlanamadı");
        close(fd);
        return -1;
    }

    // Buffer request
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if(ioctl(fd, VIDIOC_REQBUFS, &req) < 0){
        perror("Buffer request hatası");
        close(fd);
        return -1;
    }

    struct v4l2_buffer buf;
    memset(&buf,0,sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0){
        perror("Query buffer hatası");
        close(fd);
        return -1;
    }

    void *buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

    // Stream başlat
    if(ioctl(fd, VIDIOC_QBUF, &buf) < 0){ perror("QBUF hatası"); return -1; }
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_STREAMON, &type) < 0){ perror("Stream başlatılamadı"); return -1; }

    printf("Kamera açıldı, bir frame alınıyor...\n");

    // Frame al
    if(ioctl(fd, VIDIOC_DQBUF, &buf) < 0){ perror("Frame alınamadı"); return -1; }

    // SOD image oluştur
    sod_img frame = sod_make_image(fmt.fmt.pix.width, fmt.fmt.pix.height, 3);
    unsigned char *yuyv = (unsigned char*)buffer;
    int width = fmt.fmt.pix.width;
    int height = fmt.fmt.pix.height;

    for(int y=0; y<height; y++){
        for(int x=0; x<width; x+=2){
            int idx = (y*width + x)*2;
            unsigned char Y0 = yuyv[idx + 0];
            unsigned char U  = yuyv[idx + 1];
            unsigned char Y1 = yuyv[idx + 2];
            unsigned char V  = yuyv[idx + 3];

            // İlk pixel
            int r = clamp(Y0 + 1.402*(V-128));
            int g = clamp(Y0 - 0.344136*(U-128) - 0.714136*(V-128));
            int b = clamp(Y0 + 1.772*(U-128));
            frame.data[0*frame.w*frame.h + y*frame.w + x] = r/255.0f;
            frame.data[1*frame.w*frame.h + y*frame.w + x] = g/255.0f;
            frame.data[2*frame.w*frame.h + y*frame.w + x] = b/255.0f;

            // İkinci pixel
            r = clamp(Y1 + 1.402*(V-128));
            g = clamp(Y1 - 0.344136*(U-128) - 0.714136*(V-128));
            b = clamp(Y1 + 1.772*(U-128));
            frame.data[0*frame.w*frame.h + y*frame.w + x+1] = r/255.0f;
            frame.data[1*frame.w*frame.h + y*frame.w + x+1] = g/255.0f;
            frame.data[2*frame.w*frame.h + y*frame.w + x+1] = b/255.0f;
        }
    }

    // PNG kaydet
    sod_img_save_as_png(frame, "frame_test.png");
    printf("Frame kaydedildi: frame_test.png\n");

    sod_free_image(frame);
    munmap(buffer, buf.length);
    close(fd);

    return 0;
}