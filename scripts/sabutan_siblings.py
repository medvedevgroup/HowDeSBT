#!/usr/bin/env python
"""
Report sibling sets in a sabutan tree.
"""

from sys                import argv,stdin,stdout,stderr,exit
from sabutan_tree_parse import read_sabutan_tree_file


def usage(s=None):
	message = """
usage: cat sabutan_tree_file | sabutan_siblings [options]
  --withparent   report the parents as the first column in each line
                 (by default we only report siblings)"""

	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():

	# parse the command line

	reportParent = False

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg in ["--withparent","--withparents","--parent","--parents"]):
			reportParent = True
		else:
			usage("unrecognized option: %s" % arg)

	# read the tree

	forest = read_sabutan_tree_file(stdin,keepFileExtension=True)
	assert (len(forest) != 0), "input has no tree"

	# report the siblings

	if (len(forest) > 1):
		if (reportParent): print "forest",
		print " ".join([root.name for root in forest])

	for root in forest:
		preOrder = root.pre_order()
		for node in preOrder:
			if (node.children == []): continue
			if (reportParent): print node.name,
			print " ".join([child.name for child in node.children])


if __name__ == "__main__": main()
