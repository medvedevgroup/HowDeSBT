#!/usr/bin/env python
"""
Determine the proper size for BFs in an SBT, based on the theoretical query
false positive rate.
"""

from sys  import argv,stdin,stdout,stderr,exit
from math import ceil

try:
	from query_fp_rate_compiled import query_fp_rate
except ImportError:
	from query_fp_rate          import query_fp_rate


def usage(s=None):
	message = """

usage: determine_sbt_bf_size <size> [options]
  --queryfp=<probability>  (QFP) target for query false positive rate
  --experiment=<count>     (X) largest number of kmers in the set represented
                           by a bloom filter (not including kmer false
                           positives)
  --query=<count>          (Q) largest number of kmers in a query
  --theta=<value>          (T) search theta -- a query will be reported as a
                           match if this fraction of its kmers are present in
                           the experiment; 0<value<1
  --containment=<value>    (C) containment index -- the fraction of a query's
                           kmers that are present in the experiment; 0<value<1
  --containment=theta-<value>  special case, to specify C relative to theta
  --hashes=<number>        (H) number of hash functions
                           (by default this is 1)
  --resolution=<number>    (M) how close we need to get, in number of bits of
                           bloom filter size; the result we report will be a
                           multiple of this
  --show:parameters        report parameters with the result
  --show:search            report each step of the search"""

	if (s == None): exit (message)
	else:           exit ("%s%s" % (s,message))


