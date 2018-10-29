#!/bin/bash

# order_query_results <fasta1> [<fasta2> ...] <howdesbt_out> <file_to_create>

thisScriptDir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

queryFiles=$1
shift
while [ $# -gt 2 ]; do
    queryFiles="${queryFiles} $1"
    shift
    done

howdesbtOutput=$1
orderedOutput=$2

specificity=0   # everything we search for here is a hit anyway

# collect queries by leaf they hit

cat ${howdesbtOutput} \
  | grep -v "^#" \
  | awk 'BEGIN   { qName = "NA"; }
         /^[*]/  { qName = substr($1,2); }
         !/^[*]/ { print qName,$0 }' \
  | sort -k 2 \
  | awk '{
         if ($2 == leaf)
           queries=queries","$1;
         else
           {  
           if (queries!="") print leaf,queries;
           leaf    = $2;
           queries = $1;
           }
         }
     END {
         if (queries!="") print leaf,queries;
         }' \
  > temp.${howdesbtOutput}.leaf_to_queries

# for each leaf that was hit, perform a single-leaf "countallkmerhits" search
# for just those queries that hit it

leafNum=0
numLeafs=`wc -l temp.${howdesbtOutput}.leaf_to_queries | awk '{ print $1 }'`
:> temp.${howdesbtOutput}.kmers.xxx
cat temp.${howdesbtOutput}.leaf_to_queries \
  | while read leaf queries ; do
      leafNum=$((leafNum+1))
      echo ${leaf}.bf > temp.${howdesbtOutput}.${leaf}.sbt
      numQueries=`echo ${queries} | tr "," " " | awk '{ print NF }'`
      #
      echo "=== #${leafNum} of ${numLeafs}, ${leaf} (${numQueries} queries) ==="
      #
      cat ${queryFiles} \
        | python ${thisScriptDir}/pluck_from_fasta.py --sequences=${queries} \
        | howdesbt query \
          --tree=temp.${howdesbtOutput}.${leaf}.sbt --threshold=${specificity} \
          --countallkmerhits \
        | awk '/^[*]/  { }
               !/^[*]/ { print $0 }' \
        | awk '{ print $1,$3,$4,$5 }' \
        >> temp.${howdesbtOutput}.kmers.xxx
      rm temp.${howdesbtOutput}.${leaf}.sbt
      done

# sort the results by leaf, then by score; note that the order of the queries
# might not match the order in the original howdesbt output

cat temp.${howdesbtOutput}.kmers.xxx \
  | sort -k 1,1d -k 4,4nr \
  > ${orderedOutput}

# cleanup

rm temp.${howdesbtOutput}.leaf_to_queries
rm temp.${howdesbtOutput}.kmers.xxx

