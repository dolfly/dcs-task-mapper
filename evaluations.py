#!/usr/bin/env python

import math
import re
import string
import sys

# Inteconnect area for one PE. In multiprocessor system this value is
# multiplied with the number of PEs to get the total IC area.
ic_per_pe_area = 0.001 * 0.001 / 10

sa_methods = {'fast_hybrid_gm_sa' : 0, 'fast_hybrid_gm_sa_autotemp' : 0,
              'iterated_simulated_annealing' : 0, 'iterated_simulated_annealing_autotemp' : 0,
              'simulated_annealing' : 0, 'simulated_annealing_autotemp' : 0,
              'slow_hybrid_gm_sa' : 0, 'slow_hybrid_gm_sa_autotemp' : 0,
              'simulated_annealing_autotemp3' : 0}


# Classes to filter result data. These are used with Python filter() operation.

class Acceptor_Filter:
    """ Simulated annealing acceptor function is filtered. Typical values
    are exponential and original. """

    def __init__(self, acceptor):
        self.acceptor = acceptor

    def filter(self, ctx):
        return ctx.acceptor == self.acceptor

class Edge_Filter:
    def __init__(self, edges):
        edges = int(edges)
        assert(edges > 0)
        self.edges = edges

    def filter(self, ctx):
        return ctx.edges == self.edges

class Graph_Filter:
    def __init__(self, graph):
        assert(len(graph) > 0)
        self.graphs = graph.split(':')

    def filter(self, ctx):
        return ctx.graph in self.graphs

class Graph_Regex_Filter:
    def __init__(self, regex):
        self.re = re.compile(regex)

    def filter(self, ctx):
        return self.re.search(ctx.graph) != None

class k_Filter:
    def __init__(self, k):
        k = float(k)
        assert(k >= 0.0)
        self.k = k

    def filter(self, ctx):
        return ctx.power_k == self.k

class L_Filter:
    def __init__(self, L):
        L = float(L)
        assert(L > 0)
        self.L = L

    def filter(self, ctx):
        return ctx.L == self.L

class Method_Filter:
    def __init__(self, method):
        assert(len(method) > 0)
        self.method = method

    def filter(self, ctx):
        return ctx.method == self.method

class PE_Filter:
    def __init__(self, pes):
        assert(pes > 0)
        self.pes = pes

    def filter(self, ctx):
        return ctx.pes == self.pes

comparators = {'eq': lambda x, y: x == y,
               'lt': lambda x, y: x < y,
               'le': lambda x, y: x <= y,
               'gt': lambda x, y: x > y,
               'ge': lambda x, y: x >= y,
               'ne': lambda x, y: x != y,
              }

def xint(s, errvalue=None):
    try:
        x = int(s)
    except ValueError:
        x = errvalue
    return x

class Value_Filter:
    def __init__(self, expr):
        assert(len(expr) >= 3)
        if expr[0].isdigit():
            self.comp = comparators['eq']
            self.value = xint(expr)
        else:
            self.comp = comparators.get(expr[0:2])
            self.value = xint(expr[2:])
        assert(self.comp != None)
        assert(self.value != None)

    def filter(self, ctx):
        assert False

class IC_Freq_Filter(Value_Filter):
    def __init__(self, expr):
        Value_Filter.__init__(self, expr)

    def filter(self, ctx):
        return self.comp(ctx.ic_freq, self.value)

class PE_Freq_Filter(Value_Filter):
    def __init__(self, expr):
        Value_Filter.__init__(self, expr)

    def filter(self, ctx):
        for pedata in ctx.pedatas:
            if not self.comp(pedata[0], self.value):
                return False
        return True

class Task_Filter:
    def __init__(self, ntasks):
        assert(ntasks > 0)
        self.ntasks = ntasks

    def filter(self, ctx):
        return ctx.ntasks == self.ntasks

class Elitism_Filter:
    def __init__(self, elitism):
        assert(elitism >= 0)
        self.elitism = elitism

    def filter(self, ctx):
        return ctx.elitism == self.elitism

class Population_Filter:
    def __init__(self, population):
        assert(population > 0)
        self.population = population

    def filter(self, ctx):
        return ctx.population == self.population


class Chromosome_Mutation_Filter:
    def __init__(self, chromosome_mutation):
        assert(chromosome_mutation >= 0.0 and chromosome_mutation <= 1.0)
        self.chromosome_mutation = chromosome_mutation

    def filter(self, ctx):
        return math.fabs(ctx.chromosome_mutation - self.chromosome_mutation) < 0.0000001


