// cmd_bit_stats.cc-- report file size and occupancy stats for a sequence
// bloom tree

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <random>

#include "utilities.h"
#include "bloom_tree.h"
#include "file_manager.h"

#include "support.h"
#include "commands.h"
#include "cmd_bit_stats.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;
#define u32 std::uint32_t
#define u64 std::uint64_t


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

	detActive = new u32[endPosition-startPosition];
	howActive = new u32[endPosition-startPosition];
	howOne    = new u32[endPosition-startPosition];

	for (u64 pos=startPosition ; pos<endPosition ; pos++)
		{
		detActive[pos-startPosition]
		  = howActive[pos-startPosition]
		  = howOne[pos-startPosition] = 0;
		}

	// compute the stats

	nodeNum = 0;
	collect_stats(root);

	// report the stats

	cout << "#pos"
	     << "\tdetActive"
	     << "\thowActive"
	     << "\thowOne"
	     << endl;

	for (u64 pos=startPosition ; pos<endPosition ; pos++)
		{
		cout << pos
		     << "\t" << detActive[pos-startPosition]
		     << "\t" << howActive[pos-startPosition]
		     << "\t" << howOne[pos-startPosition]
		     << endl;
		}

	// cleanup

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	delete root;
	delete[] detActive;
	delete[] howActive;
	delete[] howOne;

	return EXIT_SUCCESS;
	}


void BitStatsCommand::collect_stats
   (BloomTree*	node,
	const void*	activeBits)  // bit vector with bfWidth bits
	{
	// skip through dummy nodes

	if (node->is_dummy())
		{
		if (dbgTraversal)
			cerr << "(skipping through dummy node)" << endl;

		for (const auto& child : node->children)
			collect_stats(child,activeBits);
		return;
		}

	if (dbgTraversal)
		{
		nodeNum++;
		cerr << "collecting stats at "
			 << "#" << nodeNum
			 << " " << node->name
		     << endl;
		}

	// make sure this node is compatible with the root

	BloomFilter* bf = node->real_filter();
	bf->preload();

	node->bf->is_consistent_with(rootBf, /*beFatal*/ true);

	// reconstruct the original det,how, first decompressing (if needed), then
	// unsqueezing to recover inactive bit positions; note that if activeBits
	// is null, we don't need to unsqueeze det

……… allocate uncDet, uncHow?

	if (bit vectors are rrr, or roar, or allOnes/allZeros)
		decompress them

	if (activeBits != nullptr)
		uncDet = unsqueeze by activeBits

	uncHow = unsqueeze by uncDet

	// count stats

	for (u64 pos=startPosition ; pos<endPosition ; pos++)
		{
		if (……pos not in activeBits) continue
		detActive[pos-startPosition]++;
		if (……pos in uncDet) howActive[pos-startPosition]++;
		if (……pos in uncHow) howOne[pos-startPosition]++;
		}

	// count stats in the children (if any)

	for (const auto& child : node->children)
		collect_stats(child,uncDet);

	delete uncDet; or delete[]?
	delete uncHow; or delete[]?
	}
