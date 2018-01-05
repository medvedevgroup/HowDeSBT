#!/usr/bin/env python
"""
Recover the tree relationship from a sabutan tree hierarchy file.

The tree hierarchy file looks something like this (below).  It is a pre-order
traversal of the tree, and the asterisks indicate the depth of each node.
  node5169.bf
  *node5160.bf
  **SRR765.bf
  **node5146.bf
  ***SRR364.bf
  ***node5124.bf
  ****SRR767.bf
  ****SRR673.bf
  ****SRR211.bf
  *SRR769.bf
   ...
"""

from sys import argv,stdin,stdout,stderr,exit


def usage(s=None):
	message = """
usage: cat sabutan_tree_file | recover_sabutan_tree [options]
  --show=preorder        list the tree in pre-order
                         (this is the default)
  --show=leafgroups      list all leaf groups
  --show=height          for each node, list max distance to a leaf, and number
                         of descendants
  --show:subtree=<node>  create a listing file for a node and its descendants
  --filespec=<spec>      spec describing how node names are to be output; for
                         example /usr/nwu253/sabutan/compressed/{name}.rrr.bf"""

	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():
	global debug

	# parse the command line

	showWhat = "pre order"
	fileSpec = None
	debug    = {}

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg == "--show=preorder"):
			showWhat = "pre order"
		elif (arg == "--show=leafgroups"):
			showWhat = "leaf groups"
		elif (arg == "--show=height"):
			showWhat = "height etc"
		elif (arg.startswith("--show:subtree=")) or (arg.startswith("--subtree=")):
			showWhat = "subtree"
			nodeName = argVal
		elif (arg.startswith("--filespec=")):
			if ("{name}" not in argVal):
				usage("filespec MUST contain {name}\n(in \"%s\"" % arg)
			fileSpec = argVal
		elif (arg == "--debug"):
			debug["debug"] = True
		elif (arg.startswith("--debug=")):
			for name in argVal.split(","):
				debug[name] = True
		elif (arg.startswith("--")):
			usage("unrecognized option: %s" % arg)
		else:
			usage("unrecognized option: %s" % arg)

	# process the tree

	tree = read_sabutan_tree_file(stdin)

	if (showWhat == "pre order"):
		tree.list_pre_order()
	elif (showWhat == "leaf groups"):
		tree.list_leaf_groups()
	elif (showWhat == "height etc"):
		tree.compute_height_etc()
		tree.list_height_etc()
	elif (showWhat == "subtree"):
		nameToNode = tree.build_dict()
		assert (nodeName in nameToNode), \
		       "unknown node: \"%s\"" % nodeName
		subtree = nameToNode[nodeName]
		subtree.list_pre_order(fileSpec=fileSpec)
	else:
		assert (False), \
		       "internal error: unknown operation \"%s\"" % showWhat


def read_sabutan_tree_file(f,keepFileExtension=False):
	nodeStack = []
	topLevelSame = False
	topLevel = None

	for (level,name) in read_sabutan_list(f,keepFileExtension=keepFileExtension):
		#print >>stderr, "read %d %s" % (level,name)
		while (topLevelSame) and (level < topLevel):
			(sibLevel,sibling) = nodeStack.pop()
			siblings = [sibling]
			(peekLevel,_) = nodeStack[-1]
			while (peekLevel == sibLevel):
				(_,sibling) = nodeStack.pop()
				siblings += [sibling]
				(peekLevel,_) = nodeStack[-1]
			siblings.reverse()
			(parentLevel,parent) = nodeStack[-1]
			assert (parentLevel == topLevel-1)

			#print >>stderr, "  assigning %s children %s" \
			#              % (parent.name,
			#                 ",".join([child.name for child in children]))
			parent.children = siblings
			for sibling in siblings:
				sibling.parent = parent

			topLevel = parentLevel
			if (len(nodeStack) < 2):
				topLevelSame = False
			else:
				(prevLevel,_) = nodeStack[-2]
				topLevelSame = (prevLevel == topLevel)

		if (level < 0): break   # (end-of-list)

		#print >>stderr, "pushing %d %s" % (level,name)
		nodeStack += [(level,TreeNode(name))]
		topLevelSame = (level == topLevel)
		topLevel = level

	assert (len(nodeStack) == 1)
	(_,tree) = nodeStack[0]

	return tree


def read_sabutan_list(f,keepFileExtension=False):
	for line in f:
		line = line.strip()

		indent = 0
		while (line[indent] == "*"):
			indent += 1

		name = line[indent:]
		if ("/" in name): name = name.split("/")[-1]
		if (not keepFileExtension): name = name.split(".bf")[0]

		yield (indent,name)

	yield (-1,"end-of-list")


class TreeNode(object):

	def __init__(self,name,parent=None,children=None):
		self.name = name
		if (children == None): self.children = []
		else:                  self.children = list(children)
		self.parent = parent
		self.depth  = None

	def compute_depth(self,depth=1):  # root has depth 1
		self.depth = depth
		for child in self.children:
			child.compute_depth(depth+1)

	def build_dict(self,nameToNode=None):
		if (nameToNode == None): nameToNode = {}
		nameToNode[self.name] = self
		for child in self.children:
			child.build_dict(nameToNode)
		return nameToNode

	def pre_order(self,order=None):
		if (order == None): order = [self]
		else:               order += [self]
		for child in self.children:
			child.pre_order(order)
		return order

	def list_pre_order(self,f=None,indent=0,fileSpec=None):
		if (f == None): f = stdout
		name = self.name
		if (fileSpec != None):
			name = fileSpec.replace("{name}",name)
		print >>f, "%s%s" % ("*"*indent,name)
		for child in self.children:
			child.list_pre_order (f,indent=indent+1,fileSpec=fileSpec)

	def list_leaf_groups(self,f=None):
		if (f == None): f = stdout

		allChildrenAreLeafs = True
		for child in self.children:
			childIsLeaf = (child.children == [])
			if (not childIsLeaf):
				allChildrenAreLeafs = False
				break

		if (allChildrenAreLeafs):
			print >>f, " ".join([child.name for child in self.children])
		else:
			for child in self.children:
				child.list_leaf_groups(f)

	def compute_height_etc(self):
		self.height      = 1   # (longest path to a leaf, including self)
		self.subtreeSize = 1   # (nodes and leaves in subtreee, including self)
		for child in self.children:
			(childHeight,childSubtreeSize) = child.compute_height_etc()
			self.height = max(self.height,childHeight+1)
			self.subtreeSize += childSubtreeSize
		return (self.height,self.subtreeSize)

	def list_height_etc(self,f=None,indent=0):
		if (f == None): f = stdout
		print >>f, "%s%s\theight=%d\tdescendants=%d" \
		         % ("*"*indent,self.name,self.height,self.subtreeSize-1)
		for child in self.children:
			child.list_height_etc(f,indent=indent+1)


if __name__ == "__main__": main()
