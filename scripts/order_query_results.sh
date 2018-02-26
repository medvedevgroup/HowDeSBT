#!/bin/bash

queryFile=$1
sabutanOutput=$2
orderedOutput=$3
specificity=0   # everything we search for here is a hit anyway

thisScriptDir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
echo ${thisScriptDir}

# collect queries by leaf they hit

cat ${sabutanOutput} \
  | grep -v "^#" \
  | sed "s/.*\///" \
  | sed "s/\..*\.bf/.bf/" \
  | awk 'BEGIN   { qName = "NA"; }
         !/^SRR/ { qName = substr($1,2); }
         /^SRR/  { print qName,$0 }' \
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
  > temp.${sabutanOutput}.leaf_to_queries

# for each leaf that was hit, perform a single-leaf "countallkmerhits" search
# for just those queries that hit it

leafNum=0
numLeafs=`wc -l temp.${sabutanOutput}.leaf_to_queries | awk '{ print $1 }'`
:> temp.${sabutanOutput}.kmers.xxx
cat temp.${sabutanOutput}.leaf_to_queries \
  | while read leaf queries ; do
      leafNum=$((leafNum+1))
      echo ${leaf}.bf > temp.${sabutanOutput}.${leaf}.sbt
      numQueries=`echo ${queries} | tr "," " " | awk '{ print NF }'`
      #
      echo "=== #${leafNum} of ${numLeafs}, ${leaf} (${numQueries} queries) ==="
      #
      cat ${queryFile} \
        | python ${thisScriptDir}/pluck_from_fasta.py --sequences=${queries} \
        | ${sabutan} query \
          --tree=temp.${sabutanOutput}.${leaf}.sbt --threshold=${specificity} \
          --countallkmerhits \
        | awk '/^[*]/  { }
               !/^[*]/ { print $0 }' \
        | awk '{ print $1,$3,$4,$5 }' \
        >> temp.${sabutanOutput}.kmers.xxx
      rm temp.${sabutanOutput}.${leaf}.sbt
      done

# sort the results by leaf, then by score; note that the order of the queries
# might not match the order in the original sabutan output

cat temp.${sabutanOutput}.kmers.xxx \
  | sort -k 1,1d -k 4,4nr \
  > ${orderedOutput}

# cleanup

rm temp.${sabutanOutput}.leaf_to_queries
rm temp.${sabutanOutput}.kmers.xxx

