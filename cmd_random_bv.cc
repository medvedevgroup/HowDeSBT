// cmd_random_bv.cc-- generate random bit vectors

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>
#include <random>

#include "utilities.h"
#include "prng.h"
#include "bit_vector.h"
#include "bloom_filter.h"

#include "support.h"
#include "commands.h"
#include "cmd_random_bv.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u32 std::uint32_t
#define u64 std::uint64_t


void RandomBVCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- generate random bit vectors" << endl;
	}

void RandomBVCommand::usage
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
	s << "  <filename>      (cumulative) a bit vector file, either .bv, .rrr or .roar" << endl;
	s << "  --vectors=<N>   number of bit vectors to generate for each filename; this" << endl;
	s << "                  requires that the filename contain the substring {number}" << endl;
	s << "  --bits=<N>      number of bits in each bit vector" << endl;
	s << "                  (default is " << defaultNumBits << ")" << endl;
	s << "  --density=<P>   probability of a bit in the vector(s) being 1" << endl;
	s << "                  (default is " << defaultDensity << ")" << endl;
	s << "  --ones=<N>      number of 1s in the vector(s)" << endl;
	s << "                  (this should be used exclusive of --density)" << endl;
	s << "  --seed=<string> random number generator seed" << endl;
	}

void RandomBVCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  count" << endl;
	}

void RandomBVCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;
	vector<string> tempBvFilenames;
	bool	densitySpecified, onesSpecified;

	// defaults

	prngSeed      = "";
	numVectors    = 1;
	numBits       = defaultNumBits;
	density       = defaultDensity;
	numOnes       = 0;
	asBloomFilter = false;

	// skip command name

	argv = _argv+1;  argc = _argc - 1;
	if (argc <= 0) chastise ();

	//////////
	// scan arguments
	//////////

	densitySpecified = onesSpecified = false;

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

		// --bits=<N>

		if ((is_prefix_of (arg, "--bits="))
		 ||	(is_prefix_of (arg, "B="))
		 ||	(is_prefix_of (arg, "--B=")))
			{ numBits = string_to_unitized_u64(argVal);  continue; }

		// --density=<P>

		if ((is_prefix_of (arg, "--density="))
		 ||	(is_prefix_of (arg, "P="))
		 ||	(is_prefix_of (arg, "--P=")))
			{
			density = string_to_probability(argVal);
			densitySpecified = true;
			continue;
			}

		// --ones=<N>

		if (is_prefix_of (arg, "--ones="))
			{
			numOnes = string_to_unitized_u64(argVal);
			density = -1.0;
			onesSpecified = true;
			continue;
			}

		// --seed=<string>

		if (is_prefix_of (arg, "--seed="))
			{ prngSeed = argVal;  continue; }

		// (unadvertised) --asfilter

		if (arg == "--asfilter")
			{ asBloomFilter = true;  continue; }

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

	if (onesSpecified)
		{
		if (densitySpecified)
			chastise ("can't use both --density and --ones");

		if (numOnes > numBits)
			chastise ("--ones=" + std::to_string(numOnes) + " > --bits=" + std::to_string(numBits));
		}

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

int RandomBVCommand::execute()
	{
	std::mt19937* prng = seeded_prng(prngSeed);

	// (if specified) generate vectors probabilistically for a target density

	if (density >= 0)
		{
		std::bernoulli_distribution flipper(density);

		for (const auto& bvFilename : bvFilenames)
			{
			BitVector* bv = BitVector::bit_vector (bvFilename);
			bv->new_bits (numBits);

			u64 onesCount = 0;
			for (u64 i=0 ; i<numBits ; i++)
				{
				if (flipper(*prng))
					{ bv->write_bit (i, 1);  onesCount++; }
				}

			if (contains(debug,"count"))
				cerr << "generated " << bv->identity() << " with " << onesCount << " 1s" << endl;

			if (not asBloomFilter)
				{
				bv->save();
				delete bv;
				}
			else
				{
				string bfFilename = strip_suffix(bvFilename,".bv") + ".bf";
				BloomFilter* bf = new BloomFilter(bfFilename,/*kmerSize*/20,
				                                  /*numHashes*/1,0,0,
				                                  numBits);
				bf->new_bits (bv);
				bf->save();
				delete bf;
				delete bv;
				}
			}
		}

	// otherwise, generate vectors with a specified number of bits

	else
		{
		std::uniform_real_distribution<> spinner(0.0,1.0);

		for (const auto& bvFilename : bvFilenames)
			{
			BitVector* bv = BitVector::bit_vector (bvFilename);
			bv->new_bits (numBits);

			u64 bitsLeft = numBits;
			u64 onesLeft = numOnes;

			u64 onesCount = 0;
			for (u64 i=0 ; i<numBits ; i++)
				{
				if (onesLeft == 0) break;
				if (spinner(*prng) * bitsLeft < onesLeft) // (spinner < onesLeft/bitsLeft)
					{ bv->write_bit (i, 1);  onesCount++;  onesLeft--; }
				bitsLeft--;
				}

			if (contains(debug,"count"))
				cerr << "generated " << bv->identity() << " with " << onesCount << " 1s" << endl;

			if (not asBloomFilter)
				{
				bv->save();
				delete bv;
				}
			else
				{
				string bfFilename = strip_suffix(bvFilename,".bv") + ".bf";
				BloomFilter* bf = new BloomFilter(bfFilename,/*kmerSize*/20,
				                                  /*numHashes*/1,0,0,
				                                  numBits);
				bf->new_bits (bv);
				bf->save();
				delete bf;
				delete bv;
				}
			}
		}

	delete prng;
	return EXIT_SUCCESS;
	}
