#!/usr/bin/env python
"""
Compute some special stats in a howdesbt allsome tree.
"""

from sys              import argv,stdin,stdout,stderr,exit
from howde_tree_parse import read_howde_tree_file


def usage(s=None):
	message = """
usage: cat annotated_tree_file | howde_allsome_stats [options]
  --table        print the table
                 (by default we don't print the table)

  Input is a tree hieracrchy file with three extra columns -- the number of
  bits in the node's bitvectors, the number of 1s in the node's all
  bitvector, and the number of 1s in the node's some bitvector.

  node1.allsome.bf                        40000 0    24205
  *node2.allsome.bf                       40000 0    14482
  **node4.allsome.bf                      40000 187  7867
  ***EXPERIMENT16.subsample.allsome.bf    40000 4351 0
  ***node8.allsome.bf                     40000 267  4263
  ****EXPERIMENT1.subsample.allsome.bf    40000 1982 0
  ****EXPERIMENT15.subsample.allsome.bf   40000 2281 0
  **node5.allsome.bf                      40000 1    9957
  ***node9.allsome.bf                     40000 12   5283
  ****EXPERIMENT19.subsample.allsome.bf   40000 3004 0
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

	forest = read_howde_tree_file(stdin,keepTags=True)
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
