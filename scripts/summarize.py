"""mediana, minimo e maximo; speed-up e eficiencia. le results/runs.csv."""
import csv
import statistics
import sys
from collections import defaultdict

path = sys.argv[1] if len(sys.argv) > 1 else "results/runs.csv"
runs = defaultdict(list)
balance = defaultdict(list)
for line in open(path, encoding="utf-8"):
    line = line.strip()
    if line.startswith("run,"):
        p = line.split(",")
        key = (p[1], int(p[2]), int(p[3]), int(p[4]), int(p[5]), int(p[6]), int(p[7]))
        runs[key].append((float(p[8]), p[9], p[10], int(p[11]), int(p[12]), int(p[13])))
    elif line.startswith("balance,"):
        p = line.split(",")
        # balance,rank,tasks,busy,host  -- sem P; o arquivo e' cronologico.
        # a campanha grava balance logo apos o run, entao o script de campanha
        # ja separa balance_<P>_<g>_<copies>.csv. aqui so' avisamos.
        balance[p[1]].append(p)

def med(xs):
    return statistics.median(xs)

seq_keys = [k for k in runs if k[0] == "seq" and k[3] == 1]
if not seq_keys:
    sys.exit("falta o sequencial com copies=1")
# um unico tamanho de referencia
ref = seq_keys[0]
t1 = med([t[0] for t in runs[ref]])
print(f"T1 mediana = {t1:.4f} s  checksum = {runs[ref][0][1]}")
print(f"referencia key = {ref}")
print()
print("version P G copies n med min max S E rays_ratio checksum_ok")
for key in sorted(runs, key=lambda k: (k[0], k[3], k[2], k[1])):
    xs = [t[0] for t in runs[key]]
    m, lo, hi = med(xs), min(xs), max(xs)
    rays = runs[key][0][4]
    rays_ref = runs[ref][0][4]
    checks = {t[1] for t in runs[key]}
    ok = all(t[2] == "1" for t in runs[key]) and checks == {runs[ref][0][1]}
    if key[0] == "seq" and key[3] == 1:
        S, E = 1.0, 1.0
    elif key[3] == 1:
        S = t1 / m
        E = S / key[1]
    else:
        S = t1 / m
        E = S  # eficiencia fraca = T1/Tp
    print(f"{key[0]} {key[1]} {key[2]} {key[3]} {len(xs)} {m:.4f} {lo:.4f} {hi:.4f} {S:.3f} {E:.3f} {rays/rays_ref:.3f} {ok}")
