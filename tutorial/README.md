# Tutorial, Creating a HowDe Sequence Bloom Filter Tree

(1) Estimate the best bloom filter size.

```bash  
ntcard --kmer=20 --pref=EXPERIMENTS EXPERIMENT*.fastq.gz
cat EXPERIMENTS_k20.hist
```

ntcard reports F0≈2.1 million and f1≈1.3 million. F0 is an estimate of the
number of distinct 20-mers among all our experiments, and f1 is an estimate of
the number of those that occur only once. When we build our bloom filters, we
will exclude those that occur only once. So we'll use the difference F0-f1 as
our bloom filter size. This is about 800K.

_Note that this size is an overestimate._

(2) Convert the fasta files to bloom filter bit vectors.

We use "howdesbt makebf" independently on each fastq file.

_As shown here, we use the same minimim abundance for each bloom filter.
However, this is not necessary, and an abundance cutoff derived from, say, the
size of the fasta file, could be used._

```bash  
bf_size=800K
min_abundance=2
ls EXPERIMENT*.fastq.gz \
  | sed "s/\.fastq\.gz//" \
  | while read experiment ; do
      gzip -dc ${experiment}.fastq.gz \
        > ${experiment}.fastq
      howdesbt makebf K=20 --min=${min_abundance} --bits=${bf_size} \
        ${experiment}.fastq \
        --out=${experiment}.bf
      rm ${experiment}.fastq
      done
```

The result of this step is a bloom filter for each of the fastq files, named
EXPERIMENT1.bf, EXPERIMENT2.bf, etc.

(3) Create a tree topology.

We use "howdesbt cluster" to create a tree topology in which the experiments'
bloom filters correspond to leaves. Similar leaves are grouped together to
improve the performance of later operations. Note that this step does not
actually create any of the bloom filters for the tree -- it only creates the
topology.

The cluster command estimates the similarity from a subset of the bits in the
filters. In this example we use 80K bits (10% of the 800K bits in each filter).
This significantly reduces the program's memory and I/O footprints, as well as
runtime, while still providing a reasonably good estimate of similarity.

The --nodename option specifies that the names that will be used for internal
nodes in the tree. This must contain "{number}", which will be replaced by a
number from 1 to the count of internal nodes.

```bash  
cluster_bits=80K
ls EXPERIMENT*.bf > leafnames
howdesbt cluster --list=leafnames --bits=${clusterBits} \
  --tree=union.sbt --nodename=node{number} --keepallnodes
rm leafnames
```

The result of this step is a tree topology file, union.sbt. Note that no
bloom filters are actually created in this step.

(4) Build the "determined,brief" tree, compressed as RRR.

We use "howdesbt build" to build the filter files for the tree. We have a
choice of several filter arrangements (a simple union tree, an allsome tree,
a determined tree, or a determined,brief tree), and how the files should be
compressed (RRR, roaring bit strings, or no compression).

```bash  
howdesbt build --determined,brief --rrr --tree=union.sbt \
  --outtree=detbrief.rrr.sbt
```

The result of this step is bloom filter files for the leaves and internal
nodes, in "determined,brief" format and compressed with RRR. And a new topology
file named detbrief.rrr.sbt. The bloom filter files are named
EXPERIMENT1.detbrief.rrr.bf, ..., node1.detbrief.rrr.bf, etc.

(5) Run a batch of queries.

We use "howdesbt query" to search the tree for "hits" for query sequences. The
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
howdesbt query --tree=detbrief.rrr.sbt \
    queries.fa \
  > queries.hits
```

_I need to describe the output format_

(5B) Ordering query results by how good they are.

_This section needs to be written._
