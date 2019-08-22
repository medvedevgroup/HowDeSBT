#!/usr/bin/env python
"""
Estimate the proper size for BFs in an SBT, based on the theoretical bloom
filter false positive rate.
"""

from sys  import argv,stdin,stdout,stderr,exit
from math import ceil,log


def usage(s=None):
	message = """

usage: simple_bf_size_estimate <distinct_kmers> <fprate> [options]
   <distinct_kmers>  number of distinct kmers that will be represented by the
                     bloom filter
   <fprate>          the (desired) bloom filter false positive rate
  --hashes=<number>  (H) number of hash functions
                     (by default this is 1)

NOTE: This script just gives a simple estimate of bloom filter size based on
      theoretical *bloom filter* false positive rate. For a better estimate, in
      *query* false positive rate, use determine_sbt_bf_size."""

	if (s == None): exit (message)
	else:           exit ("%s%s" % (s,message))


def main():
	global debug

	# parse the command line

	distinctKmers = None
	fpRate        = None
	numHashFuncs  = 1
	debug         = []

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg.startswith("--hashes=")) or (arg.startswith("H=")):
			numHashFuncs = int(argVal)
			assert (numHashFuncs > 0)
		elif (arg == "--debug"):
			debug += ["debug"]
		elif (arg.startswith("--debug=")):
			debug += argVal.split(",")
		elif (arg.startswith("--")):
			usage("unrecognized option: %s" % arg)
		elif (distinctKmers == None):
			distinctKmers = int_with_unit(arg)
			assert (distinctKmers > 0)
		elif (fpRate == None):
			fpRate = parse_probability(arg)
		else:
			usage("unrecognized option: %s" % arg)

	if (distinctKmers == None): usage("you have to tell me how many distinct kmers will be in the bloom filter")
	if (fpRate        == None): usage("you have to give me the desired false positive rate for the bloom filter")

	# estimate the bloom filter size

	if (numHashFuncs == 1):
		numBits = -distinctKmers/log(1-fpRate)
	else:
		numBits = -(numHashFuncs*distinctKmers) / log(1-(fpRate)**(1/numHashFuncs))
	numBits = int(ceil(numBits))

	print "#numItems\tbfFP\tH\tB"
	print "%d\t%.6f\t%d\t%d" % (distinctKmers,fpRate,numHashFuncs,numBits)


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
