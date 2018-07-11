#!/usr/bin/env python
"""
Draw a picture of an sabutan tree, in svg.
"""

from sys                import argv,stdin,stdout,stderr,exit
from math               import ceil
from struct             import unpack
from sabutan_tree_parse import read_sabutan_tree_file
from pysvg.structure    import svg as Svg
from pysvg.builders     import StyleBuilder
from pysvg.shape        import rect as SvgRect
from pysvg.text         import text as SvgText
from pysvg.shape        import path as SvgPath

class DrawingControl: pass
dc = DrawingControl()


def usage(s=None):
	message = """
usage: cat sabutan_tree_file | sabutan_tree_to_svg [options]
  --bitvectors=<template>    read bits vectors corresponding to each leaf; this
                             has a form like {leaf}.bf.bv
                             (by default, bit vectors are not read)
  --group=<N>                show bits in groups of N
                             (default is 5)
  --nodes:union              show nodes with cup and cap fields
                             (this is the default)
  --nodes:allsome            show nodes with all/some fields
  --nodes:determined         show nodes with determined/how fields
  --nodes:determined,active  show nodes with determined/how fields, with active
                             bits
  --bits:squares             show bits as filled squares
                             (this is the default)
  --bits:binary              show bits in binary
                             (--group is ignored, we group by 4s)
  --top-to-bottom            tree root is at top
                             (this is the default)
  --left-to-right            tree root is at left
  --out=<filename>           this should end with ".svg"
                             (default is sabutan_tree_to_svg.svg)
  --node:width=<points>      node/leaf width
  --node:height=<points>     node/leaf height
  --node:sepwidth=<points>   node/leaf separating width
  --node:sepheight=<points>  node/leaf separating height
  --node:font=<points>       node/leaf font size"""

	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():
	global dc,orientation,showNodesAs,showBitsAs,bitGroupSizes
	global tree,nameToNode,preOrder,depthToNames
	global debug

	# set drawing controls

	dc.lineThickness       = 0.75
	dc.branchThickness     = 0.75
	dc.bitOutlineThickness = 0.75
	dc.lineColor           = svg_color((0,0,0))       # (black)
	dc.leafFillColor       = svg_color((.75,.90,.98)) # (light blue)
	dc.nodeFillColor       = svg_color((.95,.95,.95)) # (light gray)
	dc.branchColor         = svg_color((0,0,0))       # (black)
	dc.bitOutlineColor     = svg_color((.5,.5,.5))    # (gray)
	dc.bit1Color           = svg_color((0,0,0))       # (black)
	dc.bit0Color           = svg_color((1,1,1))       # (white)
	dc.bitXColor           = svg_color((.75,.75,.75)) # (gray)

	dc.borderTop      = 2
	dc.borderLeft     = 2
	dc.borderBottom   = dc.borderTop
	dc.borderRight    = dc.borderLeft
	dc.nodeWdt        = 160
	dc.nodeHgt        = 57
	dc.nodeSepWdt     = 5
	dc.nodeSepHgt     = 21
	dc.bitWdt         = 10
	dc.bitHgt         = dc.bitWdt
	dc.bitGroupSepWdt = 3
	dc.bitPrefixSkip  = 1.4*dc.bitWdt

	dc.nameFont      = "Courier New"
	dc.nameFontSize  = 11
	(dc.nameCapsHgt,dc.nameDescHgt) = (16,6)  # (relative to 16 pt font)
	dc.nameFontLineHgt = 19.2                 # (relative to 16 pt font)
	dc.nameFontCharWdt = 12.8                 # (relative to 16 pt font)

	# parse the command line

	bitVectorTemplate = None
	svgFilename       = "sabutan_tree_to_svg.svg"
	orientation       = "T2B"
	bitGroupSizes     = None
	showNodesAs       = "union"
	showBitsAs        = "squares"
	debug             = {}

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg in ["--bitvectors","--bits="]):
			bitVectorTemplate = "{leaf}"
		elif (arg.startswith("--bitvectors=")) or (arg.startswith("--bits=")):
			bitVectorTemplate = argVal
			if (not "{leaf}" in bitVectorTemplate):
				usage("(in %s) template fails to contain {leaf}" % arg)
		elif (arg in ["--group=none","--chunk=none"]):
			bitGroupSizes = []
		elif (arg.startswith("--group=")) or (arg.startswith("--chunk=")):
			bitGroupSizes = map(int,argVal.split(","))
		elif (arg == "--nodes:union"):
			showNodesAs = "union"
		elif (arg == "--nodes:allsome"):
			showNodesAs = "all/some"
		elif (arg == "--nodes:determined") or (arg == "--nodes:det"):
			showNodesAs = "determined/how"
		elif (arg == "--nodes:determined,active")      or (arg == "--nodes:det,active") \
		  or (arg == "--nodes:determined,informative") or (arg == "--nodes:det,informative"):
			showNodesAs = "determined/how/active"
		elif (arg == "--bits:squares") or (arg == "--bits:square"):
			showBitsAs = "squares"
		elif (arg == "--bits:binary"):
			showBitsAs = "text"
		elif (arg in ["--top-to-bottom","--toptobottom","--top2bottom"]):
			orientation = "T2B"
		elif (arg in ["--left-to-right","--lefttoright","--left2right"]):
			orientation = "L2R"
		elif (arg.startswith("--out=")):
			svgFilename = argVal
			if (not svgFilename.endswith(".svg")):
				svgFilename = svgFilename + ".svg"
		elif (arg.startswith("--node:width=")) or (arg.startswith("--node:W=")):
			dc.nodeWdt = float(argVal)
		elif (arg.startswith("--node:height=")) or (arg.startswith("--node:H=")):
			dc.nodeHgt = float(argVal)
		elif (arg.startswith("--node:sepwidth=")) or (arg.startswith("--node:SW=")):
			dc.nodeSepWdt = float(argVal)
		elif (arg.startswith("--node:sepheight=")) or (arg.startswith("--node:SH=")):
			dc.nodeSepHgt = float(argVal)
		elif (arg.startswith("--node:font=")) or (arg.startswith("--node:F=")):
			dc.nameFontSize = float(argVal)
		elif (arg == "--debug"):
			debug["debug"] = True
		elif (arg.startswith("--debug=")):
			for name in argVal.split(","):
				debug[name] = True
		elif (arg.startswith("--")):
			usage("unrecognized option: %s" % arg)
		else:
			usage("unrecognized option: %s" % arg)

	if (bitGroupSizes == None):
		bitGroupSizes = [5]
	elif (bitGroupSizes == []):
		bitGroupSizes = None

	if (bitGroupSizes != None) and (len(bitGroupSizes) > 1):
		usage("multiple groups aren't implemented yet (sorry)")

	# adjust the layout parameters

	if (dc.nameFontSize < 16):
		scale = dc.nameFontSize / 16.0
		dc.nameCapsHgt     *= scale
		dc.nameDescHgt     *= scale
		dc.nameFontLineHgt *= scale
		dc.nameFontCharWdt *= scale

	if (orientation == "L2R"):
		(dc.nodeSepWdt,dc.nodeSepHgt) = (dc.nodeSepHgt,dc.nodeSepWdt)

	# read the tree

	forest = read_sabutan_tree_file(stdin,debug="treeparse" in debug)
	assert (len(forest) != 0), "input has no tree"
	assert (len(forest) == 1), "input is a forest (not supported yet)"
	tree = forest[0]

	tree.compute_depth()
	nameToNode = tree.build_dict()
	preOrder   = tree.pre_order()

	for name in nameToNode:
		node = nameToNode[name]
		if (len(node.children) == 1):
			assert (False), "node \"%s\" has exactly one child" % name

	depthToNames = {}
	for node in preOrder:
		if (node.depth not in depthToNames): depthToNames[node.depth] = []
		depthToNames[node.depth] += [node.name]

	if ("depth" in debug):
		for depth in depthToNames:
			print >>stderr, "depth %s: %s" % (depth,",".join(depthToNames[depth]))

	if (bitVectorTemplate != None):
		read_leaf_bits(bitVectorTemplate)
		node = find_a_leaf()
		dc.nodeWdt = drawn_bits_width(node.numBits) + 3

	if (bitVectorTemplate != None):
		if (showNodesAs == "all/some"):
			compute_all_some()
			dc.nodeHgt = 5 * dc.nameFontLineHgt + 3
		elif (showNodesAs == "determined/how"):
			compute_determined_how(withActive=False)
			dc.nodeHgt = 6 * dc.nameFontLineHgt + 3
		elif (showNodesAs == "determined/how/active"):
			compute_determined_how(withActive=True)
			dc.nodeHgt = 6 * dc.nameFontLineHgt + 3
		else: # if (showNodesAs == "union"):
			compute_union()
			dc.nodeHgt = 3 * dc.nameFontLineHgt + 3

	# layout the tree

	(width,height) = layout_tree()

	# draw the tree

	svg = draw_tree(width,height)
	svg.save(svgFilename)