class Gene_Mutation_Filter:
    def __init__(self, gene_mutation):
        assert(gene_mutation >= 0.0 and gene_mutation <= 1.0)
        self.gene_mutation = gene_mutation

    def filter(self, ctx):
        return math.fabs(ctx.gene_mutation - self.gene_mutation) < 0.0000001


# Result_Context holds data for a single optimization run

class Result_Context:
    def __init__(self):
        self.acceptor = None
        self.best_cost = []
        self.best_objective = None
        self.best_time = None
        self.edges = None
        self.evals = []
        self.graph = None
        self.initial_objective = None
        self.initial_time = None
        self.gains = []
        self.method = None
        self.ntasks = None
        self.pedatas = []
        self.pes = None
        self.sa = False
        self.saved = 0
        self.total_evals = None
        self.text_data = []
        self.utilizations = []
        self.ic_utilizations = []
        self.ic_freq = None
        self.ic_areas = []
        self.power_k = 0.0

        self.elitism = None
        self.population = None
        self.chromosome_mutation = None
        self.gene_mutation = None


    def __str__(self):
        return '%d costs %d tasks %d pes %d totalevals' %(len(self.best_cost), self.ntasks, self.pes, self.total_evals)


    def finalize(self):

        N = len(self.best_cost)
        assert(N > 0)

        if self.sa:
            if self.L < 0:
                self.L = self.ntasks * (self.pes - 1)
                assert(self.L > 0)

            x = self.total_evals / len(self.best_cost)

            # Generate number list of evaluations: x, 2x, 3x, ..., Nx
            self.evals = range(x, x * (N + 1), x)

        # Optimize redundant cost values away
        evals = []
        best_cost = []
        old_cost = old_eval = None

        for i in xrange(len(self.best_cost)):
            new_cost = self.best_cost[i]
            new_eval = self.evals[i]
            if new_cost != old_cost:
                best_cost.append(new_cost)
                evals.append(new_eval)
            old_cost = new_cost
            old_eval = new_eval

        self.best_cost = best_cost
        self.evals = evals
        self.saved = N - len(self.best_cost)

        # Compute gain values
        assert(self.initial_objective != None)

        for v in self.best_cost:
            self.gains.append(self.initial_objective / v)


    def add_cost(self, iterations, value):
        if iterations != None:
            self.evals.append(iterations)

        self.best_cost.append(value)


    def add_data(self, s):
        self.text_data.append(s)


    def update(self, label, values):
        if label == 'best_cost_so_far':
            self.add_cost(None, float(values[1]))

        elif label == 'best_ga_cost_so_far':
            self.add_cost(int(values[3]), float(values[0]))

        elif label == 'best_gm_cost_so_far':
            self.add_cost(int(values[1]), float(values[2]))

        elif label == 'best_osm_cost_so_far':
            self.add_cost(int(values[2]), float(values[3]))

        elif label == 'best_random_cost_so_far':
            self.add_cost(int(values[0]), float(values[1]))

        elif label == 'best_sa_cost_so_far':
            self.add_cost(int(values[1]), float(values[2]))

        elif label == 'sa_acceptor':
            self.acceptor = values[0]

        elif label == 'edges':
            self.edges = int(values[0])
            assert(self.edges > 0)

        elif label == 'evaluations':
            self.total_evals = int(values[0])
            assert(self.total_evals > 0)

        elif label == 'initial_objective':
            self.initial_objective = float(values[0])
            assert(self.initial_objective > 0)

        elif label == 'initial_time':
            self.initial_time = float(values[0])
            assert(self.initial_time > 0.0)

        elif label == 'best_objective':
            self.best_objective = float(values[0])
            assert(self.best_objective > 0.0)

        elif label == 'best_time':
            self.best_time = float(values[0])
            assert(self.best_time > 0.0)

        elif label == 'MARK':
            self.graph = values[0]

        elif label == 'ntasks':
            assert(len(values) == 1)
            self.ntasks = int(values[0])
            assert(self.ntasks > 0)

        elif label == 'optimization_method':

            global sa_methods

            self.method = values[0]

            if self.method in ['group_migration', 'optimal_subset_mapping',
                               'random_mapping', 'genetic_algorithm']:
                return
            if sa_methods.has_key(self.method):
                self.sa = True
                return

            sys.stderr.write('Unknown method: %s\n' %(self.method))
            sys.exit(1)

        elif label == 'pe':
            freq = int(values[3])
            assert(freq > 0)
            perf = float(values[5])
            assert(perf > 0)
            area = float(values[7])
            assert(area > 0)
            self.pedatas.append((freq, perf, area))

        elif label == 'pes':
            self.pes = int(values[0])
            assert(self.pes > 0)

        elif label == 'sa_schedule_max':
            self.L = int(values[0])

        elif label == 'interconnect':
            freq = int(values[3])
            assert(freq > 0)

            if self.ic_freq != None and self.ic_freq != freq:
                sys.stderr.write('Non-symmetric buses\n')
                sys.exit(1)

            if len(values) >= 10:
                area = float(values[5])
            else:
                global ic_per_pe_area

                area = float(ic_per_pe_area * len(self.pedatas))

            self.ic_freq = freq
            self.ic_areas.append(area)

        elif label == 'ic_utilisation' or label == 'ic_utilisations':
            for i in range(len(self.ic_areas)):
                u = float(values[i])
                assert(u >= 0.0 and u <= 1.0)
                self.ic_utilizations.append(u)

        elif label == 'pe_utilisations':
            for value in values:
                util = float(value)
                assert(util >= 0 and util <= 1)
                self.utilizations.append(util)

        elif label == 'power_k':
            self.power_k = float(values[0])
            assert(self.power_k >= 0.0)

        elif label == 'population_size':
            self.population = int(values[0])

        elif label == 'elitism':
            self.elitism = int(values[0])

        elif label == 'chromosome_mutation_probability':
            self.chromosome_mutation = float(values[0])

        elif label == 'gene_mutation_probability':
            self.gene_mutation = float(values[0])


