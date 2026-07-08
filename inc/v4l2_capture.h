#ifndef _V4L2_CAP_H
#define _V4L2_CAP_H

#include <unistd.h>
#include "rti_conf.h"
#include "ps.h"

struct rti_cam_desc
{
    int fd;
    void * usr_buf_start[RTI_V4L2_BUF_COUNT][MAX_PLANES];
    size_t usr_buf_length[RTI_V4L2_BUF_COUNT][MAX_PLANES];
    unsigned int num_planes;
};

struct rti_cam_desc * rti_camera_init(void);
void rti_camera_release(struct rti_cam_desc * desc);
int get_16_grayscale_images(GImage * images, struct rti_cam_desc * desc);

#endif