# read_leaf_bits --

def read_leaf_bits(bitVectorTemplate):
	for node in preOrder:
		isLeaf = (node.children == [])
		if (not isLeaf): continue

		bitVectorFilename = bitVectorTemplate.replace("{leaf}",node.name)
		(node.numBits,node.bitsCup) = read_bit_vector(bitVectorFilename)


# find_a_leaf --

def find_a_leaf():
	for node in preOrder:
		isLeaf = (node.children == [])
		if (isLeaf): return node

	return None

# compute_union--

def compute_union():
	maxDepth = max([depth for depth in depthToNames])

	for depth in xrange(maxDepth,0,-1):  # (depth is 1-based)
		for name in depthToNames[depth]:
			node = nameToNode[name]
			isLeaf = (node.children == [])
			if (isLeaf):
				node.bitsCap = node.bitsCup
				continue

			numBits = None
			for child in node.children:
				if (numBits == None): numBits = child.numBits
				else:                 assert (child.numBits == numBits)

			bitsCup = bitsCap = None
			for child in node.children:
				if (bitsCup == None):
					bitsCup = child.bitsCup
					bitsCap = child.bitsCap
				else:
					bitsCup |= child.bitsCup
					bitsCap &= child.bitsCap

			node.numBits = numBits
			node.bitsCup = bitsCup
			node.bitsCap = bitsCap


