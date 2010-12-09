#!/usr/bin/python

import os, sys

i = 1
if i >= len(sys.argv):
    raise "no filename given"
fname = sys.argv[i]

dname = os.path.dirname(fname)
bname = os.path.basename(fname)
sys.path = [dname] + sys.path
if bname.endswith('.py'):
    bname = bname[0:-3]
m = __import__(bname)

G_outs = {}
G_outbytes = {}
G_cycles = {}

old_to_new = {}

taskid = 0
ntasks = 0

for node in m.task:
    name = node[0]
    old_to_new[name] = str(taskid)
    G_outs[str(taskid)] = []
    taskid = taskid + 1

ntasks = taskid

for node in m.task:
    newname = old_to_new[node[0]]
    i = 1
    while i < len(node):
        if node[i] == 'N':
            for oldpname in node[i + 1]:
                parentname = old_to_new[oldpname]
                G_outs[parentname].append(newname)
            i += 2
        elif node[i] == 'OUT':
            G_outbytes[newname] = int(node[i + 1])
            i += 2
        elif node[i] == 'OPS':
            G_cycles[newname] = int(node[i + 1][0][1][0])
            i += 2
        else:
            i += 1

# sys.stderr.write("outs %s\n" %(str(G_outs)))
# sys.stderr.write("outbytes %s\n" %(str(G_outbytes)))
# sys.stderr.write("outcycles %s\n" %(str(G_cycles)))

out = sys.stdout
out.write('tasks\n')
out.write('\ttask_list %d\n' %(ntasks))
for i in xrange(ntasks):
    out.write('\t\ttask\t%d out ' %(i))
    nouts = len(G_outs[str(i)])
    if nouts > 0:
        out.write('1 %d %d ' %(G_outbytes[str(i)], nouts))
        for j in xrange(nouts):
            out.write('%s ' %(G_outs[str(i)][j]))
    else:
        out.write('0 ')
    out.write('weight %d\n' %(G_cycles[str(i)]))
out.write('\tdefault_mapping\t0\n')
out.write('\tmapping_list 2\n')
out.write('\t\tmap 0 0 map %s 0\n' %(old_to_new['main_result_0']))
out.write('\tstatic_list 2 0 %s\n' %(old_to_new['main_result_0']))
