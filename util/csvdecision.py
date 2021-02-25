#!/usr/bin/env python3

import sys, csv
from os import listdir, path
from os.path import isfile, join

def main(mypath):
    costfiles = [f for f in listdir(mypath) if isfile(join(mypath, f))]
    costfiles.sort()
    with open(path.join(mypath, "decision.csv"), "w") as csvfile:
        csvwriter = csv.writer(csvfile, delimiter=",")
        csvwriter.writerow([None, None, "CPU", "PIM", "Reuse", "Switch"])
        for name in costfiles:
            workload = name[:-4]
            rows = [
                [workload, "CPU-only", None, None, None, None],
                [None, "PIM-only", None, None, None, None],
                [None, "MPKI", None, None, None, None],
                [None, "Greedy", None, None, None, None],
                [None, "Opt", None, None, None, None],
            ]
            print(name)
            with open(join(mypath, name), "r") as f:
                line = f.readline().split() # CPU-only
                rows[0][2] = line[4]

                line = f.readline().split() # PIM-only
                rows[1][3] = line[4]

                line = f.readline()
                line = f.readline().split() # MPKI
                rows[2][2] = line[7]
                rows[2][3] = line[10]
                rows[2][4] = line[13]
                rows[2][5] = line[16]

                line = f.readline().split() # Greedy
                rows[3][2] = line[7]
                rows[3][3] = line[10]
                rows[3][4] = line[13]
                rows[3][5] = line[16]

                line = f.readline().split() # Reuse
                rows[4][2] = line[7]
                rows[4][3] = line[10]
                rows[4][4] = line[13]
                rows[4][5] = line[16]

            csvwriter.writerows(rows)

if __name__ == "__main__":
    if len(sys.argv) == 2:
        main(sys.argv[1])
