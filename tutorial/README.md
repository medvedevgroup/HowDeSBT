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
show how this setting can be estimated from the data. Similarly, all Bloom
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
see 1-ALT1 and 1-ALT2.

In the original SBT paper (Solomon, Brad, and Carl Kingsford. "Fast search of
thousands of short-read sequencing experiments.") it was argued that the size
of the Bloom filters should be set to an estimate of the total number of unique
k-mers in the union of the experiments. We use ntcard to estimate this count.

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

Here we use the default threshold (called "theta" in the SBT literature) of 70%
for all queries. Alternatively, the query command allows a separate threshold
to be applied to each query file.

```bash  
howdesbt query --tree=howde.sbt queries.fa > queries.dat
```

The first part of output file, queries.dat, is shown below. This reports that
QUERY_001 theta-matches three experiments, EXPERIMENT2, EXPERIMENT5, and
EXPERIMENT15. QUERY_002 theta-matches four experiments, QUERY_003 theta-matches
two, and so on. The actual file includes results for all 300 queries.

```bash  
*QUERY_001 3
EXPERIMENT2
EXPERIMENT5
EXPERIMENT15
*QUERY_002 4
EXPERIMENT4
EXPERIMENT13
EXPERIMENT17
EXPERIMENT18
*QUERY_003 2
EXPERIMENT5
EXPERIMENT12
 ...
```

## Alternatives for estimating the Bloom filter size.

### (1-ALT1) Set the _Query_ false positive rate for the largest experiment.

A query false positive occurs when SBT reports that a query theta-matches an
experiment, when in fact it does not. This can happen because Bloom filter
false positives inflate the count of query kmers present in the experiment.

This is more likely to happen for larger experiments, because the Bloom filter
false positive rate increases with the number of distinct kmers in the
experiment. To bound the query false positive rate we needn't consider any
experiments except the largest.

The query false positive rate also depends on the query size, the threshold
(theta), and how close the query would be to the threshold (epsilon). A smaller
epsilon means fewer Bloom filter false positives are needed to achieve theta,
increasing the query false positive rate. Similarly a smaller query increases
the query false positive rate.

To find size of the the largest experiment, we run ntcard independently on each
experiment, and the estimated number of distinct kmers are collected into a
table. As in earlier example, we assume that when we build our Bloom filters
we'll exclude those that occur only once.

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

The resulting table shows the largest experiment has 92,864 distinct kmers.
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

We can then use the determine_sbt_bf_size script to estimate the Bloom filter
size. We have to make some choices about the other parameters. Here we set the
query size to 300, which is approximately the size of the smallest query in
this example. We set theta to the search default of 70%, with epsilon of 5%,
and set the desired query false positive rate to 10%.

```bash  
./scripts/determine_sbt_bf_size.py \
  --experiment=92864 --query=300 \
  --theta=70% --epsilon=5% \
  --queryfp=10%
```

The output shows the number of bits is 811,606.

```bash  
#numItems bfFP     numHashes numBits
92864     0.100000 1         881393
```

### (1-ALT2) Set the _Bloom filter_ false positive rate for the largest experiment.

As we found in the previous section, the largest number of distinct kmers is
92,864. If we want a Bloom filter with a 10% false positive rate, we can
determine the appropriate size using the simple_bf_size_estimate script.

```bash  
./scripts/simple_bf_size_estimate.py 92864 10%
```

The output shows the number of bits to be about 880K.

```bash  
#numItems bfFP     numHashes numBits
92864     0.100000 1         881393
```

## Alternative for querying -- Sorting matched queries by kmer hit counts, and adjusting for Bloom filter false positives.

_Note: the adjustment capability described here is not available for SBTs
built with versions earlier than 2.0._

The original SBT design concept was that it would only report whether a query
is (or is not) a theta-match for an experiment. This allows the search to
ignore many branches of the tree, improving search speed.

In some uses, it is desirable not just to know what queries match an
experiment, but to order them from best match to least. HowDeSBT supports this,
albeit at a sacrifice of some speed. Moreover, it can adjust the number of
matching kmers downward to account for Bloom filter false positives.

### (5-ALT) Run a batch of queries

We can run the same query as earlier, but sorting by adjusted kmer count.

```bash  
howdesbt query --tree=howde.sbt --adjust --sort queries.fa > adjusted_queries.dat
```

As before, the number of hits for each query are reported (see table below).
However, for each hit we now see the fraction of kmers in common (according to
the Bloom filters), and an adjusted version of that fraction. The hits are
ordered by decreasing adjusted fraction.

For example, according to the Bloom filters, QUERY_001 had 600 kmers in common
with EXPERIMENT15 and 605 with EXPERIMENT5. But since EXPERIMENT5 has a higher
Bloom filter false positive rate, after adjustment we estimate there are really
564 and 544 kmers in common, respectively.

```bash  
*QUERY_001 3
EXPERIMENT15 600/790 0.759494 564/790 0.713924
EXPERIMENT5  605/790 0.765823 544/790 0.688608
EXPERIMENT2  559/790 0.707595 509/790 0.644304
*QUERY_002 4
EXPERIMENT18 330/403 0.818859 310/403 0.769231
EXPERIMENT17 291/403 0.722084 280/403 0.694789
EXPERIMENT4  284/403 0.704715 265/403 0.657568
EXPERIMENT13 291/403 0.722084 260/403 0.645161
*QUERY_003 2
EXPERIMENT12 490/637 0.769231 441/637 0.692308
EXPERIMENT5  458/637 0.718995 399/637 0.626374
 ...
```
