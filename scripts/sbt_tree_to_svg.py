#!/usr/bin/env python
"""
Draw a picture of an SBT listing file, in svg.
"""

from sys              import argv,stdin,stdout,stderr,exit
from math             import ceil
from struct           import unpack
from recover_sbt_tree import read_sbt_tree_file
from pysvg.structure  import svg as Svg
from pysvg.builders   import StyleBuilder
from pysvg.shape      import rect as SvgRect
from pysvg.text       import text as SvgText
from pysvg.shape      import path as SvgPath

class DrawingControl: pass
dc = DrawingControl()


def usage(s=None):
	message = """
usage: cat sbt_listing_file | sbt_tree_to_svg [options]
  --bitvectors=<template>    read bits vectors corresponding to each leaf;
                             this has a form like {leaf}.bf.bv
                             (by default, bit vectors are not read)
  --node:width=<points>      set node/leaf width
  --node:height=<points>     set node/leaf height
  --node:sepwidth=<points>   set node/leaf separating width
  --node:sepheight=<points>  set node/leaf separating height
  --node:font=<points>       set node/leaf font size
  --out=<filename>           this should end with ".svg"
                             (default is sbt_tree_to_svg.svg)"""

	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():
	global dc
	global tree,nameToNode,preOrder,depthToNames
	global debug

	# set drawing controls

	dc.lineThickness   = 0.75
	dc.branchThickness = 0.75
	dc.lineColor       = svg_color((0,0,0))       # (black)
	dc.leafFillColor   = svg_color((.75,.90,.98)) # (light blue)
	dc.nodeFillColor   = svg_color((.95,.95,.95)) # (light gray)
	dc.branchColor     = svg_color((0,0,0))       # (black)

	dc.borderTop     = 2
	dc.borderLeft    = 2
	dc.nodeWidth     = 160
	dc.nodeHeight    = 57
	dc.nodeSepWidth  = 5
	dc.nodeSepHeight = 21

	dc.nameFont      = "Courier New"
	dc.nameFontSize  = 11
	(dc.nameCapsHgt,dc.nameDescHgt) = (16,6)  # (relative to 16 pt font)
	dc.nameFontLineHgt = 19.2                 # (relative to 16 pt font)

	# parse the command line

	bitVectorTemplate = None
	svgFilename       = "sbt_tree_to_svg.svg"
	debug = {}

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg.startswith("--bitvectors=")) or (arg.startswith("--bits=")):
			bitVectorTemplate = argVal
			if (not "{leaf}" in bitVectorTemplate):
				usage("(in %s) template fails to contain {leaf}" % arg)
		elif (arg.startswith("--node:width=")) or (arg.startswith("--node:W=")):
			dc.nodeWidth = float(argVal)
		elif (arg.startswith("--node:height=")) or (arg.startswith("--node:H=")):
			dc.nodeHeight = float(argVal)
		elif (arg.startswith("--node:sepwidth=")) or (arg.startswith("--node:SW=")):
			dc.nodeSepWidth = float(argVal)
		elif (arg.startswith("--node:sepheight=")) or (arg.startswith("--node:SH=")):
			dc.nodeSepHeight = float(argVal)
		elif (arg.startswith("--node:font=")) or (arg.startswith("--node:F=")):
			dc.nameFontSize = float(argVal)
		elif (arg.startswith("--out=")):
			svgFilename = argVal
			if (not svgFilename.endswith(".svg")):
				svgFilename = svgFilename + ".svg"
		elif (arg == "--debug"):
			debug["debug"] = True
		elif (arg.startswith("--debug=")):
			for name in argVal.split(","):
				debug[name] = True
		elif (arg.startswith("--")):
			usage("unrecognized option: %s" % arg)
		else:
			usage("unrecognized option: %s" % arg)

	# read the tree

	tree = read_sbt_tree_file(stdin)
	nameToNode = tree.build_dict()
	preOrder   = tree.pre_order()

	for name in nameToNode:
		node = nameToNode[name]
		if ((node.left == None) != (node.right == None)):
			if (node.left != None):
				assert (False), "node \"%s\" has a left child but no right" % name
			else:
				assert (False), "node \"%s\" has a right child but no left" % name

	depthToNames = {}
	for (depth,name) in preOrder:
		if (depth not in depthToNames): depthToNames[depth] = []
		depthToNames[depth] += [name]

	if ("depth" in debug):
		for depth in depthToNames:
			print >>stderr, "depth %s: %s" % (depth,",".join(depthToNames[depth]))

	if (bitVectorTemplate != None):
		read_leaf_bits(bitVectorTemplate)

	if (bitVectorTemplate != None):
		compute_all_some()

	# layout the tree

	layout_tree()

	# draw the tree

	svg = draw_tree()
	svg.save(svgFilename)


