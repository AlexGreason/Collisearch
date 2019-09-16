#!/usr/bin/env python3.7
import argparse
import os
import subprocess
from multiprocessing import Process, Queue
from time import perf_counter as clock
import requests

from shinjuku import lt

from shinjuku.search import dijkstra
from shinjuku.transcode import encode_comp, decode_comp, realise_comp

true = set([])


def getsortedsls():
    objs = []
    r = requests.get("https://catagolue.appspot.com/textcensus/b3s23/synthesis-costs")
    lines = r.content.decode().partition("\n")[2].splitlines()
    for (k, line) in enumerate(lines, 1):
        apgcode, cost = [x.strip('"') for x in line.split(",")]
        if not apgcode.startswith("xs"):
            continue  # not a still life
        cost = int(cost)
        if cost == 999999_999999_999999:
            min_paths[apgcode] = (9999, None, None)  # infinity
            cost = 9999
        if cost < 100000_000000_000002:  # pseudo
            pass
        else:
            true.add(apgcode)
            cost -= 100000_000000_000000  # cost according to Cata
        if apgcode not in min_paths:
            min_paths[apgcode] = (cost, None, None)
        sjk_cost = min_paths[apgcode][0]
        if sjk_cost > cost:
            min_paths[apgcode][0] = cost
        objs.append((apgcode, cost))
    objs.sort(key=lambda x: x[1])
    return objs


def cost(apgcode):
    if apgcode in min_paths:
        return min_paths[apgcode][0]
    return -1


def workerfunc(outqueue, inqueue, id):
    os.chdir("./C++")
    proc = subprocess.Popen("./GameOfLife", stdout=subprocess.PIPE, stdin=subprocess.PIPE)
    res = ""
    while True:
        try:
            inqueue.get_nowait()
            return
        except:
            line = proc.stdout.readline().decode("utf-8")
            if line != "\n":
                res += line
            else:
                res = res.replace('\n', '')
                reslist = res.split(' ')
                outqueue.put(reslist + [id])
                res = ""


def getargs():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-d", "--out-dir", help="directory to store synthesis rles in",
                        default="./collisearch_results")
    args = parser.parse_args()
    return args


def makemosaic(synths, targetdir):
    mosaic = lt.pattern()
    nsynths = len(synths)
    for i, synth in enumerate(synths):
        xoff = (i % 50) * 100
        yoff = (i // 50) * 100
        rle = synths[synth][0]
        mosaic += lt.pattern(rle)(xoff, yoff)
    mosaic.write_rle(f"%s/%s.rle" % (targetdir, nsynths))


def backtrack(apgcode, sofar, used_by, min_paths):
    pred = min_paths[apgcode][1]
    if pred is None:
        return
    sofar.add(apgcode)
    if pred not in used_by:
        used_by[pred] = set([])
    for x in sofar:
        used_by[pred].add(x)
    backtrack(pred, sofar, used_by, min_paths)


def getuses(min_paths):
    used_by = {}
    i = 0
    for code in min_paths:
        backtrack(code, set([]), used_by, min_paths)
        i += 1
    return used_by


def printuses(used_by, apgcode):
    if apgcode not in used_by:
        return "no uses"
    uses = used_by[apgcode]
    return f"used by {len(uses)}, first 5 uses: {list(uses)[:5]}"


if __name__ == "__main__":
    min_paths = dijkstra()
    objs = getsortedsls()
    costfile = open("./C++/costs.txt", "w")
    for x in objs:
        # print(f"{x[0]} {x[1]}")
        costfile.write(f"{x[0]} {x[1]}\n")
    used_by = getuses(min_paths)
    compfile = open(
        "./collisearch_out.sjk",
        "a")
    compfile.write("\n")
    args = getargs()
    nthreads = 6

    resqueue = Queue()
    workqueue = Queue()
    arguments = [resqueue, workqueue]
    workers = [Process(target=workerfunc, args=arguments + [i]) for i in range(nthreads)]
    upto = [0 for i in range(nthreads)]
    starttime = clock()
    [w.start() for w in workers]

    synths = {}
    prevmax = 0
    try:
        while True:
            synth = resqueue.get()
            prevcost = 9999
            if len(synth) == 6:
                upto[synth[5]] = int(synth[4])
                apgcode = synth[0]
                if apgcode in synths:
                    prevcost = synths[apgcode][1]
                else:
                    prevcost = min(prevcost, int(synth[1]))
                if int(synth[2]) < prevcost:
                    comp = encode_comp(synth[3])
                    #input, output, cost = decode_comp(comp)
                    print(comp)
                    compfile.write(comp + "\n")
                    compfile.flush()
                    truestr = "true" if apgcode in true else "pseudo"
                    filename = f"%spattern.rle" % os.getpid()
                    msg = f"Object {synth[0]} ({truestr}) (previous cost {prevcost}, this cost {synth[2]}) " \
                          f"({printuses(used_by, synth[0])}) produced by the following collision: \n{lt.pattern(synth[3]).rle_string(filename=filename)}"
                    print(msg)
                    synths[apgcode] = (synth[3], int(synth[2]))
                    print(f"%s total synths\n" % len(synths))
                    makemosaic(synths, args.out_dir)
            else:
                upto[synth[1]] = int(synth[0])
            if sum(upto) >= prevmax + 10000:
                prevmax = sum(upto)
                print(f"tested {sum(upto)} in %.2f seconds, per-thread {upto}\n" % (clock() - starttime,))
    except KeyboardInterrupt:
        [workqueue.put("terminate") for w in workers]
        [w.terminate() for w in workers]
        raise KeyboardInterrupt
