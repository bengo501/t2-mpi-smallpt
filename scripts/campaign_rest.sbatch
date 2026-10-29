#!/bin/bash
#SBATCH -J t2rest
#SBATCH -N 2
#SBATCH --exclusive
#SBATCH -w atlantica[03-04]
#SBATCH -t 04:00:00
#SBATCH -o results/slurm-rest-%j.out
# continua a campanha 41169 nos mesmos nos: P=64 da forte, a fraca e o probe.
# a forte com P=64 nao coube sem -O (overcommit).

set -u
cd "${SLURM_SUBMIT_DIR:-$PWD}"

export PATH=/LADAPPs/OpenMPI/openmpi-4.1.1/bin:$PATH
export LD_LIBRARY_PATH=/LADAPPs/OpenMPI/openmpi-4.1.1/lib:${LD_LIBRARY_PATH:-}

W=320
H=240
SPP=64
REPS=10
G=$(cat results/chosen_g.txt)
CSV=results/runs.csv
REF_CS=$(awk -F, '/^run,seq,/{print $10; exit}' "$CSV")
MPI=./bin/smallpt_mpi

echo "G=$G REF=$REF_CS nodes=$SLURM_JOB_NODELIST"

grab() { grep -E '^(run|balance|probe),' "$1" >> "$CSV" || true; }

check_log() {
    local log="$1" what="$2"
    if [ ! -s "$log" ]; then
        echo "ERRO vazio: $what" | tee -a results/raw/stderr.txt
        exit 4
    fi
    local cs ok
    cs=$(awk -F, '/^run,mpi,/{print $10}' "$log" | tail -1)
    ok=$(awk -F, '/^run,mpi,/{print $11}' "$log" | tail -1)
    if [ "$cs" != "$REF_CS" ] || [ "$ok" != "1" ]; then
        echo "ERRO equivalencia $what cs=$cs ok=$ok" | tee -a results/raw/stderr.txt
        exit 3
    fi
}

one_mpi() {
    local p="$1" copies="$2" log="$3"
    local extra=()
    if [ "$p" -gt 32 ]; then extra=(-O); fi
    srun "${extra[@]}" -N 2 -n "$p" --distribution=block \
        "$MPI" -w "$W" -h "$H" -s "$SPP" -g "$G" --copies "$copies" \
        > "$log" 2>> results/raw/stderr.txt
    grab "$log"
}

echo "=== forte P=64 ==="
for r in $(seq 1 "$REPS"); do
    echo "forte P=64 rep $r"
    one_mpi 64 1 "results/raw/strong_p64_r${r}.out"
    check_log "results/raw/strong_p64_r${r}.out" "forte P=64 rep=$r"
done

echo "=== fraca ==="
for p in 2 4 8 16 32 64; do
    for r in $(seq 1 "$REPS"); do
        echo "fraca P=$p rep $r"
        one_mpi "$p" "$p" "results/raw/weak_p${p}_r${r}.out"
        check_log "results/raw/weak_p${p}_r${r}.out" "fraca P=$p rep=$r"
    done
done

echo "=== probe ==="
for r in $(seq 1 "$REPS"); do
    echo "probe rep $r"
    srun -N 2 -n 32 --distribution=block \
        "$MPI" -w "$W" -h "$H" -s "$SPP" -g "$G" --probe \
        > "results/raw/probe_r${r}.out" 2>> results/raw/stderr.txt
    grab "results/raw/probe_r${r}.out"
done

echo "=== fim rest ==="
date
