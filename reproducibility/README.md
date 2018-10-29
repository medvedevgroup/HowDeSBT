## Experiments from the HowDe-SBT manuscript

### HowDe-SBT

#### Converting fastq to Bloom filters

_This section needs to be written._

```bash  
abundance=(lookup mantis threshold for experiment1.fastq) 
howdesbt makebf experiment1.fastq K=20 --min=${abundance} \
  --bits=2G --out=experiment1.bf
```

#### Determining tree topology

_This section needs to be written._

```bash  
ls experiment*.bf > filterlist
howdesbt cluster --list=filterlist --bits=500K \
  --tree=uncompressed.culled.sbt --nodename=node{number}.bf
```

#### Creating the internal Bloom filters)

_This section needs to be written._

```bash  
howdesbt build --determined,brief --rrr \
  --tree=uncompressed.culled.sbt --outtree=howde.culled.rrr.sbt
```

#### Performing queries

_This section needs to be written._

### AllSome-SBT

#### Converting fastq to Bloom filters

_This section needs to be written._

#### Determining tree topology

_This section needs to be written._

#### Creating the internal Bloom filters)

_This section needs to be written._

#### Performing queries

```bash  
howdesbt query --tree=howde.culled.rrr.sbt query.fa --threshold=0.9
```

### SSBT

#### Converting fastq to Bloom filters

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
