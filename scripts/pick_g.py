"""escolhe a granularidade de menor mediana em P=32, copies=1."""
import statistics
import sys
from collections import defaultdict

path = sys.argv[1]
groups = defaultdict(list)
for line in open(path, encoding="utf-8"):
    if not line.startswith("run,mpi,"):
        continue
    p = line.strip().split(",")
    # run,mpi,P,G,copies,...,time
    if p[2] != "32" or p[4] != "1":
        continue
    groups[int(p[3])].append(float(p[8]))
if not groups:
    sys.exit("nenhuma medicao P=32")
best = min(groups, key=lambda g: statistics.median(groups[g]))
print(best)
