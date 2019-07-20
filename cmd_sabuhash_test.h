#ifndef cmd_sabuhash_test_H
#define cmd_sabuhash_test_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "hash.h"
#include "commands.h"

class SabuhashTestCommand: public Command
	{
public:
	static const std::uint32_t defaultKmerSize = 20;

public:
	SabuhashTestCommand(const std::string& name): Command(name) {}
	virtual ~SabuhashTestCommand();
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);
	virtual void perform_hash_test (const std::string& seq,
	                                const std::uint64_t* data=nullptr);

	std::vector<std::string> seqFilenames;
	std::uint32_t kmerSize;
	bool useStringKmers;
	bool negateHash;
	std::uint32_t modulus;
	std::uint64_t hashSeed;
	HashCanonical* hasher = nullptr;
	};

#endif // cmd_sabuhash_test_H
