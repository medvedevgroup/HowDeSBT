#!/usr/bin/python
"""
Count the number of times each line occurs in a file.
"""

from sys  import argv,stdin,stderr,exit
from math import ceil


def usage(s=None):
	message = """
usage: cat items_file | count_duplicates [options] > item_counts_file
  --integers             items are integers
                         (by default, we treat items as strings)
  --hex[:<digits>]       items are hexadecimal integers;  <digits> is the
                         minimum width for output
  --report=<min>..<max>  only report items with counts in the specified range;
                         either <min> or <max> can be omitted
  --count,item           report results with count in first column
                         (this is the default)
  --item,count           report results with item in first column
  --quiet                don't report stats to stderr
  --nosort               don't sort the items for output
                         (by default, items are sorted by item name)
  --sort=count           sort the items for output, by decreasing count
  --head=<number>        limit the number of input lines
  --progress=<number>    periodically report how many lines we've read

 The input file (stdin) is usually a list of items, for example a list of
 organisms like this.

	mule
	donkey
	mule
	jackass

 The output is a list of the counts of each item, sorted by item name, like
 this (count and name are seprated by a tab character)

	1 donkey
	2 mule
	1 jackass"""

	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():
	global debug

    # parse the command line

	minCount = maxCount = None
	itemType            = "string"
	itemFormat          = "%s"
	reportAs            = "count,item"
	quiet               = False
	sortBy              = "item"
	headLimit           = None
	reportProgress      = None
	debug               = []

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg == "--integers") or (arg == "--integer") or (arg == "--int"):
			itemType   = "int"
			itemFormat = "%s"
		elif (arg == "--hex"):
			itemType   = "hex"
			itemFormat = "%X"
		elif (arg.startswith("--hex:")):
			itemType = "hex"
			argVal = arg.split(":",1)[1]
			if (argVal.endswith(":upper")): itemFormat = "%0" + argVal[:-6] + "X"
			else:                           itemFormat = "%0" + argVal      + "x"
		elif (arg.startswith("--report=")):
			if (".." not in argVal):
				minCount = maxCount = int_with_unit(argVal)
			else:
				(minCount,maxCount) = argVal.split("..",1)
				if (minCount == ""): minCount = None
				else:                minCount = int_with_unit(minCount)
				if (maxCount == ""): maxCount = None
				else:                maxCount = int_with_unit(maxCount)
				if (minCount != None) and (maxCount != None):
					assert (minCount <= maxCount)
		elif (arg == "--count,item"):
			reportAs = "count,item"
		elif (arg == "--item,count"):
			reportAs = "item,count"
		elif (arg == "--quiet"):
			quiet = True
		elif (arg == "--nosort"):
			sortBy = None
		elif (arg == "--sort=count"):
			sortBy = "count"
		elif (arg.startswith("--head=")):
			headLimit = int_with_unit(argVal)
		elif (arg.startswith("--progress=")):
			reportProgress = int_with_unit(argVal)
		elif (arg == "--debug"):
			debug += ["debug"]
		elif (arg.startswith("--debug=")):
			debug += argVal.split(",")
		elif (arg.startswith("--")):
			usage("unrecognized option: %s" % arg)
		else:
			usage("unrecognized option: %s" % arg)

	if (minCount == None): minCount = 0

	if (reportAs == "item,count"): reportFormat = itemFormat + "\t%d"
	else:                          reportFormat = "%d\t" + itemFormat

	# read the items

	itemToCount = {}
	occupied = singletons = 0

	itemNum = 0
	for line in stdin:
		itemNum += 1
		if (headLimit != None) and (itemNum > headLimit):
			print >>stderr, "limit of %s items reached" % (commatize(headLimit))
			break
		if (reportProgress != None) and (itemNum % reportProgress == 0):
			print >>stderr, "progress: item %s / %s distinct (%.1f%%) / %s non-singletons (%.1f%%)" \
			              % (commatize(itemNum),
			                 commatize(occupied),(100.0*occupied)/itemNum,
			                 commatize(occupied-singletons),(100.0*(occupied-singletons))/itemNum)

		item = line.rstrip()
		if   (itemType == "int"): item = int(item)
		elif (itemType == "hex"): item = int(item,16)

		if (item not in itemToCount):
			itemToCount[item] = 1
			occupied   += 1
			singletons += 1
		else:
			itemToCount[item] += 1
			if (itemToCount[item] == 2): singletons -= 1

	# sort 'em

	if (sortBy == "item"):
		items = [item for item in itemToCount]
		items.sort()
	elif (sortBy == "count"):
		items = [(-itemToCount[item],item) for item in itemToCount]
		items.sort()
		items = [item for (_,item) in items]
	else:
		items = itemToCount

	# spit 'em out

	total = 0
	(maxObserved,maxItem) = (-1,None)
	for item in items:
		count = itemToCount[item]
		if (count < minCount): continue
		if (maxCount != None) and (count > maxCount): continue

		if (reportAs == "item,count"): print reportFormat % (item,count)
		else:                          print reportFormat % (count,item)
		total += count

		if (count > maxObserved):
			(maxObserved,maxItem) = (count,item)

	if (not quiet):
		print >>stderr, "%d items, max is %s (%d)" % (total,maxItem,maxObserved)


# int_with_unit--
#	Parse a string as an integer, allowing unit suffixes

def int_with_unit(s):
	if (s.endswith("K")):
		multiplier = 1000
		s = s[:-1]
	elif (s.endswith("M")):
		multiplier = 1000 * 1000
		s = s[:-1]
	elif (s.endswith("G")):
		multiplier = 1000 * 1000 * 1000
		s = s[:-1]
	else:
		multiplier = 1

	try:               return          int(s)   * multiplier
	except ValueError: return int(ceil(float(s) * multiplier))


# commatize--
#	Convert a numeric string into one with commas.

def commatize(s):
	if (type(s) != str): s = str(s)
	(prefix,val,suffix) = ("",s,"")
	if (val.startswith("-")): (prefix,val) = ("-",val[1:])
	if ("." in val):
		(val,suffix) = val.split(".",1)
		suffix = "." + suffix

	try:    int(val)
	except: return s

	digits = len(val)
	if (digits > 3):
		leader = digits % 3
		chunks = []
		if (leader != 0):
			chunks += [val[:leader]]
		chunks += [val[ix:ix+3] for ix in xrange(leader,digits,3)]
		val = ",".join(chunks)

	return prefix + val + suffix


if __name__ == "__main__": main()
