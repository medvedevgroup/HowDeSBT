#ifndef cmd_combine_bf_H
#define cmd_combine_bf_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "commands.h"

class CombineBFCommand: public Command
	{
public:
	CombineBFCommand(const std::string& name): Command(name) {}
	virtual ~CombineBFCommand() {}
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);
	virtual std::string combine_bloom_filters (void);

	std::vector<std::string> bfFilenames;
	std::string listFilename;
	std::string inTreeFilename;
	std::string outTreeFilename;
	std::string unityFilename;
	bool dryRun;
	bool beQuiet;
	bool trackMemory;
	int  combinationsCounter;
	};

#endif // cmd_combine_bf_H
