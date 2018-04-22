// cmd_bf_operate.cc-- perform some user-specified operation on bloom filters

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

#include "utilities.h"
#include "bit_utilities.h"
#include "bit_vector.h"
#include "file_manager.h"

#include "support.h"
#include "commands.h"
#include "cmd_bf_operate.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u8  std::uint8_t
#define u32 std::uint32_t
#define u64 std::uint64_t

void BFOperateCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- perform some user-specified operation on bloom filters" << endl;
	}

void BFOperateCommand::usage
   (std::ostream& s,
	const string& message)
	{
	if (!message.empty())
		{
		s << message << endl;
		s << endl;
		}

	short_description(s);
	s << "usage: " << commandName << " <filename> [<filename>..] [options]" << endl;
	//    123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789
	s << "  <filename>        (cumulative) a bloom filter file, extension .bf; only" << endl;
	s << "                    simple uncompressed bloom filters are supported; there" << endl;
	s << "                    should be as many bloom filter as the operation needs," << endl;
	s << "                    usually 2." << endl;
	s << "  --out=<filename>  name for the resulting bloom filter file" << endl;
	s << "  --and             output = a AND b" << endl;
	s << "  --or              output = a OR b" << endl;
	s << "  --xor             output = a XOR b" << endl;
	s << "  --eq              output = a EQ b" << endl;
	s << "  --not             output = NOT a  (i.e. 1s complement)" << endl;
	}

void BFOperateCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  (none, yet)" << endl;
	}

void BFOperateCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// skip command name

	argv = _argv+1;  argc = _argc - 1;
	if (argc <= 0) chastise ();

	//////////
	// scan arguments
	//////////

	for (int argIx=0 ; argIx<argc ; argIx++)
		{
		string arg = argv[argIx];
		string argVal;
		if (arg.empty()) continue;

		string::size_type argValIx = arg.find('=');
		if (argValIx == string::npos) argVal = "";
		                         else argVal = arg.substr(argValIx+1);

		// --help, etc.

		if ((arg == "--help")
		 || (arg == "-help")
		 || (arg == "--h")
		 || (arg == "-h")
		 || (arg == "?")
		 || (arg == "-?")
		 || (arg == "--?"))
			{ usage (cerr);  std::exit (EXIT_SUCCESS); }

		if ((arg == "--help=debug")
		 || (arg == "--help:debug")
		 || (arg == "?debug"))
			{ debug_help(cerr);  std::exit (EXIT_SUCCESS); }

		// --out=<filename>

		if ((is_prefix_of (arg, "--out="))
		 ||	(is_prefix_of (arg, "--output=")))
			{ outputFilename = argVal;  continue; }

		// operations (some unadvertised)

		if ((arg == "--and") || (arg == "--AND") || (arg == "AND"))
			{ operation = "and";  continue; }

		if ((arg == "--or") || (arg == "--OR") || (arg == "OR"))
			{ operation = "or";  continue; }

		if ((arg == "--xor") || (arg == "--XOR") || (arg == "XOR"))
			{ operation = "xor";  continue; }

		if ((arg == "--eq") || (arg == "--EQ") || (arg == "EQ") || (arg == "=="))
			{ operation = "eq";  continue; }

		if ((arg == "--not") || (arg == "--NOT") || (arg == "NOT") || (arg == "--complement"))
			{ operation = "complement";  continue; }

		// (unadvertised) debug options

		if (arg == "--debug")
			{ debug.insert ("debug");  continue; }

		if (is_prefix_of (arg, "--debug="))
			{
		    for (const auto& field : parse_comma_list(argVal))
				debug.insert(to_lower(field));
			continue;
			}

		// unrecognized --option

		if (is_prefix_of (arg, "--"))
			chastise ("unrecognized option: \"" + arg + "\"");

		// <filename>

		if (is_suffix_of (arg, ".bf"))
			{ bfFilenames.emplace_back(strip_blank_ends(arg));  continue; }

		// unrecognized argument

		chastise ("unrecognized argument: \"" + arg + "\"");
		}

	// sanity checks

	if (outputFilename.empty())
		chastise ("an output bloom filter filename is required (--out)");

	if (operation.empty())
		chastise ("an operation is required (e.g. --AND)");

	if (operation == "and")
		{
		if (bfFilenames.size() != 2)
			chastise ("AND requires two input bloom filters");
		}
	else if (operation == "or")
		{
		if (bfFilenames.size() != 2)
			chastise ("OR requires two input bloom filters");
		}
	else if (operation == "xor")
		{
		if (bfFilenames.size() != 2)
			chastise ("XOR requires two input bloom filters");
		}
	else if (operation == "eq")
		{
		if (bfFilenames.size() != 2)
			chastise ("EQ requires two input bloom filters");
		}
	else if (operation == "complement")
		{
		if (bfFilenames.size() != 1)
			chastise ("NOT requires one input bloom filter");
		}

	return;
	}


