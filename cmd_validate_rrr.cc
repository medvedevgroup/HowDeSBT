// cmd_validate_rrr.cc-- validate rrr correctness, regarding sdsl issue 365

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <iomanip>

#include "utilities.h"
#include "bit_vector.h"
#include "file_manager.h"

#include "support.h"
#include "commands.h"
#include "cmd_validate_rrr.h"

using std::string;
using std::vector;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
#define u32 std::uint32_t
#define u64 std::uint64_t

void ValidateRrrCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- validate rrr correctness, regarding sdsl-lite issue 365" << endl;
	}

void ValidateRrrCommand::usage
   (std::ostream& s,
	const string& message)
	{
	if (!message.empty())
		{
		s << message << endl;
		s << endl;
		}

	short_description(s);
	s << "usage: " << commandName << endl;
	//    123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789
	s << "  The implementation of RRR in sdsl-lite, prior to April 2017, made use of a" << endl;
	s << "  shift operation that produces an undefined result according to the C++" << endl;
	s << "  standard. It has been observed that this caused silent problems in RRR" << endl;
	s << "  compression when the code was compiled with a certain compiler (clang). This" << endl;
	s << "  command tests whether this build exhibits that problem, and will suggest" << endl;
	s << "  corrective action or workarounds." << endl;
	}

void ValidateRrrCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  (none, yet)" << endl;
	}

void ValidateRrrCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// skip command name

	argv = _argv+1;  argc = _argc - 1;
	// if (argc <= 0) chastise ();   for this command, this is not a problem

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

		// unrecognized argument

		chastise ("unrecognized argument: \"" + arg + "\"");
		}

	return;
	}


int ValidateRrrCommand::execute()
	{
	// create a sparse uncompressed bitvector

	u32   size    = 3*1000;
	float density = 0.05;

	sdslbitvector bv(size);

	u32 lfsr = 1;            // linear feedback shift register
	for (u64 ix=0 ; ix<u64(size*density) ; ix++)
		{
		lfsr = (lfsr >> 1) ^ (-(lfsr & 1) & 0x80200003);
		u32 pos = lfsr % size;
		if (pos >= RRR_BLOCK_SIZE) bv[pos] = 1;
		}

	// fill the first RRR block to roughly 50% capacity; mid-range capacity is
	// the worst case for RRR

	for (u64 pos=0 ; pos<RRR_BLOCK_SIZE ; pos++)
		{
		lfsr = (lfsr >> 1) ^ (-(lfsr & 1) & 0x80200003);
		if ((lfsr & 1) != 0) bv[pos] = 1;
		}

	// create an RRR vector from the uncompressed vector;  if the problem
	// occurs, this is where it will happen

	rrrbitvector bvRrr(bv);

	// validate that the two vectors report the same bit values; if the problem
	// occurs, it will be evident within the first block

	u64 differences = 0;
	for (u64 pos=0 ; pos<bvRrr.size() ; pos++)
		{
		if ((bvRrr[pos] == 1) != (bv[pos] == 1)) differences++;
		}

	// clean up

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	// tell the user how it went

	if (differences == 0)
		{
		cout << "TEST SUCCEEDED" << std::endl;
		return EXIT_SUCCESS;
		}
	else
		{
		cout << "TEST FAILED" << std::endl;
		//       123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789
		cout << "  Some differences were observed between the bits written to an RRR vector and" << std::endl;
		cout << "  the bits read from it. Possible corrective actions:" << std::endl;
		cout << "    (1) Install a newer version of sdsl-lite, one that resolves issue 365." << std::endl;
		cout << "        Versions cloned from github on or after April 2017 are expected to" << std::endl;
		cout << "        resolve this issue." << std::endl;
		cout << "    (2) Reduce the RRR block size to 127, by adding -DRRR_BLOCK_SIZE=127 to" << std::endl;
		cout << "        CXXFLAGS in the Makefile, or changing the definition of RRR_BLOCK_SIZE" << std::endl;
		cout << "        in bit_vector.h. Be aware that bloom filter and bit vector files" << std::endl;
		cout << "        written with different RRR block sizes are incompatible with each" << std::endl;
		cout << "        other." << std::endl;
		cout << "    (3) Use a different compiler. The problem has been observed with clang but" << std::endl;
		cout << "        not with gcc." << std::endl;
		return EXIT_FAILURE;
		}

	}

