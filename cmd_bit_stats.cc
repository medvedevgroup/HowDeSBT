// cmd_bit_stats.cc-- report active-bit stats for a sequence bloom tree

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <random>

#include "utilities.h"
#include "bit_utilities.h"
#include "bit_vector.h"
#include "bloom_tree.h"
#include "file_manager.h"

#include "support.h"
#include "commands.h"
#include "cmd_bit_stats.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u8  std::uint8_t
#define u32 std::uint32_t
#define u64 std::uint64_t

static string bit_array_string (const void* bits, const u64 numBits);


void BitStatsCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- report bit stats for a tree" << endl;
	}

void BitStatsCommand::usage
   (std::ostream& s,
	const string& message)
	{
	if (!message.empty())
		{
		s << message << endl;
		s << endl;
		}

	short_description(s);
	s << "usage: " << commandName << " <filename> [options]" << endl;
	//    123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789
	s << "  <filename>      name of the tree toplogy file" << endl;
	s << "  <start>..<end>  interval of bits to use from each filter; stats are collected" << endl;
	s << "                  only on this subset of each filter's bits" << endl;
	s << "                  (by default we use all bits from each filter)" << endl;
	s << "  --bits=<N>      number of bits to use from each filter; same as 0..<N>" << endl;
	}

void BitStatsCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  trackmemory" << endl;
	s << "  topology" << endl;
	s << "  load" << endl;
	s << "  traversal" << endl;
	s << "  bits" << endl;
	}

void BitStatsCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

	startPosition = 0;
	endPosition   = UINT64_MAX;   // this will be reduced to min filter length
	showAs        = "n_x s_x";

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

		// --bits=<N>

		if ((is_prefix_of (arg, "--bits="))
		 ||	(is_prefix_of (arg, "B="))
		 ||	(is_prefix_of (arg, "--B=")))
			{
			startPosition = 0;
			endPosition   = string_to_unitized_u64(argVal);
			continue;
			}

		// --show:actives (unadvertised)

		if ((arg == "--show:actives")
		 || (arg == "--actives")
		 || (arg == "--asactives"))
			{ showAs = "det.active how.active how.one";  continue; }

		// --show:activeone (unadvertised)

		if ((arg == "--show:activeone")
		 || (arg == "--activeone")
		 || (arg == "--asactiveone"))
			{ showAs = "det.active how.one";  continue; }

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

		// (unadvertised) --tree=<filename>, --topology=<filename>

		if ((is_prefix_of (arg, "--tree="))
		 ||	(is_prefix_of (arg, "--intree="))
		 ||	(is_prefix_of (arg, "--topology=")))
			{
			if (not inTreeFilename.empty())
				chastise ("unrecognized option: \"" + arg + "\""
				          "\ntree topology file was already given as \"" + inTreeFilename + "\"");
			inTreeFilename = argVal;
			continue;
			}

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

		if (not inTreeFilename.empty())
			chastise ("unrecognized option: \"" + arg + "\""
			          "\ntree topology file was already given as \"" + inTreeFilename + "\"");
		inTreeFilename = arg;
		}

	// sanity checks

	if (inTreeFilename.empty())
		chastise ("you have to provide a tree topology file");

	return;
	}

