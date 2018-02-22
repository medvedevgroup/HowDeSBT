// cmd_load_test.cc-- test loading of bit vectors

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

#include "utilities.h"
#include "prng.h"
#include "bit_vector.h"
#include "file_manager.h"

#include "support.h"
#include "commands.h"
#include "cmd_load_test.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u64 std::uint64_t

void LoadTestCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- test loading of bit vectors" << endl;
	}

void LoadTestCommand::usage
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
	s << "  <filename>       (cumulative) a bit vector file, either .bv, .rrr or .roar" << endl;
	s << "  --vectors=<N>    number of bit vectors to generate for each filename; this" << endl;
	s << "                   requires that the filename contain the substring {number}" << endl;
	s << "  --lookup=<N>     perform N bit-lookups on each vector; note that the" << endl;
	s << "                   positions read are not necessarily distinct" << endl;
	s << "                   (default is " << defaultNumLookups << ")" << endl;
	s << "  --report:count   report the number of 0s and 1s read" << endl;
	s << "  --seed=<string>  random number generator seed" << endl;
	}

void LoadTestCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  lookups" << endl;
	s << "  numbits" << endl;
	}

void LoadTestCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;
	vector<string> tempBvFilenames;

	// defaults

	prngSeed    = "";
	numVectors  = 1;
	reportCount = false;

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

		// --vectors=<N>

		if (is_prefix_of (arg, "--vectors="))
			{
			numVectors = string_to_int(argVal);
			if (numVectors < 1)
				chastise ("--vectors must be at least one (in \"" + arg + "\"");
			continue;
			}

		// --lookup=<N>

		if ((is_prefix_of (arg, "--lookup="))
		 ||	(is_prefix_of (arg, "--lookups="))
		 ||	(is_prefix_of (arg, "L="))
		 ||	(is_prefix_of (arg, "--L=")))
			{ numLookups = string_to_unitized_u64(argVal);  continue; }

		// --report:count

		if (arg == "--report:count")
			{ reportCount = true;  continue; }

		// --seed=<string>

		if (is_prefix_of (arg, "--seed="))
			{ prngSeed = argVal;  continue; }

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

		if (BitVector::valid_filename (arg))
			{ tempBvFilenames.emplace_back(strip_blank_ends(arg));  continue; }

		// unrecognized argument

		chastise ("unrecognized argument: \"" + arg + "\"");
		}

	// sanity checks

	if (numVectors > 1)
		{
		int numExpandables = 0;
		for (const auto& bvFilename : tempBvFilenames)
			{
			std::size_t fieldIx = bvFilename.find ("{number}");
			if (fieldIx != string::npos)
				numExpandables++;
			}
		if (numExpandables == 0)
			chastise ("--vectors requires at least one filename containing \"{number}\"");
		}

	// filename expansion -- copy filenames from tempBvFilenames to bvFilenames,
	// replacing {number} with the counted vector number

	expand_filenames (tempBvFilenames, numVectors, bvFilenames);

	if (bvFilenames.empty())
		chastise ("at least one bit vector filename is required");

	return;
	}


int LoadTestCommand::execute()
	{
	std::mt19937* prng = nullptr;
	u64 positionsSize = std::max((u64)1,numLookups);
	u64 positions[positionsSize];

	if (numLookups > 0)
		prng = seeded_prng(prngSeed);

	if (contains(debug,"lookups"))
		cout << "numLookups=" << numLookups << endl;

	for (const auto& bvFilename : bvFilenames)
		{
		BitVector* bv = BitVector::bit_vector (bvFilename);
		bv->load();
		u64 numBits = bv->num_bits();

		if (contains(debug,"numbits"))
			cerr << bv->identity() << " has " << numBits << " bits" << endl;

		if (numBits == 0)
			{
			cerr << "[BitVector lookups] (empty bitvector) " << bvFilename << endl;
			continue;
			}

		if (numLookups > 0)
			{
			std::uniform_int_distribution<u64> spinner(0,numBits-1);
			for (u64 ix=0 ; ix<numLookups ; ix++)
				positions[ix] = spinner(*prng);

			u64 onesSeen = 0;
			auto startTime = get_wall_time();
			for (u64 ix=0 ; ix<numLookups ; ix++)
				onesSeen += (*bv)[positions[ix]];
			auto elapsedTime = elapsed_wall_time(startTime);
			cerr << "[BitVector lookups] " << elapsedTime << " secs " << bvFilename << endl;

			if (reportCount)
				cout << bvFilename            << ": "
				     << numLookups            << " lookups "
				     << (numLookups-onesSeen) << " zeros "
				     << onesSeen              << " ones" << endl;
			}

		delete bv;
		}

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	if (prng != nullptr) delete prng;
	return EXIT_SUCCESS;
	}
