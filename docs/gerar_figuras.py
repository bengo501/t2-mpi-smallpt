"""diagramas do readme. o texto fica no matplotlib para nao sair torto."""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

OUT = "docs/figuras"


def box(ax, x, y, w, h, text, fc, ec="#1f2933"):
    p = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.08",
                       facecolor=fc, edgecolor=ec, linewidth=1.2)
    ax.add_patch(p)
    ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=9, color="#102027")


def arrow(ax, x1, y1, x2, y2, text=None, color="#334155"):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>", mutation_scale=12,
                                 color=color, linewidth=1.3))
    if text:
        ax.text((x1 + x2) / 2, (y1 + y2) / 2 + 0.12, text, ha="center", va="bottom",
                fontsize=8, color=color)


def protocolo():
    fig, ax = plt.subplots(figsize=(10.2, 3.4))
    ax.set_xlim(0, 12.4)
    ax.set_ylim(0, 3.6)
    ax.axis("off")
    fig.suptitle("quatro mensagens. o escravo começa, e o resultado já pede a próxima linha", fontsize=12, y=0.98)
    passos = [
        (0.25, "1", "escravo livre", "TAG_PEDIDO\n1 double", "#dcfce7"),
        (3.3, "2", "mestre entrega y0", "TAG_TAREFA\n3 inteiros", "#dbeafe"),
        (6.35, "3", "pixels de volta", "TAG_RESULTADO\ntambém é pedido", "#fef3c7"),
        (9.4, "4", "saco vazio", "TAG_FIM\n0 inteiros", "#fee2e2"),
    ]
    for x, n, titulo, corpo, cor in passos:
        circ = plt.Circle((x + 0.28, 2.85), 0.22, facecolor="#1e3a8a", edgecolor="none")
        ax.add_patch(circ)
        ax.text(x + 0.28, 2.85, n, ha="center", va="center", color="white", fontsize=11, fontweight="bold")
        ax.text(x + 0.62, 2.85, titulo, ha="left", va="center", fontsize=10, color="#0f172a")
        box(ax, x, 0.55, 2.7, 1.7, corpo, cor)
    for x in (2.95, 6.0, 9.05):
        arrow(ax, x, 1.4, x + 0.35, 1.4)
    ax.text(6.2, 0.18, "o mestre usa MPI_ANY_SOURCE e guarda a linha em task_of[rank]", ha="center", fontsize=9, color="#334155")
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    fig.savefig(f"{OUT}/protocolo.png", dpi=150, bbox_inches="tight", facecolor="white")
    plt.close()


def saco():
    fig, ax = plt.subplots(figsize=(9.2, 3.2))
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 4)
    ax.axis("off")
    ax.set_title("uma tarefa é uma linha. G = 1 manteve 240 tarefas para 31 escravos", fontsize=12)
    colors = ["#bfdbfe", "#93c5fd", "#60a5fa", "#3b82f6", "#1d4ed8"]
    for i, c in enumerate(colors):
        y = 3.2 - i * 0.55
        ax.add_patch(FancyBboxPatch((0.4, y), 7.2, 0.42, boxstyle="round,pad=0.01,rounding_size=0.05",
                                    facecolor=c, edgecolor="#1e3a8a", linewidth=0.8))
        tom = "#0f172a" if i < 3 else "white"
        ax.text(0.55, y + 0.21, f"linha {i}", va="center", fontsize=8, color=tom)
    ax.text(4.0, 0.35, "240 linhas  ·  cada uma volta para a própria posição", ha="center", fontsize=9)
    box(ax, 8.3, 2.3, 3.2, 1.2, "G = 16\n15 tarefas\n16 escravos\nsem trabalho", "#fee2e2")
    box(ax, 8.3, 0.6, 3.2, 1.2, "G = 1\n240 tarefas\n7 a 9 por escravo\n2,60 s", "#dcfce7")
    fig.tight_layout()
    fig.savefig(f"{OUT}/saco_de_linhas.png", dpi=150, bbox_inches="tight", facecolor="white")
    plt.close()


if __name__ == "__main__":
    protocolo()
    saco()
    print("ok")
