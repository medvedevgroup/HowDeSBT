#!/usr/bin/env python
"""
Find the maximum depth in a sabutan tree.
"""

from sys           import argv,stdin,stdout,stderr,exit
from sabutan_parse import read_sabutan_tree_file


def usage(s=None):
	message = """
usage: cat sabutan_tree_file | sabutan_max_depth"""

	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():

	# parse the command line

	if (len(argv) != 1):
		usage("give me no arguments")

	# read the tree

	forest = read_sabutan_tree_file(stdin,keepFileExtension=True)
	assert (len(forest) != 0), "input has no tree"

	# find the deepest node

	maxDepth = None

	for tree in forest:
		tree.compute_depth()
		preOrder = tree.pre_order()
		treeMaxDepth = max([node.depth for node in preOrder])

		if (maxDepth == None) or (treeMaxDepth > maxDepth):
			maxDepth = treeMaxDepth

	print "max depth is %d" % maxDepth


if __name__ == "__main__": main()