int BitStatsCommand::execute()
	{
	dbgTraversal = (contains(debug,"traversal"));
	dbgBits      = (contains(debug,"bits"));
	startTime    = get_wall_time();

	if (contains(debug,"trackmemory"))
		{
		FileManager::trackMemory = true;
		BloomTree::trackMemory   = true;
		BloomFilter::trackMemory = true;
		BitVector::trackMemory   = true;
		}

	BloomTree* root = BloomTree::read_topology(inTreeFilename);

	if (contains(debug,"topology"))
		root->print_topology(cerr,/*level*/0,/*format*/topofmt_nodeNames);

	if (contains(debug,"load"))
		{
		vector<BloomTree*> preOrder;
		root->pre_order(preOrder);

		for (const auto& node : preOrder)
			node->reportLoad = true;
		}

	// make sure we can support this tree and interval

	rootBf = root->real_filter();
	if (rootBf == nullptr)
		fatal ("internal error: BitStatsCommand::execute() unable to locate any bloom filter");
	rootBf->preload();

	bfKind = rootBf->kind();
	if (bfKind != bfkind_determined_brief)
		fatal ("error: only " + BloomFilter::filter_kind_to_string(bfkind_determined_brief,false)
		     + " trees are currently supported;"
		     + " (" + rootBf->filename + " is " + BloomFilter::filter_kind_to_string(bfKind,false) + ")");

	bfWidth = rootBf->num_bits();
	if (startPosition >= bfWidth)
		fatal ("error: " + std::to_string(startPosition) + ".." + std::to_string(endPosition)
		     + " extends beyond the filters in \"" + inTreeFilename + "\" "
		     + " (they have only " + std::to_string(bfWidth) + " bits)");
	if (endPosition > bfWidth) endPosition = bfWidth;

	// allocate counting arrays

	detActive = howActive = howOne = nullptr;

	detActive = new u32[endPosition-startPosition];
	howOne    = new u32[endPosition-startPosition];
	if (showAs == "det.active how.active how.one")
		howActive = new u32[endPosition-startPosition];

	for (u64 pos=startPosition ; pos<endPosition ; pos++)
		{
		if (detActive != nullptr) detActive[pos-startPosition] = 0;
		if (howActive != nullptr) howActive[pos-startPosition] = 0;
		if (howOne    != nullptr) howOne   [pos-startPosition] = 0;
		}

	// compute the stats

	BitVector* activeBv = new BitVector(bfWidth);
	activeBv->fill(1);

	nodeNum = 0;
	collect_stats(root,activeBv);

	// report the stats

	if (showAs == "det.active how.active how.one")
		{
		cout << "#pos"
		     << "\tdetActive"
		     << "\thowActive"
		     << "\thowOne"
		     << endl;

		for (u64 pos=startPosition ; pos<endPosition ; pos++)
			{
			// $$$ validate that detActive = 2*howActive-1
			cout << pos
			     << "\t" << detActive[pos-startPosition]
			     << "\t" << howActive[pos-startPosition]
			     << "\t" << howOne[pos-startPosition]
			     << endl;
			}
		}
	else if (showAs == "det.active how.one")
		{
		cout << "#pos"
		     << "\tdetActive"
		     << "\thowOne"
		     << endl;

		for (u64 pos=startPosition ; pos<endPosition ; pos++)
			{
			cout << pos
			     << "\t" << detActive[pos-startPosition]
			     << "\t" << howOne[pos-startPosition]
			     << endl;
			}
		}
	else // if (showAs == "n_x s_x")
		{
		cout << "#x"
		     << "\tn_x"
		     << "\ts_x"
		     << endl;

		for (u64 pos=startPosition ; pos<endPosition ; pos++)
			{
			u32 s = detActive[pos-startPosition];
			u32 l = (detActive[pos-startPosition] + 1) / 2;
			double h = ((double) howOne[pos-startPosition]) / l;
			cout << pos
			     << "\t" << s
			     << "\t" << h
			     << endl;
			}
		}

	// cleanup

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	delete root;
	if (detActive != nullptr) delete[] detActive;
	if (howActive != nullptr) delete[] howActive;
	if (howOne    != nullptr) delete[] howOne;
	delete activeBv;

	return EXIT_SUCCESS;
	}


