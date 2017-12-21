#ifndef cmd_make_bv_H
#define cmd_make_bv_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "hash.h"
#include "commands.h"

class MakeBVCommand: public Command
	{
public:
	static const std::uint32_t defaultKmerSize = 20;
	static const std::uint32_t defaultMinAbundance = 1;
	static const std::uint32_t defaultNumThreads = 1;
	static const std::uint64_t defaultNumBits = 500*1000;

public:
	MakeBVCommand(const std::string& name): Command(name) {}
	virtual ~MakeBVCommand();
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);
	virtual void make_bit_vector_fasta (void);
	virtual void make_bit_vector_kmers (void);

	std::string listFilename;
	std::vector<std::string> seqFilenames;
	bool inputIsKmers;
	std::string bvFilename;
	std::string asPerFilename;
	std::uint32_t kmerSize;
	std::uint32_t minAbundance;
	bool          minAbundanceSet;
	std::uint32_t numThreads;
	std::uint64_t hashSeed;
	std::uint64_t numBits;
	HashCanonical* hasher = nullptr;
	};

#endif // cmd_make_bv_H
