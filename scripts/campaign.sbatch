#!/bin/bash
#SBATCH -J t2smallpt
#SBATCH -N 2
#SBATCH --exclusive
#SBATCH -t 06:00:00
#SBATCH -o results/slurm-%j.out
# campanha unica, sempre nos 2 nos desta alocacao.
# distribuicao block: metade dos ranks em cada no (o mestre fica no primeiro).

set -u
cd "${SLURM_SUBMIT_DIR:-$PWD}"

export PATH=/LADAPPs/OpenMPI/openmpi-4.1.1/bin:$PATH
export LD_LIBRARY_PATH=/LADAPPs/OpenMPI/openmpi-4.1.1/lib:${LD_LIBRARY_PATH:-}

mkdir -p results/raw bin
make clean && make

echo "$SLURM_JOB_NODELIST" | tee results/nodes.txt
srun -N 1 -n 1 bash -c 'hostname; echo ----; lscpu' > results/lscpu.txt

# preenchido depois da calibracao. valores iniciais conservadores.
W=${W:-320}
H=${H:-240}
SPP=${SPP:-64}
REPS=${REPS:-10}
G_LIST=${G_LIST:-"1 4 16"}

SEQ=./bin/smallpt_seq
MPI=./bin/smallpt_mpi
CSV=results/runs.csv
: > "$CSV"

grab() {
    local src="$1" dst="$2"
    grep -E '^(run|balance|probe),' "$src" >> "$dst" || true
}

one_seq() {
    local rep="$1" copies="$2" log="$3"
    srun -N 1 -n 1 "$SEQ" -w "$W" -h "$H" -s "$SPP" -g 1 --copies "$copies" \
        > "$log" 2>> results/raw/stderr.txt
    grab "$log" "$CSV"
}

one_mpi() {
    local p="$1" g="$2" copies="$3" log="$4"
    local extra=()
    if [ "$p" -gt 32 ]; then
        # -O (overcommit) permite mais processos do que CPUs.
        # --oversubscribe e' outra coisa: dividir o no com outro job,
        # e a particao LAD_geral nao permite.
        extra=(-O)
    fi
    srun "${extra[@]}" -N 2 -n "$p" --distribution=block \
        "$MPI" -w "$W" -h "$H" -s "$SPP" -g "$g" --copies "$copies" \
        > "$log" 2>> results/raw/stderr.txt
    grab "$log" "$CSV"
}

echo "=== sequencial de referencia (${REPS} reps) ==="
for r in $(seq 1 "$REPS"); do
    echo "seq rep $r"
    one_seq "$r" 1 "results/raw/seq_r${r}.out"
done

REF_CS=$(awk -F, '/^run,seq,/{print $10; exit}' "$CSV")
echo "checksum de referencia: $REF_CS"

echo "=== granularidade em P=32 ==="
for g in $G_LIST; do
    for r in $(seq 1 "$REPS"); do
        echo "gran P=32 g=$g rep $r"
        one_mpi 32 "$g" 1 "results/raw/gran_g${g}_r${r}.out"
        cs=$(awk -F, '/^run,mpi,/{print $10}' "results/raw/gran_g${g}_r${r}.out" | tail -1)
        ok=$(awk -F, '/^run,mpi,/{print $11}' "results/raw/gran_g${g}_r${r}.out" | tail -1)
        if [ "$cs" != "$REF_CS" ] || [ "$ok" != "1" ]; then
            echo "ERRO de equivalencia g=$g rep=$r cs=$cs ok=$ok" | tee -a results/raw/stderr.txt
            exit 3
        fi
    done
done

G=$(python3 scripts/pick_g.py "$CSV")
echo "$G" | tee results/chosen_g.txt
echo "granularidade escolhida: $G"

echo "=== escalabilidade forte (copies=1, g=$G) ==="
for p in 2 4 8 16 32 64; do
    if [ "$p" = "32" ]; then
        # ja medido na varredura de granularidade
        continue
    fi
    for r in $(seq 1 "$REPS"); do
        echo "forte P=$p rep $r"
        one_mpi "$p" "$G" 1 "results/raw/strong_p${p}_r${r}.out"
        cs=$(awk -F, '/^run,mpi,/{print $10}' "results/raw/strong_p${p}_r${r}.out" | tail -1)
        ok=$(awk -F, '/^run,mpi,/{print $11}' "results/raw/strong_p${p}_r${r}.out" | tail -1)
        if [ "$cs" != "$REF_CS" ] || [ "$ok" != "1" ]; then
            echo "ERRO de equivalencia forte P=$p rep=$r" | tee -a results/raw/stderr.txt
            exit 3
        fi
    done
done

echo "=== escalabilidade fraca (copies=P, g=$G) ==="
for p in 2 4 8 16 32 64; do
    for r in $(seq 1 "$REPS"); do
        echo "fraca P=$p rep $r"
        one_mpi "$p" "$G" "$p" "results/raw/weak_p${p}_r${r}.out"
        cs=$(awk -F, '/^run,mpi,/{print $10}' "results/raw/weak_p${p}_r${r}.out" | tail -1)
        ok=$(awk -F, '/^run,mpi,/{print $11}' "results/raw/weak_p${p}_r${r}.out" | tail -1)
        if [ "$cs" != "$REF_CS" ] || [ "$ok" != "1" ]; then
            echo "ERRO de equivalencia fraca P=$p rep=$r cs=$cs ok=$ok" | tee -a results/raw/stderr.txt
            exit 3
        fi
    done
done

echo "=== probe de comunicacao vs computacao (P=32, g=$G) ==="
for r in $(seq 1 "$REPS"); do
    echo "probe rep $r"
    srun -N 2 -n 32 --distribution=block \
        "$MPI" -w "$W" -h "$H" -s "$SPP" -g "$G" --probe \
        > "results/raw/probe_r${r}.out" 2>> results/raw/stderr.txt
    grab "results/raw/probe_r${r}.out" "$CSV"
done

echo "=== fim ==="
date
