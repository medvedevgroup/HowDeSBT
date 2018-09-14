// cmd_dump_bf.cc-- dump the content of a bloom filter to the console

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <vector>

#include "utilities.h"
#include "prng.h"
#include "bloom_filter.h"
#include "file_manager.h"

#include "support.h"
#include "commands.h"
#include "cmd_dump_bf.h"

using std::string;
using std::vector;
using std::pair;
using std::cout;
using std::cerr;
using std::endl;
#define u8  std::uint8_t
#define u32 std::uint32_t
#define u64 std::uint64_t


#define HEX_04        std::setfill('0')<<std::setw(4)<<std::right<<std::hex<<std::uppercase
#define HEX_X08 "0x"<<std::setfill('0')<<std::setw(8)<<std::right<<std::hex<<std::uppercase


void DumpBFCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- dump the content of a bloom filter to the console" << endl;
	}

void DumpBFCommand::usage
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
	s << "  <filename>      (cumulative) a bloom filter file (usually .bf)" << endl;
	s << "  --bits=<N>      limit of the number of bits to display from each filter" << endl;
	s << "                  (default is " << defaultEndPosition << ")" << endl;
	s << "  <start>..<end>  interval of bits to display from each filter" << endl;
	s << "                  (exclusive of --bits)" << endl;
	s << "  --wrap=<N>      number of bit positions allowed on a line" << endl;
	s << "                  (by default all positions are on the same line)" << endl;
	s << "  --chunk=<N>     number of bit positions shown in each chunk" << endl;
	s << "                  (default is 10)" << endl;
	s << "  --as01          show each bit as a 0 or 1" << endl;
	s << "                  (by default we show zeros as '-' and ones as '+')" << endl;
	s << "  --complement    show the bitwise complement of each filter" << endl;
	s << "  --show:density  show fraction of ones in the filter (instead of showing bits)" << endl;
	s << "  --show:checksum show a checksum of filter's bits (instead of showing bits)" << endl;
	s << "  --show:integers show bit positions as a list of integers" << endl;
	s << "  --show:header   show the filter's header info (instead of any bit data)" << endl;
	}

void DumpBFCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  interval" << endl;
	s << "  singleton" << endl;
	}

void DumpBFCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

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

		// --show:density, etc. (some unadvertised)

		if ((arg == "--show:density")
		 || (arg == "--density")
		 || (arg == "--asdensity"))
			{ showAs = "density";  continue; }

		if ((arg == "--show:integers")
		 || (arg == "--show:ints")
		 || (arg == "--integers")
		 || (arg == "--ints")
		 || (arg == "--asintegers")
		 || (arg == "--asints"))
			{ showAs = "integers";  continue; }

		if ((arg == "--show:ranks")
		 || (arg == "--show:rank")
		 || (arg == "--ranks")
		 || (arg == "--rank")
		 || (arg == "--asranks")
		 || (arg == "--asrank"))
			{ showAs = "ranks";  continue; }

		if ((arg == "--show:header")
		 || (arg == "--header"))
			{ showAs = "header";  continue; }

		if ((arg == "--show:checksum")
		 || (arg == "--show:crc")
		 || (arg == "--checksum")
		 || (arg == "--crc"))
			{ showAs = "checksum";  continue; }

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
			try
				{
				startPosition = string_to_unitized_u64(arg.substr (0, separatorIx));
				endPosition   = string_to_unitized_u64(arg.substr (separatorIx+2));
				if (endPosition <= startPosition)
					chastise ("bad interval: " + arg + " (end <= start)");
				}
			catch (const std::invalid_argument& ex)
				{
				chastise ("bad interval: " + arg + " (parsing error: \"" + ex.what() + "\")");
				}
			intervalSet = true;
			continue;
			}

		// <filename>

		bfFilenames.emplace_back(strip_blank_ends(arg));
		}

	// sanity checks

	if ((showAs == "density") or (showAs == "checksum"))
		{
		if (not intervalSet)
			{
			startPosition = 0;
			endPosition   = UINT64_MAX; // this will be reduced to each filter's length
			}
		}

	if (bfFilenames.empty())
		chastise ("at least one bloom filter filename is required");

	return;
	}


