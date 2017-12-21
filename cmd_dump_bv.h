#ifndef cmd_dump_bv_H
#define cmd_dump_bv_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "commands.h"

class DumpBVCommand: public Command
	{
public:
	static const std::uint64_t defaultEndPosition = 100;

public:
	DumpBVCommand(const std::string& name): Command(name) {}
	virtual ~DumpBVCommand() {}
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);

	std::vector<std::string> bvFilenames;
	int numVectors;
	std::uint64_t startPosition;	// origin-zero, half-open
	std::uint64_t endPosition;
	std::uint32_t lineWrap;
	std::uint32_t chunkSize;
	char alphabet[2];
	std::string showAs;
	bool doComplement;
	};

#endif // cmd_dump_bv_H