int BFOperateCommand::execute()
	{
	if      (operation == "and")          op_and();
	else if (operation == "or")           op_or();
	else if (operation == "xor")          op_xor();
	else if (operation == "eq")           op_eq();
	else if (operation == "complement")   op_complement();

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	return EXIT_SUCCESS;
	}


void BFOperateCommand::op_and()
	{
	BloomFilter* bfA = BloomFilter::bloom_filter(bfFilenames[0]);
	BloomFilter* bfB = BloomFilter::bloom_filter(bfFilenames[1]);
	bfA->load();
	bfB->load();
	BitVector* bvA = bfA->bvs[0];
	BitVector* bvB = bfB->bvs[0];

	u64 numBits = bfA->num_bits();
	if (bfB->num_bits() != numBits)
		fatal ("error: \"" + bfFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
			 + ", but  \"" + bfFilenames[1] + "\" has " + std::to_string(bfB->num_bits()));

	BloomFilter* dstBf = BloomFilter::bloom_filter(bfA,outputFilename);
	dstBf->new_bits(bvA,bvcomp_uncompressed,0);

	dstBf->intersect_with(bvB);
	dstBf->save();

	delete bfA;
	delete bfB;
	delete dstBf;
	}


void BFOperateCommand::op_or()
	{
	BloomFilter* bfA = BloomFilter::bloom_filter(bfFilenames[0]);
	BloomFilter* bfB = BloomFilter::bloom_filter(bfFilenames[1]);
	bfA->load();
	bfB->load();
	BitVector* bvA = bfA->bvs[0];
	BitVector* bvB = bfB->bvs[0];

	u64 numBits = bfA->num_bits();
	if (bfB->num_bits() != numBits)
		fatal ("error: \"" + bfFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
			 + ", but  \"" + bfFilenames[1] + "\" has " + std::to_string(bfB->num_bits()));

	BloomFilter* dstBf = BloomFilter::bloom_filter(bfA,outputFilename);
	dstBf->new_bits(bvA,bvcomp_uncompressed,0);

	dstBf->union_with(bvB);
	dstBf->save();

	delete bfA;
	delete bfB;
	delete dstBf;
	}


void BFOperateCommand::op_xor()
	{
	BloomFilter* bfA = BloomFilter::bloom_filter(bfFilenames[0]);
	BloomFilter* bfB = BloomFilter::bloom_filter(bfFilenames[1]);
	bfA->load();
	bfB->load();
	BitVector* bvA = bfA->bvs[0];
	BitVector* bvB = bfB->bvs[0];

	u64 numBits = bfA->num_bits();
	if (bfB->num_bits() != numBits)
		fatal ("error: \"" + bfFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
			 + ", but  \"" + bfFilenames[1] + "\" has " + std::to_string(bfB->num_bits()));

	BloomFilter* dstBf = BloomFilter::bloom_filter(bfA,outputFilename);
	dstBf->new_bits(bvA,bvcomp_uncompressed,0);

	dstBf->xor_with(bvB);
	dstBf->save();

	delete bfA;
	delete bfB;
	delete dstBf;
	}


void BFOperateCommand::op_eq()
	{
	BloomFilter* bfA = BloomFilter::bloom_filter(bfFilenames[0]);
	BloomFilter* bfB = BloomFilter::bloom_filter(bfFilenames[1]);
	bfA->load();
	bfB->load();
	BitVector* bvA = bfA->bvs[0];
	BitVector* bvB = bfB->bvs[0];

	u64 numBits = bfA->num_bits();
	if (bfB->num_bits() != numBits)
		fatal ("error: \"" + bfFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
			 + ", but  \"" + bfFilenames[1] + "\" has " + std::to_string(bfB->num_bits()));

	BloomFilter* dstBf = BloomFilter::bloom_filter(bfA,outputFilename);
	dstBf->new_bits(bvA,bvcomp_uncompressed,0);

	dstBf->xor_with(bvB);
	dstBf->complement();
	dstBf->save();

	delete bfA;
	delete bfB;
	delete dstBf;
	}


void BFOperateCommand::op_complement()
	{
	BloomFilter* bf = BloomFilter::bloom_filter(bfFilenames[0]);
	bf->load();
	BitVector* bv = bf->bvs[0];

	BloomFilter* dstBf = BloomFilter::bloom_filter(bf,outputFilename);
	dstBf->new_bits(bv,bvcomp_uncompressed,0);

	dstBf->complement();
	dstBf->save();

	delete bf;
	delete dstBf;
	}
