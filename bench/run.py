#!/usr/bin/env python3
"""Sigma benchmarks vs Node.js, Python and plain C.

    python3 bench/run.py            # run everything, 10 rounds each
    python3 bench/run.py -n 20

For each language this reports:
  * work:  time measured *inside* the program around the benchmark itself
           (what README numbers usually mean), best of N
  * total: wall-clock time of the whole process, startup included, best of N
           (Sigma's binary is compiled once up front and served from the cache)
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SIG = os.path.join(ROOT, "build", "sig")
BENCHES = ["sum", "fib", "words"]


def run_once(cmd):
    start = time.perf_counter()
    out = subprocess.run(cmd, cwd=HERE, capture_output=True, text=True, check=True).stdout
    total = time.perf_counter() - start
    m = re.search(r"(?:loop|time): ([0-9.]+) ms", out)
    return (float(m.group(1)) if m else float("nan")), total * 1000, out.strip()


def best(cmd, rounds):
    works, totals, out = [], [], ""
    for _ in range(rounds):
        w, t, out = run_once(cmd)
        works.append(w)
        totals.append(t)
    return min(works), min(totals), out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-n", type=int, default=10, help="rounds per benchmark")
    args = ap.parse_args()
    if not os.access(SIG, os.X_OK):
        sys.exit("build Sigma first: ./build.sh")

    langs = [("sigma", [SIG, "{b}.sgm"])]
    if shutil.which("node"):
        langs.append(("node", ["node", "{b}.js"]))
    langs.append(("python", [sys.executable, "{b}.py"]))

    c_bin = os.path.join(HERE, ".sum_c")
    if shutil.which("cc") and subprocess.run(["cc", "-O2", "sum.c", "-o", c_bin], cwd=HERE).returncode == 0:
        c_ok = True
    else:
        c_ok = False

    print(f"best of {args.n} runs; work = timed inside the program, total = whole process\n")
    for b in BENCHES:
        print(f"== {b}")
        rows = []
        for name, cmd in langs:
            cmd = [c.format(b=b) for c in cmd]
            if name == "sigma":
                subprocess.run(cmd, cwd=HERE, capture_output=True, check=True)  # warm the compile cache
            rows.append((name,) + best(cmd, args.n))
        if b == "sum" and c_ok:
            rows.append(("c -O2",) + best([c_bin], args.n))
        for name, work, total, out in rows:
            print(f"  {name:<8} work {work:9.3f} ms   total {total:8.1f} ms   | {out}")
        print()
    if c_ok:
        os.remove(c_bin)


if __name__ == "__main__":
    main()
