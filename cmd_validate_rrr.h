#ifndef cmd_validate_rrr_H
#define cmd_validate_rrr_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "commands.h"

class ValidateRrrCommand: public Command
	{
public:
	static const std::uint32_t defaultKmerSize = 20;

public:
	ValidateRrrCommand(const std::string& name): Command(name) {}
	virtual ~ValidateRrrCommand() {}
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);
	};

#endif // cmd_validate_rrr_H