int DumpBFCommand::execute()
	{
	if (contains(debug,"interval"))
		cerr << "interval is " << startPosition << ".." << endPosition << endl;

	// preload every filter, so we can determine how many characters to devote
	// to a name and count fields

	nameWidth      = 0;
	onesCountWidth = 0;
	for (const auto& bfFilename : bfFilenames)
		{
		if (contains(debug,"singleton"))
			{
			BloomFilter* bf = BloomFilter::bloom_filter(bfFilename);
			bf->preload();
			u64 numBits = bf->num_bits();

			u64 startPos   = std::min (startPosition, numBits);
			u64 endPos     = std::min (endPosition,   numBits);
			onesCountWidth = std::max (onesCountWidth, std::to_string(endPos-startPos).length());

			nameWidth = std::max (nameWidth, bf->identity().length());

			delete bf;
			}

		else
			{
			std::ifstream* in = FileManager::open_file(bfFilename,std::ios::binary|std::ios::in,
			                                           /* positionAtStart*/ true);
			if (not *in)
				fatal ("error: failed to open \"" + bfFilename + "\"");
			vector<pair<string,BloomFilter*>> content
				= BloomFilter::identify_content(*in,bfFilename);

			for (const auto& templatePair : content)
				{
				string       bfName = templatePair.first;
				BloomFilter* bf     = templatePair.second;
				u64 numBits = bf->num_bits();

				u64 startPos   = std::min (startPosition, numBits);
				u64 endPos     = std::min (endPosition,   numBits);
				onesCountWidth = std::max (onesCountWidth, std::to_string(endPos-startPos).length());

				for (int bvIx=0 ; bvIx<bf->numBitVectors ; bvIx++)
					{
					if      (showAs == "header")  bfName = bfFilename;
					else if (content.size() == 1) bfName = bfFilename;
											 else bfName += "[" + bfFilename + "]";
					if (bf->numBitVectors > 1) bfName += "." + std::to_string(bvIx);
					nameWidth = std::max (nameWidth, bfName.length());
					}

				delete bf;

				if (showAs == "header") break;
				}

			FileManager::close_file(in);
			}

		}

	for (const auto& bfFilename : bfFilenames)
		{
		if (contains(debug,"singleton"))
			{
			BloomFilter* bf = BloomFilter::bloom_filter(bfFilename);
			if (showAs == "header")
				bf->preload();
			else
				{
				bf->load();
				if (doComplement) bf->complement(-1);
				}
			dump_one_bloom_filter (bf->identity(), bf);
			delete bf;
			}
		else
			{
			std::ifstream* in = FileManager::open_file(bfFilename,std::ios::binary|std::ios::in,
			                                           /* positionAtStart*/ true);
			if (not *in)
				fatal ("error: failed to open \"" + bfFilename + "\"");
			vector<pair<string,BloomFilter*>> content
				= BloomFilter::identify_content(*in,bfFilename);

			for (const auto& bfPair : content)
				{
				string       bfName = bfPair.first;
				BloomFilter* bf     = bfPair.second;
				if (showAs != "header")
					{
					bf->load();
					if (doComplement) bf->complement(-1);
					}

				if      (showAs == "header")  bfName = bfFilename;
				else if (content.size() == 1) bfName = bfFilename;
				                         else bfName += "[" + bfFilename + "]";

				dump_one_bloom_filter (bfName, bf);
				delete bf;

				if (showAs == "header") break;
				}

			FileManager::close_file(in);
			}
		}

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	return EXIT_SUCCESS;
	}


