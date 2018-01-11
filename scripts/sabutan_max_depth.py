#!/usr/bin/env python
"""
Find the maximum depth in a sabutan tree.
"""

from sys                  import argv,stdin,stdout,stderr,exit
from recover_sabutan_tree import read_sabutan_tree_file


def usage(s=None):
	message = """
usage: cat sabutan_tree_file | sabutan_max_depth"""

	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():

	# parse the command line

	if (len(argv != 1)):
		usage("give me no arguments")

	# read the tree

	tree = read_sabutan_tree_file(stdin,keepFileExtension=True)

	# find the deepest node

	tree.compute_depth()
	preOrder = tree.pre_order()

	maxDepth = max([node.depth for node in preOrder])
	print "max depth is %d" % maxDepth


if __name__ == "__main__": main()
