#!/usr/bin/env python
"""
Rename nodes an sabutan tree, so that higher nodes have lower node numbers.
"""

from sys                  import argv,stdin,stdout,stderr,exit
from recover_sabutan_tree import read_sabutan_tree_file

class DrawingControl: pass
dc = DrawingControl()

# $$$ http://ascii-table.com/unicode-index-i.php
#     intersection character is U+2229
#     union        character is U+222A

def usage(s=None):
	message = """
usage: cat sabutan_tree_file | rename_sabutan_nodes <template> [options]
  <template>   template for names to assign to internal tree nodes; this must
               contain the substring {node}"""

	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():
	global debug

	# parse the command line

	nameTemplate = None
	debug        = {}

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg == "--debug"):
			debug["debug"] = True
		elif (arg.startswith("--debug=")):
			for name in argVal.split(","):
				debug[name] = True
		elif (arg.startswith("--")):
			usage("unrecognized option: %s" % arg)
		elif ("{node}" in arg):
			nameTemplate = arg
		else:
			usage("unrecognized option: %s" % arg)

	if (nameTemplate == None):
		usage("you must give me a template for the new node names")

	# read the tree

	tree = read_sabutan_tree_file(stdin,keepFileExtension=True)
	tree.compute_depth()
	preOrder = tree.pre_order()

	# assign new names

	depthToNodes = {}
	for node in preOrder:
		if (node.depth not in depthToNodes): depthToNodes[node.depth] = []
		depthToNodes[node.depth] += [node]

	nodeNumber = 0
	for depth in depthToNodes:
		for node in depthToNodes[depth]:
			if (len(node.children) == 0): continue
			nodeNumber += 1
			node.name = nameTemplate.replace("{node}",str(nodeNumber))

	# output the tree topology, with the new names

	tree.list_pre_order()


if __name__ == "__main__": main()
