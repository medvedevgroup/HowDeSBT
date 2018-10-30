## Experiments from the HowDe-SBT manuscript

### Preliminaries

#### SRA files

Evaluation was based on 2,585 human RNA-seq runs from blood, brain, and breast
tissues, downloaded from the Sequence Read Archive (SRA),
(https://www.ncbi.nlm.nih.gov/sra). The accession numbers for these runs are
listed in experiment_list.txt.

These are the same RNA-seq runs used in the original Solomon-Kingsford SBT
manuscript, and manuscripts for AllSome-SBT and SSBT. However, 67 of the
RNA-seq runs used in those projects were effectively empty &mdash; they were
comprised of reads shorter than 20 bp and thus corresponded to empty bloom
filters. This were eliminated in this project. The mantis manuscript also uses
this dataset, reduced to 2,586 runs.

#### Abundance thresholds

Kmer abundance thresholds for the RNA-seq runs are listed in
abundance_thresholds.mantis.txt. This file was copied from the mantis
distribution (see section on mantis below for which version), but excluding the
67 empty runs. These thresholds were used to evaluate all four tools.

Earlier evaluation used different abundance thresholds (mentioned briefly in
the manuscript). These are found in abundance_thresholds.sk_inferred.txt.
These were reverse engineered by observing the number of 1s in each Bloom
filter provided to support the original Solomon-Kingsford SBT manuscript,
estimating the number of kmers added to the bloom filter, then finding the
abundance threshold that best matched hat number of kmers

The reverse engineered thresholds are in many cases lower than the mantis
thresholds, thus accepting more distinct kmers into the data structures.
Unfortunately we had difficulty parameterizing mantis to work with these
thresholds, so we used the higher thresholds so that we could include mantis
in our comparisons.

#### Converting SRA files to jellyfish

In order to facilitate experimentation with different abundance thresholds,
each experiment was processed with jellyfish to save kmers and counts.

Jellyfish version 2.2.6 was used.

```bash  
fastq-dump --fasta experiment1.sra --stdout \
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

AllSome-SBT was fetched as follows on Feb/14/2018.
```bash  
git clone https://github.com/medvedevgroup/bloomtree-allsome
```

#### Converting jellyfish files to Bloom filters

AllSome-SBT uses the same file format as SSBT, so we share the same leaf
Bloom filter files created by SSBT.

Note that AllSome-SBT and SSBT share source code ancestry &mdash; the leaf
Bloom filters AllSome-SBT would have built would be identical to those SSBT
built, and the run time is expected to be the same.

#### Determining tree topology

For direct comparison to HowDe-SBT, we borrow the tree topology created by
HowDe-SBT. However, we need the non-culled version of that tree. So we use
HowDe-SBT to create the topology, and perform a simple topology format
conversion.

Note that AllSome-SBT and HowDe-SBT use the same clustering process to create
a tree topology. Though HowDe-SBT has re-implemented this process, we expect
the run time to be similar. Moreover, this is a small fraction of the total
construction time (roughly 5 to 10 minutes).

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

Split SBT Version 0.1 was used, from
https://github.com/Kingsford-Group/splitsbt/releases/tag/v.0.1 .

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

Squeakr exact was fetched as follows on May/17/2018.
```bash  
git clone https://github.com/splatlab/squeakr --branch exact
```

Mantis was fetched as follows on May/15/2018.
```bash  
git clone https://github.com/splatlab/mantis
```

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

```bash  
mantis query -p CDBG/ -o query_batch.mantis.out query_batch.fa
```

