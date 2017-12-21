#ifndef jelly_kmers_H
#define jelly_kmers_H

//----------
//
// The implementation of MerCounter and the sample code are based on code
// fragments in these two projects:
//	Jellyfish-2.2.6               bc_main.cc
//	Kingsford-Group/bloomtree     Count.cc
//
//----------
//
// Typical use:
//
//    #include "jelly_kmers.h"
//
//    const std::uint64_t hashSize    = 10*1000*1000;
//    const std::uint32_t numReprobes = 126;
//    const std::uint32_t counterLen  = 7;
//
//    unsigned int savedKmerSize = jellyfish::mer_dna::k();
//    jellyfish::mer_dna::k(kmerSize);
//
//    mer_hash_type merHash (hashSize, kmerSize*2, counterLen, numThreads, numReprobes);
//
//    MerCounter counter(numThreads, merHash, seqFilenames.begin(), seqFilenames.end());
//    counter.exec_join (numThreads);
//
//    const auto jfAry = merHash.ary();
//    const auto end   = jfAry->end();
//    for (auto kmer=jfAry->begin() ; kmer!=end ; ++kmer)
//        {
//        auto& keyValuePair = *kmer;
//        ... do something with the kmer
//        }
//
//    jellyfish::mer_dna::k(savedKmerSize);    // restore jellyfish kmer size
//
//----------

#include <string>
#include <vector>

#include <jellyfish/mer_dna.hpp>
#include <jellyfish/thread_exec.hpp>
#include <jellyfish/hash_counter.hpp>
#include <jellyfish/stream_manager.hpp>
#include <jellyfish/mer_overlap_sequence_parser.hpp>
#include <jellyfish/mer_iterator.hpp>

typedef jellyfish::cooperative::hash_counter<jellyfish::mer_dna>                                              mer_hash_type;
typedef jellyfish::mer_overlap_sequence_parser<jellyfish::stream_manager<std::vector<std::string>::iterator>> sequence_parser_type;
typedef jellyfish::mer_iterator<sequence_parser_type, jellyfish::mer_dna>                                     mer_iterator_type;


class MerCounter : public jellyfish::thread_exec
	{
	mer_hash_type&                    merHash;
	jellyfish::stream_manager<std::vector<std::string>::iterator > streams;
	sequence_parser_type              parser;

public:
	MerCounter
	   (int numThreads,
		mer_hash_type& _merHash,
		std::vector<std::string>::iterator filenamesBegin,
		std::vector<std::string>::iterator filenamesEnd)
		  :	merHash(_merHash),
			streams(filenamesBegin, filenamesEnd),
			parser(jellyfish::mer_dna::k(), streams.nb_streams(),
			       3*numThreads, 4096, streams)
		{ }

	virtual void start (int thid)
		{
		mer_iterator_type mers(parser, /*canonical*/ true);
		for ( ; mers ; ++mers)
			merHash.add (*mers, 1);
		merHash.done();
		}
	};

#endif // jelly_kmers_H
