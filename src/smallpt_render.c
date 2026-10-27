/* Unica unidade de compilacao do nucleo numerico. Sequencial e MPI
 * ligam este objeto, compilado com as mesmas flags, para o checksum
 * ser identico. */
#include "smallpt.h"

void setup_camera(int w, int h, Camera *cam) { sp_setup_camera(w, h, cam); }

void render_rows(int y0, int nrows, int w, int h, int spp, uint64_t seed,
                 const Camera *cam, double *out) {
    sp_render_rows(y0, nrows, w, h, spp, seed, cam, out);
}

int nchunks_of(int h, int g) { return sp_nchunks_of(h, g); }

void chunk_of(int id, int h, int g, int *y0, int *nrows) {
    sp_chunk_of(id, h, g, y0, nrows);
}

double checksum_image(const double *img, int w, int h) {
    return sp_checksum_image(img, w, h);
}

int copies_match(const double *img, int copies, int w, int h) {
    return sp_copies_match(img, copies, w, h);
}