def count_duplicity(l):
    """count_duplicity(list) -> list

    Counts the number of unique elements in a given list, and returns the
    count and the associated element in a new list.
    Given a list, return a new list that consists of pairs (n, x) where
    n is the number of instances of x in the original list. The order of
    appearance in the original list is not preserved for the final list."""

    if len(l) == 0:
        return []

    l = list(l)
    l.sort()
    u = []
    prev = l[0]
    count = 0

    for e in l:
        if e == prev:
            count += 1
        else:
            u.append((count, prev))
            count = 1
            prev = e

    u.append((count, prev))

    return u

def finalize_and_add_context(contexts, ctx):
    global allow_incomplete_results

    if allow_incomplete_results:
        try:
            ctx.finalize()
        except:
            return
    else:
        ctx.finalize()

    contexts.append(ctx)


def read_result_data(contexts, f, filter_mode):
    ctx = None

    while True:
        line = f.readline()
        if len(line) == 0:
            break

        fields = line.split()
        if len(fields) == 0:
            continue

        label = fields[0]
        if label[-1] == ':':
            label = label[:-1]

        values = fields[1:]

        if label == 'MARK':
            if ctx != None:
                finalize_and_add_context(contexts, ctx)

            ctx = Result_Context()

        assert(ctx != None)
        ctx.update(label, values)

        if filter_mode:
            ctx.add_data(line)

    assert(ctx != None)

    finalize_and_add_context(contexts, ctx)


def print_contexts(contexts):
    for ctx in contexts:
        for s in ctx.text_data:
            sys.stdout.write(s)


def mean_and_std(l):
    s = 0.0
    mean = sum(l) / len(l)
    for v in l:
        s += pow(v - mean, 2)
    s = s / len(l)
    s = math.sqrt(s)
    return (mean, s)