# read_leaf_bits --

def read_leaf_bits(bitVectorTemplate):
	for (_,name) in preOrder:
		node = nameToNode[name]
		isLeaf = (node.left == None)
		if (not isLeaf): continue

		bitVectorFilename = bitVectorTemplate.replace("{leaf}",name)
		(node.numBits,node.bitsUnion) = read_bit_vector(bitVectorFilename)
		node.bitsIntersection = node.bitsUnion
		node.bitsAll          = node.bitsUnion
		node.bitsSome         = 0


# compute_all_some -- 

def compute_all_some():
	maxDepth = max([depth for depth in depthToNames])

	for depth in xrange(maxDepth,-1,-1):
		for name in depthToNames[depth]:
			node = nameToNode[name]
			isLeaf = (node.left == None)
			if (isLeaf): continue

			assert (node.left.numBits == node.right.numBits)
			node.numBits          = node.left.numBits
			node.bitsUnion        = node.left.bitsUnion        | node.right.bitsUnion
			node.bitsIntersection = node.left.bitsIntersection & node.right.bitsIntersection
			node.bitsSome         = node.bitsUnion & ~node.bitsIntersection
			node.bitsAll          = node.bitsIntersection
			node.left.bitsAll     = node.left.bitsAll  & ~node.bitsIntersection
			node.right.bitsAll    = node.right.bitsAll & ~node.bitsIntersection

# read_bit_vector--
#  Bit vectors are stored with 4 leading bytes (little endian) indicating
#  the number of bits in the file, and the next 4 bytes containing some unknown
#  information.  E.g.
#    00000000  00 94 35 77 00 00 00 00  01 00 00 00 00 10 00 00
#    00000010  00 00 00 00 00 00 00 00  00 00 00 04 00 10 00 00

def read_bit_vector(filename):
	f = file(filename,"rb")
	numBits = unpack("<L",f.read(4))[0]

	f.seek(8)

	bits = 0
	for byte in f.read(numBits/8):
		bits = (bits << 8) + reverse_bits(ord(byte))

	f.close()

	if ("bits" in debug):
		numNybbles = (numBits+3) / 4
		print >>stderr, "%s: %0*X" % (filename,numNybbles,bits)

	return (numBits,bits)


# reverse_bits--
#	Reverse the order of the bits in a byte

def reverse_bits(byte):
	reversed = 0;
	for bit in xrange(8):
		reversed <<= 1
		if (byte & 1 != 0): reversed += 1
		byte     >>= 1
	return reversed


# layout_tree--