void DumpBFCommand::dump_one_bloom_filter
   (const string&		_bfName,
	const BloomFilter*	bf)
	{
	std::ios::fmtflags saveCoutFlags(cout.flags());
	cout << std::setfill(' ');

	u64 numBits = bf->num_bits();
	u64 startPos = std::min (startPosition, numBits);
	u64 endPos   = std::min (endPosition,   numBits);

	for (int bvIx=0 ; bvIx<bf->numBitVectors ; bvIx++)
		{
		BitVector* bv = bf->get_bit_vector(bvIx);
		u32 compressor = bv->compressor();

		u64 bvNumBits = numBits;
		if (bv->isResident) bvNumBits = bv->num_bits();
		u64 bvEndPos = std::min(endPos,bvNumBits);

		string bfName = _bfName;
		if (bf->numBitVectors > 1) bfName += "." + std::to_string(bvIx);

		cout.flags(saveCoutFlags);
		if (showAs == "header")
			{
			// $$$ bf->numBits doesn't reflect the bit*vector*s actual bit
			//     count, but the goal here is to display the bf file's header,
			//     and the bv's have not been completely loaded
			cout << std::setfill(' ') << std::setw(nameWidth+1) << std::left << bfName
				 << " (" << BitVector::compressor_to_string(compressor) << ")"
				 << " k="       << bf->kmerSize
				 << " hashes="  << bf->numHashes
				 << " seed="    << bf->hashSeed1 << "," << bf->hashSeed2
				 << " modulus=" << bf->hashModulus
				 << " bits="    << bf->numBits
				 << " segment=" << HEX_X08 << bv->offset
				 << ".."        << HEX_X08 << (bv->offset+bv->numBytes)
				 << endl;
			}
		else if (showAs == "density")
			{
			// $$$ we could use rank() to compute this
			u64 onesCount = 0;
			for (u64 pos=startPos ; pos<bvEndPos ; pos++)
				{ if ((*bv)[pos] == 1) onesCount++; }
			cout << std::setfill(' ') << std::setw(nameWidth+1) << std::left << bfName
				 << std::setw(onesCountWidth) << std::right << onesCount
				 << "/" << std::setw(onesCountWidth) << std::left << (bvEndPos-startPos)
				 << " " << std::fixed << std::setprecision(6) << (onesCount/double(bvEndPos-startPos)) << endl;
			}
		else if (showAs == "checksum")
			{
			u32 crc = 0;
			u8  byte = 0;
			int bitsInByte = 0;
			for (u64 pos=startPos ; pos<bvEndPos ; pos++)
				{
				byte = (byte << 1) | (*bv)[pos];
				if (++bitsInByte < 8) continue;
				crc = update_crc(crc,byte);
				bitsInByte = 0;
				}
			if (bitsInByte > 0)
				crc = update_crc(crc,byte<<(8-bitsInByte));
			cout << std::setfill(' ') << std::setw(nameWidth+1) << std::left << bfName
				 << " " << HEX_04 << (crc >> 16)
				 << " " << HEX_04 << (crc & 0x0000FFFF) << endl;
			}
		else if (showAs == "integers")
			{
			string intsStr = "";
			u64 onesCount = 0;
			for (u64 pos=startPos ; pos<bvEndPos ; pos++)
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

			cout << std::setfill(' ') << std::setw(nameWidth+1) << std::left << bfName
				 << std::setw(onesCountWidth) << std::right << onesCount
				 << " " << intsStr << endl;
			}
		else if (showAs == "ranks")
			{
			std::stringstream posSs;
			if (startPos > 0) posSs << "...";
			for (u64 pos=startPos ; pos<bvEndPos ; pos++)
				posSs << " " << std::setfill(' ') << std::setw(onesCountWidth) << std::to_string(pos);

			std::stringstream bitsSs;
			if (startPos > 0) bitsSs << "...";
			for (u64 pos=startPos ; pos<bvEndPos ; pos++)
				{
				char bitCh = ((*bv)[pos] == 0)? alphabet[0] : alphabet[1];
				bitsSs << " " << std::setfill(' ') << std::setw(onesCountWidth) << bitCh;
				}
			if (endPos < numBits) bitsSs << "...";

			std::stringstream rank0Ss;
			if (startPos > 0) rank0Ss << "...";

			std::stringstream select0Ss;
			if (startPos > 0) select0Ss << "...";

			std::stringstream rank1Ss;
			if (startPos > 0) rank1Ss << "...";

			std::stringstream select1Ss;
			if (startPos > 0) select1Ss << "...";

			bool rankSupported = false;
			if ((compressor == bvcomp_uncompressed)
			 || (compressor == bvcomp_rrr)
			 || (compressor == bvcomp_roar))
				{
				rankSupported = true;

				sdslrank0   bvRanker0(bv->bits);
				sdslrank1   bvRanker1(bv->bits);
				sdslselect0 bvSelector0(bv->bits);
				sdslselect1 bvSelector1(bv->bits);

				size_t numZeros = bvRanker0(bv->numBits);
				size_t numOnes  = bvRanker1(bv->numBits);

				for (u64 pos=startPos ; pos<bvEndPos ; pos++)
					{
					u64 posRank = bvRanker0.rank(pos);
					rank0Ss << " " << std::setfill(' ') << std::setw(onesCountWidth) << std::to_string(posRank);
					}

				for (u64 pos=startPos ; pos<bvEndPos ; pos++)
					{
					if ((pos < 1) || (pos > numZeros))
						select0Ss << " " << std::setfill(' ') << std::setw(onesCountWidth) << "*";
					else
						{
						u64 posSelect = bvSelector0.select(pos);
						select0Ss << " " << std::setfill(' ') << std::setw(onesCountWidth) << std::to_string(posSelect);
						}
					}

				for (u64 pos=startPos ; pos<bvEndPos ; pos++)
					{
					u64 posRank = bvRanker1.rank(pos);
					rank1Ss << " " << std::setfill(' ') << std::setw(onesCountWidth) << std::to_string(posRank);
					}

				for (u64 pos=startPos ; pos<bvEndPos ; pos++)
					{
					if ((pos < 1) || (pos > numOnes))
						select1Ss << " " << std::setfill(' ') << std::setw(onesCountWidth) << "*";
					else
						{
						u64 posSelect = bvSelector1.select(pos);
						select1Ss << " " << std::setfill(' ') << std::setw(onesCountWidth) << std::to_string(posSelect);
						}
					}
				}
			else if (compressor == bvcomp_rrr)
				{
				rankSupported = true;

				RrrBitVector* rrrBv = (RrrBitVector*) bv;
				rrrrank0   bvRanker0(rrrBv->rrrBits);
				rrrrank1   bvRanker1(rrrBv->rrrBits);
				rrrselect0 bvSelector0(rrrBv->rrrBits);
				rrrselect1 bvSelector1(rrrBv->rrrBits);

				size_t numZeros = bvRanker0(rrrBv->numBits);
				size_t numOnes  = bvRanker1(rrrBv->numBits);

				for (u64 pos=startPos ; pos<bvEndPos ; pos++)
					{
					u64 posRank = bvRanker0.rank(pos);
					rank0Ss << " " << std::setfill(' ') << std::setw(onesCountWidth) << std::to_string(posRank);
					}

				for (u64 pos=startPos ; pos<bvEndPos ; pos++)
					{
					if ((pos < 1) || (pos > numZeros))
						select0Ss << " " << std::setfill(' ') << std::setw(onesCountWidth) << "*";
					else
						{
						u64 posSelect = bvSelector0.select(pos);
						select0Ss << " " << std::setfill(' ') << std::setw(onesCountWidth) << std::to_string(posSelect);
						}
					}

				for (u64 pos=startPos ; pos<bvEndPos ; pos++)
					{
					u64 posRank = bvRanker1.rank(pos);
					rank1Ss << " " << std::setfill(' ') << std::setw(onesCountWidth) << std::to_string(posRank);
					}

				for (u64 pos=startPos ; pos<bvEndPos ; pos++)
					{
					if ((pos < 1) || (pos > numOnes))
						select1Ss << " " << std::setfill(' ') << std::setw(onesCountWidth) << "*";
					else
						{
						u64 posSelect = bvSelector1.select(pos);
						select1Ss << " " << std::setfill(' ') << std::setw(onesCountWidth) << std::to_string(posSelect);
						}
					}
				}

			if (rankSupported)
				{
				cout << std::setfill(' ') << std::setw(nameWidth+1) << std::left  << bfName
					 << posSs.str() << endl;
				cout << std::setfill(' ') << std::setw(nameWidth+1) << std::right << "bits:"
					 << bitsSs.str() << endl;
				cout << std::setfill(' ') << std::setw(nameWidth+1) << std::right << "rank0:"
					 << rank0Ss.str() << endl;
				cout << std::setfill(' ') << std::setw(nameWidth+1) << std::right << "select0:"
					 << select0Ss.str() << endl;
				cout << std::setfill(' ') << std::setw(nameWidth+1) << std::right << "rank1:"
					 << rank1Ss.str() << endl;
				cout << std::setfill(' ') << std::setw(nameWidth+1) << std::right << "select1:"
					 << select1Ss.str() << endl;
				}
			else
				{
				cout << std::setfill(' ') << std::setw(nameWidth+1) << std::left << bv->identity()
				     << " (rank is not supported for this vector type)" << endl;
				}
			}
		else // if (showAs == "bits")
			{
			string idStr = bfName;
			string bitsStr = "";
			u64 onesCount = 0;
			u64 bitsInLine = 0;
			for (u64 pos=startPos ; pos<bvEndPos ; pos++)
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

		cout.flags(saveCoutFlags);
		}

	}
