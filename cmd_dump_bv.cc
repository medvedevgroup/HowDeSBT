// cmd_dump_bv.cc-- dump the content of bit vectors to the console

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
#include "cmd_dump_bv.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u8  std::uint8_t
#define u32 std::uint32_t
#define u64 std::uint64_t

void DumpBVCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- dump the content of bit vectors to the console" << endl;
	}

void DumpBVCommand::usage
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
	s << "  <filename>:<type>[:<offset>][:<bytes>] bit vector is embedded in another" << endl;
	s << "                  file; <type> is bv, rrr or roar; <offset> is location within" << endl;
	s << "                  the file" << endl;
	s << "  --vectors=<N>   number of bit vectors to generate for each filename; this" << endl;
	s << "                  requires that the filename contain the substring {number}" << endl;
	s << "  --bits=<N>      limit of the number of bits to display from each bit vector" << endl;
	s << "                  (default is " << defaultEndPosition << ")" << endl;
	s << "  <start>..<end>  interval of bits to display from each bit vector" << endl;
	s << "                  (exclusive of --bits)" << endl;
	s << "  --wrap=<N>      number of bit positions allowed on a line" << endl;
	s << "                  (by default all positions are on the same line)" << endl;
	s << "  --chunk=<N>     number of bit positions shown in each chunk" << endl;
	s << "                  (default is 10)" << endl;
	s << "  --as01          show each bit as a 0 or 1" << endl;
	s << "                  (by default we show zeros as '-' and ones as '+')" << endl;
	s << "  --complement    show the bitwise complement of each vector" << endl;
	s << "  --show:density  show fraction of ones in the vector (instead of showing bits)" << endl;
	s << "  --show:integers show bit positions as a list of integers" << endl;
	}

void DumpBVCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  interval" << endl;
	}

void DumpBVCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;
	vector<string> tempBvFilenames;

	// defaults

	numVectors    = 1;
	startPosition = 0;                   bool intervalSet = false;
	endPosition   = defaultEndPosition;
	lineWrap      = 0;
	chunkSize     = 10;
	alphabet[0]   = '-';
	alphabet[1]   = '+';
	showAs        = "bits";
	doComplement  = false;

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

		// --bits=<N>

		if ((arg == "--bits=all")
		 ||	(arg == "B=all")
		 ||	(arg == "--B=all"))
			{
			startPosition = 0;
			endPosition   = UINT64_MAX; // this will be reduced to each filter's length
			intervalSet   = true;
			continue;
			}

		if ((is_prefix_of (arg, "--bits="))
		 ||	(is_prefix_of (arg, "B="))
		 ||	(is_prefix_of (arg, "--B=")))
			{
			startPosition = 0;
			endPosition   = string_to_unitized_u64(argVal);
			intervalSet   = true;
			continue;
			}

		// --wrap=<N>

		if (is_prefix_of (arg, "--wrap="))
			{
			lineWrap = string_to_u32(argVal);
			continue;
			}

		// --chunk=<N>

		if (is_prefix_of (arg, "--chunk="))
			{
			chunkSize = string_to_u32(argVal);
			if (chunkSize == 0) chunkSize = 1;
			continue;
			}

		// --as01

		if ((arg == "--as01")
		 || (arg == "--as:01")
		 || (arg == "--asdigits")
		 || (arg == "--as:digits")
		 || (arg == "--digits"))
			{
			alphabet[0] = '0';
			alphabet[1] = '1';
			continue;
			}

		// --complement

		if ((arg == "--complement"))
			{ doComplement = true;  continue; }

		// --show:density

		if ((arg == "--show:density")
		 || (arg == "--density")
		 || (arg == "--asdensity"))
			{ showAs = "density";  continue; }

		// --show:integers

		if ((arg == "--show:integers")
		 || (arg == "--show:ints")
		 || (arg == "--integers")
		 || (arg == "--ints")
		 || (arg == "--asintegers")
		 || (arg == "--asints"))
			{ showAs = "integers";  continue; }

		// --show:crc (unadvertised)

		if ((arg == "--show:crc")
		 || (arg == "--crc"))
			{ showAs = "crc";  continue; }

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

		// <start>..<end>
		// (we exclude ":" because some filename forms have ":" and ".."

		std::size_t colonIx = arg.find (':');
		std::size_t separatorIx = arg.find ("..");
		if ((colonIx == string::npos) && (separatorIx != string::npos))
			{
			startPosition = string_to_unitized_u64(arg.substr (0, separatorIx));
			endPosition   = string_to_unitized_u64(arg.substr (separatorIx+2));
			if (endPosition <= startPosition)
				chastise ("bad interval: " + arg + " (end <= start)");
			intervalSet = true;
			continue;
			}

		// <filename>

		if (BitVector::valid_filename (arg))
			{ tempBvFilenames.emplace_back(strip_blank_ends(arg));  continue; }

		// <filename>:<type>[:<offset>]

		if (arg.find(':') != string::npos)
			{ tempBvFilenames.emplace_back(strip_blank_ends(arg));  continue; }

		// unrecognized argument

		chastise ("unrecognized argument: \"" + arg + "\"");
		}

	// sanity checks

	if ((showAs == "density") or (showAs == "crc"))
		{
		if (not intervalSet)
			{
			startPosition = 0;
			endPosition   = UINT64_MAX; // this will be reduced to each vector's length
			}
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


int DumpBVCommand::execute()
	{
	if (contains(debug,"interval"))
		cerr << "interval is " << startPosition << ".." << endPosition << endl;

	std::ios::fmtflags saveCoutFlags(cout.flags());

	size_t nameWidth      = 0;
	size_t onesCountWidth = 0;
	for (const auto& bvFilename : bvFilenames)
		{
		BitVector* bv = BitVector::bit_vector(bvFilename);
		u64 numBits = bv->num_bits();

		u64 startPos   = std::min (startPosition, numBits);
		u64 endPos     = std::min (endPosition,   numBits);
		onesCountWidth = std::max (onesCountWidth, std::to_string(endPos-startPos).length());

		nameWidth = std::max (nameWidth, bv->identity().length());

		delete bv;
		}

	for (auto& bvFilename : bvFilenames)
		{
		BitVector* bv = BitVector::bit_vector (bvFilename);
		bv->load();
		if (doComplement) bv->complement();
		u64 numBits = bv->num_bits();

		u64 startPos = std::min (startPosition, numBits);
		u64 endPos   = std::min (endPosition,   numBits);

		cout.flags(saveCoutFlags);
		if (showAs == "density")
			{
			// $$$ we could use rank() to compute this
			u64 onesCount = 0;
			for (u64 pos=startPos ; pos<endPos ; pos++)
				{ if ((*bv)[pos] == 1) onesCount++; }
			cout << std::setfill(' ') << std::setw(nameWidth+1) << std::left << bv->identity()
			     << std::setw(onesCountWidth) << std::right << onesCount
			     << "/" << std::setw(onesCountWidth) << std::left << (endPos-startPos)
			     << " " << std::fixed << std::setprecision(6) << (onesCount/double(endPos-startPos)) << endl;
			}
		else if (showAs == "crc")
			{
			u32 crc = 0;
			u8  byte = 0;
			int bitsInByte = 0;
			for (u64 pos=startPos ; pos<endPos ; pos++)
				{
				byte = (byte << 1) | (*bv)[pos];
				if (++bitsInByte < 8) continue;
			    crc = update_crc(crc,byte);
				bitsInByte = 0;
				}
			if (bitsInByte > 0)
			    crc = update_crc(crc,byte<<(8-bitsInByte));
			cout << std::setfill(' ') << std::setw(nameWidth+1) << std::left << bv->identity()
			     << " " << std::setfill('0') << std::setw(4) << std::right << std::hex << std::uppercase << (crc >> 16)
			     << " " << std::setfill('0') << std::setw(4) << std::right << std::hex << std::uppercase << (crc & 0x0000FFFF) << endl;
			}
		else if (showAs == "integers")
			{
			string intsStr = "";
			u64 onesCount = 0;
			for (u64 pos=startPos ; pos<endPos ; pos++)
				{
				if ((*bv)[pos] == 1)
					{
					if (intsStr.empty())
						intsStr += std::to_string(pos);
					else
						intsStr += "," + std::to_string(pos);
					onesCount++;
					}
				}

			cout << std::setfill(' ') << std::setw(nameWidth+1) << std::left << bv->identity()
			     << std::setw(onesCountWidth) << std::right << onesCount
			     << " " << intsStr << endl;
			}
		else // if (showAs == "bits")
			{
			string idStr = bv->identity();
			string bitsStr = "";
			u64 onesCount = 0;
			u64 bitsInLine = 0;
			for (u64 pos=startPos ; pos<endPos ; pos++)
				{
				if (((pos % chunkSize) == 0) && (not bitsStr.empty()))
					bitsStr += " ";
				if ((*bv)[pos] == 0)   bitsStr += alphabet[0];
				                else { bitsStr += alphabet[1];  onesCount++; }

				bitsInLine++;
				if ((lineWrap != 0) and (bitsInLine == lineWrap))
					{
					cout << std::setfill(' ') << std::setw(nameWidth+1) << std::left << idStr
					     << std::setw(onesCountWidth) << ""
					     << " " << bitsStr << endl;
					idStr = "";
					bitsStr = "";
					bitsInLine = 0;
					}
				}
			if (startPos > 0)     bitsStr =  "..." + bitsStr;
			if (endPos < numBits) bitsStr += "...";

			cout << std::setfill(' ') << std::setw(nameWidth+1) << std::left << idStr
			     << std::setw(onesCountWidth) << std::right << onesCount
			     << " " << bitsStr << endl;
			}

		delete bv;
		}

	cout.flags(saveCoutFlags);

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	return EXIT_SUCCESS;
	}
