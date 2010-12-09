#!/usr/bin/env python

import copy
import getopt
import math
import struct
import sys
from sys import argv

if __name__ == '__main__':
    # Import Psyco if available
    try:
        import psyco
        psyco.full()
    except ImportError:
        pass


def assertion(condition, msg):
    if condition == False:
        error(msg)


def debug(msg):
    sys.stderr.write(msg)


def error(reason):
    sys.stderr.write('error: %s' %(reason))
    raise 'foo'


def debug_plot(datafilename):
    datafile = open(datafilename, 'r')
    vec = []
    while True:
        line = datafile.readline().strip()
        if len(line) == 0:
            break
        fields = line.strip().split()
        if len(fields) != 2:
            continue
        if fields[0] != 'accepted_objective:':
            continue
        vec.append(float(fields[1]))
    simple_plot(sys.stdout, vec)


def get_gain_by_t(record, T):
    costlist = record['best_cost_so_far']
    cll = len(costlist)
    assert(cll > 0)
    gainvalue = None
    for j in xrange(cll):
        if costlist[j][0] < T:
            initial_cost = float(record['initial_objective'])
            gainvalue = initial_cost / costlist[j - 1][1]
            break
    if gainvalue == None:
        gainvalue = float(record['gain'])
    return gainvalue


def valuesort(values):
    sortpairs = zip(map(int, values), values)
    sortpairs.sort()
    return map(lambda x: x[1], sortpairs)


def mean_and_standard_deviation(l):
    s = 0.0
    mean = sum(l) / len(l)
    for v in l:
        s += pow(v - mean, 2)
    s = s / len(l)
    s = math.sqrt(s)
    return (mean, s)


def gen_matrix(optdict, records):
    format = optdict['matrix']
    fields = format.split(',')
    assertion(len(fields) == 3, 'Want 3 fields for gen_matrix: %s' %(format))
    xlabel = fields[0]
    ylabel = fields[1]
    resultlabel = fields[2]

    T = optdict.get('t')
    if T != None:
        assert(resultlabel == 'gain')
        T = float(T)

    if xlabel == 't':
        assert(T == None)
        assert(resultlabel == 'gain')

    xvalues = {}
    yvalues = {}
    values = {}

    nrecords = 0

    for record in records:
        if xlabel == 't':
            tlist = map(lambda p: p[0], record['best_cost_so_far'])
            valuepairlist = zip(tlist, tlist)
        else:
            xvalue = record.get(xlabel)
            if xvalue == None:
                sys.stderr.write('Matrix is based on %d records\n' %(nrecords))
                break
            valuepairlist = [(xvalue, None)]

        yvalue = record[ylabel]

        if ylabel == 'optimization_method':
            yvalue = {'simulated_annealing' : 1,
                      'simulated_annealing_autotemp' : 2,
                      'fast_hybrid_gm_sa' : 3,
                      'fast_hybrid_gm_sa_autotemp' : 4,
                      'slow_hybrid_gm_sa' : 5,
                      'slow_hybrid_gm_sa_autotemp' : 6,
                      'iterated_simulated_annealing' : 7, 
                      'iterated_simulated_annealing_autotemp' : 8,
                      'group_migration' : 9,
                      'random_mapping' : 10,
                      'optimal_subset_mapping' : 11,
                      } [yvalue]

        yvalues[yvalue] = True

        for valuepair in valuepairlist:

            (xvalue, T) = valuepair

            nrecords += 1
            xvalues[xvalue] = True

            if values.has_key(xvalue) == False:
                values[xvalue] = {}

            xd = values[xvalue]
            yl = xd.get(yvalue)

            if yl == None:
                yl = []
                xd[yvalue] = yl

            if T == None:
                resultvalue = float(record[resultlabel])
            else:
                resultvalue = get_gain_by_t(record, T)

            yl.append(resultvalue)


    deviations = {}

    for xvalue in values:
        xd = values[xvalue]
        for yvalue in xd:
            l = xd[yvalue]
            #sys.stderr.write('%s,%s: %s\n' %(str(xvalue), str(yvalue), str(l)))
            (mean, std) = mean_and_standard_deviation(l)
            xd[yvalue] = mean
            deviations[(xvalue, yvalue)] = std

    xvalues = valuesort(xvalues.keys())
    yvalues = valuesort(yvalues.keys())

    sys.stdout.write('rows = [')
    for xvalue in xvalues:
        sys.stdout.write('%e, ' %(float(xvalue)))
    sys.stdout.write('];\n')

    sys.stdout.write('columns = [')
    for yvalue in yvalues:
        sys.stdout.write('%e, ' %(float(yvalue)))
    sys.stdout.write('];\n')

    # write mean
    sys.stdout.write('A = [\n')
    for xvalue in xvalues:
        for yvalue in yvalues:
            if values[xvalue].has_key(yvalue) == False:
                out = 0.0
            else:
                out = values[xvalue][yvalue]
            sys.stdout.write('%e ' %(out))
        sys.stdout.write('\n')
    sys.stdout.write('];\n')

    # write standard deviation
    sys.stdout.write('stdA = [\n')
    for xvalue in xvalues:
        for yvalue in yvalues:
            out = deviations.get((xvalue, yvalue))
            if out == None:
                out = 0.0
            sys.stdout.write('%e ' %(out))
        sys.stdout.write('\n')
    sys.stdout.write('];\n')


