## Tutorial, Creating a HowDe Sequence Bloom Filter Tree

### Overview.

This directory contains twenty files of sequenced reads, EXPERIMENT1.fastq.gz,
EXPERIMENT2.fastq.gz, etc, each representing a sequencing experiment. (These
are not actual reads, but model reads sampled from a set of transcript-like
sequences). There is also a file, queries.fa, that contains 300 query
sequences.

An SBT represents a set of sequencing experiments, where each experiment is a
set of sequences (e.g. reads). The `makebf` subcommand is used to convert each
experiment into a Bloom filter of its kmers. The `cluster` and `build`
subcommands convert these Bloom filters into an SBT, and the `query` subcommand
then uses the SBT to identify experiments likely to contain a given query
sequence. Typically, low-abundance kmers are left out of the Bloom filters.

Note that all Bloom filters must have the same number of bits. In step 1 we
show how this setting can be estimated from the data. Similarly, all bloom
filters must have the same k-mer size. Here we use K=20, which is typical in
the SBT literature.

In this tutorial the reads for an experiment are in a single file. But in
practice they are often spread among several files. For example, paired
sequencing often produces two files per sequencing run or per lane. So while
step 2 shows `makebf` used with only one input fastq file, it can accept any
number of fasta or fastq files.

### (1) Estimate the best Bloom filter size.

There are a few schemes for deciding what size to use for an SBT's Bloom
filters. Here we describe the scheme analogous to the one recommended in the
original SBT paper. Alternate schemes are described later in this document;
see ALT1 and ALT2.

In the original SBT paper [Solomon 2016], it was argued that the size of the
Bloom filters should be set to an estimate of the total number of unique k-mers
in the union of the experiments. We use ntcard to estimate this count.

```bash  
ntcard --kmer=20 --pref=EXPERIMENTS EXPERIMENT*.fastq.gz
cat EXPERIMENTS_k20.hist
F1	7806530
F0	2114882
f1	1360743
f2	74131
 ...
```

In EXPERIMENTS_k20.hist, ntcard reports F0≈2.1 million and f1≈1.3 million. F0
is an estimate of the number of distinct 20-mers among all our experiments, and
f1 is an estimate of the number of those that occur only once. When we build
our Bloom filters, we will exclude those that occur only once. So we'll use the
difference F0-f1 as our Bloom filter size. This is about 800K.

_Note that this size is an overestimate._ However, tests have shown that
overestimating the Bloom filter size has only a minor effect on the overall
HowDe-SBT file sizes and query times.

_Also note that this size is much smaller than would be typical for real data._
This tutorial has atypically small reads files and a small number of
experiments. SBTs for real data typically use much larger Bloom filters, with
sizes in the billions.


Version 1.0.1 of ntcard was used, fetched as follows:
```bash  
git clone https://github.com/bcgsc/ntCard --branch "1.0.1"
```

### (2) Convert the fasta files to Bloom filter bit vectors.

We use `howdesbt makebf` to create a Bloom filter for each experiment.

_The --min setting causes low-abundance kmers to be discarded. Such kmers are
expected to contain sequencing errors. As shown here, we use the same minimim
abundance for each Bloom filter. However, this is not necessary, and an
abundance cutoff derived from the size of the fasta/fastq files, or the
distribution of kmer counts, could be used._

_The --stats option would not normally be used. We use it here only as part of
the validation process._

```bash  
ls EXPERIMENT*.fastq.gz \
  | sed "s/\.fastq\.gz//" \
  | while read experiment ; do
      gzip -dc ${experiment}.fastq.gz \
        > ${experiment}.fastq
      howdesbt makebf K=20 --min=2 --bits=800K \
        ${experiment}.fastq \
        --out=${experiment}.bf \
        --stats
      rm ${experiment}.fastq
      done
```

The result of this step is a Bloom filter for each experiment, named
EXPERIMENT1.bf, EXPERIMENT2.bf, etc.

### (2a) Validation.

When the --stats option is used with makebf, a small stats file is created for
each experiment,  named EXPERIMENT1.stats, EXPERIMENT2.stats, etc.

These can be collected into a table, like this:
```bash  
cat EXPERIMENT*.stats \
  | sort \
  | awk '/^#/ { if (n++ == 0) print $1,$3,$4 }
        !/^#/ { print $1,$3,$4 }' \
```
and your result should match the table below.
```bash  
#filename       numBits kmersAdded
EXPERIMENT1.bf  800000  50886
EXPERIMENT2.bf  800000  63657
EXPERIMENT3.bf  800000  26860
EXPERIMENT4.bf  800000  47006
EXPERIMENT5.bf  800000  91929
EXPERIMENT6.bf  800000  37736
EXPERIMENT7.bf  800000  72996
EXPERIMENT8.bf  800000  61366
EXPERIMENT9.bf  800000  65365
EXPERIMENT10.bf 800000  26906
EXPERIMENT11.bf 800000  43542
EXPERIMENT12.bf 800000  93117
EXPERIMENT13.bf 800000  78632
EXPERIMENT14.bf 800000  96079
EXPERIMENT15.bf 800000  56645
EXPERIMENT16.bf 800000  94638
EXPERIMENT17.bf 800000  31226
EXPERIMENT18.bf 800000  76546
EXPERIMENT19.bf 800000  63051
EXPERIMENT20.bf 800000  21149
```

### (3) Create a tree topology.

We use `howdesbt cluster` to create a tree topology in which the experiments'
Bloom filters correspond to leaves. Similar leaves are grouped together to
improve the performance of later operations. Note that this step does not
actually create any of the Bloom filters for the tree &mdash; it only creates
the topology.

