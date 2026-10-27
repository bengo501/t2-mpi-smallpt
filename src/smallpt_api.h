#ifndef SMALLPT_API_H
#define SMALLPT_API_H

#include <stdint.h>

typedef struct { double x, y, z; } Vec;
typedef struct { Vec o, d, cx, cy; } Camera;

void setup_camera(int w, int h, Camera *cam);
void render_rows(int y0, int nrows, int w, int h, int spp, uint64_t seed,
                 const Camera *cam, double *out);
int nchunks_of(int h, int g);
void chunk_of(int id, int h, int g, int *y0, int *nrows);
double checksum_image(const double *img, int w, int h);
int copies_match(const double *img, int copies, int w, int h);

#endif
