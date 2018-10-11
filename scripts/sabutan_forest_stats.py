#!/usr/bin/env python
"""
Report simple stats about the forest in a sabutan tree.
"""

from sys                import argv,stdin,stdout,stderr,exit
from sabutan_tree_parse import read_sabutan_tree_file


def usage(s=None):
	message = """
usage: cat sabutan_tree_file | sabutan_forest_stats"""

	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():

	# parse the command line

	if (len(argv) > 1): usage("give me no arguments")

	# read the tree

	forest = read_sabutan_tree_file(stdin,keepFileExtension=True)
	assert (len(forest) != 0), "input has no tree"

	# collect the stats

	stats = []

	for tree in forest:
		tree.compute_depth()
		preOrder = tree.pre_order()

		numLeaves = 0
		for node in preOrder:
			if (len(node.children) == 0): numLeaves += 1

		treeDepth = max([node.depth for node in preOrder])

		stats += [(len(preOrder),numLeaves,treeDepth,tree.name)]

	# report the stats

	stats.sort()
	stats.reverse()

	print "# %d tree(s)" % len(forest)

	for (treeSize,numLeaves,treeDepth,name) in stats:
		print "%s treesize=%d leaves=%d depth=%s" \
		    % (name,treeSize,numLeaves,treeDepth)


if __name__ == "__main__": main()
