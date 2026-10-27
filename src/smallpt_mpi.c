/*
 * smallpt_mpi.c — mestre/escravo com saco de linhas (T2).
 *
 * Requisitos do enunciado:
 *   1. Iniciativa do escravo: ele pede trabalho; o resultado vale como
 *      novo pedido.
 *   2. O mestre recebe com MPI_ANY_SOURCE (quem ficar pronto primeiro).
 *   3. A ordem original e' mantida. Cada escravo tem no maximo uma
 *      tarefa por vez, entao o mestre guarda em task_of[rank] qual
 *      tarefa entregou e copia o resultado para essa posicao.
 *   4. Saco vazio: mensagem de termino com tag propria (TAG_FIM).
 *   5. Qualquer P >= 2 e qualquer numero de tarefas (ultimo bloco pode
 *      ser menor que G; copies e altura nao precisam ser multiplos).
 *   6. Granularidade G = linhas por mensagem, lida com -g.
 *
 * Tags (o tipo da mensagem distingue o que ela significa):
 *   TAG_PEDIDO  (1) escravo -> mestre, 1 double. "estou livre".
 *   TAG_TAREFA  (2) mestre -> escravo, 3 ints: y0, nrows, id da tarefa.
 *   TAG_RESULTADO (3) escravo -> mestre, nrows*w*3 doubles (RGB).
 *   TAG_FIM     (4) mestre -> escravo, 0 ints. encerra o escravo.
 *   TAG_PING/PONG (5/6) so' no modo --probe: ida e volta sem calculo.
 *   TAG_PROBE_GO / TAG_PROBE_TIME (7/8) medem o custo de uma tarefa.
 *
 * Regiao medida: MPI_Wtime no mestre, do instante em que a distribuicao
 * comeca ate' o ultimo resultado. Alocacao, camera e checksum ficam fora.
 */
#include "smallpt_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define TAG_PEDIDO 1
#define TAG_TAREFA 2
#define TAG_RESULTADO 3
#define TAG_FIM 4
#define TAG_PING 5
#define TAG_PONG 6
#define TAG_PROBE_GO 7
#define TAG_PROBE_TIME 8
#define TAG_STATS 9

static void usage(const char *argv0) {
    fprintf(stderr,
            "uso: %s [-w W] [-h H] [-s SPP] [-g G] [--copies N] [--seed N] [--probe]\n"
            "  -g G        linhas por mensagem (granularidade)\n"
            "  --copies N  repeticoes da imagem (escalabilidade fraca: N = P)\n"
            "  --probe     mede RTT de uma mensagem e o custo de uma tarefa\n",
            argv0);
}

static int parse_args(int argc, char **argv, int *w, int *h, int *spp, int *g,
                      int *copies, uint64_t *seed, int *probe) {
    *w = 320; *h = 240; *spp = 4; *g = 1; *copies = 1; *seed = 12345; *probe = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-w") && i + 1 < argc) *w = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-h") && i + 1 < argc) *h = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-s") && i + 1 < argc) *spp = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-g") && i + 1 < argc) *g = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--copies") && i + 1 < argc) *copies = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) *seed = (uint64_t)atoll(argv[++i]);
        else if (!strcmp(argv[i], "--probe")) *probe = 1;
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 2; }
        else { fprintf(stderr, "opcao desconhecida: %s\n", argv[i]); usage(argv[0]); return 2; }
    }
    if (*w <= 0 || *h <= 0 || *spp <= 0 || *g <= 0 || *copies <= 0) {
        fprintf(stderr, "erro: w, h, spp, g e copies devem ser positivos\n");
        return 2;
    }
    return 0;
}

/* Ida e volta de um buffer do tamanho de uma tarefa, sem renderizar,
 * e o tempo de renderizar uma tarefa no escravo local (rank 1) e no
 * primeiro escravo do outro no (rank size/2, com distribuicao block). */
