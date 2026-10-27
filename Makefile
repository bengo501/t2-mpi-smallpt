# mesmas flags nas duas versoes. sem -march=native: o binario e' gerado
# no no de login e roda nos nos de compute do mesmo cluster.
CC      ?= gcc
MPICC   ?= mpicc
CFLAGS  ?= -std=c11 -O3 -ffast-math -Wall -Wextra -Wno-unused-parameter
LIBS     = -lm

.PHONY: all clean dirs

all: dirs bin/smallpt_seq bin/smallpt_mpi

dirs:
	mkdir -p bin results results/raw

MPI_INC := $(shell $(MPICC) --showme:compile 2>/dev/null)

bin/smallpt_render.o: src/smallpt_render.c src/smallpt.h src/smallpt_api.h
	$(CC) $(CFLAGS) -c -o $@ src/smallpt_render.c

bin/smallpt_seq: src/smallpt_seq.c src/smallpt_api.h bin/smallpt_render.o
	$(CC) $(CFLAGS) -o $@ src/smallpt_seq.c bin/smallpt_render.o $(LIBS)

bin/smallpt_mpi: src/smallpt_mpi.c src/smallpt_api.h bin/smallpt_render.o
	$(CC) $(CFLAGS) $(MPI_INC) -c -o bin/smallpt_mpi.o src/smallpt_mpi.c
	$(MPICC) -o $@ bin/smallpt_mpi.o bin/smallpt_render.o $(LIBS)

clean:
	rm -rf bin