def gen_plot(plotfilename, records, fieldname):
    fieldindex = {'memory' : 0, 'objective' : 1, 'time' : 2} [fieldname]

    vectors = grab_data(records, fieldindex)

    minsize = 0
    have_safemin = False
    for v in vectors:
        minsize = max(minsize, len(v))
        if have_safemin == True:
            safemin = min(safemin, v[0][fieldindex])
        else:
            safemin = v[0][fieldindex]
            have_safemin = True

    outf = open(plotfilename, 'w')

    outf.write('minvec = [')
    minindex = 0
    for i in xrange(minsize):
        oldmin = safemin
        for v in vectors:
            if i < len(v):
                safemin = min(safemin, v[i][fieldindex])
        if safemin < oldmin:
            minindex = i
        outf.write('%e, ' %(safemin))
    outf.write('];\n')

    i = 0
    for v in vectors:
        outf.write('x%d = [' %(i))
        for valuetriple in v:
            outf.write('%e, ' %(valuetriple[fieldindex]))
        outf.write('];\n')
        i += 1

    outf.write('plot(minvec, \'k\', [%d], [%f], \'xr\', ' %(minindex + 1, safemin * 0.97))
    colordict = {0 : 'r', 1 : 'g', 2 : 'b'}
    for i in xrange(len(vectors)):
        outf.write('x%d, \'%s\'' %(i, colordict[i % 3]))
        if i < (len(vectors) - 1):
            outf.write(', ')
    outf.write(')\n')
    outf.close()


def gen_summary(summaryfilename, records, fieldname):
    fieldindex = {'memory' : 0, 'objective' : 1, 'time' : 2} [fieldname]

    gains = []
    time_gains = []
    memory_gains = []
    meanccr = 0.0
    optimization_time = 0.0
    meanedges = 0.0

    minindexes = []
    minimums = []
    meaniterations = 0

    for i in xrange(len(records)):
        record = records[i]

        if fieldname == 'objective':
            value = record['gain']
        elif fieldname == 'time':
            value = record['time_gain']
        elif fieldname == 'memory_gain':
            value = record['memory_gain']
        else:
            error('Illegal fieldname %s\n' %(fieldname))
        gains.append(float(value))
        time_gains.append(float(record['time_gain']))
        memory_gains.append(float(record['memory_gain']))
        meanccr += float(record['ccr'])
        optimization_time += float(record['optimization_time'])
        if record.has_key('edges'):
            meanedges += float(record['edges'])

        inf = open(record['data_file'], 'r')
        bytes = inf.read()
        inf.close()
        v = grab_vector(bytes)
        minindex = 0
        minimum = v[0][fieldindex]
        meaniterations += len(v)
        for i in xrange(len(v)):
            value = v[i][fieldindex]
            if value < minimum:
                minimum = value
                minindex = i
        minindexes.append(minindex)
        minimums.append(minimum)

    meanccr = meanccr / len(records)
    mean_optimization_time = optimization_time / len(records)
    meanedges = meanedges / len(records)
    meaniterations = meaniterations / len(records)

    #debug('%s\n' %(str(gains)))
    #debug('%s\n' %(str(minindexes)))
    #debug('%s\n' %(str(minimums)))

    s = 'mean/meani/median/mediani/timegain/memorygain/meaniters/meanccr/meanedges/time: %f %d %f %d %f %f %d %f %f %f\n' \
        %(mean(gains), mean(minindexes), median(gains), median(minindexes), mean(time_gains), mean(memory_gains), meaniterations, meanccr, meanedges, mean_optimization_time)
    #debug(s)

    outf = open(summaryfilename, 'w')
    outf.write(s)
    outf.close()


def grab_data(records, fieldindex):
    vectors = []
    assertion(len(records) > 0, 'Must have \>0 records in grab_data\n')
    for record in records:
        inf = open(record['data_file'], 'r')
        bytes = inf.read()
        inf.close()
        v = grab_vector(bytes)
        vectors.append(v)

    return vectors


