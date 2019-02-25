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
sequence.

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

```bash  
ntcard --kmer=20 --pref=EXPERIMENTS EXPERIMENT*.fastq.gz
cat EXPERIMENTS_k20.hist
```

ntcard reports F0≈2.1 million and f1≈1.3 million. F0 is an estimate of the
number of distinct 20-mers among all our experiments, and f1 is an estimate of
the number of those that occur only once. When we build our Bloom filters, we
will exclude those that occur only once. So we'll use the difference F0-f1 as
our Bloom filter size. This is about 800K.

_Note that this size is an overestimate._ However, tests have shown that
overestimating the Bloom filter size has only a minor effect on the overall
HowDe-SBT file sizes and query times.

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

```bash  
ls EXPERIMENT*.fastq.gz \
  | sed "s/\.fastq\.gz//" \
  | while read experiment ; do
      gzip -dc ${experiment}.fastq.gz \
        > ${experiment}.fastq
      howdesbt makebf K=20 --min=2 --bits=800K \
        ${experiment}.fastq \
        --out=${experiment}.bf
      rm ${experiment}.fastq
      done
```

The result of this step is a Bloom filter for each experiment, named
EXPERIMENT1.bf, EXPERIMENT2.bf, etc.

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