def gen_evaluation_list(contexts, gain_function):
    # Find maximum of minimum evals so that each graph starts from
    # the same evalution value. This is done to avoid ugly pictures,
    # but some data is lost.
    maxmin = 0
    for ctx in contexts:
        maxmin = max(maxmin, ctx.evals[0])

    # Compute a list of unique evaluation numbers
    evals = {}

    for ctx in contexts:
        for e in ctx.evals:
            if e >= maxmin:
                evals[e] = None

    evals = evals.keys()
    evals.sort()

    memory_save_mode = True

    if memory_save_mode:
        # Memory saving algorithm

        # Create "state" containg an evaluation number index and the best
        # cost for each context. The whole state is updated each iteration.
        state = [(0, None, None)] * len(contexts)

        yvalues = []
        stds = []

        nextp = 0

        for e_i in xrange(len(evals)):

            e = evals[e_i]

            # Compute mean and standard deviation for this state (e)
            values = []

            for i in xrange(len(contexts)):
                ctx = contexts[i]

                (ctx_i, gainval, costval) = state[i]

                while ctx_i < len(ctx.evals):
                    # If ctx.evals[ctx_i] == e, a new c value is chosen.
                    if ctx.evals[ctx_i] == e:
                        gainval = ctx.gains[ctx_i]
                        costval = ctx.best_cost[ctx_i]
                        break

                    # If ctx.evals[ctx_i] > e, the old c value is preserved.
                    elif ctx.evals[ctx_i] > e:
                        break

                    ctx_i += 1

                state[i] = (ctx_i, gainval, costval)

                lasteval = max(min(e, ctx.evals[-1]), 1)
                if gain_function != None:
                    if gainval != None:
                        # Compute gain function from the last known evaluation
                        # position of the context
                        values.append(gain_function(ctx, gainval, lasteval))
                else:
                    if costval != None:
                        values.append(costval)

            (mean, std) = mean_and_std(values)
            yvalues.append(mean)
            stds.append(std)

            # Print progress
            p = e_i * 100 / len(evals)
            if p == nextp:
                sys.stderr.write('%d complete\n' %(p))
                nextp += 5

    else:
        # Algorithm version 2: wastes memory but possibly faster than previous

        # Add costs to evaluation number buckets

        # Create buckets for different evaluation numbers
        buckets = []
        for i in xrange(len(evals)):
            buckets.append([])

        for ctx in contexts:
            j = 0
            c = None
            for i in xrange(len(ctx.evals)):
                e = ctx.evals[i]

                while evals[j] < e:
                    if c != None:
                        buckets[j].append(c)
                    j += 1

                c = ctx.gains[i]
                buckets[j].append(c)
                j += 1

            assert(c != None)

        # Compute average and std inside each bucket
        yvalues = []
        stds = []
        for bucket in buckets:
            (mean, std) = mean_and_std(bucket)
            yvalues.append(mean)
            stds.append(std)

    return [evals, yvalues, stds]


def gen_ga_plot(contexts):
    evalues = {}
    pvalues = {}
    for ctx in contexts:
        evalues[ctx.elitism] = True
        pvalues[ctx.population] = True

    evalues = evalues.keys()
    evalues.sort()
    pvalues = pvalues.keys()
    pvalues.sort()

    bigl = []

    for pv in pvalues:
        l = []
        for ctx in contexts:
            if ctx.population == pv:
                l.append(ctx)

        m = []
        for ev in evalues:
            for ctx in l:
                if ctx.elitism == ev:
                    m.append(ctx.gains[-1])

        bigl.append(m)

    sys.stdout.write('A = [')
    for row in bigl:
        for v in row:
            sys.stdout.write('%e, ' %(v))

        for i in xrange(len(row), len(evalues)):
            sys.stdout.write('1.0, ')

        sys.stdout.write(';\n')
    sys.stdout.write('];\n')


def unique_tuples(tuples):
    """ unique_tuples(tuples) -> list

    Given a list of tuples, returns list of unique tuples. That is, no two
    tuples in the resulting list are the same. Tuples in the final list
    are in sorted order, and therefore tuples must be comparable objects."""

    if len(tuples) == 0:
        return []

    l = list(tuples)
    l.sort()

    uniqlist = [l[0]]

    for i in xrange(1, len(l)):
        if l[i] != l[i - 1]:
            uniqlist.append(l[i])

    return uniqlist


def compute_arch_code_map(contexts):

    global archmap
    archmap = {}

    triples = []

    # First, sort architectures into increasing performance and area order
    for ctx in contexts:
        for (freq, perf, area) in ctx.pedatas:
            ops = int(freq * perf)
            triples.append((ops, area, (freq, perf, area)))

    # Remove duplicate triples, get unique triples in sorted order
    triples = unique_tuples(triples)

    # Assign character codes starting from letter A to PEs, in increasing
    # performance and area order. The slowest PE gets letter A.
    charcode = 0x41
    for (ops, area, archtriple) in triples:
        archmap[archtriple] = chr(charcode)
        charcode += 1


def get_arch_code(pedatas):
    global archmap

    archletters = []
    for archtriple in pedatas:
        archletters.append(archmap[archtriple])

    archletters.sort()

    return string.join(archletters, '')


