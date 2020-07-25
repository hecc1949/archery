#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "utils.h"

void flushFramebuffer(void)
{
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    char *frameBuffer = 0;
    int fbFd = 0;
    long int memsize = 0;
    u_int8_t *src, *dst;

    fbFd = open("/dev/fb0", O_RDWR);
    if (fbFd == -1)
    {
        return;
    }
    if (::ioctl(fbFd, FBIOGET_FSCREENINFO, &finfo) == -1)
    {
        return;
    }
    if (ioctl(fbFd, FBIOGET_VSCREENINFO, &vinfo) == -1)
    {
        return;
    }

    frameBuffer = (char *)mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED,fbFd, 0);
    if (frameBuffer == MAP_FAILED)
    {
        return;
    }

    //双缓存的framebuffer有2帧(page)图像，把当前显示帧的图像复制到另一帧，以避免开窗口video显示闪烁.
    //==>效果不好
    memsize = finfo.line_length*vinfo.yres;
    src = (u_int8_t *)(frameBuffer + finfo.line_length*vinfo.yoffset);
    dst = (u_int8_t *)(frameBuffer+ finfo.line_length*(vinfo.yres - vinfo.yoffset));
    ::memcpy(dst, src, memsize);

    munmap(frameBuffer, finfo.smem_len);
    ::close(fbFd);
}