void BitStatsCommand::collect_stats
   (BloomTree*	node,
	BitVector*	activeBv)  // bit vector with bfWidth bits
	{
	// skip through dummy nodes

	if (node->is_dummy())
		{
		if (dbgTraversal)
			cerr << "(skipping through dummy node)" << endl;

		for (const auto& child : node->children)
			collect_stats(child,activeBv);
		return;
		}

	if (dbgTraversal)
		{
		double elapsedTime = elapsed_wall_time(startTime);

		nodeNum++;
		cerr << "[" << std::setprecision(6) << std::fixed << elapsedTime << " secs]" 
		     << " collecting stats at #" << nodeNum << " " << node->name
		     << endl;
		}

	if (dbgBits)
		cerr << "  activeBv  = " << bit_array_string(activeBv->bits->data(),bfWidth) << endl;

	// make sure this node is compatible with the root

	node->load();
	node->bf->is_consistent_with(rootBf, /*beFatal*/ true);

	// === reconstruct the original determined,how, first decompressing (if ===
	// === needed), then unsqueezing to recover inactive bit positions      ===

	// decompress determined; note that we allocate a full-width bit vector;
	// the decompressed vector will be usually be shorter, but we'll be
	// unqueezing into the full width

	BitVector* bvDet = node->bf->get_bit_vector(0);
	BitVector* uncDet = new BitVector(bfWidth);
	u64 detNumBits = bvDet->num_bits();
	RrrBitVector* rrrBvDet = nullptr;

	switch (bvDet->compressor())
		{
		case bvcomp_uncompressed:
			bitwise_fill(uncDet->bits->data(),0,bfWidth);
			bitwise_copy(bvDet->bits->data(),uncDet->bits->data(),detNumBits);
			break;
		case bvcomp_rrr:
			rrrBvDet = (RrrBitVector*) bvDet;
			decompress_rrr(rrrBvDet->rrrBits,uncDet->bits->data(),bfWidth);
			break;
		case bvcomp_zeros:
			bitwise_fill(uncDet->bits->data(),0,bfWidth);
			break;
		case bvcomp_ones:
			bitwise_fill(uncDet->bits->data(),0,bfWidth);
			bitwise_fill(uncDet->bits->data(),1,detNumBits);
			break;
		default:
			fatal ("error: compression type " + BitVector::compressor_to_string(bvDet->compressor())
			     + " is not yet supported, for \"" + node->bfFilename + "\" determined");
			break;
		}

	if (dbgBits)
		{
		//cerr << "  detType   = " << BitVector::compressor_to_string(bvDet->compressor()) << endl;
		//cerr << "  detBits   = " << detNumBits << endl;
		cerr << "  det.brief = " << bit_array_string(uncDet->bits->data(),detNumBits) << endl;
		}

	// decompress how; note that we allocate a full-width bit vector, same as
	// for determined

	BitVector* bvHow = node->bf->get_bit_vector(1);
	BitVector* uncHow = new BitVector(bfWidth);
	u64 howNumBits = bvHow->num_bits();
	RrrBitVector* rrrBvHow = nullptr;

	switch (bvHow->compressor())
		{
		case bvcomp_uncompressed:
			bitwise_fill(uncHow->bits->data(),0,bfWidth);
			bitwise_copy(bvHow->bits->data(),uncHow->bits->data(),howNumBits);
			break;
		case bvcomp_rrr:
			rrrBvHow = (RrrBitVector*) bvHow;
			decompress_rrr(rrrBvHow->rrrBits,uncHow->bits->data(),bfWidth);
			break;
		case bvcomp_zeros:
			bitwise_fill(uncHow->bits->data(),0,bfWidth);
			break;
		case bvcomp_ones:
			bitwise_fill(uncHow->bits->data(),0,bfWidth);
			bitwise_fill(uncHow->bits->data(),1,howNumBits);
			break;
		default:
			fatal ("error: compression type " + BitVector::compressor_to_string(bvHow->compressor())
			     + " is not yet supported, for \"" + node->bfFilename + "\" how");
			break;
		}

	if (dbgBits)
		{
		//cerr << "  howType   = " << BitVector::compressor_to_string(bvHow->compressor()) << endl;
		//cerr << "  howBits   = " << howNumBits << endl;
		cerr << "  how.brief = " << bit_array_string(uncHow->bits->data(),howNumBits) << endl;
		}

	// unsqueeze determined, by activeBv; this will expand determined to full
	// width

	BitVector* tmpBv = new BitVector(bfWidth);
	bitwise_unsqueeze (uncDet->bits->data(),  detNumBits,
	                   activeBv->bits->data(),bfWidth,
	                   tmpBv->bits->data(),   bfWidth);
	std::swap(uncDet,tmpBv);

	if (dbgBits)
		cerr << "  det       = " << bit_array_string(uncDet->bits->data(),bfWidth) << endl;

	// unsqueeze how by determined; this will expand how to full width

	bitwise_unsqueeze (uncHow->bits->data(),howNumBits,
	                   uncDet->bits->data(),bfWidth,
	                   tmpBv->bits->data(), bfWidth);
	std::swap(uncHow,tmpBv);

	if (dbgBits)
		cerr << "  how       = " << bit_array_string(uncHow->bits->data(),bfWidth) << endl;

	// count stats

	for (u64 pos=startPosition ; pos<endPosition ; pos++)
		{
		u64 posWhole = pos / 64;
		u64 posPart  = pos % 64;
		u64 posMask  = ((u64)1) << posPart;

		if ((activeBv->bits->data()[posWhole] & posMask) == 0) continue;
		if (detActive != nullptr)
			detActive[pos-startPosition]++;
		if (howActive != nullptr)
			{
			if ((uncDet->bits->data()[posWhole] & posMask) != 0)
				howActive[pos-startPosition]++;
			}
		if (howOne != nullptr)
			{
			if ((uncHow->bits->data()[posWhole] & posMask) != 0)
				howOne[pos-startPosition]++;
			}
		}

	// count stats in the children (if any); note that we compute the remaining
	// active bits as tmpBv = activeBv maskby uncDet

	if (node->num_children() > 0)
		{
		bitwise_mask(activeBv->bits->data(),uncDet->bits->data(),tmpBv->bits->data(),bfWidth);
		for (const auto& child : node->children)
			collect_stats(child,tmpBv);
		}

	node->unloadable();
	delete uncDet;
	delete uncHow;
	delete tmpBv;
	}