def main():
	global debug

	# parse the command line

	queryFpTarget  = None
	experimentSize = None
	querySize      = None
	theta          = None
	containment    = None
	numHashFuncs   = 1
	resolutionBits = 1
	showParameters = False
	showSearch     = False
	debug          = []

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if  (arg.startswith("--queryfp="))  or (arg.startswith("QFP=")) \
		 or (arg.startswith("--queryfpr=")) or (arg.startswith("QFPR=")):
			queryFpTarget = parse_probability(argVal)
			if (queryFpTarget == 0):
				usage("target query false positive rate cannot be exactly zero")
			if (queryFpTarget == 1):
				usage("target query false positive rate cannot be exactly one")
		elif (arg.startswith("--experiment=")) or (arg.startswith("X=")):
			experimentSize = int_with_unit(argVal)
		elif (arg.startswith("--query=")) or (arg.startswith("Q=")):
			querySize = int_with_unit(argVal)
		elif (arg.startswith("--theta=")) or (arg.startswith("T=")):
			theta = parse_probability(argVal)
		elif (arg.startswith("--containment=")) or (arg.startswith("C=")):
			if (argVal.startswith("theta-")):
				containment = ("theta-",parse_probability(argVal.split("-",1)[1]))
			else:
				containment = parse_probability(argVal)
		elif (arg.startswith("--hashes=")) or (arg.startswith("H=")):
			numHashFuncs = int(argVal)
			assert (numHashFuncs > 0)
		elif (arg.startswith("--resolution=")) or (arg.startswith("M=")):
			resolutionBits = int_with_unit(argVal)
			if (resolutionBits < 1): resolutionBits = 1
		elif (arg in ["--show:parameters","--show=parameters"]):
			showParameters = True
		elif (arg in ["--show:search","--show=search"]):
			showSearch = True
		elif (arg == "--debug"):
			debug += ["debug"]
		elif (arg.startswith("--debug=")):
			debug += argVal.split(",")
		elif (arg.startswith("--")):
			usage("unrecognized option: %s" % arg)
		elif (bfSize == None):
			if ("," in arg): bfSize = tuple(map(int_with_unit,arg.split(",",2)))
			else:            bfSize = int_with_unit(arg)
		else:
			usage("unrecognized option: %s" % arg)

	if (queryFpTarget  == None): usage("you have to give me the target query false positive rate")
	if (experimentSize == None): usage("you have to give me the experiment size")
	if (querySize      == None): usage("you have to give me the query size")
	if (theta          == None): usage("you have to give me theta")
	if (containment    == None): usage("you have to give me the containment index")

	if (type(containment) == tuple) and (containment[0] == "theta-"):
		containmentDelta = containment[1]
		containment = theta - containmentDelta

	# increase bloom filter size until false postive rate is below target
	#
	# nota bene: we 'know' that query_fp_rate(bfSize) is monotonically
	#            decreasing, and approaches zero asymptotically

	if (showParameters) or (showSearch):
		print "#%s" % "\t".join(["B","H","X","Q","T","C","bfFP","qFP"])

	prevBfSize = None
	bfSize = 1000
	while (True):
		(queryFpRate,info) = query_fp_rate(bfSize,
		                                   numHashFuncs=numHashFuncs,
		                                   experimentSize=experimentSize,
		                                   querySize=querySize,
		                                   theta=theta,
		                                   containment=containment)

		if (showSearch):
			print "\t".join([str(bfSize),
			                 str(numHashFuncs),
			                 str(experimentSize),
			                 str(querySize),
			                 str(theta),
			                 str(containment),
			                 "%.9f" % info.bfFpRate,
			                 "%.9f" % queryFpRate])

		if (queryFpRate < queryFpTarget): break
		prevBfSize = bfSize
		bfSize *= 4

	# use binary search to zero in on the proper bloom filter size; note that
	# if our first attempt (using a ridiculously small size) was good enough,
	# we're done

	if (prevBfSize == None):
		properBfSize = bfSize
	else:
		loBfSize = prevBfSize
		hiBfSize = bfSize

		while (loBfSize<hiBfSize-resolutionBits):
			bfSize = (loBfSize + hiBfSize) // 2
			(queryFpRate,info) = query_fp_rate(bfSize,
			                                   numHashFuncs=numHashFuncs,
			                                   experimentSize=experimentSize,
			                                   querySize=querySize,
			                                   theta=theta,
			                                   containment=containment)

			if (showSearch):
				print "\t".join([str(bfSize),
				                 str(numHashFuncs),
				                 str(experimentSize),
				                 str(querySize),
				                 str(theta),
				                 str(containment),
				                 "%.9f" % info.bfFpRate,
				                 "%.9f" % queryFpRate])

			if (queryFpRate > queryFpTarget):
				loBfSize = bfSize
			else:
				hiBfSize = bfSize

		properBfSize = hiBfSize

	# report the proper bloom filter size

	bfSize = properBfSize
	bfSize -= bfSize % resolutionBits

	(queryFpRate,info) = query_fp_rate(bfSize,
	                                   numHashFuncs=numHashFuncs,
	                                   experimentSize=experimentSize,
	                                   querySize=querySize,
	                                   theta=theta,
	                                   containment=containment)

	if (queryFpRate > queryFpTarget):
		bfSize += resolutionBits
		(queryFpRate,info) = query_fp_rate(bfSize,
		                                   numHashFuncs=numHashFuncs,
		                                   experimentSize=experimentSize,
		                                   querySize=querySize,
		                                   theta=theta,
		                                   containment=containment)

	if (showParameters) or (showSearch):
		print "\t".join([str(bfSize),
		                 str(numHashFuncs),
		                 str(experimentSize),
		                 str(querySize),
		                 str(theta),
		                 str(containment),
		                 "%.9f" % info.bfFpRate,
		                 "%.9f" % queryFpRate])
	else:
		print bfSize

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


# parse_probability--
#	Parse a string as a probability

def parse_probability(s,strict=True):
	scale = 1.0
	if (s.endswith("%")):
		scale = 0.01
		s = s[:-1]

	try:
		p = float(s)
	except:
		try:
			(numer,denom) = s.split("/",1)
			p = float(numer)/float(denom)
		except:
			raise ValueError

	p *= scale

	if (strict) and (not 0.0 <= p <= 1.0):
		raise ValueError

	return p


if __name__ == "__main__": main()