def layout_tree():
	maxDepth = max([depth for depth in depthToNames])

	# horizontally place leaves at tight spacing across the bottom row;  note
	# that we will shift these to the right later if their ancestors
	# necessitate it

	depth = maxDepth

	xOccupied = 0.0
	for name in depthToNames[depth]:
		node = nameToNode[name]
		node.x = xOccupied + dc.nodeSepWidth
		node.subtreeShift = 0
		xOccupied = node.x + dc.nodeWidth

		if ("layout" in debug):
			print >>stderr, "[d=%d] %s.x <- %.2f" % (depth,name,node.x)

	if ("layout" in debug):
		print >>stderr

	# moving up the tree, horizontally place nodes of successive levels;  we
	# consider two positions for each node, the midpoint of its children (if
	# it is not a leaf), or adjacent to the most recently occupied position;
	# when the midpoint would overlap the occupied position, which shift the
	# node to the right, and record the amount of shift to be applied to its
	# subtree

	for depth in xrange(maxDepth-1,-1,-1):
		xOccupied = 0.0
		xShifted  = 0.0
		for name in depthToNames[depth]:
			node = nameToNode[name]

			if (node.left == None):
				node.x = xOccupied + dc.nodeSepWidth
				node.subtreeShift = 0
				if ("layout" in debug):
					print >>stderr, "[d=%d] %s.x <- %.2f" \
					              % (depth,name,node.x)
			else:
				xMiddle = xShifted + (node.left.x + node.right.x) / 2.0
				if (xMiddle >= xOccupied + dc.nodeSepWidth):
					node.x = xMiddle
					node.subtreeShift = xShifted
					if ("layout" in debug):
						print >>stderr, "[d=%d] %s.x <- %.2f" \
						              % (depth,name,node.x)
				else:
					node.x = xOccupied + dc.nodeSepWidth
					subtreeShift = node.x - xMiddle
					node.subtreeShift = subtreeShift + xShifted
					xShifted += subtreeShift
					if ("layout" in debug):
						print >>stderr, "[d=%d] %s.x <- %.2f (shifted %.2f from middle=%.2f)" \
						              % (depth,name,node.x,
						                 node.subtreeShift,xMiddle)

			xOccupied = node.x + dc.nodeWidth

		if ("layout" in debug):
			print >>stderr

	# propagate subtree shifts, from the top of the tree down

	for depth in xrange(maxDepth+1):
		for name in depthToNames[depth]:
			node = nameToNode[name]
			if (node.parent == None): continue
			if (node.parent.subtreeShift == 0.0): continue
			node.x            += node.parent.subtreeShift
			node.subtreeShift += node.parent.subtreeShift
			if ("layout" in debug):
				print >>stderr, "[d=%d] %s.x <- %.2f (%s shifted %.2f)" \
				              % (depth,name,node.x,
				                 node.parent.name,node.parent.subtreeShift)

	if ("layout" in debug):
		print >>stderr

	# assign vertical positions (based simply on the node's depth), and
	# calculate the leftmost position

	minX = None
	yDrawing = dc.borderTop
	for depth in depthToNames:
		for name in depthToNames[depth]:
			node = nameToNode[name]
			node.y = yDrawing
			if (minX == None) or (node.x < minX):
				minX = node.x
		yDrawing += dc.nodeHeight + dc.nodeSepHeight

	# shift nodes so that leftmost position is on the border

	for depth in depthToNames:
		for name in depthToNames[depth]:
			node = nameToNode[name]
			node.x += dc.borderLeft - minX
			#if ("layout" in debug):
			#	print >>stderr, "%s.x <- %.2f" % (name,node.x)

	#if ("layout" in debug):
	#	print >>stderr


# draw_tree--