static string bit_array_string (const void* bits, const u64 numBits)
	{
	u32 chunkSize = 10;
	u64 numChars;
	if (numBits == 0) numChars = 0;
	             else numChars = numBits + ((numBits-1)/chunkSize);

	string s(numChars,'#');  // (the # to be obvious if the string isn't filled)
	if (numBits == 0) return s;

	u64*	scan = (u64*) bits;
	u64		n, ix;

	ix = 0;
	for (n=numBits ; n>=64 ; n-=64)
		{
		u64 word = *(scan++);
		for (u32 pos=0 ; pos<64 ; pos++)
			{
			if (ix % (chunkSize+1) == chunkSize) s[ix++] = ' ';
			s[ix++] = ((word&1) == 1)? '+' : '-';
			word >>= 1;
			}
		}

	if (n == 0) return s;

	u8*	scanb = (u8*) scan;
	for ( ; n>=8 ; n-=8)
		{
		u8 word = *(scanb++);
		for (u32 pos=0 ; pos<8 ; pos++)
			{
			if (ix % (chunkSize+1) == chunkSize) s[ix++] = ' ';
			s[ix++] = ((word&1) == 1)? '+' : '-';
			word >>= 1;
			}
		}

	if (n == 0) return s;

	u8 word = *(scanb++);
	for ( ; n>0 ; n--)
		{
		if (ix % (chunkSize+1) == chunkSize) s[ix++] = ' ';
		s[ix++] = ((word&1) == 1)? '+' : '-';
		word >>= 1;
		}

	return s;
	}

