#ifndef cmd_random_bv_H
#define cmd_random_bv_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "commands.h"

class RandomBVCommand: public Command
	{
public:
	static const std::uint64_t defaultNumBits = 500*1000;
	static constexpr float     defaultDensity = 0.10;

public:
	RandomBVCommand(const std::string& name): Command(name) {}
	virtual ~RandomBVCommand() {}
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);

	std::string prngSeed;
	std::vector<std::string> bvFilenames;
	int numVectors;
	std::uint64_t numBits;
	float density;
	std::uint64_t numOnes;  // used only if density < 0
	bool asBloomFilter;
	};

#endif // cmd_random_bv_H
