#ifndef _PS_H
#define _PS_H

#include "rti_conf.h"

typedef struct
{
    unsigned char P[FRAME_HEIGHT][FRAME_WIDTH];
}GImage;

void gamma_tables_init(void);
void rti_ps_init(void);
void photometric_stereo(const GImage * images, float * normals, float * albedo);
void render_by_Lambertian_Diffuse(GImage * images, float * normals, float * albedo, const float light_dir[3]);

#endif
