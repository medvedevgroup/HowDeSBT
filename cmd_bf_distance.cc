// cmd_bf_distance.cc-- compute the bitwise distance between bloom filters

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <iomanip>

#include "utilities.h"
#include "bit_utilities.h"
#include "bit_vector.h"
#include "bloom_filter.h"
#include "file_manager.h"

#include "support.h"
#include "commands.h"
#include "cmd_bf_distance.h"

using std::string;
using std::vector;
using std::pair;
using std::cout;
using std::cerr;
using std::endl;
#define u8 std::uint8_t
#define u32 std::uint32_t
#define u64 std::uint64_t


void BFDistanceCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- compute the bitwise distance between bloom filters" << endl;
	}

void BFDistanceCommand::usage
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
	s << "  <filename>        (cumulative) a bloom filter file (usually .bf)" << endl;
	s << "                    only filters with uncompressed bit vectors are allowed" << endl;
	s << "  --list=<filename> file containing a list of bloom filters" << endl;
	s << "  <start>..<end>    interval of bits to use from each filter; distance is" << endl;
	s << "                    calculated only on this subset of each filter's bits" << endl;
	s << "                    (by default we use all bits from each filter)" << endl;
	s << "  --bits=<N>        number of bits to use from each filter; same as 0..<N>" << endl;
	s << "  --show:hamming    show the distance as hamming distance" << endl;
	s << "                    (this is the default)" << endl;
	s << "  --show:intersect  show the 'distance' as the number of 1s in common" << endl;
	s << "  --show:union      show the 'distance' as the number of 1s in either" << endl;
	s << "  --show:theta      show the 'distance' from A to B as N/D, where D is the" << endl;
	s << "                    number of 1s in A and N is the number of 1s A and B have" << endl;
	s << "                    in common; when A is a query and B is a node, this metric" << endl;
	s << "                    corresponds to the threshold setting in the query command" << endl;
	}

void BFDistanceCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  interval" << endl;
	}

void BFDistanceCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

	listFilename   = "";
	startPosition  = 0;
	endPosition    = UINT64_MAX;   // this will be reduced to min filter length
	showDistanceAs = "hamming";

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

		// --list=<filename>

		if (is_prefix_of (arg, "--list="))
			{ listFilename = argVal;  continue; }

		// --bits=<N>

		if ((is_prefix_of (arg, "--bits="))
		 ||	(is_prefix_of (arg, "B="))
		 ||	(is_prefix_of (arg, "--B=")))
			{
			startPosition = 0;
			endPosition   = string_to_unitized_u64(argVal);
			continue;
			}

		// --show:hamming, etc.

		if ((arg == "--show:hamming")
		 || (arg == "--show:xor")
		 || (arg == "--show:different")
		 || (arg == "--hamming"))
			{ showDistanceAs = "hamming";  continue; }

		if ((arg == "--show:intersect")
		 || (arg == "--show:intersection")
		 || (arg == "--show:and")
		 || (arg == "--show:both")
		 || (arg == "--intersect")
		 || (arg == "--intersection"))
			{ showDistanceAs = "intersection";  continue; }

		if ((arg == "--show:union")
		 || (arg == "--show:or")
		 || (arg == "--show:either")
		 || (arg == "--union")
		 || (arg == "--or")
		 || (arg == "--either"))
			{ showDistanceAs = "union";  continue; }


		if (arg == "--show:theta")
			{ showDistanceAs = "theta";  continue; }

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

		std::size_t separatorIx = arg.find ("..");
		if (separatorIx != string::npos)
			{
			startPosition = string_to_unitized_u64(arg.substr (0, separatorIx));
			endPosition   = string_to_unitized_u64(arg.substr (separatorIx+2));
			if (endPosition <= startPosition)
				chastise ("bad interval: " + arg + " (end <= start)");
			continue;
			}

		// <filename>

		bfFilenames.emplace_back(strip_blank_ends(arg));
		}

	// sanity checks

	if (startPosition % 8 != 0)
		chastise ("the bit interval's start (" + std::to_string(startPosition)
		        + ") has to be a multiple of 8");

	if ((bfFilenames.empty()) and (listFilename.empty()))
		chastise ("at least one bloom filter filename is required");

	return;
	}


