#ifndef cmd_bf_distance_H
#define cmd_bf_distance_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "commands.h"

class BFDistanceCommand: public Command
	{
public:
	BFDistanceCommand(const std::string& name): Command(name) {}
	virtual ~BFDistanceCommand() {}
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);

	std::vector<std::string> bfFilenames;
	std::string listFilename;
	std::uint64_t startPosition;	// origin-zero, half-open
	std::uint64_t endPosition;
	std::string showDistanceAs;
	};

#endif // cmd_bf_distance_H