# compute_all_some --

def compute_all_some():
	maxDepth = max([depth for depth in depthToNames])

	for depth in xrange(maxDepth,0,-1):  # (depth is 1-based)
		for name in depthToNames[depth]:
			node = nameToNode[name]
			isLeaf = (node.children == [])
			if (isLeaf):
				node.bitsCap  = node.bitsCup
				node.bitsAll  = node.bitsCup
				node.bitsSome = 0
				continue

			numBits = None
			for child in node.children:
				if (numBits == None): numBits = child.numBits
				else:                 assert (child.numBits == numBits)

			bitsCup = bitsCap = None
			for child in node.children:
				if (bitsCup == None):
					bitsCup = child.bitsCup
					bitsCap = child.bitsCap
				else:
					bitsCup |= child.bitsCup
					bitsCap &= child.bitsCap

			node.numBits  = numBits
			node.bitsCup  = bitsCup
			node.bitsCap  = bitsCap
			node.bitsAll  = bitsCap
			node.bitsSome = node.bitsCup & ~node.bitsCap

			for child in node.children:
				child.bitsAll &= ~node.bitsCap


# compute_determined_how--

def compute_determined_how(withActive=False):
	maxDepth = max([depth for depth in depthToNames])

	for depth in xrange(maxDepth,0,-1):  # (depth is 1-based)
		for name in depthToNames[depth]:
			node = nameToNode[name]
			isLeaf = (node.children == [])
			if (isLeaf):
				node.bitsCap    = node.bitsCup
				node.bitsCapNot = all_ones(node.numBits) & ~node.bitsCup
				node.bitsDet    = all_ones(node.numBits)
				node.bitsHow    = node.bitsCup
				if (withActive):
					node.bitsDetInf = all_ones(node.numBits)
					node.bitsHowInf = all_ones(node.numBits)
				continue

			numBits = None
			for child in node.children:
				if (numBits == None): numBits = child.numBits
				else:                 assert (child.numBits == numBits)

			bitsCup = bitsCap = bitsCapNot = None
			for child in node.children:
				if (bitsCup == None):
					bitsCup    = child.bitsCup
					bitsCap    = child.bitsCap
					bitsCapNot = child.bitsCapNot
				else:
					bitsCup    |= child.bitsCup
					bitsCap    &= child.bitsCap
					bitsCapNot &= child.bitsCapNot

			node.numBits    = numBits
			node.bitsCup    = bitsCup
			node.bitsCap    = bitsCap
			node.bitsCapNot = bitsCapNot
			node.bitsDet    = bitsCap | bitsCapNot
			node.bitsHow    = bitsCap

			if (withActive):
				node.bitsDetInf = all_ones(numBits)
				node.bitsHowInf = node.bitsDet

				if (node.bitsDet != 0):
					for child in node.children:
						child.bitsDetInf &= ~node.bitsDet
						child.bitsHowInf &= ~node.bitsDet


