## Experiments from the HowDe-SBT manuscript

#### Converting sra files to jellyfish

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
howdesbt query --tree=howde.culled.rrr.sbt query_batch.fa --threshold=0.9
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

First we convert query_batch.fa to query_batch.sequences, containing the fasta
sequences one per line, with no sequence names.

```bash  
bt query-redux --query-threshold 0.9 \
  rrr.split.allsome query_batch.sequences /dev/stdout
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

_This section needs to be written._

#### Performing queries

_This section needs to be written._

### mantis

#### Computing CQF slots

_This section needs to be written._

#### Creating the CQFs

_This section needs to be written._

#### Building the CDGB

_This section needs to be written._

#### Performing queries

_This section needs to be written._
