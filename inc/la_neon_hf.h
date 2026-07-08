#ifndef _LA_NEON_HF_H
#define _LA_NEON_HF_H

typedef struct 
{
    float m[3][3];
}Mat3;

void neon_compute_ATB(const float AT[3][16], const float B[16], float ATB[3]);
void neon_3x3mat_multi_vec(const float A[9], const float v[3], float out[3]);

float hf_3x3mat_det(const Mat3 * A);
Mat3 hf_3x3mat_inv(const Mat3 * A);

#endif