# read_bit_vector--
#  Bit vectors are stored is either "raw" bitvectors (from sdsl) or sabutan
#  bloom filter files.
#
#  Raw bit vectors have 4 leading bytes (little endian) indicating the number
#  of bits in the file, and the next 4 bytes containing some unknown
#  information.  E.g. (here the number of bits is 0x77359400, which is 2
#  billion):
#    00000000  00 94 35 77 00 00 00 00  01 00 00 00 00 10 00 00
#    00000010  00 00 00 00 00 00 00 00  00 00 00 04 00 10 00 00
#     ...
#
#  Sabutan bloom filter files have a header that indicates where, within the
#  binary file, the raw bit vector(s) can be found.  Here we hardwire it so we
#  always use the first bit vector (bv0).  For more detail on the file format,
#  find bloom_filter_file.h in the sabutan source code.

bffileHeaderMagic = 0xD532006662544253  # (see sabutan bloom_filter_file.h)
bffileBV0         = 0x50
bffileBV0Offset   = bffileBV0 + 8

def read_bit_vector(filename):
	f = file(filename,"rb")
	numBits = unpack("<L",f.read(4))[0]

	offsetToBits = 8
	if (numBits == bffileHeaderMagic & 0xFFFFFFFF):
		header2 = unpack("<L",f.read(4))[0]
		assert (header2 == bffileHeaderMagic >> 32)
		f.seek(bffileBV0Offset)
		bv0Offset = unpack("<Q",f.read(8))[0]
		f.seek(bv0Offset)
		numBits = unpack("<L",f.read(4))[0]
		offsetToBits = bv0Offset + 8

	f.seek(offsetToBits)

	bits = 0
	fullBytes = numBits/8
	extraBits = numBits - 8*fullBytes
	for byte in f.read(fullBytes):
		bits = (bits << 8) + reverse_bits(ord(byte))

	if (extraBits > 0):
		byte = f.read(1)[0]
		bits = (bits << extraBits) + (reverse_bits(ord(byte)) >> (8-extraBits))

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

	# note: the follow descriptions assume top-to-bottom orientation;  the code
	# handles left-to-right orientation also
	#
	# horizontally place leaves at tight spacing across the bottom row;  note
	# that we will shift these to the right later if their ancestors
	# necessitate it

	depth = maxDepth

	if (orientation == "T2B"):
		xOccupied = 0.0
		for name in depthToNames[depth]:
			node = nameToNode[name]
			node.x = xOccupied + dc.nodeSepWdt
			node.subtreeShift = 0
			xOccupied = node.x + dc.nodeWdt

			if ("layout" in debug):
				print >>stderr, "[d=%d] %s.x <- %.2f" % (depth,name,node.x)

	else: # if (orientation == "L2R"):
		yOccupied = 0.0
		for name in depthToNames[depth]:
			node = nameToNode[name]
			node.y = yOccupied + dc.nodeSepHgt
			node.subtreeShift = 0
			yOccupied = node.y + dc.nodeHgt

			if ("layout" in debug):
				print >>stderr, "[d=%d] %s.y <- %.2f" % (depth,name,node.y)

	if ("layout" in debug):
		print >>stderr

	# moving up the tree, horizontally place nodes of successive levels;  we
	# consider two positions for each node, the midpoint of its children (if
	# it is not a leaf), or adjacent to the most recently occupied position;
	# when the midpoint would overlap the occupied position, which shift the
	# node to the right, and record the amount of shift to be applied to its
	# subtree

	if (orientation == "T2B"):
		for depth in xrange(maxDepth-1,0,-1):  # (depth is 1-based)
			xOccupied = 0.0
			xShifted  = 0.0
			for name in depthToNames[depth]:
				node = nameToNode[name]
				isLeaf = (node.children == [])

				if (isLeaf):
					node.x = xOccupied + dc.nodeSepWdt
					node.subtreeShift = 0
					if ("layout" in debug):
						print >>stderr, "[d=%d] %s.x <- %.2f" \
									  % (depth,name,node.x)
				else:
					xMiddle = xShifted \
							+ sum(child.x for child in node.children) / len(node.children)

					if (xMiddle >= xOccupied + dc.nodeSepWdt):
						node.x = xMiddle
						node.subtreeShift = xShifted
						if ("layout" in debug):
							print >>stderr, "[d=%d] %s.x <- %.2f" \
										  % (depth,name,node.x)
					else:
						node.x = xOccupied + dc.nodeSepWdt
						subtreeShift = node.x - xMiddle
						node.subtreeShift = subtreeShift + xShifted
						xShifted += subtreeShift
						if ("layout" in debug):
							print >>stderr, "[d=%d] %s.x <- %.2f (shifted %.2f from middle=%.2f)" \
										  % (depth,name,node.x,
											 node.subtreeShift,xMiddle)

				xOccupied = node.x + dc.nodeWdt

	else: # if (orientation == "L2R"):
		for depth in xrange(maxDepth-1,0,-1):  # (depth is 1-based)
			yOccupied = 0.0
			yShifted  = 0.0
			for name in depthToNames[depth]:
				node = nameToNode[name]
				isLeaf = (node.children == [])

				if (isLeaf):
					node.y = yOccupied + dc.nodeSepHgt
					node.subtreeShift = 0
					if ("layout" in debug):
						print >>stderr, "[d=%d] %s.y <- %.2f" \
									  % (depth,name,node.y)
				else:
					yMiddle = yShifted \
							+ sum(child.y for child in node.children) / len(node.children)

					if (yMiddle >= yOccupied + dc.nodeSepHgt):
						node.y = yMiddle
						node.subtreeShift = yShifted
						if ("layout" in debug):
							print >>stderr, "[d=%d] %s.y <- %.2f" \
										  % (depth,name,node.y)
					else:
						node.y = yOccupied + dc.nodeSepHgt
						subtreeShift = node.y - yMiddle
						node.subtreeShift = subtreeShift + yShifted
						yShifted += subtreeShift
						if ("layout" in debug):
							print >>stderr, "[d=%d] %s.y <- %.2f (shifted %.2f from middle=%.2f)" \
										  % (depth,name,node.y,
											 node.subtreeShift,yMiddle)

				yOccupied = node.y + dc.nodeHgt

		if ("layout" in debug):
			print >>stderr

	# propagate subtree shifts, from the top of the tree down

	if (orientation == "T2B"):
		for depth in xrange(1,maxDepth+1):  # (depth is 1-based)
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

	else: # if (orientation == "L2R"):
		for depth in xrange(1,maxDepth+1):  # (depth is 1-based)
			for name in depthToNames[depth]:
				node = nameToNode[name]
				if (node.parent == None): continue
				if (node.parent.subtreeShift == 0.0): continue
				node.y            += node.parent.subtreeShift
				node.subtreeShift += node.parent.subtreeShift
				if ("layout" in debug):
					print >>stderr, "[d=%d] %s.y <- %.2f (%s shifted %.2f)" \
								  % (depth,name,node.y,
									 node.parent.name,node.parent.subtreeShift)

	if ("layout" in debug):
		print >>stderr

	# assign vertical positions (based simply on the node's depth), and
	# calculate the leftmost position

	if (orientation == "T2B"):
		minX = None
		yDrawing = dc.borderTop
		for depth in xrange(1,maxDepth+1):  # (depth is 1-based)
			for name in depthToNames[depth]:
				node = nameToNode[name]
				node.y = yDrawing
				if (minX == None) or (node.x < minX):
					minX = node.x
			yDrawing += dc.nodeHgt + dc.nodeSepHgt

	else: # if (orientation == "L2R"):
		minY = None
		xDrawing = dc.borderLeft
		for depth in xrange(1,maxDepth+1):  # (depth is 1-based)
			for name in depthToNames[depth]:
				node = nameToNode[name]
				node.x = xDrawing
				if (minY == None) or (node.y < minY):
					minY = node.y
			xDrawing += dc.nodeWdt + dc.nodeSepWdt

	# shift nodes so that leftmost position is on the border

	if (orientation == "T2B"):
		for depth in depthToNames:
			for name in depthToNames[depth]:
				node = nameToNode[name]
				node.x += dc.borderLeft - minX

	else: # if (orientation == "L2R"):
		for depth in depthToNames:
			for name in depthToNames[depth]:
				node = nameToNode[name]
				node.y += dc.borderLeft - minY

	# determine bounding box

	maxX = max([node.x+dc.nodeWdt for node in preOrder])
	maxY = max([node.y+dc.nodeHgt for node in preOrder])

	return (maxX+dc.borderRight , maxY+dc.borderBottom)