class Arch_Result:
    def __init__(self, archnumber, k):
        self.ctxs = []
        self.T = 0.0
        self.P = None
        self.energy = 0.0
        self.staticenergy = 0.0
        self.dynenergy = 0.0
        self.icenergy = 0.0
        self.icstaticenergy = 0.0
        self.icdynenergy = 0.0
        self.archnumber = archnumber
        self.static_energy_proportions = []
        self.static_energy_proportion = None
        assert(k >= 0.0)
        self.k = k
        self.Apes = None
        self.Aic = None
        self.Atot = None
        self.fmax = None
        self.npes = None
        self.method = None
        self.best_cost_sum = 0.0
        self.archcode = None
        self.evaluations = None
        self.pe_utilization = None
        self.ic_utilization = None
        self.gain = None
        self.speedup = None

    def info(self):
        l = []

        for (freq, perf, area) in self.ctxs[0].pedatas:
            freq = freq / 1000000
            area = round(area * 1E8) / 100.0
            s = '%.3d MHz %.2f %.2f mm^2' %(freq, perf, area)
            l.append(s)

        npes = len(l)

        pes = count_duplicity(l)
        l = []
        for (n, pe) in pes:
            l.append('%d x %s' %(n, pe))

        return string.join(l, ', ')

    def get_arch_code(self):
        return get_arch_code(self.ctxs[0].pedatas)

    def set_arch_code(self):
        self.archcode = self.get_arch_code()

    def finalize(self, refarch):
        self.P = self.energy / self.T

        (mean, std) = mean_and_std(self.static_energy_proportions)
        self.static_energy_proportion = mean

        self.Atot = self.Aic + self.Apes
        self.method = self.ctxs[0].method

        self.set_arch_code()

        if refarch == None:
            self.refgain = 0.0
            self.refspeedup = 0.0
            self.refarchcode = 'None'
        else:
            self.refgain = refarch.energy / self.energy
            self.refspeedup = refarch.T / self.T
            self.refarchcode = refarch.archcode

        self.total_evals = 0
        peutils = []
        icutils = []

        for ctx in self.ctxs:
            self.total_evals += ctx.total_evals

            (mean, std) = mean_and_std(ctx.utilizations)
            peutils.append(mean)

            (mean, std) = mean_and_std(ctx.ic_utilizations)
            icutils.append(mean)

        (pemean, std) = mean_and_std(peutils)
        (icmean, std) = mean_and_std(icutils)
        self.pe_utilization = pemean
        self.ic_utilization = icmean

    def update(self, ctx):
        """ Compute dynamic and static energy for each architecture and
            graph. The static energy is the same for each graph, because
            it is purely architecture dependent, but the dynamic energy
            varies.

            Static energy is:  A_total * T * f_max
            Dynamic energy is: SUM(A_i * T * f_i * k * U_i)

            Where k is an arbitrary constant that is the same for all
            architectures. k is used to control the dynamic energy proportion.

            Total energy is the sum of static and dynamic energy.
        """

        self.ctxs.append(ctx)

        self.npes = ctx.pes

        self.best_cost_sum += ctx.best_objective

        T = ctx.best_time
        self.T += T

        if self.Apes == None:
            self.Apes = 0.0
            self.Aic = 0.0
            self.fmax = 0

            for (freq, perf, area) in ctx.pedatas:
                self.Apes += area
                self.fmax = max(self.fmax, freq)

            # Only one freq for ICs
            self.fmax = max(self.fmax, ctx.ic_freq)

            for area in ctx.ic_areas:
                self.Aic += area

        staticenergy = self.Apes * T * self.fmax

        dynenergy = 0.0
        for i in range(ctx.pes):
            (freq, perf, area) = ctx.pedatas[i]
            utilization = ctx.utilizations[i]
            dynenergy += area * T * freq * self.k * utilization

        icstaticenergy = self.Aic * T * self.fmax

        icdynenergy = 0.0
        for i in range(len(ctx.ic_utilizations)):
            icutil = ctx.ic_utilizations[i]
            icarea = ctx.ic_areas[i]
            icdynenergy += icarea * T * ctx.ic_freq * self.k * icutil

        staticenergy += icstaticenergy
        dynenergy += icdynenergy

        totalenergy = staticenergy + dynenergy

        self.static_energy_proportions.append(staticenergy / totalenergy)
        self.energy += totalenergy
        self.icenergy += icstaticenergy + icdynenergy
        self.staticenergy += staticenergy
        self.dynenergy += dynenergy


