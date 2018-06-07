#!/usr/bin/env python
"""
Compute some special stats in a sabutan allsome tree.
"""

from sys                import argv,stdin,stdout,stderr,exit
from sabutan_tree_parse import read_sabutan_tree_file


def usage(s=None):
	message = """
usage: cat annotated_tree_file | sabutan_allsome_stats [options]
  --table        print the table
                 (by default we don't print the table)

  Input is a tree hieracrchy file with two extra columns, the number of 1s in
  the node's all and some bitvectors, respectively.

  node1.allsome.bf          0        1981168343
  *node2.allsome.bf         408      886469084
  **node4.allsome.bf        95       498278915
  ***node8.allsome.bf       640      285714006
  ****node16.allsome.bf     28013409 134312368
  *****SRR953492.allsome.bf 64744298 0
  *****SRR953493.allsome.bf 69568070 0
  ****node17.allsome.bf     0        174165865
  *****node26.allsome.bf    869      82689667
  ******node44.allsome.bf   5639628  36696635
   ..."""


	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():

	# parse the command line

	reportTable = False
	fileSpec = None

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg == "--table"):
			reportTable = True
		elif (arg.startswith("--")):
			usage("unrecognized option: %s" % arg)
		else:
			usage("unrecognized option: %s" % arg)

	# read the tree

	forest = read_sabutan_tree_file(stdin,keepTags=True)
	assert (len(forest) != 0), "input has no tree"

	# compute the stats

	if (reportTable):
		print "#node\tbits\tall\tsome\tpresent\tabsent\tunresolved"

	sumPresent = sumAbsent = 0
	for root in forest:
		preOrder = root.pre_order()
		for node in preOrder:
			assert (len(node.tags) == 3)
			node.bfSize   = int(node.tags[0])
			node.allOnes  = int(node.tags[1])
			node.someOnes = int(node.tags[2])

			node.present    = node.allOnes
			node.unresolved = node.someOnes

			parent = node.parent
			if (parent == None):
				node.absent = node.bfSize - (node.allOnes + node.someOnes)
			else:
				assert (node.bfSize == parent.bfSize)
				node.absent = parent.unresolved - (node.allOnes + node.someOnes)

			sumPresent += node.present
			sumAbsent  += node.absent

			if (reportTable):
				print "%s\t%d\t%d\t%d\t%d\t%d\t%d" \
				    % (node.name,
				       node.bfSize,node.allOnes,node.someOnes,
				       node.present,node.absent,node.unresolved)

	denom = float(sumPresent+sumAbsent)
	output = "present/absent = %d/%d = %.1f%%/%.1f%%" \
	       % (sumPresent,sumAbsent,100*sumPresent/denom,100*sumAbsent/denom)

	if (reportTable): print >>stderr, output
	else:             print           output
	

if __name__ == "__main__": main()
