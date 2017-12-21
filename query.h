#ifndef query_H
#define query_H

#include <string>
#include <vector>
#include <iostream>

#include "bloom_filter.h"
#include "query.h"

//----------
//
// classes in this module--
//
//----------

struct querydata
	{
	std::string name;
	std::string seq;	// nucleotide sequence
	};


class Query
	{
public:
	Query(const querydata& qd);
	virtual ~Query();

	virtual void kmerize (BloomFilter* bf, bool distinct=false);
	virtual void sort_kmer_positions ();
	virtual void dump_kmer_positions (std::uint64_t numUnresolved=-1);
	virtual std::uint64_t kmer_positions_hash (std::uint64_t numUnresolved=-1);

public:
	std::string name;
	std::string seq;	// nucleotide sequence
	std::vector<std::uint64_t> kmerPositions;

	std::uint64_t numPositions;
	std::uint64_t neededToPass;
	std::uint64_t neededToFail;
	std::uint64_t numUnresolved;
	std::uint64_t numPassed;
	std::uint64_t numFailed;
	std::uint64_t nodesExamined;
    std::vector<std::string> matches;  // names of leaves that match this query
    std::vector<std::uint64_t> matchesNumPassed;  // numPassed corresponding to
	                                   // .. each match (only used by
	                                   // .. BloomTree::batch_count_kmer_hits)

    std::vector<std::uint64_t> numUnresolvedStack;
    std::vector<std::uint64_t> numPassedStack;
    std::vector<std::uint64_t> numFailedStack;
    std::vector<std::uint64_t> dbgKmerPositionsHashStack;

public:
	bool dbgKmerize    = false;
	bool dbgKmerizeAll = false;

public:
	static void read_query_file (std::istream& in, const std::string& filename,
	                             std::vector<Query*>& queries);
	};

#endif // query_H