def draw_tree():
	if (dc.nameFontSize < 16):
		scale = dc.nameFontSize / 16.0
		nameCapsHgt     = dc.nameCapsHgt     * scale
		nameDescHgt     = dc.nameDescHgt     * scale
		nameFontLineHgt = dc.nameFontLineHgt * scale

	nameStyle = StyleBuilder()
	nameStyle.setFontFamily(fontfamily=dc.nameFont)
	nameStyle.setFontSize("%spt" % dc.nameFontSize)
	nameStyle.setTextAnchor("left")
	nameStyle = nameStyle.getStyle()

	svg = Svg()

	# draw nodes

	for depth in depthToNames:
		for name in depthToNames[depth]:
			node = nameToNode[name]
			isLeaf = (node.left == None)

			yLine = node.y + nameCapsHgt + 1

			ob = SvgRect(node.x,node.y,dc.nodeWidth,dc.nodeHeight,
			             id="%s_box" % name)
			ob.set_stroke(dc.lineColor)
			ob.set_stroke_width(dc.lineThickness)
			if (isLeaf): ob.set_fill(dc.leafFillColor)
			else:        ob.set_fill(dc.nodeFillColor)
			svg.addElement(ob)

			ob = SvgText("%s" % name,
						 node.x+1,yLine,
						 id="%s_name" % node)
			ob.set_style(nameStyle)
			svg.addElement(ob)
			yLine += nameFontLineHgt

			if (hasattr(node,"bitsUnion")):
				ob = SvgText("U:"+bits_to_string(node.numBits,node.bitsUnion),
							 node.x+1,yLine,
							 id="%s_Bunion" % node)
				ob.set_style(nameStyle)
				svg.addElement(ob)
			yLine += nameFontLineHgt

			if (hasattr(node,"bitsIntersection")):
				ob = SvgText("I:"+bits_to_string(node.numBits,node.bitsIntersection),
							 node.x+1,yLine,
							 id="%s_Bintersection" % node)
				ob.set_style(nameStyle)
				svg.addElement(ob)
			yLine += nameFontLineHgt

			if (hasattr(node,"bitsAll")):
				ob = SvgText("A:"+bits_to_string(node.numBits,node.bitsAll),
							 node.x+1,yLine,
							 id="%s_Ball" % node)
				ob.set_style(nameStyle)
				svg.addElement(ob)
			yLine += nameFontLineHgt

			if (hasattr(node,"bitsSome")):
				ob = SvgText("S:"+bits_to_string(node.numBits,node.bitsSome),
							 node.x+1,yLine,
							 id="%s_Bsome" % node)
				ob.set_style(nameStyle)
				svg.addElement(ob)
			yLine += nameFontLineHgt

	# draw branches

	for depth in depthToNames:
		for name in depthToNames[depth]:
			node = nameToNode[name]
			if (node.left == None): continue

			(leftStartX, leftStartY)  = (node.x + 0.4*dc.nodeWidth, node.y + dc.nodeHeight)
			(rightStartX,rightStartY) = (node.x + 0.6*dc.nodeWidth, node.y + dc.nodeHeight)
			(leftEndX,   leftEndY)    = (node.left.x  + 0.5*dc.nodeWidth, node.right.y)
			(rightEndX,  rightEndY)   = (node.right.x + 0.5*dc.nodeWidth, node.right.y)

			draw_branch(svg,"%s_left_branch" %node,leftStartX, leftStartY, leftEndX, leftEndY)
			draw_branch(svg,"%s_right_branch"%node,rightStartX,rightStartY,rightEndX,rightEndY)

	return svg


# draw_branch--

def draw_branch(svg,id,xs,ys,xe,ye):
	svgPath =  ["M%f,%f" % (xs,ys)]
	svgPath += ["C%f,%f %f,%f %f,%f" % (xs,ye,xe,ys,xe,ye)]

	ob = SvgPath(pathData=" ".join(svgPath),id=id)
	ob.set_stroke_width(dc.branchThickness)
	ob.set_stroke(dc.branchColor)
	ob.set_fill("none")
	svg.addElement(ob)


# bits_to_string--

def bits_to_string(numBits,bits):
	s = format(bits,"0%db"%numBits)
	ss = []
	for ix in xrange(0,numBits,4):
		ss += [s[ix:ix+4]]
	s = " ".join(ss)
	return s


# svg_color--
#	Convert a color from the 0..1 box to the 0..255 box

def svg_color((r,g,b)):
	return "rgb(%d,%d,%d)" \
	     % (int(ceil(255*r)),int(ceil(255*g)),int(ceil(255*b)))


if __name__ == "__main__": main()
