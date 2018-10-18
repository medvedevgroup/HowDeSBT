#ifndef cmd_dump_bf_H
#define cmd_dump_bf_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "bloom_filter.h"
#include "commands.h"

class DumpBFCommand: public Command
	{
public:
	static const std::uint64_t defaultEndPosition = 100;

public:
	DumpBFCommand(const std::string& name): Command(name) {}
	virtual ~DumpBFCommand() {}
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);
	virtual void dump_one_bloom_filter (const std::string& bfName, const BloomFilter* bf);

	std::vector<std::string> bfFilenames;
	std::uint64_t startPosition;	// origin-zero, half-open
	std::uint64_t endPosition;
	std::uint32_t lineWrap;
	std::uint32_t chunkSize;
	char alphabet[2];
	std::string showAs;
	bool doComplement;

	size_t nameWidth;
	size_t onesCountWidth;
	};

#endif // cmd_dump_bf_H