def print_arch_list(archs):
    if len(archs) > 0:
        refarchcode = archs[0][2].refarchcode
    else:
        refarchcode = 'None'

    sys.stdout.write('Arch code\tCost sum\tT\tEnergy\tIC energy\tGain (%s)\tSpeedup (%s)\tk\tStatic proportion\tStatic energy\tDynamic energy\tA\tA pes\tA IC\tPEs\tPE util.\tIC util.\tEvaluations\tOpt. Method\tArch info\n' %(refarchcode, refarchcode))
    for (x, y, arch) in archs:
        s = '%s' %(arch.archcode)
        s += '\t%.9f' %(arch.best_cost_sum)
        s += '\t%.9f' %(arch.T)
        s += '\t%.9f' %(arch.energy)
        s += '\t%.9f' %(arch.icenergy)
        s += '\t%.3f' %(arch.refgain)
        s += '\t%.3f' %(arch.refspeedup)
        s += '\t%.9f' %(arch.k)
        s += '\t%.2f' %(arch.static_energy_proportion)
        s += '\t%.9f' %(arch.staticenergy)
        s += '\t%.9f' %(arch.dynenergy)
        s += '\t%.9f' %(arch.Atot)
        s += '\t%.9f' %(arch.Apes)
        s += '\t%.9f' %(arch.Aic)
        s += '\t%d' %(arch.npes)
        s += '\t%.3f' %(arch.pe_utilization)
        s += '\t%.3f' %(arch.ic_utilization)
        s += '\t%d' %(arch.total_evals)
        s += '\t%s' %(arch.method)
        s += '\t' + arch.info()
        s += '\n'
        sys.stdout.write(s)


def print_paretos(triples):
    """ Find pareto optimums of a list of triplets.

    Test data: triples = [(1,5,None), (1,4,None), (2,5,None), (2,4,None), (2,3,None), (2,2,None), (3,1,None), (4,1,None), (5,1,None), (5,2,None)]
    """

    if len(triples) == 0:
        return []

    # Assume that the first field in triples increases, and the second
    # field decreases. The third field is arbitrary data.
    triples = list(triples)
    triples.sort()

    # Get unique values of the first field, and the maximum of second values
    firstvalues = {}
    secondval = triples[0][1]
    for (x, y, z) in triples:
        firstvalues[x] = True
        secondval = max(secondval, y)
    firstvalues = firstvalues.keys()
    firstvalues.sort()

    paretos = []

    for value in firstvalues:
        l = filter(lambda x: x[0] == value, triples)

        assert(len(l) > 0)

        # Swap first and second field for sorting
        l = map(lambda x: (x[1], x[0], x[2]), l)
        l.sort()

        # Swap fields back
        l = map(lambda x: (x[1], x[0], x[2]), l)

        # Find points with the second field at most as large as "secondval"
        i = 0
        while i < len(l) and l[i][1] <= secondval:
            paretos.append(l[i])
            i += 1

        # Second value always decreases
        secondval = min(secondval, l[0][1])

    print_arch_list(paretos)


def sort_archs(archs, opts):
    # Produce a list of architectures sorted with respect to total
    # execution time sum. Use "--method METHOD" with this.
    l = []
    for arch in archs:
        l.append((arch.best_cost_sum, arch.T, arch))

    # Make a copy of the arch list for later Pareto point analysis
    ltemp = list(l)

    l.sort()

    sys.stdout.write('First field sorted\n')
    print_arch_list(l)

    # Swap first two elements in the triple, and sort with respect to
    # the switched order. Python's sort is stable :)
    l = map(lambda x: (x[1], x[0], x[2]), l)
    l.sort()

    # Write some empty lines to make space for temporary graphs in the
    # gnumeric sheet
    sys.stdout.write('\n' * 20)

    sys.stdout.write('Second field sorted\n')
    print_arch_list(l)

    sys.stdout.write('\n' * 20)
    sys.stdout.write('Pareto optimums\n')
    print_paretos(ltemp)


def plot_archs(archs, opts):
    # Produce a plot(x, y, 'o') vector for Octave/Matlab, where
    # x vector is the energy (unit is comparable for all results, but not
    # fixed to any absolute scale), and y vector is the execution time sum
    # for all graphs. Use "--method METHOD" with this.

    Tvector = []
    Evector = []
    for arch in archs:
        Tvector.append(arch.T)
        Evector.append(arch.energy)

    for (name, l) in [('x', Evector), ('y', Tvector)]:
        sys.stdout.write('%s = [' %(name))
        for v in l:
            sys.stdout.write('%e ' %(v))
        sys.stdout.write('];\n')


def plot_specific_arch(archs, opts):

    # gain_function = lambda ctx, gain, e: gain

    # Make all Gain values comparable in plots by setting the reference
    # initial objective value to be that of the reference architecture's
    # mean best objective
    refarchcode = archs[0].refarchcode
    refarchs = filter(lambda arch: arch.archcode == refarchcode, archs)
    assert(len(refarchs) == 1)
    refarch = refarchs[0]
    base_initial_objective = refarch.energy / len(refarch.ctxs)
    gain_function = lambda ctx, gain, e: gain * base_initial_objective / ctx.initial_objective

    # Get archs matching the arch code
    archcode = opts['archcode']
    archs = filter(lambda arch: arch.archcode == archcode, archs)

    contexts = []
    for arch in archs:
        contexts += arch.ctxs

    # Now, plot contexts from these archs
    [evals, gains, stds] = gen_evaluation_list(contexts, gain_function)

    for (name, l) in [('x', evals), ('y', gains), ('s', stds)]:
        sys.stdout.write('%s = [' %(name))
        for v in l:
            if name == 'x':
                sys.stdout.write('%d ' %(v))
            else:
                sys.stdout.write('%e ' %(v))
        sys.stdout.write('];\n')