# draw_tree--

def draw_tree(width,height):
	global nameStyle

	nameStyle = StyleBuilder()
	nameStyle.setFontFamily(fontfamily=dc.nameFont)
	nameStyle.setFontSize("%spt" % dc.nameFontSize)
	nameStyle.setTextAnchor("left")
	nameStyle = nameStyle.getStyle()

	svg = Svg(width=width,height=height)

	# draw nodes

	for depth in depthToNames:
		for name in depthToNames[depth]:
			node = nameToNode[name]
			draw_node(svg,node,name)

	# draw branches

	for depth in depthToNames:
		for name in depthToNames[depth]:
			node = nameToNode[name]
			isLeaf = (node.children == [])
			if (isLeaf): continue

			numChildren = len(node.children)
			for (i,child) in enumerate(node.children):
				rootFrac = 0.4 + (0.2*i)/(numChildren-1)
				sinkFrac = 0.5

				if (orientation == "T2B"):
					(rootX,rootY) = (node.x  + rootFrac*dc.nodeWdt, node.y+dc.nodeHgt)
					(sinkX,sinkY) = (child.x + sinkFrac*dc.nodeWdt, child.y)
					draw_vert_branch(svg,"%s_branch_%d"%(name,i),rootX,rootY,sinkX,sinkY)
				else: # if (orientation == "L2R"):
					(rootX,rootY) = (node.x+dc.nodeWdt, node.y  + rootFrac*dc.nodeHgt)
					(sinkX,sinkY) = (child.x,           child.y + sinkFrac*dc.nodeHgt)
					draw_horz_branch(svg,"%s_branch_%d"%(name,i),rootX,rootY,sinkX,sinkY)

	return svg


