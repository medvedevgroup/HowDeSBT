#ifndef cmd_validate_tree_H
#define cmd_validate_tree_H

#include <string>
#include <cstdlib>
#include <iostream>

#include "commands.h"

class ValidateTreeCommand: public Command
	{
public:
	ValidateTreeCommand(const std::string& name): Command(name) {}
	virtual ~ValidateTreeCommand() {}
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);

	std::string inTreeFilename;
	};

#endif // cmd_validate_tree_H
