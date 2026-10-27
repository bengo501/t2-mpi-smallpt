/*
 * smallpt_seq.c — versao sequencial de referencia do T2 (sem MPI).
 *
 * A regiao medida e' so' o laco que renderiza as linhas, o mesmo limite
 * da versao paralela: alocacao, camera e checksum ficam de fora.
 * --copies N repete a mesma imagem N vezes (escalabilidade fraca).
 * Cada copia usa a mesma semente, entao o custo por tarefa nao muda.
 */
#define _POSIX_C_SOURCE 200809L
#include "smallpt_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "uso: %s [-w W] [-h H] [-s SPP] [-g G] [--copies N] [--seed N]\n",
            argv0);
}

int main(int argc, char **argv) {
    int w = 320, h = 240, spp = 4, g = 1, copies = 1;
    uint64_t seed = 12345;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-w") && i + 1 < argc) w = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-h") && i + 1 < argc) h = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-s") && i + 1 < argc) spp = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-g") && i + 1 < argc) g = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--copies") && i + 1 < argc) copies = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint64_t)atoll(argv[++i]);
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else { fprintf(stderr, "opcao desconhecida: %s\n", argv[i]); usage(argv[0]); return 2; }
    }
    if (w <= 0 || h <= 0 || spp <= 0 || g <= 0 || copies <= 0) {
        fprintf(stderr, "erro: w, h, spp, g e copies devem ser positivos\n");
        return 2;
    }

    Camera cam;
    setup_camera(w, h, &cam);

    size_t pix3 = (size_t)w * (size_t)h * 3;
    double *img = (double *)calloc((size_t)copies * pix3, sizeof(double));
    if (!img) { fprintf(stderr, "sem memoria\n"); return 1; }

    int chunks = nchunks_of(h, g);
    int tasks = copies * chunks;

    /* mesmo limite da versao MPI: so' a renderizacao entra no tempo */
    double t0 = now_s();
    for (int id = 0; id < tasks; ++id) {
        int y0, nrows;
        chunk_of(id, h, g, &y0, &nrows);
        int copy = id / chunks;
        double *dst = img + ((size_t)copy * (size_t)h + (size_t)y0) * (size_t)w * 3;
        render_rows(y0, nrows, w, h, spp, seed, &cam, dst);
    }
    double elapsed = now_s() - t0;

    double checksum = checksum_image(img, w, h);
    int ok = copies_match(img, copies, w, h);
    long long pixels = (long long)copies * w * h;
    long long rays = pixels * 4LL * spp;

    printf("run,seq,1,%d,%d,%d,%d,%d,%.6f,%.17g,%d,%lld,%lld,%d\n",
           g, copies, w, h, spp, elapsed, checksum, ok, pixels, rays, tasks);
    fflush(stdout);
    free(img);
    return ok ? 0 : 3;
}