int BFDistanceCommand::execute()
	{
	// load filenames from a list file (if necessary)

	if (not listFilename.empty())
		{
		std::ifstream in (listFilename);
	    if (not in)
			fatal ("error: failed to open \"" + listFilename + "\"");

		string line;
		int lineNum = 0;
		while (std::getline (in, line))
			{
			lineNum++;
			bfFilenames.emplace_back(strip_blank_ends(line));
			}

		in.close();
		}

	// preload all the filters, adjusting the bit limits if necessary

	vector<BitVector*> bvs;

	u64 startPos = startPosition;
	u64 endPos   = endPosition;
	bool lengthNotModified = true;
	size_t nameWidth = 0;

	for (const auto& bfFilename : bfFilenames)
		{
		std::ifstream* in = FileManager::open_file(bfFilename,std::ios::binary|std::ios::in,
		                                           /* positionAtStart*/ true);
		if (not *in)
			fatal ("error: failed to open \"" + bfFilename + "\"");
		vector<pair<string,BloomFilter*>> content
			= BloomFilter::identify_content(*in,bfFilename);

		int bvNum = 0;
		for (const auto& templatePair : content)
			{
			BloomFilter* bf = templatePair.second;
			u64 numBits = bf->num_bits();

			if ((lengthNotModified) and (numBits != endPos))
				{ if (endPos != UINT64_MAX) lengthNotModified = false; }

			startPos = std::min (startPos, numBits);
			endPos   = std::min (endPos,   numBits);

			for (int bvIx=0 ; bvIx<bf->numBitVectors ; bvIx++)
				{
				bvNum++;
				BitVector* bv = bf->surrender_bit_vector(bvIx);
				if (bv->compressor() != bvcomp_uncompressed)
					cerr << "warning: ignoring compressed bit vector #" << bvNum
					     << " in \"" << bfFilename << "\""
					     << " (" << bv->class_identity() << ")" << endl;
				else
					{
					string name = bv->filename + ":" + std::to_string(bv->offset);
					nameWidth = std::max (nameWidth,name.length());
					bvs.emplace_back(bv);
					}
				}
			delete bf;
			}

		FileManager::close_file(in);
		}

	if (bvs.empty())
		fatal ("error: found no uncompressed bit vectors");

	if (endPos != endPosition)
		{
		if ((endPosition == UINT64_MAX) and (lengthNotModified))
			cerr << "bit interval is " << startPos << ".." << endPos << endl;
		else
			cerr << "warning: reducing bit interval to " << startPos << ".." << endPos << endl;
		}
	if (startPos >= endPos)
		fatal ("error: the bit interval "
		     + std::to_string(startPos) + ".."  + std::to_string(endPos)
		     + " is empty");

	startPosition = startPos;
	endPosition   = endPos;
	u64 numBits = endPos - startPos;

	size_t distanceWidth = std::to_string(endPos-startPos).length();

	// compute the distances
	// $$$ this is pretty intrusive into BitVector

	if (contains(debug,"interval"))
		cerr << "interval is " << startPosition << ".." << endPosition << endl;

	bool distanceIsSymmetric = (showDistanceAs != "theta");

	std::ios::fmtflags saveCoutFlags(cout.flags());
	auto saveCoutPrecision = cout.precision();

	bool isFirst = true;
	u32 numVectors = bvs.size();
	for (u32 uIx=0 ; uIx<numVectors ; uIx++)
		{
		BitVector* u = bvs[uIx];
		u->load();
		string uName = u->filename + ":" + std::to_string(u->offset);
		const void*	uBits = ((u8*) u->bits->data()) + (startPosition/8);

		u64 numer = 0;
		u64 denom = 0;
		if (showDistanceAs == "theta")
			denom = bitwise_count(uBits,numBits);

		for (u32 vIx=0 ; vIx<numVectors ; vIx++)
			{
			if (vIx == uIx) continue;
			if ((distanceIsSymmetric) and (vIx < uIx)) continue;

			BitVector* v = bvs[vIx];
			v->load();
			string vName = v->filename + ":" + std::to_string(v->offset);
			const void*	vBits = ((u8*) v->bits->data()) + (startPosition/8);

			cout << std::setw(nameWidth+1) << std::left << uName
			     << std::setw(nameWidth+1) << std::left << vName;

			if (showDistanceAs == "intersection")
				{
				u64 distance = bitwise_and_count(uBits,vBits,numBits);
				cout << std::setw(distanceWidth) << std::right << distance;
				}
			else if (showDistanceAs == "union")
				{
				u64 distance = bitwise_or_count(uBits,vBits,numBits);
				cout << std::setw(distanceWidth) << std::right << distance;
				}
			else if (showDistanceAs == "theta")
				{
				double distance;
				numer = bitwise_and_count(uBits,vBits,numBits);
				cout << std::setw(distanceWidth) << std::right << numer
				     << "/" << std::setw(distanceWidth) << std::left << denom;
				if (denom > 0)
					{
					distance = numer / float(denom);
					cout.precision(4);
					cout << " " << std::setw(6) << std::left << distance;
					}
				}
			else // if (showDistanceAs == "hamming")
				{
				u64 distance = hamming_distance(uBits,vBits,numBits);
				cout << std::setw(distanceWidth) << std::right << distance;
				}

			if (isFirst)
				{ cout << " (" << showDistanceAs << ")";  isFirst = false; }
			cout << endl;

			// v->unloadable(); øøø add this
			cout.flags(saveCoutFlags);
			cout.precision(saveCoutPrecision);
			}
		// u->unloadable(); øøø add this
		}

	// clean up

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	for (const auto& bv : bvs)
		delete bv;

	return EXIT_SUCCESS;
	}
