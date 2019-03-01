#ifndef cmd_query_bf_H
#define cmd_query_bf_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "bloom_filter.h"
#include "commands.h"

class QueryBFCommand: public Command
	{
public:
	QueryBFCommand(const std::string& name): Command(name) {}
	virtual ~QueryBFCommand();
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);
	virtual void read_queries (void);

	std::vector<std::string> bfFilenames;
	std::vector<std::string> queryFilenames;
	std::vector<double> queryThresholds;
	double generalQueryThreshold;
	bool distinctKmers;
	bool reportAllKmers;

	std::vector<Query*> queries;
	};

#endif // cmd_query_bf_H
