## Experiments from the HowDe-SBT manuscript

#### Converting SRA files to jellyfish

In order to facilitate experimentation with different abundance thresholds,
each experiment was processed with jellyfish to save kmers and counts.

```bash  
time fastq-dump --fasta experiment1.sra --stdout \
  | jellyfish count --mer-len=20 --canonical \
      --size=3G --lower-count=1 --threads=3 \
      /dev/stdin --output=/dev/stdout \
  | gzip \
  > experiment1.jf.gz
```

### HowDe-SBT

#### Converting jellyfish files to Bloom filters

```bash  
abundance=(look up mantis threshold for experiment1) 
gzip -dc experiment1.jf.gz \
  | jellyfish dump --column --lower-count=${abundance} /dev/stdin \
  | awk '{ print $1 }' \
  | howdesbt makebf /dev/stdin --kmersin K=20 --min=${abundance} \
      --bits=2G --out=experiment1.bf
```

#### Determining tree topology

```bash  
ls experiment*.bf > filterlist
howdesbt cluster --list=filterlist --bits=500K \
  --tree=uncompressed.culled.sbt --nodename=node{number}.bf
```

#### Creating the internal Bloom filters

```bash  
howdesbt build --determined,brief --rrr \
  --tree=uncompressed.culled.sbt --outtree=howde.culled.rrr.sbt
```

#### Performing queries

```bash  
howdesbt query --tree=howde.culled.rrr.sbt query_batch.fa --threshold=0.9 \
  > query_batch.howde.out
```

### AllSome-SBT

#### Converting jellyfish files to Bloom filters

AllSome-SBT uses the same file format as SSBT, so we share the same leaf
Bloom Filter files created by SSBT.

#### Determining tree topology

For direct comparison to HowDe-SBT, we borrow the tree topology created by
HowDe-SBT. However, we need the non-culled version of that tree. So we use
HowDe-SBT to create the topology, and perform a simple topology format
conversion.

```bash  
ls experiment*.bf > filterlist
howdesbt cluster --list=filterlist --bits=500K --keepallnodes \
  --tree=uncompressed.non_culled.sbt --nodename=node{number}.bf

cat uncompressed.non_culled.sbt \
  | sed "s/\.bf/.bf.bv/" \
  | awk '{ if (NR==1) print $0",hashfile";
                 else print $0;  }' \
  > uncompressed.non_culled.allsome
```

#### Creating the internal Bloom filters

_This section needs to be written._

Build the uncompressed tree from the leaves according to the topology ... in
sbt parlance this is a "rebuild"

```bash  
ssbt hashes --k 20 hashfile 1

bt rebuild uncompressed.non_culled.allsome

bt split uncompressed.non_culled.allsome uncompressed.split.allsome

cat uncompressed.non_culled.allsome \
  | sed "s/\.bf\.bv/.bf-allsome.bv.rrr/" \
  > rrr.split.allsome

bt compress-rrr-double hashfile \
  node1.bf-all.bv node1.bf-some.bv node1.bf-allsome.bv
```

#### Performing queries

First we have to convert query_batch.fa to query_batch.sequences, containing
the fasta sequences one per line, with no sequence names.

```bash  
bt query-redux --query-threshold 0.9 \
  rrr.split.allsome query_batch.sequences query_batch.allsome.out
```

### SSBT

#### Converting jellyfish files to Bloom filters

```bash  
ssbt hashes --k 20 hashfile 1

abundance=(look up mantis threshold for experiment1) 
gzip -dc experiment1.jf.gz \
  | jellyfish dump --column --lower-count=${abundance} /dev/stdin \
  | awk '{ print ">"(++n); print $1 }' \
  | ssbt count --cutoff 1 --threads=4 hashfile 2000000000 \
      /dev/stdin experiment1
```

#### Creating the tree (topology and internal Bloom filters

```bash  
ls experiment*.sim.bf.bv > filterlist

ssbt build --sim-type 2 \
  hashfile filterlist sim_type_2.ssbt

ssbt compress sim_type_2.ssbt sim_type_2.compressed.ssbt
```

#### Performing queries

First we have to convert query_batch.fa to query_batch.sequences, containing
the fasta sequences one per line, with no sequence names.

```bash  
ssbt query --query-threshold 0.9 \
  sim_type_2.compressed.ssbt query_batch.sequences query_batch.ssbt.out
```

### mantis

#### Creating the CQFs

We first convert the jellyfish file to a fastq file containing all the kmers,
in the same abundance as in the original SRA file.

```bash  
log2Slots=(look up slots for experiment1) 
squeakr-exact/squeakr_count -k 20 -s ${log2Slots} -t 5 -f experiment1.fastq
```

#### Building the CDGB

```bash  
ulimit -Sn 3000
${mantis_may_15_2018} build \
  -i solomon_experiments.cqfs \
  -c experiment_cutoffs.lst \
  -o CDBG
```

#### Performing queries

_This section needs to be written._

```bash  
mantis query -p CDBG/ -o query_batch.mantis.out query_batch.fa
```

