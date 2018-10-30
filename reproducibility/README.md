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
abundance=(lookup mantis threshold for experiment1.fastq) 
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

#### Creating the internal Bloom filters)

```bash  
howdesbt build --determined,brief --rrr \
  --tree=uncompressed.culled.sbt --outtree=howde.culled.rrr.sbt
```

#### Performing queries

```bash  
howdesbt query --tree=howde.culled.rrr.sbt query.fa --threshold=0.9
```

### AllSome-SBT

#### Converting jellyfish files to Bloom filters

_This section needs to be written._

#### Determining tree topology

_This section needs to be written._

#### Creating the internal Bloom filters)

_This section needs to be written._

#### Performing queries

_This section needs to be written._

### SSBT

#### Converting jellyfish files to Bloom filters

_This section needs to be written._

#### Creating the tree (topology and internal Bloom filters)

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