# draw_node--

glyphUnion           = "&#x222A;"   # (see http://ascii-table.com/unicode-index-u.php)
glyphIntersection    = "&#x2229;"
glyphDotIntersection = "&#x2A40;"

def draw_node(svg,node,name):
	isLeaf = (node.children == [])

	yLine = node.y + dc.nameCapsHgt + 1

	ob = SvgRect(node.x,node.y,dc.nodeWdt,dc.nodeHgt,
				 id="%s_box" % name)
	ob.set_stroke(dc.lineColor)
	ob.set_stroke_width(dc.lineThickness)
	if (isLeaf): ob.set_fill(dc.leafFillColor)
	else:        ob.set_fill(dc.nodeFillColor)
	svg.addElement(ob)

	ob = SvgText("%s" % name,
				 node.x+1,yLine,
				 id="%s_name" % node.name)
	ob.set_style(nameStyle)
	svg.addElement(ob)
	yLine += dc.nameFontLineHgt

	if (hasattr(node,"bitsCup")):
		draw_bits(svg,"%s_Bcup" % node.name,
		              node.x+1,yLine,glyphUnion,node.numBits,node.bitsCup)
		yLine += dc.nameFontLineHgt

	if (hasattr(node,"bitsCap")):
		draw_bits(svg,"%s_Bcap" % node.name,
		          node.x+1,yLine,glyphIntersection,node.numBits,node.bitsCap)
		yLine += dc.nameFontLineHgt

	if (hasattr(node,"bitsCapNot")):
		draw_bits(svg,"%s_Bcapnot" % node.name,
		          node.x+1,yLine,glyphDotIntersection,node.numBits,node.bitsCapNot)
		yLine += dc.nameFontLineHgt

	if (hasattr(node,"bitsAll")):
		draw_bits(svg,"%s_Ball" % node.name,
		          node.x+1,yLine,"A",node.numBits,node.bitsAll)
		yLine += dc.nameFontLineHgt

	if (hasattr(node,"bitsSome")):
		draw_bits(svg,"%s_Bsome" % node.name,
		          node.x+1,yLine,"S",node.numBits,node.bitsSome)
		yLine += dc.nameFontLineHgt

	if (hasattr(node,"bitsDet")):
		bitsDetInf = node.bitsDetInf if (hasattr(node,"bitsDetInf")) else None
		draw_bits(svg,"%s_Bdet" % node.name,
		          node.x+1,yLine,"D",node.numBits,node.bitsDet,bitsDetInf)
		yLine += dc.nameFontLineHgt

	if (hasattr(node,"bitsHow")):
		bitsHowInf = node.bitsHowInf if (hasattr(node,"bitsHowInf")) else None
		draw_bits(svg,"%s_Bhow" % node.name,
		          node.x+1,yLine,"H",node.numBits,node.bitsHow,bitsHowInf)
		yLine += dc.nameFontLineHgt