def power_mode_plot(contexts, k, opts):
    sys.stderr.write('WARNING: Power mode code is specific to one data set.\n')

    compute_arch_code_map(contexts)

    archs = []
    i = 0
    narch = 100
    ngraphs = 10

    assert(len(contexts) == narch * ngraphs)

    for archi in range(narch):
        arch = Arch_Result(archi, k)

        for graphi in range(ngraphs):
            arch.update(contexts[i])
            i += 1

        arch.finalize(None)

        archs.append(arch)

    opts['backend'](archs, opts)


def get_arch_finger(ctx):
    architems = []

    for i in range(ctx.pes):
        (freq, perf, area) = ctx.pedatas[i]
        architems.append('%d:%e:%e' %(freq, perf, area))

    architems.sort()

    return string.join(architems, ':')


def power_mode_plot_2(contexts, k, opts):
    sys.stderr.write('WARNING: Power mode code is specific to one data set.\n')

    compute_arch_code_map(contexts)

    archno = 0
    arch_fingers = {}
    arch_ctx_lists = {}

    for ctx in contexts:
        arch_finger = get_arch_finger(ctx)

        if arch_fingers.has_key(arch_finger) == False:
            arch_fingers[arch_finger] = archno
            archno += 1

        arch_id = arch_fingers[arch_finger]

        if arch_ctx_lists.has_key(arch_id) == False:
            arch_ctx_lists[arch_id] = []

        arch_ctx_lists[arch_id].append(ctx)

    archs = []
    ngraphs = 10
    nctxs = 0

    for arch_id in range(archno):
        
        arch = Arch_Result(arch_id, k)

        for ctx in arch_ctx_lists[arch_id]:
            arch.update(ctx)
            nctxs += 1

        archs.append(arch)

    refarch = None
    for arch in archs:
        if refarch == None or arch.T > refarch.T:
            refarch = arch

        # Use AA architecture if it exists
        if arch.get_arch_code() == 'AA':
            refarch = arch
            break

    assert(refarch != None)

    refarch.set_arch_code()
    sys.stderr.write('Reference arch is %s\n' %(refarch.archcode))

    for arch in archs:
        arch.finalize(refarch)

    assert(nctxs == len(contexts))

    opts['backend'](archs, opts)


def print_per_graph_stats(contexts):
    graphs = {}
    methods = {}
    pes = {}

    for ctx in contexts:
        graphs[ctx.graph] = []
        methods[ctx.method] = None
        pes[ctx.pes] = None

    for ctx in contexts:
        gname = ctx.graph
        gain = ctx.gains[-1]
        graphs[gname].append(gain)

    pes = pes.keys()
    pes.sort()
    sys.stdout.write('Number of PEs in these data sets: ')
    for n in pes:
        sys.stdout.write('%d ' %(n))
    sys.stdout.write('\n')

    methods = methods.keys()
    methods.sort()

    for method in methods:
        sys.stdout.write('%s ' %(method))

    gnames = graphs.keys()
    gnames.sort()

    for gname in gnames:
        (mean, std) = mean_and_std(graphs[gname])
        sys.stdout.write('%s %.3f (%e) ' %(gname, mean, std))

    sys.stdout.write('\n')

# Collect all results into this list:
contexts = []

filters = []

gain_function = lambda ctx, c, e: c

per_graph = False
filter_mode = False
contexts_read = False
ga_plot = False
allow_incomplete_results = False
total_combined_cost = False
power_mode = False
power_mode_2 = False
k = 1.0
power_mode_opts = {'backend' : plot_archs}

i = 1

