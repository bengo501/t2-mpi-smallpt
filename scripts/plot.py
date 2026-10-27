"""graficos do relatorio: speed-up forte (com reta ideal) e eficiencia forte/fraca."""
import statistics
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

runs = defaultdict(list)
for line in open("results/runs.csv", encoding="utf-8"):
    if not line.startswith("run,"):
        continue
    p = line.strip().split(",")
    key = (p[1], int(p[2]), int(p[3]), int(p[4]))
    runs[key].append(float(p[8]))

seq = [k for k in runs if k[0] == "seq" and k[3] == 1]
t1 = statistics.median(runs[seq[0]])
g_ref = seq[0][2]

strong = sorted(k for k in runs if k[0] == "mpi" and k[3] == 1 and k[2] == g_ref)
weak = sorted(k for k in runs if k[0] == "mpi" and k[3] == k[1] and k[2] == g_ref)

ps = [k[1] for k in strong]
sp = [t1 / statistics.median(runs[k]) for k in strong]
ef_s = [s / p for s, p in zip(sp, ps)]
pw = [k[1] for k in weak]
ef_w = [t1 / statistics.median(runs[k]) for k in weak]

fig, ax = plt.subplots(figsize=(5.2, 3.4))
ax.plot(ps, sp, "o-", label="medido")
ax.plot(ps, ps, "k--", label="ideal S = P")
ax.set_xlabel("processos P")
ax.set_ylabel("speed-up")
ax.set_xticks(ps)
ax.legend()
ax.grid(True, alpha=0.3)
fig.tight_layout()
fig.savefig("results/speedup.pdf")
fig.savefig("results/speedup.png", dpi=140)

fig, ax = plt.subplots(figsize=(5.2, 3.4))
ax.plot(ps, ef_s, "o-", label="forte  S/P")
if pw:
    ax.plot(pw, ef_w, "s-", label="fraca  T1/Tp")
ax.set_xlabel("processos P")
ax.set_ylabel("eficiencia")
ax.set_ylim(0, 1.05)
ax.set_xticks(sorted(set(ps + pw)))
ax.legend()
ax.grid(True, alpha=0.3)
fig.tight_layout()
fig.savefig("results/efficiency.pdf")
fig.savefig("results/efficiency.png", dpi=140)
print("figuras em results/speedup.pdf e results/efficiency.pdf")
