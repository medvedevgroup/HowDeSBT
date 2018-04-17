# Tutorial, Creating a Bloom Filter Tree

(1) Estimate the best bloom filter size.

_This section is not yet written. For now, we magically assume the size is
500,000._

(2) Convert the fasta files to bloom filter bit vectors.

We use "sabutan makebf" independently on each fasta file. As shown here, we
use the same minimim abundance for each bloom filter. However, this is not
necessary, and an abundance cutoff derived from, say, the size of the fasta
file, could be used.

We don't specify the output file name here. We let sabutan use the default
naming convention, which for example will produce expriment1.bf from
expriment1.fa. (Note that the echo command in this loop assumes that.) The
makebf command does have an option to assign a different name. If you do that,
be sure the name ends in ".bf".

```bash  
bf_size=500000
min_abundance=3
ls experiment*.fa \
  | while read f ; do
      bf=`echo ${f} | sed "s/\.fa/.bf/"`
      echo "=== converting ${f} to ${bf} ==="
      sabutan makebf --min=${min_abundance} K=20 --bits=${bf_size} ${f}
      done
```

The result of this step is a bloom filter for each of the fasta files, named
expriment1.bf, expriment2.bf, etc.

(2-ALT) Convert kmer files to bloom filter bit vectors.

Kmer files can be used as input instead of fasta or fastq files. This option is
useful if you have some scheme for deciding which kmers belong in the bloom
filter that's inconsistent with sabutan's simplistic abundance cutoff.

In this case, each line of the sequence input file(s) is a single kmer, as the
first field in the line. Any additional fields on the line are ignored. For
example, with K=20 this might be
```bash  
ATGACCAGATATGTACTTGC
TCTGCGAACCCAGACTTGGT
CAAGACCTATGAGTAGAACG
 ...
```

A loop converting a collection of kmer files to bloom filters might look like
this.

```bash  
bf_size=500000
ls experiment*.kmers \
  | while read f ; do
      bf=`echo ${f} | sed "s/\.kmers/.bf/"`
      echo "=== converting ${f} to ${bf} ==="
      sabutan makebf --kmersin K=20 --bits=${bf_size} ${f}
      done
```

(3) Create a tree topology.

We use "sabutan cluster" to create a tree topology in which the experiments'
bloom filters correspond to leaves. Similar leaves are grouped together to
improve the performance of later operations. Note that this step does not
actually create any of the bloom filters for the tree -- it only creates the
topology.

The cluster command estimates the similarity from a subset of the bits in the
filters. In this example we use 50K bits (10% of the 500K bits in each filter).
This significantly reduces the program's memory and I/O footprints, as well as
runtime, while still providing a reasonably good estimate of similarity.

```bash  
cluster_bits=50000
ls experiment*.bf > leafnames
sabutan cluster --list=leafnames --bits=${cluster_bits} \
  --tree=example.sbt --node=node{node}.bf
```

The result of this step is a tree topology file, example.sbt. Note that no
bloom filters are actually created in this step.

(4) Build the "determined,brief" tree, compressed as RRR.

We use "sabutan cluster" to build the filter files for the tree. We have a
choice of several filter arrangements (a simple union tree, an allsome tree,
a determined tree, or a determined,brief tree), and how the files should be
compressed (RRR, roaring bit strings, or no compression).

```bash  
sabutan build --determined,brief --rrr --tree=example.sbt \
  --outtree=detbrief.rrr.sbt
```

The result of this step is bloom filter files for the leaves and internal
nodes, in "determined,brief" format and compressed with RRR. And a new topology
file named detbrief.rrr.sbt. The bloom filter files are named
experiment1.detbrief.rrr.bf, ..., node1.detbrief.rrr.bf, etc.

(5) Run a batch of queries.

We use "sabutan query" to search for "hits" for query sequences. The input here
is in fasta format, including headers for each sequence. Names from the headers
are used in the output to identify a query.

For backward compatibility with earlier tools, sabutan can also recognize a
fasta-like file that contains no headers. In that case, each input line is
considered as a separate query sequence, and in the output the sequence itself
is used in place of the query name.

Here we give each query file its own threshold (called "theta" in the SBT
literature). Alternatively, the query command allows a single threshold to be
applied to all query files.

```bash  
sabutan query --tree=detbrief.rrr.sbt \
    queries1.fa=0.5 queries2.fa=0.7 \
  > queryresults
```

The resulting queryresults should be identical to queryresults.expected.

_I need to describe the output format_

(6) Ordering query results by how good they are.

_This section needs to be rewritten, as order_query_results is no longer
used._

```bash  
../scripts/order_query_results.sh \
  queries1.fa queries2.fa \
  queryresults \
  queryresults.kmers
```

The resulting queryresults.kmers should be identical to
queryresults.kmers.expected, which is also shown below. The first column shows
the name of a query sequence, as appeared within a fasta file. The second
column gives the name of an experiment which the query 'hit'. The third column
shows the fraction of the query's kmers that are present in the experiment
(more correctly, in the the experiment's bloom filter). The fourth column gives
that fraction as a a number. Note that queries are sorted by name, which may
not match their order in the sabutan output file.

```bash  
EXAMPLE_QUERY1 experiment8  481/481 1.000000
EXAMPLE_QUERY1 experiment10 355/481 0.738046
EXAMPLE_QUERY3 experiment2  481/481 1.000000
EXAMPLE_QUERY4 experiment1  387/481 0.804574
EXAMPLE_QUERY5 experiment3  481/481 1.000000
EXAMPLE_QUERY5 experiment6  481/481 1.000000
```