static void run_probe(int rank, int size, int w, int h, int spp, int g, uint64_t seed) {
    int local = 1;
    int remote = size / 2;
    if (remote == local) remote = (size > 2) ? 2 : 1;
    int nrows = g < h ? g : h;
    int nd = nrows * w * 3;
    double *buf = (double *)calloc((size_t)nd, sizeof(double));
    if (!buf) MPI_Abort(MPI_COMM_WORLD, 1);

    Camera cam;
    setup_camera(w, h, &cam);

    int same = (local == remote);
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        double t0, rtt_l, rtt_r, comp_l, comp_r;
        t0 = MPI_Wtime();
        MPI_Send(buf, nd, MPI_DOUBLE, local, TAG_PING, MPI_COMM_WORLD);
        MPI_Recv(buf, nd, MPI_DOUBLE, local, TAG_PONG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        rtt_l = MPI_Wtime() - t0;
        if (!same) {
            t0 = MPI_Wtime();
            MPI_Send(buf, nd, MPI_DOUBLE, remote, TAG_PING, MPI_COMM_WORLD);
            MPI_Recv(buf, nd, MPI_DOUBLE, remote, TAG_PONG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            rtt_r = MPI_Wtime() - t0;
        } else {
            rtt_r = rtt_l;
        }

        int y0 = h / 2;
        if (y0 + nrows > h) y0 = 0;
        int meta[2] = {y0, nrows};
        MPI_Send(meta, 2, MPI_INT, local, TAG_PROBE_GO, MPI_COMM_WORLD);
        MPI_Recv(&comp_l, 1, MPI_DOUBLE, local, TAG_PROBE_TIME, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (!same) {
            MPI_Send(meta, 2, MPI_INT, remote, TAG_PROBE_GO, MPI_COMM_WORLD);
            MPI_Recv(&comp_r, 1, MPI_DOUBLE, remote, TAG_PROBE_TIME, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        } else {
            comp_r = comp_l;
        }

        char host_l[MPI_MAX_PROCESSOR_NAME], host_r[MPI_MAX_PROCESSOR_NAME];
        MPI_Recv(host_l, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, local, TAG_PONG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (!same)
            MPI_Recv(host_r, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, remote, TAG_PONG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        else
            memcpy(host_r, host_l, MPI_MAX_PROCESSOR_NAME);

        printf("probe,%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%s,%s\n",
               size, g, nd * (int)sizeof(double), w, spp,
               rtt_l, rtt_r, comp_l, comp_r, host_l, host_r);
        fflush(stdout);
    } else if (rank == local || rank == remote) {
        MPI_Status st;
        MPI_Recv(buf, nd, MPI_DOUBLE, 0, TAG_PING, MPI_COMM_WORLD, &st);
        MPI_Send(buf, nd, MPI_DOUBLE, 0, TAG_PONG, MPI_COMM_WORLD);
        int meta[2];
        MPI_Recv(meta, 2, MPI_INT, 0, TAG_PROBE_GO, MPI_COMM_WORLD, &st);
        double *row = (double *)malloc((size_t)meta[1] * (size_t)w * 3 * sizeof(double));
        double t0 = MPI_Wtime();
        render_rows(meta[0], meta[1], w, h, spp, seed, &cam, row);
        double comp = MPI_Wtime() - t0;
        MPI_Send(&comp, 1, MPI_DOUBLE, 0, TAG_PROBE_TIME, MPI_COMM_WORLD);
        char host[MPI_MAX_PROCESSOR_NAME];
        int namelen = 0;
        MPI_Get_processor_name(host, &namelen);
        MPI_Send(host, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, TAG_PONG, MPI_COMM_WORLD);
        free(row);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    free(buf);
}

int main(int argc, char **argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int w, h, spp, g, copies, probe;
    uint64_t seed;
    int rc = 0;
    if (rank == 0) rc = parse_args(argc, argv, &w, &h, &spp, &g, &copies, &seed, &probe);
    MPI_Bcast(&rc, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rc) { MPI_Finalize(); return rc; }
    MPI_Bcast(&w, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&h, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&spp, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&g, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&copies, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&probe, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&seed, 1, MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);

    if (size < 2) {
        if (rank == 0) fprintf(stderr, "use pelo menos 2 processos\n");
        MPI_Finalize();
        return 1;
    }
    if (probe) {
        run_probe(rank, size, w, h, spp, g, seed);
        MPI_Finalize();
        return 0;
    }

    Camera cam;
    setup_camera(w, h, &cam);

    int chunks = nchunks_of(h, g);
    int ntasks = copies * chunks;
    int max_rows = g < h ? g : h;
    int max_nd = max_rows * w * 3;
    size_t pix3 = (size_t)w * (size_t)h * 3;

    double *img = NULL;
    double *buf = (double *)malloc((size_t)max_nd * sizeof(double));
    int *task_of = NULL;
    int *count_of = NULL;
    double *busy_of = NULL;
    char (*hosts)[MPI_MAX_PROCESSOR_NAME] = NULL;
    if (!buf) MPI_Abort(MPI_COMM_WORLD, 1);

    if (rank == 0) {
        img = (double *)calloc((size_t)copies * pix3, sizeof(double));
        task_of = (int *)malloc((size_t)size * sizeof(int));
        count_of = (int *)calloc((size_t)size, sizeof(int));
        busy_of = (double *)calloc((size_t)size, sizeof(double));
        hosts = calloc((size_t)size, MPI_MAX_PROCESSOR_NAME);
        if (!img || !task_of || !count_of || !busy_of || !hosts) MPI_Abort(MPI_COMM_WORLD, 1);
        for (int i = 0; i < size; ++i) task_of[i] = -1;
    }

    char myhost[MPI_MAX_PROCESSOR_NAME];
    int namelen = 0;
    MPI_Get_processor_name(myhost, &namelen);
    MPI_Gather(myhost, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
               hosts, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);

    /* Fora do cronometro: todo mundo pronto, memoria ja' alocada.
     * O mestre le t0 e so' entao entra no recv; os escravos enviam
     * o primeiro pedido em seguida. */
    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = 0, t1 = 0;
    if (rank == 0) t0 = MPI_Wtime();

    if (rank == 0) {
        int next = 0, finished = 0, got = 0;
        MPI_Status st;
        while (finished < size - 1) {
            MPI_Recv(buf, max_nd, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG,
                     MPI_COMM_WORLD, &st);
            int src = st.MPI_SOURCE;
            if (st.MPI_TAG == TAG_RESULTADO) {
                int id = task_of[src];
                int y0, nrows;
                chunk_of(id, h, g, &y0, &nrows);
                int copy = id / chunks;
                double *dst = img + ((size_t)copy * (size_t)h + (size_t)y0) * (size_t)w * 3;
                memcpy(dst, buf, (size_t)nrows * (size_t)w * 3 * sizeof(double));
                got++;
            }
            if (next < ntasks) {
                int id = next++;
                int y0, nrows;
                chunk_of(id, h, g, &y0, &nrows);
                int meta[3] = {y0, nrows, id};
                MPI_Send(meta, 3, MPI_INT, src, TAG_TAREFA, MPI_COMM_WORLD);
                task_of[src] = id;
            } else {
                MPI_Send(NULL, 0, MPI_INT, src, TAG_FIM, MPI_COMM_WORLD);
                finished++;
            }
        }
        t1 = MPI_Wtime();
        if (got != ntasks) {
            fprintf(stderr, "erro: recebidas %d tarefas, esperadas %d\n", got, ntasks);
            MPI_Abort(MPI_COMM_WORLD, 4);
        }
    } else {
        double pedido = 0;
        double busy = 0;
        int done_tasks = 0;
        MPI_Send(&pedido, 1, MPI_DOUBLE, 0, TAG_PEDIDO, MPI_COMM_WORLD);
        while (1) {
            int meta[3];
            MPI_Status st;
            MPI_Recv(meta, 3, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            if (st.MPI_TAG == TAG_FIM) break;
            int y0 = meta[0], nrows = meta[1];
            double tb = MPI_Wtime();
            render_rows(y0, nrows, w, h, spp, seed, &cam, buf);
            busy += MPI_Wtime() - tb;
            done_tasks++;
            MPI_Send(buf, nrows * w * 3, MPI_DOUBLE, 0, TAG_RESULTADO, MPI_COMM_WORLD);
        }
        /* So' responde depois que o mestre ja' fechou t1. Se estes
         * numeros fossem enviados com TAG_RESULTADO durante o laco,
         * o recv com ANY_SOURCE os confundiria com um bloco de pixels. */
        MPI_Recv(&pedido, 1, MPI_DOUBLE, 0, TAG_STATS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Send(&busy, 1, MPI_DOUBLE, 0, TAG_STATS, MPI_COMM_WORLD);
        MPI_Send(&done_tasks, 1, MPI_INT, 0, TAG_STATS, MPI_COMM_WORLD);
    }

    /* Contagem e tempo ocupado chegam depois de t1: nao entram na medida. */
    if (rank == 0) {
        for (int src = 1; src < size; ++src) {
            double go = 0;
            MPI_Send(&go, 1, MPI_DOUBLE, src, TAG_STATS, MPI_COMM_WORLD);
            MPI_Recv(&busy_of[src], 1, MPI_DOUBLE, src, TAG_STATS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&count_of[src], 1, MPI_INT, src, TAG_STATS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        double elapsed = t1 - t0;
        double checksum = checksum_image(img, w, h);
        int ok = copies_match(img, copies, w, h);
        long long pixels = (long long)copies * w * h;
        long long rays = pixels * 4LL * spp;
        printf("run,mpi,%d,%d,%d,%d,%d,%d,%.6f,%.17g,%d,%lld,%lld,%d\n",
               size, g, copies, w, h, spp, elapsed, checksum, ok, pixels, rays, ntasks);
        for (int src = 1; src < size; ++src) {
            printf("balance,%d,%d,%.6f,%s\n", src, count_of[src], busy_of[src], hosts[src]);
        }
        fflush(stdout);
        free(img); free(task_of); free(count_of); free(busy_of); free(hosts);
    }
    free(buf);
    MPI_Finalize();
    return 0;
}
