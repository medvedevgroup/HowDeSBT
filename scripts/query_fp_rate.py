#!/usr/bin/env python

from scipy.stats import binom
from math        import ceil,exp


# query_fp_rate--
#	Compute the probability of a query being a false positive; called as a
#	positive when it is actually a negative.

class QueryFpRateInfo: pass

def query_fp_rate(bfSize,numHashFuncs=1,
                  experimentSize=None,querySize=None,theta=None,containment=None):
	if (isinstance(bfSize,dict)):
		assert (numHashFuncs   == 1)    \
		   and (experimentSize == None) \
		   and (querySize      == None) \
		   and (theta          == None) \
		   and (containment    == None)
		params = bfSize
		bfSize         = params["bfSize"]
		numHashFuncs   = params["numHashFuncs"]
		experimentSize = params["experimentSize"]
		querySize      = params["querySize"]
		theta          = params["theta"]
		containment    = params["containment"]

	queryPositiveKmers = int(round(containment*querySize))
	kmersNeededToPass  = int(ceil(theta*querySize))

	bfFpRate = bloom_filter_fp_rate(bfSize,experimentSize,numHashFuncs)

	queryNegativeKmers = querySize - queryPositiveKmers
	queryFpRate = bernoulli_at_least(
					kmersNeededToPass-queryPositiveKmers, # num successes
					queryNegativeKmers,                   # num trials
					bfFpRate)                             # success rate

	info = QueryFpRateInfo()
	info.bfFpRate           = bfFpRate
	info.queryPositiveKmers = queryPositiveKmers
	info.queryNegativeKmers = queryNegativeKmers
	info.kmersNeededToPass  = kmersNeededToPass

	return (queryFpRate,info)


# query_fp_bound--
#	Compute the upper bound of the probability of a query being a false
#	positive; called as a positive when it is actually a negative.
#
# For a given theta, a query Q is epsilon-far (from being a theta match) for
# experiment D if its containment index C(Q,D) < theta-epsilon.
#
# If Q is epsilon-far from D, and r is D's bloom filter false positive rate,
# then
#	                                        1 (eps-r)^2
#	P[Q is a query false positive] <= exp(- - --------- |Q|)
#	                                        3     r
#
# The arguments are overloaded-- the function can be called in several
# different ways. Values can be passed as named arguments, or as a single
# dict. And the bloom filter false positive rate cane be provided, or it can
# be computed here from the bloom filter size and experiment size.

def query_fp_bound(bfSize,numHashFuncs=1,
                   experimentSize=None,querySize=None,epsilon=None):
	# parse out the arguments

	if (isinstance(bfSize,dict)):
		assert (numHashFuncs   == 1)    \
		   and (experimentSize == None) \
		   and (querySize      == None) \
		   and (epsilon        == None)
		params = bfSize
		if ("bfFpRate" in params):
			bfFpRate       = params["bfFpRate"]
			querySize      = params["querySize"]
			epsilon        = params["epsilon"]
		else:
			bfSize         = params["bfSize"]
			numHashFuncs   = params["numHashFuncs"]
			experimentSize = params["experimentSize"]
			querySize      = params["querySize"]
			epsilon        = params["epsilon"]
			bfFpRate       = None
	elif (isinstance(bfSize,float)) or (bfSize == 0):
		assert (numHashFuncs   == 1)    \
		   and (experimentSize == None)
		bfFpRate = bfSize
		assert (bfFpRate < 1.0)
	else:
		bfFpRate = None

	assert (epsilon != None)

	# if bfFpRate hasn't been supplied, compute it

	if (bfFpRate == None):
		bfFpRate = bloom_filter_fp_rate(bfSize,experimentSize,numHashFuncs)

	# compute the query false positive upper bound

	r = bfFpRate
	if (r == 0.0):
		queryFpBound = 0.0
	else:
		queryFpBound = exp(-(querySize*(epsilon-r)**2) / (3*r))

	return queryFpBound


# bernoulli_at_least--
#	Compute the probability of getting at least k successes in n Bernoulli
#	trials.
#
# This is the sum for k<=k'<=n of
#	p(k' successes in n trials) = n_choose_k' p^k' q^(n-k')
#
# references:
#	[1] https://en.wikipedia.org/wiki/Bernoulli_trial
#	[2] https://docs.scipy.org/doc/scipy/reference/generated/scipy.stats.binom.html#scipy.stats.binom

def bernoulli_at_least(numSuccesses,numTrials,p=0.5):
	if (numSuccesses <= 0): return 1.0
	if (numSuccesses > numTrials): return 0.0

	#if ("bernoulli" in debug):
	#	print >>stderr, "1-binom.cdf(%d,%d,%f)" % (numSuccesses-1,numTrials,p)

	return 1-binom.cdf(numSuccesses-1,numTrials,p)


# bloom_filter_fp_rate--
#	Compute the theoretical false positive rate of a bloom filter.
#
# Assuming independence of items inserted, and that the filter is reasonably
# large, the fals positive rate can be approximated by
#	(1-e^(-kn/m))^k
# where
#	m is the number of bits
#	n is the number of items distinct items that have been inserted into the bf
#	k is the number of hash functions
#
# references:
#	[2] https://en.wikipedia.org/wiki/Bloom_filter

def bloom_filter_fp_rate(bfSize,numItems,numHashes=1):
	m = bfSize
	n = numItems
	k = numHashes
	return (1-exp(-float(k*n)/m))**k