# draw_horz_branch--

def draw_horz_branch(svg,id,xs,ys,xe,ye):
	svgPath =  ["M%f,%f" % (xs,ys)]
	svgPath += ["C%f,%f %f,%f %f,%f" % (xe,ys,xs,ye,xe,ye)]

	ob = SvgPath(pathData=" ".join(svgPath),id=id)
	ob.set_stroke_width(dc.branchThickness)
	ob.set_stroke(dc.branchColor)
	ob.set_fill("none")
	svg.addElement(ob)


# draw_vert_branch--

def draw_vert_branch(svg,id,xs,ys,xe,ye):
	svgPath =  ["M%f,%f" % (xs,ys)]
	svgPath += ["C%f,%f %f,%f %f,%f" % (xs,ye,xe,ys,xe,ye)]

	ob = SvgPath(pathData=" ".join(svgPath),id=id)
	ob.set_stroke_width(dc.branchThickness)
	ob.set_stroke(dc.branchColor)
	ob.set_fill("none")
	svg.addElement(ob)


# draw_bits--

def draw_bits(svg,id,x,y,prefix,numBits,bits,activeBits=None):
	# prefix is assumed to be a single character

	if (showBitsAs == "text"):
		draw_bits_as_string(svg,id,x,y,prefix,numBits,bits)
		return

	ob = SvgText(prefix,x,y,id=id)
	ob.set_style(nameStyle)
	svg.addElement(ob)

	x += dc.bitPrefixSkip
	y -= dc.bitHgt-1

	if (activeBits == None):
		infBit = 1

	for bitNum in xrange(numBits):
		bitPos = numBits-1 - bitNum
		bit = bits & (1 << bitPos)
		if (activeBits != None):
			infBit = activeBits & (1 << bitPos)

		if (bitGroupSizes == None): groupNum = 0
		else:                       groupNum = bitNum / bitGroupSizes[0]
		bitX = bitNum*dc.bitWdt + groupNum*dc.bitGroupSepWdt

		ob = SvgRect(x+bitX,y,dc.bitWdt,dc.bitHgt,
					 id="%s_bit%d" % (id,bitNum))
		ob.set_stroke(dc.bitOutlineColor)
		ob.set_stroke_width(dc.bitOutlineThickness)
		if   (infBit == 0): ob.set_fill(dc.bitXColor)
		elif (bit    == 0): ob.set_fill(dc.bit0Color)
		else:               ob.set_fill(dc.bit1Color)
		svg.addElement(ob)


# draw_bits_as_string--

def draw_bits_as_string(svg,id,x,y,prefix,numBits,bits):
	ob = SvgText(prefix+":"+bits_to_string(numBits,bits),x,y,id=id)
	ob.set_style(nameStyle)
	svg.addElement(ob)


# drawn_bits_width--

def drawn_bits_width(numBits):
	if (showBitsAs == "text"):
		charsForBits = numBits + (numBits-1)/4
		return (2+charsForBits)*dc.nameFontCharWdt
	else: # if (showBitsAs == "squares"):
		if (bitGroupSizes == 0): numSeparators = 0
		else:                    numSeparators = (numBits-1)/bitGroupSizes[0]
		return dc.bitPrefixSkip \
		     + numBits*dc.bitWdt \
		     + numSeparators*dc.bitGroupSepWdt


# bits_to_string--

def bits_to_string(numBits,bits):
	s = format(bits,"0%db"%numBits)
	ss = []
	for ix in xrange(0,numBits,4):
		ss += [s[ix:ix+4]]
	s = " ".join(ss)
	return s


# all_ones--

def all_ones(numBits):
	return (1 << numBits) - 1


# svg_color--
#	Convert a color from the 0..1 box to the 0..255 box

def svg_color((r,g,b)):
	return "rgb(%d,%d,%d)" \
	     % (int(ceil(255*r)),int(ceil(255*g)),int(ceil(255*b)))


if __name__ == "__main__": main()