The cluster command estimates the similarity from a subset of the bits in the
filters. In this example we use 80K bits (10% of the 800K bits in each filter).
This significantly reduces the program's memory and I/O footprints, as well as
runtime, while still providing a reasonably good estimate of similarity.

The --nodename option specifies that the names that will be used for internal
nodes in the tree. This must contain the substring "{number}", which will be
replaced by a number from 1 to the count of internal nodes.

```bash  
ls EXPERIMENT*.bf > leafnames
howdesbt cluster --list=leafnames --bits=80K \
  --tree=union.sbt --nodename=node{number} --keepallnodes
rm leafnames
```

The result of this step is a tree topology file, union.sbt. Note that no
Bloom filters are actually created in this step.

### (4) Build the HowDeSBT nodes.

We use `howdesbt build` to build the Bloom filter files for the tree.

```bash  
howdesbt build --HowDe --tree=union.sbt --outtree=howde.sbt
```

The result of this step is Bloom filter files for the leaves and internal
nodes, in HowDeSBT format and compressed with RRR. And a new topology
file named howde.sbt. The Bloom filter files are named
EXPERIMENT1.detbrief.rrr.bf, ..., node1.detbrief.rrr.bf, ... etc.

### (5) Run a batch of queries.

We use `howdesbt query` to search the tree for "hits" for query sequences. The
input here is in fasta format, including headers for each sequence. Names from
the headers are used in the output to identify a query.

For backward compatibility with earlier tools, howdesbt can also recognize a
fasta-like file that contains no headers. In that case, each input line is
considered as a separate query sequence, and in the output the sequence itself
is used in place of the query name.

Here we give each query file its own threshold (called "theta" in the SBT
literature). Alternatively, the query command allows a single threshold to be
applied to all query files.

```bash  
howdesbt query --tree=howde.sbt queries.fa > queries.dat
```

The first part of output file, queries.dat, is shown below. This reports that
QUERY_001 theta-matches two experiments, EXPERIMENT5 and EXPERIMENT15.
QUERY_002 theta-matches two other experiments, QUERY_003 theta-matches one, and
so on. The actual file includes results for all 300 queries.

```bash  
*QUERY_001 2
EXPERIMENT5
EXPERIMENT15
*QUERY_002 2
EXPERIMENT17
EXPERIMENT18
*QUERY_003 1
EXPERIMENT12
 ...
```

## Alternatives for estimated the Bloom filter size.

### (ALT1) Set the _Query_ false positive rate for the largest experiment.

_to be written_

_A _query_ false positive occurs when_

### (ALT2) Set the _Bloom filter_ false positive rate for the largest experiment.

Be aware that the Bloom filter false positive rate is different that the query
false positive rate. A Bloom filter false positive is kmer which the Bloom
filter reports as present in the experiment when it is not.

We run ntcard independently on each experiment, and the estimated number of
distinct kmers are collected into a table. As in the examples above, we assume
that when we build our Bloom filters we'll exclude those that occur only once.

```bash  
:> EXPERIMENTS.ntcard.dat    
ls EXPERIMENT*.fastq.gz \
  | sed "s/.*\///" \
  | sed "s/\.fastq\.gz//" \
  | while read experiment ; do
      ntcard --kmer=20 --pref=${experiment} ${experiment}.fastq.gz
      cat ${experiment}_k20.hist \
        | awk '{
               if      ($1=="F0") F0=$2;
               else if ($1=="f1") f1=$2;
               }
           END {
               print experiment,F0,f1,F0-f1;
               }' experiment=${experiment} \
        >> EXPERIMENTS.ntcard.dat    
      rm ${experiment}_k20.hist
      done

cat EXPERIMENTS.ntcard.dat \
  | sort -nr -k 4 \
  | awk '{
         if (n++ == 0) print "#name F0 f1 numKmers";
         print $0;
         }'
```

The resulting table:
```bash  
#name        F0     f1     numKmers
EXPERIMENT16 208961 116097 92864
EXPERIMENT12 198465 106625 91840
EXPERIMENT5  198401 108865 89536
EXPERIMENT14 203393 114945 88448
EXPERIMENT13 172672 95424  77248
EXPERIMENT18 167296 91584  75712
EXPERIMENT7  160704 88512  72192
EXPERIMENT9  142656 79104  63552
EXPERIMENT19 141440 78144  63296
EXPERIMENT2  144256 81344  62912
EXPERIMENT8  132608 70336  62272
EXPERIMENT15 123648 66688  56960
EXPERIMENT1  110400 61952  48448
EXPERIMENT4  103168 56384  46784
EXPERIMENT11 91264  49408  41856
EXPERIMENT6  84544  45760  38784
EXPERIMENT17 72256  38912  33344
EXPERIMENT3  62656  34432  28224
EXPERIMENT10 61184  33536  27648
EXPERIMENT20 46592  27136  19456
```

The largest number of distinct kmers is 92,864. If we want a bloom filter
with a 10% false positive rate, we can determine the appropriate size using the
simple_bf_size_estimate script.

```bash  
./scripts/simple_bf_size_estimate.py 92864 0.10
```

In the output, B is the number of bits; in this case, it's about 880K.

```bash  
#numItems bfFP     H B
92864     0.100000 1 881393
```

## Adjusting reported kmer hit counts to account for Bloom filter false positives.

_requires version 2.0 or later_

_SBT must have been built with version 2.0 or later_

_also discuss sorting by hit count, and pruning_

### (ALT5) _need a title_

_to be written_

## References.

[Solomon 2016] Solomon, Brad, and Carl Kingsford. "Fast search of thousands of
short-read sequencing experiments." Nature biotechnology 34.3 (2016): 300.
