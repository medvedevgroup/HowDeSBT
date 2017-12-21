#ifndef cmd_load_test_H
#define cmd_load_test_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "commands.h"

class LoadTestCommand: public Command
	{
public:
	static const std::uint64_t defaultNumLookups = 100*1000;

public:
	LoadTestCommand(const std::string& name): Command(name) {}
	virtual ~LoadTestCommand() {}
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);

	std::string prngSeed;
	std::vector<std::string> bvFilenames;
	int numVectors;
	std::uint64_t numLookups;
	bool reportCount;
	};

#endif // cmd_load_test_H
