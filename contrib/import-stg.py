#!/usr/bin/python

import math, os, random, sys

def error(s):
    sys.stderr.write('ERROR: %s\n' %(s))
    sys.exit(-1)

output_mode = None
output_mean = None
output_std = None
comp_mult = None

i = 1
while i < len(sys.argv):
    if sys.argv[i] == '-c':
        if (i + 1) >= len(sys.argv):
            error("you must give a constant with -c")
        output_mode = 'normal'
        output_mean = float(sys.argv[i + 1])
        output_std = float(0)
        i += 2
        continue
    elif sys.argv[i] == '-n':
        if (i + 2) >= len(sys.argv):
            error("you must give normal variate parameters with -n")
        output_mode = 'normal'
        output_mean = float(sys.argv[i + 1])
        output_std = float(sys.argv[i + 2])
        i += 3
        continue
    elif sys.argv[i] == '-m':
        if (i + 1) >= len(sys.argv):
            error("you must give a multiplier with -m")
        comp_mult = float(sys.argv[i + 1])
        i += 2
        continue
    break

if output_mode == None:
    error("output mode must be specified")
if output_mode == 'normal':
    if output_mean == None or output_std == None:
        error("normal variate parameters not given")
if comp_mult == None:
    error("you must provide a multiplier with -m")

if i >= len(sys.argv):
    error("no filename given")
fname = sys.argv[i]
f = open(fname, 'r')

G_outs = {}
G_outbytes = {}
G_cycles = {}
old_to_new = {}

tnumb = 0

line = f.readline()
line = line.strip()
l = line.split()
ntasks = int(l[0]) + 2
if l <= 0:
    error("ntasks is non-positive")

while True:
    line = f.readline()
    if len(line) == 0:
        break
    if line[0] == '#':
        continue
    line = line.strip()
    if len(line) == 0:
        error("white space line detected")
    l = line.split()
    i = 0
    if len(l) < 3:
        error("too short an stg line")
    old_to_new[l[0]] = tnumb
    taskname = l[0]
    if not G_outs.has_key(taskname):
        G_outs[taskname] = []
    cycles = int(l[1])
    if cycles < 0:
        error("cycles must be non-negative")
    if cycles == 0:
        cycles = 1
    G_cycles[taskname] = cycles
    tnumb += 1
    ndeps = int(l[2])
    if ndeps < 0:
        error("ndeps must be non-negative")
    if len(l) != (3 + ndeps):
        error("stg line has wrong number of elements")
    i = 3
    while i < len(l):
        deptaskname = l[i]
        if not G_outs.has_key(deptaskname):
            G_outs[deptaskname] = []
        G_outs[deptaskname].append(taskname)
        i += 1

if len(old_to_new.keys()) != ntasks:
    error("not enough tasks")

for taskname in old_to_new.keys():
    if len(G_outs[taskname]) == 0:
        G_outbytes[taskname] = -1
        continue
    bytes = random.normalvariate(output_mean, output_std)
    if bytes < 1.0:
        bytes = 1.0
    G_outbytes[taskname] = int(math.floor(bytes))

out = sys.stdout
out.write('tasks\n')
out.write('\ttask_list %d\n' %(ntasks))
for i in xrange(ntasks):
    out.write('\t\ttask\t%d out ' %(i))
    nouts = len(G_outs[str(i)])
    if nouts > 0:
        out.write('1 %d %d ' %(G_outbytes[str(i)], nouts))
        for j in xrange(nouts):
            out.write('%s ' %(old_to_new[G_outs[str(i)][j]]))
    else:
        out.write('0 ')
    out.write('weight %d\n' %(int(comp_mult * G_cycles[str(i)])))
out.write('\tdefault_mapping\t0\n')
out.write('\tmapping_list 2\n')
out.write('\t\tmap 0 0 map %d 0\n' %(ntasks - 1))
out.write('\tstatic_list 2 0 %d\n' %(ntasks - 1))
