#!/usr/bin/env python
"""
Report sibling sets in a howde tree.
"""

from sys              import argv,stdin,stdout,stderr,exit
from howde_tree_parse import read_howde_tree_file


def usage(s=None):
	message = """
usage: cat howde_tree_file | howde_siblings [options]
  --leafonly     only report sibling sets that are all leaves
  --withparent   report the parents as the first column in each line
                 (by default we only report siblings)"""

	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():

	# parse the command line

	leavesOnly   = False
	reportParent = False

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg in ["--leafonly","--leavesonly","--onlyleaves"]):
			leavesOnly = True
		elif (arg in ["--withparent","--withparents","--parent","--parents"]):
			reportParent = True
		else:
			usage("unrecognized option: %s" % arg)

	# read the tree

	forest = read_howde_tree_file(stdin,keepFileExtension=True)
	assert (len(forest) != 0), "input has no tree"

	# report the siblings

	if (len(forest) > 1):
		if (reportParent): print "forest",
		print " ".join([root.name for root in forest])

	for root in forest:
		preOrder = root.pre_order()
		for node in preOrder:
			if (node.children == []): continue
			if (leavesOnly):
				isAllLeaves = True
				for child in node.children:
					if (child.children != []):
						isAllLeaves = False
						break
				if (not isAllLeaves): continue
			if (reportParent): print node.name,
			print " ".join([child.name for child in node.children])


if __name__ == "__main__": main()
