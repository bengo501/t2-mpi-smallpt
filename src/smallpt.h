/*
 * Nucleo do path tracer smallpt (Kevin Beason, 2008), reescrito em C11
 * no T1 e reutilizado aqui. Sequencial e MPI incluem este arquivo para
 * que cada pixel seja calculado pelo mesmo codigo: o checksum coincide.
 *
 * Uma tarefa e' um bloco de linhas. A semente do gerador depende so' da
 * linha global, nao de quem a renderiza, entao a imagem nao muda com o
 * numero de processos nem com a granularidade.
 */
#ifndef SMALLPT_H
#define SMALLPT_H

#define _POSIX_C_SOURCE 200809L
#include "smallpt_api.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline Vec v_(double x, double y, double z) { Vec r = {x, y, z}; return r; }
static inline Vec v_add(Vec a, Vec b) { return v_(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline Vec v_sub(Vec a, Vec b) { return v_(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline Vec v_scl(Vec a, double s) { return v_(a.x * s, a.y * s, a.z * s); }
static inline Vec v_mul(Vec a, Vec b) { return v_(a.x * b.x, a.y * b.y, a.z * b.z); }
static inline double v_dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline Vec v_cross(Vec a, Vec b) {
    return v_(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline Vec v_norm(Vec a) {
    double n = 1.0 / sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return v_(a.x * n, a.y * n, a.z * n);
}

typedef struct { Vec o, d; } Ray;
typedef enum { DIFF, SPEC, REFR } Refl_t;
typedef struct {
    double rad;
    Vec p, e, c;
    Refl_t refl;
} Sphere;

static inline double sphere_intersect(const Sphere *s, Ray r) {
    Vec op = v_sub(s->p, r.o);
    double eps = 1e-4;
    double b = v_dot(op, r.d);
    double det = b * b - v_dot(op, op) + s->rad * s->rad;
    if (det < 0) return 0;
    det = sqrt(det);
    double t = b - det;
    if (t > eps) return t;
    t = b + det;
    return (t > eps) ? t : 0.0;
}

static const Sphere kScene[] = {
    {1e5, {1e5 + 1, 40.8, 81.6}, {0, 0, 0}, {.75, .25, .25}, DIFF},
    {1e5, {-1e5 + 99, 40.8, 81.6}, {0, 0, 0}, {.25, .25, .75}, DIFF},
    {1e5, {50, 40.8, 1e5}, {0, 0, 0}, {.75, .75, .75}, DIFF},
    {1e5, {50, 40.8, -1e5 + 170}, {0, 0, 0}, {0, 0, 0}, DIFF},
    {1e5, {50, 1e5, 81.6}, {0, 0, 0}, {.75, .75, .75}, DIFF},
    {1e5, {50, -1e5 + 81.6, 81.6}, {0, 0, 0}, {.75, .75, .75}, DIFF},
    {16.5, {27, 16.5, 47}, {0, 0, 0}, {.999, .999, .999}, SPEC},
    {16.5, {73, 16.5, 78}, {0, 0, 0}, {.999, .999, .999}, REFR},
    {600, {50, 681.6 - .27, 81.6}, {12, 12, 12}, {0, 0, 0}, DIFF},
};
static const int kNumSpheres = (int)(sizeof(kScene) / sizeof(Sphere));

static inline double clamp01(double x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

static inline int intersect_scene(Ray r, double *t_out, int *id_out) {
    double inf = 1e20;
    double t = inf;
    int id = 0;
    for (int i = kNumSpheres; i--;) {
        double d = sphere_intersect(&kScene[i], r);
        if (d && d < t) {
            t = d;
            id = i;
        }
    }
    *t_out = t;
    *id_out = id;
    return t < inf;
}

typedef struct { uint64_t s; } Rng;

static inline uint64_t rng_next(Rng *r) {
    uint64_t x = r->s ? r->s : 0x1234567u;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    r->s = x;
    return x;
}
static inline double rng_uniform(Rng *r) {
    return (rng_next(r) >> 11) * (1.0 / (double)(1ULL << 53));
}

static Vec radiance(Ray r, Rng *rng) {
    Vec cl = {0, 0, 0};
    Vec cf = {1, 1, 1};
    int depth = 0;
    while (1) {
        double t;
        int id = 0;
        if (!intersect_scene(r, &t, &id)) return cl;
        const Sphere *obj = &kScene[id];
        Vec x = v_add(r.o, v_scl(r.d, t));
        Vec n = v_norm(v_sub(x, obj->p));
        Vec nl = v_dot(n, r.d) < 0 ? n : v_scl(n, -1);
        Vec f = obj->c;
        double p_max = (f.x > f.y && f.x > f.z) ? f.x : (f.y > f.z ? f.y : f.z);
        cl = v_add(cl, v_mul(cf, obj->e));
        if (++depth > 5) {
            if (depth > 64) return cl;
            if (rng_uniform(rng) < p_max) f = v_scl(f, 1.0 / p_max);
            else return cl;
        }
        cf = v_mul(cf, f);
        if (obj->refl == DIFF) {
            double r1 = 2 * M_PI * rng_uniform(rng);
            double r2 = rng_uniform(rng);
            double r2s = sqrt(r2);
            Vec w = nl;
            Vec u = v_norm(v_cross((fabs(w.x) > .1 ? v_(0, 1, 0) : v_(1, 0, 0)), w));
            Vec v = v_cross(w, u);
            Vec d = v_norm(v_add(v_add(v_scl(u, cos(r1) * r2s), v_scl(v, sin(r1) * r2s)),
                                 v_scl(w, sqrt(1 - r2))));
            r.o = x;
            r.d = d;
            continue;
        }
        if (obj->refl == SPEC) {
            Vec rd = v_sub(r.d, v_scl(n, 2 * v_dot(n, r.d)));
            r.o = x;
            r.d = rd;
            continue;
        }
        Ray reflRay;
        reflRay.o = x;
        reflRay.d = v_sub(r.d, v_scl(n, 2 * v_dot(n, r.d)));
        int into = v_dot(n, nl) > 0;
        double nc = 1, nt = 1.5;
        double nnt = into ? nc / nt : nt / nc;
        double ddn = v_dot(r.d, nl);
        double cos2t = 1 - nnt * nnt * (1 - ddn * ddn);
        if (cos2t < 0) {
            r = reflRay;
            continue;
        }
        Vec tdir = v_norm(v_sub(v_scl(r.d, nnt),
                                v_scl(n, (into ? 1 : -1) * (ddn * nnt + sqrt(cos2t)))));
        double a = nt - nc, b = nt + nc;
        double R0 = a * a / (b * b);
        double c_ = 1 - (into ? -ddn : v_dot(tdir, n));
        double Re = R0 + (1 - R0) * c_ * c_ * c_ * c_ * c_;
        double Tr = 1 - Re;
        double P = .25 + .5 * Re;
        double RP = Re / P, TP = Tr / (1 - P);
        if (depth > 2) {
            if (rng_uniform(rng) < P) {
                cf = v_scl(cf, RP);
                r = reflRay;
            } else {
                cf = v_scl(cf, TP);
                r.o = x;
                r.d = tdir;
            }
        } else {
            if (rng_uniform(rng) < .5) {
                cf = v_scl(cf, Re / .5);
                r = reflRay;
            } else {
                cf = v_scl(cf, Tr / .5);
                r.o = x;
                r.d = tdir;
            }
        }
    }
}

static void sp_setup_camera(int w, int h, Camera *cam) {
    cam->o = v_(50, 52, 295.6);
    cam->d = v_norm(v_(0, -0.042612, -1));
    cam->cx = v_(w * 0.5135 / h, 0, 0);
    cam->cy = v_scl(v_norm(v_cross(cam->cx, cam->d)), 0.5135);
}

/* Renderiza nrows linhas a partir de y0. out tem nrows*w*3 doubles (RGB). */
static void sp_render_rows(int y0, int nrows, int w, int h, int spp, uint64_t seed,
                        const Camera *cam, double *out) {
    for (int i = 0; i < nrows; ++i) {
        int y = y0 + i;
        Rng rng;
        rng.s = seed ^ ((uint64_t)(y + 1) * 0x9E3779B97F4A7C15ULL);
        for (int x = 0; x < w; ++x) {
            Vec r_pixel = {0, 0, 0};
            for (int sy = 0; sy < 2; ++sy) {
                for (int sx = 0; sx < 2; ++sx) {
                    Vec r_sub = {0, 0, 0};
                    for (int s = 0; s < spp; ++s) {
                        double r1 = 2 * rng_uniform(&rng);
                        double dx = r1 < 1 ? sqrt(r1) - 1 : 1 - sqrt(2 - r1);
                        double r2 = 2 * rng_uniform(&rng);
                        double dy = r2 < 1 ? sqrt(r2) - 1 : 1 - sqrt(2 - r2);
                        Vec d = v_add(v_add(v_scl(cam->cx, (((sx + .5 + dx) / 2 + x) / w - .5)),
                                            v_scl(cam->cy, (((sy + .5 + dy) / 2 + y) / h - .5))),
                                      cam->d);
                        Ray ray;
                        ray.o = v_add(cam->o, v_scl(d, 140));
                        ray.d = v_norm(d);
                        r_sub = v_add(r_sub, v_scl(radiance(ray, &rng), 1.0 / spp));
                    }
                    r_pixel = v_add(r_pixel, v_scl(v_(clamp01(r_sub.x), clamp01(r_sub.y),
                                                      clamp01(r_sub.z)),
                                                  .25));
                }
            }
            double *pix = out + ((size_t)i * (size_t)w + (size_t)x) * 3;
            pix[0] = r_pixel.x;
            pix[1] = r_pixel.y;
            pix[2] = r_pixel.z;
        }
    }
}

static int sp_nchunks_of(int h, int g) {
    return (h + g - 1) / g;
}

static void sp_chunk_of(int id, int h, int g, int *y0, int *nrows) {
    int per = sp_nchunks_of(h, g);
    int chunk = id % per;
    *y0 = chunk * g;
    int n = g;
    if (*y0 + n > h) n = h - *y0;
    *nrows = n;
}

static double sp_checksum_image(const double *img, int w, int h) {
    double s = 0;
    size_t n = (size_t)w * (size_t)h * 3;
    for (size_t i = 0; i < n; ++i) s += img[i];
    return s;
}

/* 1 se todas as copias sao iguais a primeira, pixel a pixel. */
static int sp_copies_match(const double *img, int copies, int w, int h) {
    size_t n = (size_t)w * (size_t)h * 3;
    for (int c = 1; c < copies; ++c) {
        if (memcmp(img, img + c * n, n * sizeof(double)) != 0) return 0;
    }
    return 1;
}

#endif
