#ifndef cmd_bf_operate_H
#define cmd_bf_operate_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "commands.h"

class BFOperateCommand: public Command
	{
public:
	BFOperateCommand(const std::string& name): Command(name) {}
	virtual ~BFOperateCommand() {}
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);
	virtual void op_and (void);
	virtual void op_or (void);
	virtual void op_xor (void);
	virtual void op_eq (void);
	virtual void op_complement (void);

	std::vector<std::string> bfFilenames;
	std::string outputFilename;
	std::string operation;
	};

#endif // cmd_bf_operate_H
