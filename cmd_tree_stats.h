#ifndef cmd_tree_stats_H
#define cmd_tree_stats_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "commands.h"

class TreeStatsCommand: public Command
	{
public:
	TreeStatsCommand(const std::string& name): Command(name) {}
	virtual ~TreeStatsCommand() {}
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);

	std::string inTreeFilename;
	bool reportOccupancy;
	};

#endif // cmd_tree_stats_H