def grab_vector(bytes):
    v = []
    idx = 0
    fmt = 'Lff'
    ssize = struct.calcsize(fmt)
    while idx < len(bytes):
        l = struct.unpack(fmt , bytes[idx:idx + ssize])
        v.append(l)
        idx += ssize
    if idx != len(bytes):
        error('byte size is not divisible with struct size: %d\n' %(ssize))
    return v


def parse_result_file(datafile):
    records = []
    accumulating = False

    while True:
        line = datafile.readline().strip()
        if len(line) == 0:
            if accumulating == True:
                records.append(accumulator)
            break
        fields = line.split()
        marker = fields[0]
        if marker.endswith(':'):
            marker = marker[0:-1]
        if marker == 'MARK':

            if accumulating == True:
                records.append(accumulator)

            accumulator = {}
            accumulating = True
            marker = 'graph-file'

        if accumulating == False:
            error('must accumulate (missing MARK?)\n')

        if len(fields) == 2:
            accumulator[marker] = fields[1]
        else:
            if marker == 'best_cost_so_far':
                if accumulator.has_key(marker) == False:
                    accumulator[marker] = []
                # pair = (temperature, cost)
                pair = (float(fields[1]), float(fields[2]))
                accumulator[marker].append(pair)
            else:
                accumulator[marker] = fields[1:]

    debug('got %d records\n' %(len(records)))

    # Finalise records: compute relative gain
    for record in records:
        record['relgain'] = float(record['gain']) / int(record['evaluations'])

    return records


def mean(vector):
    return sum(vector) / len(vector)


def median(vector):
    v = copy.copy(vector)
    v.sort()
    l = len(v)
    assertion(l > 0, 'Zero length vector given for median.\n')
    if l % 2 == 0:
        return 0.5 * (v[l / 2 - 1] + v[l / 2])
    return v[l / 2]


def select_records(records, criteria):
    selected = records
    i = 0
    while (i + 2) <= len(criteria):
        newselected = []
        key = criteria[i]
        for record in selected:
            value = record.get(key)
            if value != None:
                alternatives = criteria[i + 1].split('/')
                for alt in alternatives:
                    if value == alt:
                        newselected.append(record)
                        break
        selected = newselected
        debug('%s %d\n' %(key, len(selected)))
        i += 2
    return selected


def simple_plot(outfile, vec, vname='vec'):
    outfile.write('%s = [' %(vname))
    counter = 0
    for value in vec:
        outfile.write('%.3e, ' %(value))
        counter = (counter + 1) % 8
#        if counter == 0:
#            outfile.write('\\\n')
    outfile.write('];\n')
    outfile.write('[mini, ind] = min(%s);\n' %(vname))
    outfile.write('plot(1:%d, %s, \'r\', [ind], [0.9*mini], \'bx\')\n' %(len(vec), vname))
    outfile.write('xlabel(\'Iteration\')\n')
    outfile.write('ylabel(\'Objective value\')\n')


try:
    [options, argsleft] = getopt.getopt(sys.argv[1:], '', ['data_file=', 'debug-file', 'filter', 'graph-file=', 'matrix=', 'max=', 'ntasks=', 'objective_function=', 'optimization_method=', 'pes=', 'plot=', 'sa_heuristics=', 'summary=', 't='])
except getopt.GetoptError:
    error('unknown option\n')
except:
    error('getopt error\n')

debug('printing options:\n')

optdict = {}
criteria = []

for opt in options:
    debug('%s\n' %(str(opt)))
    key = opt[0]
    if key.startswith('--'):
        key = key[2:]
    optdict[key] = opt[1]
    if key in ['debug-file', 'max', 'plot', 'filter', 'summary', 'matrix', 't']:
        continue
    criteria.append(key)
    criteria.append(opt[1])

if len(argsleft) != 1:
    error('expecting result file given\n')

if optdict.has_key('debug-file'):
    debug_plot(argsleft[0])
    sys.exit(0)

try:
    datafile = open(argsleft[0], 'r')
except:
    error('can not open datafile: %s\n' %(argsleft[0]))

records = parse_result_file(datafile)
newrecords = select_records(records, criteria)

if optdict.has_key('max'):
    newrecords = newrecords[0 : int(optdict['max'])]

debug('selected %d records\n' %(len(newrecords)))

if len(newrecords) == 0:
    error('no data sets were selected\n')

if optdict.has_key('filter'):
    keys = newrecords[0].keys()
    keys.sort()
    for record in newrecords:
        for key in keys:
            print key, record[key]

if optdict.has_key('plot'):
    gen_plot(optdict['plot'], newrecords, 'objective')

if optdict.has_key('summary'):
    gen_summary(optdict['summary'], newrecords, 'objective')

if optdict.has_key('matrix'):
    gen_matrix(optdict, newrecords)
