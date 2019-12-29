import os
import sys
from math import log10, ceil
from graphviz import Digraph
import tikzplotlib

import numpy as np
import seaborn as sb
import matplotlib
import matplotlib.pyplot as plt
plt.switch_backend('Agg')

matplotlib.rcParams['font.serif'] = "Times New Roman"
# Then, "ALWAYS use sans-serif fonts"
matplotlib.rcParams['font.family'] = "serif"


PIMmax = 1e8
CPUmax = 1e8

def color(costdiff):
    r = 0
    g = 0
    b = 0
    if costdiff >= 1: # PIM friendly
        r = 255
        g = min([255 * log10(costdiff) / log10(PIMmax), 255])
        g = 255 - ceil(g)
        b = g
    elif costdiff <= -1: # CPU friendly
        costdiff = -costdiff
        r = min([255 * log10(costdiff) / log10(CPUmax), 255])
        r = 255 - ceil(r)
        g = 255
        b = r
    else:
        r = g = b = 255
    return ("#" + "{:02x}".format(r) + "{:02x}".format(g) + "{:02x}".format(b))

def cfgheatmap(cfg, costdiff, decision, lb, ub):
    g = Digraph(name="cfgheatmap")
    for i in range(lb, ub + 1):
        g.node(
            str(i),
            fillcolor=color(costdiff[i]),
            color=("red" if decision[i]=="P" else "green"),
            style="filled",
        )
        for elem in cfg[i]:
            g.edge(str(i), elem)
    g.render()

def heatmap(costdiff, decision, lb, ub):
    N = 10
    #costslice = [0] * (lb % N) + costdiff[lb:ub+1] + [0] * (N - ub % N - 1)
    #decisionslice = [""] * (lb % N) + decision[lb:ub+1] + [""] * (N - ub % N - 1)
    costslice = costdiff[lb:ub+1] + [0] * (N - (ub-lb) % N - 1)
    decisionslice = decision[lb:ub+1] + [""] * (N - (ub-lb) % N - 1)
    #fig, ax = plt.subplots(figsize=(11, 80))
    #fig, ax = plt.subplots(figsize=(8, 3))
    fig, ax = plt.subplots()
    #ax.xaxis.set_tick_params(labeltop="on")

    costslice = [-(log10(abs(i)) if abs(i) > 1 else 0) * np.sign(i) for i in costslice]
    # print(costslice)
    decisionslice = [("" if i != "P" else "PIM") for i in decisionslice]
    costarr = np.reshape(np.array(costslice), (-1, N))
    decisionarr = np.reshape(np.array(decisionslice), (-1, N))
    #ytick = range(lb - lb % N, ub + N - ub % N, N)
    ytick = range(0, int((ub-lb + N - (ub-lb) % N)/N))
    print(costarr)
    print(decisionarr)

    lim = abs(max(costslice, key=abs))
    sb.heatmap(costarr, cmap="RdBu",
        vmin=-lim, vmax=lim, center=0,
        fmt="s", annot=decisionarr,
        # cbar_kws={"ticks": [-7, -6, -3, 0, 3, 6, 7]},
        cbar_kws={"ticks": [-7, 0, 7]},
        yticklabels=ytick,
    )

    cb = ax.collections[-1].colorbar
    cb.ax.yaxis.set_ticks_position("none")
    #cb.ax.set_yticklabels(["CPU friendly","-1e6","-1e3","0","1e3","1e6","PIM friendly"])
    cb.ax.set_yticklabels(["PIM friendly","Neutral","CPU friendly"])

    # plt.title("Each block (X, Y) represents a basic block.", y=-0.15)

    plt.savefig("cfgheatmap.pdf", format="pdf")
    #plt.savefig("cfgheatmap.png", format="png", dpi=600)
    plt.show()

def proc(costfile, lb, ub):
    # plt.rcParams['font.serif'] = "Times New Roman"
    # plt.rcParams['font.family'] = "font.serif"
    # maxbblid = int(cfgfile.readline())
    # cfg = []
    # for line in cfgfile.readlines():
    #     cfg.append(line.split()[1:])
    # print(cfg[lb:ub+1])
    decision = []
    costdiff = []

    # skip unused lines:
    for line in costfile.readlines()[7:]:
        line = line.split()
        print(line)
        decision.append(line[1])
        costdiff.append(float(line[-3]))
    # print(decision)
    # print(costdiff)
    if lb == -1 or ub == -1:
        lb = 0
        ub = len(costdiff) - 1
    # cfgheatmap(cfg, costdiff, decision, lb, ub)
    heatmap(costdiff, decision, lb, ub)


def main(costfilename, lb="-1", ub="-1"):
    with open(costfilename, "r") as costfile:
        proc(costfile, int(lb), int(ub))

if __name__ == "__main__":
    if len(sys.argv) == 2:
        main(sys.argv[1])
    if len(sys.argv) == 3:
        main(sys.argv[1], sys.argv[2])
    if len(sys.argv) == 4:
        main(sys.argv[1], sys.argv[2], sys.argv[3])