while i < len(sys.argv):
    opt = sys.argv[i]
    next = None
    if (i + 1) < len(sys.argv):
        next = sys.argv[i + 1]

    if opt == '--acceptor':
        filters.append(Acceptor_Filter(next).filter)
        i += 2

    if opt == '--cost':
        # Compute cost instead of gain
        gain_function = None
        i += 1

    elif opt == '--total-combined-cost':
        total_combined_cost = True
        i += 1

    elif opt == '--edges':
        assert(next != None)
        filters.append(Edge_Filter(next).filter)
        i += 2

    elif opt == '--filter-mode':
        assert(contexts_read == False)
        filter_mode = True
        i += 1

    elif opt == '--graph':
        assert(next != None)
        filters.append(Graph_Filter(next).filter)
        i += 2

    elif opt == '--graph-regex':
        assert(next != None)
        filters.append(Graph_Regex_Filter(next).filter)
        i += 2

    elif opt == '--ic-freq':
        assert(next != None)
        filters.append(IC_Freq_Filter(next).filter)
        i += 2

    elif opt == '--allow-incomplete-results':
        allow_incomplete_results = True
        i += 1

    elif opt == '-L':
        assert(next != None)
        filters.append(L_Filter(next).filter)
        i += 2

    elif opt == '--method':
        assert(next != None)
        filters.append(Method_Filter(next).filter)
        i += 2

    elif opt == '--pe-freq':
        assert(next != None)
        filters.append(PE_Freq_Filter(next).filter)
        i += 2

    elif opt == '--per-graph':
        per_graph = True
        i += 1

    elif opt == '--pes':
        assert(next != None)
        filters.append(PE_Filter(int(next)).filter)
        i += 2

    elif opt == '--plot-arch':
        assert(next != None)
        power_mode_2 = True
        power_mode_opts['backend'] = plot_specific_arch
        power_mode_opts['archcode'] = str(next)
        i += 2

    elif opt == '--power-mode':
        power_mode = True
        i += 1

    elif opt == '--power-mode-2':
        power_mode_2 = True
        i += 1

    elif opt == '--relative-gain':
        gain_function = lambda ctx, c, e: c / e
        i += 1

    elif opt == '--sqrt-gain':
        gain_function = lambda ctx, c, e: c / math.sqrt(e)
        i += 1

    elif opt == '--square-gain':
        gain_function = lambda ctx, c, e: c / (e * e)
        i += 1

    elif opt == '--sort-archs':
        power_mode_opts['backend'] = sort_archs
        i += 1

    elif opt == '-k':
        assert(next != None)
        k = float(next)
        assert(k >= 0.0)

        filters.append(k_Filter(k).filter)

        i += 2

    elif opt == '--tasks':
        assert(next != None)
        filters.append(Task_Filter(int(next)).filter)
        i += 2

    # GA specific options
    elif opt == '--ga-plot':
        assert(next != None)
        ga_plot = True
        filters.append(Method_Filter('genetic_algorithm').filter)
        filters.append(Gene_Mutation_Filter(float(next)).filter)
        i += 2

    elif opt == '--elitism':
        assert(next != None)
        filters.append(Elitism_Filter(int(next)).filter)
        i += 2

    elif opt == '--population':
        assert(next != None)
        filters.append(Population_Filter(int(next)).filter)
        i += 2

    elif opt == '--chromosome-mutation':
        assert(next != None)
        filters.append(Chromosome_Mutation_Filter(float(next)).filter)
        i += 2

    elif opt == '--gene-mutation':
        assert(next != None)
        filters.append(Gene_Mutation_Filter(float(next)).filter)
        i += 2

    elif opt[0] == '-':
        sys.stderr.write('Unknown option: %s\n' %(opt))
        sys.exit(1)

    else:
        f = open(opt, 'r')
        read_result_data(contexts, f, filter_mode)
        f.close()
        contexts_read = True
        i += 1


oldN = len(contexts)

for f in filters:
    contexts = filter(f, contexts)

sys.stderr.write('%d -> %d contexts\n' %(oldN, len(contexts)))

if filter_mode:
    print_contexts(contexts)

elif per_graph:
    print_per_graph_stats(contexts)

elif ga_plot:
    gen_ga_plot(contexts)

elif total_combined_cost:
    cost = 0.0
    for ctx in contexts:
        cost += ctx.best_cost[-1]
    print cost

elif power_mode:
    power_mode_plot(contexts, k, power_mode_opts)

elif power_mode_2:
    power_mode_plot_2(contexts, k, power_mode_opts)

else:
    """ Print octave script for plotting (evaluation, gain, gain std).
        The gain function can be set with optional parameters. """

    [evals, gains, stds] = gen_evaluation_list(contexts, gain_function)

    for (name, l) in [('x', evals), ('y', gains), ('s', stds)]:
        sys.stdout.write('%s = [' %(name))
        for v in l:
            if name == 'x':
                sys.stdout.write('%d ' %(v))
            else:
                sys.stdout.write('%e ' %(v))
        sys.stdout.write('];\n')
