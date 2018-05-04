#!/usr/bin/env python
"""
Parsing support for sabutan tree hierarchy files.

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


def read_sabutan_tree_file(f,keepFileExtension=False,debug=False):
	nodeStack = []
	topLevelSame = False
	topLevel = None
	forest = []

	for (level,name) in read_sabutan_list(f,keepFileExtension=keepFileExtension):
		if (debug):
			print >>stderr, "read %d %s" % (level,name)
			for (dgbLevel,dbgNode) in nodeStack:
				print >>stderr, "  %d %s" % (dgbLevel,dbgNode.name)

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

			if (debug):
				print >>stderr, "  assigning %s children %s" \
				              % (parent.name,
				                 ",".join([child.name for child in siblings]))
			parent.children = siblings
			for sibling in siblings:
				sibling.parent = parent

			topLevel = parentLevel
			if (len(nodeStack) > 1):
				(prevLevel,_) = nodeStack[-2]
				topLevelSame = (prevLevel == topLevel)
			else:
				topLevelSame = False
				(rootLevel,tree) = nodeStack.pop()
				assert (rootLevel == 0)
				forest += [tree]
				if (debug):
					print >>stderr, "adding %s to forest" % tree.name

		if (level < 0): break   # (end-of-list)

		if (debug):
			print >>stderr, "pushing %d %s" % (level,name)
		nodeStack += [(level,TreeNode(name))]
		topLevelSame = (level == topLevel)
		topLevel = level

	if (debug):
		print >>stderr, "returning [%s]" \
		              % ",".join([tree.name for tree in forest])

	return forest


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

