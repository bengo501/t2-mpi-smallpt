# T2 — path tracer smallpt em mestre/escravo com MPI

Bernardo Klein Heitz (usuário LAD: cp33001)

Versão paralela do path tracer do T1 (smallpt, Kevin Beason, 2008, reescrito em C11). Cada tarefa é um bloco de linhas da imagem. O processo 0 é o mestre e não renderiza.

## Compilar

No Atlântica, o `mpicc` não está no `PATH` do nó de login:

```bash
export PATH=/LADAPPs/OpenMPI/openmpi-4.1.1/bin:$PATH
export LD_LIBRARY_PATH=/LADAPPs/OpenMPI/openmpi-4.1.1/lib:$LD_LIBRARY_PATH
make
```

As duas versões usam as mesmas flags: `-std=c11 -O3 -ffast-math -Wall -Wextra`.

## Rodar

```bash
# sequencial (referência T1), 1 processo, sem MPI
srun --exclusive -N 1 -n 1 ./bin/smallpt_seq -w 320 -h 240 -s 8 -g 1

# paralelo. -g é a granularidade (linhas por mensagem).
srun --exclusive -N 2 -n 8 --distribution=block \
    ./bin/smallpt_mpi -w 320 -h 240 -s 8 -g 4

# escalabilidade fraca: --copies igual ao número de processos
srun --exclusive -N 2 -n 8 --distribution=block \
    ./bin/smallpt_mpi -w 320 -h 240 -s 8 -g 4 --copies 8

# custo de uma tarefa versus ida e volta da mensagem do mesmo tamanho
srun --exclusive -N 2 -n 32 --distribution=block \
    ./bin/smallpt_mpi -w 320 -h 240 -s 8 -g 4 --probe
```

A campanha completa (10 repetições, forte, fraca, granularidade e probe), sempre nos mesmos 2 nós:

```bash
sbatch scripts/campaign.sbatch
```

`W`, `H`, `SPP`, `REPS` e `G_LIST` podem ser passados na submissão, por exemplo `sbatch --export=ALL,W=320,H=240,SPP=8 scripts/campaign.sbatch`.

## Saída

Cada execução imprime uma linha `run,...` com tempo, checksum da primeira imagem, `copies_ok`, pixels, raios primários e número de tarefas. O checksum tem de ser o mesmo da versão sequencial. Linhas `balance,rank,tarefas,tempo_ocupado,host` mostram a carga de cada escravo. O modo `--probe` imprime `probe,...` com RTT no mesmo nó, RTT no outro nó e o tempo de renderizar uma tarefa.

## Reproduzir as tabelas

```bash
python3 scripts/summarize.py results/runs.csv
python3 scripts/plot.py
```

Os tempos brutos de cada execução ficam em `results/raw/` e a saída do Slurm em `results/slurm-*.out`.

## O que cada tag significa

| tag | direção | conteúdo |
|---|---|---|
| `TAG_PEDIDO` (1) | escravo para mestre | 1 double. o escravo está livre |
| `TAG_TAREFA` (2) | mestre para escravo | 3 ints: `y0`, número de linhas, id da tarefa |
| `TAG_RESULTADO` (3) | escravo para mestre | RGB das linhas. também vale como próximo pedido |
| `TAG_FIM` (4) | mestre para escravo | 0 ints. o saco acabou |
| `TAG_STATS` (9) | depois do cronômetro | tarefas feitas e tempo ocupado de cada escravo |

O mestre não manda o índice de volta dentro dos pixels. Como cada escravo segura no máximo uma tarefa, `task_of[rank]` diz onde gravar o bloco (`src/smallpt_mpi.c`).
