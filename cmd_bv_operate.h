#ifndef cmd_bv_operate_H
#define cmd_bv_operate_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "commands.h"

class BVOperateCommand: public Command
	{
public:
	BVOperateCommand(const std::string& name): Command(name) {}
	virtual ~BVOperateCommand() {}
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);
	virtual void op_and (void);
	virtual void op_mask (void);
	virtual void op_or (void);
	virtual void op_or_not (void);
	virtual void op_xor (void);
	virtual void op_eq (void);
	virtual void op_complement (void);
	virtual void op_squeeze (bool fullLengthResult);
	virtual void op_unsqueeze (void);
	virtual void op_rrr (void);
	virtual void op_unrrr (void);

	std::vector<std::string> bvFilenames;
	std::string outputFilename;
	std::string operation;
	bool beQuiet;
	};

#endif // cmd_bv_operate_H
