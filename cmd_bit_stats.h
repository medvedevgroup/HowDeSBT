#ifndef cmd_bit_stats_H
#define cmd_bit_stats_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "utilities.h"
#include "bloom_tree.h"
#include "commands.h"

class BitStatsCommand: public Command
	{
public:
	BitStatsCommand(const std::string& name): Command(name) {}
	virtual ~BitStatsCommand() {}
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);
	virtual void collect_stats(BloomTree* node, BitVector* activeBv);

	std::string inTreeFilename;
	BloomFilter* rootBf;
	std::uint32_t bfKind;
	std::uint64_t bfWidth;
	std::uint64_t startPosition;	// origin-zero, half-open
	std::uint64_t endPosition;
	std::uint32_t* detActive;		// counting array, with end-start entries
	std::uint32_t* howActive;		//   "
	std::uint32_t* howOne;			//   "

	std::string showAs;
	bool dbgTraversal;
	bool dbgBits;
	wall_time_ty startTime;
	int nodeNum;
	};

#endif // cmd_bit_stats_H
