// cmd_node_stats.cc-- report file size and occupancy stats for a sequence
// bloom tree

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>
#include <random>

#include "utilities.h"
#include "bloom_tree.h"
#include "file_manager.h"

#include "support.h"
#include "commands.h"
#include "cmd_node_stats.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u32 std::uint32_t
#define u64 std::uint64_t


void NodeStatsCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- report file sizes and node occupancy stats for a tree" << endl;
	}

void NodeStatsCommand::usage
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
	s << "  <filename>           name of the tree toplogy file" << endl;
	s << "  --noshow:occupancy   don't report the number of 1s in each bit vector" << endl;
	s << "                       (by default we report this, but it can be slow to" << endl;
	s << "                       compute for compressed bit vector types that don't)" << endl;
	s << "                       support rank/select)" << endl;
	}

void NodeStatsCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  trackmemory" << endl;
	s << "  topology" << endl;
	s << "  load" << endl;
	s << "  traversal" << endl;
	}

void NodeStatsCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

	reportOccupancy = true;

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

		// --noshow:occupancy

		if ((arg == "--noshow:occupancy")
		 || (arg == "--nooccupancy")
		 || (arg == "--no:occupancy"))
			{ reportOccupancy = false;  continue; }

		if (arg == "--show:occupancy")
			{ reportOccupancy = true;  continue; }

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

int NodeStatsCommand::execute()
	{
	bool dbgTraversal = (contains(debug,"traversal"));

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

	vector<BloomTree*> preOrder, postOrder;
	root->pre_order(preOrder);
	root->post_order(postOrder);

	if (contains(debug,"load"))
		{
		for (const auto& node : postOrder)
			node->reportLoad = true;
		}

	bool hasOnlyChildren = false;
	for (const auto& node : postOrder)
		{
		if (node->num_children() == 1)
			{
			hasOnlyChildren = true;
			cerr << "warning: " << node->child(0)->bfFilename
				<< " is an only child" << endl;
			}
		}
	if (hasOnlyChildren)
		fatal ("error: tree contains at least one only child");

	// compute the stats

	for (const auto& node : preOrder)
		{
		if ((node->parent == nullptr)
		 || (node->parent->is_dummy()))
			node->depth = 1;
		else
			node->depth = 1+node->parent->depth;
		}

	for (const auto& node : postOrder)
		{
		if (node->num_children() == 0)
			node->height = 1;
		else
			{
			node->height = 0;
			for (const auto& child : node->children)
				{
				if (1+child->height > node->height)
					node->height = 1+child->height;
				}
			}
		}

	for (const auto& node : postOrder)
		{
		if (node->num_children() == 0)
			node->subTreeSize = 1;
		else
			{
			node->subTreeSize = 1;
			for (const auto& child : node->children)
				node->subTreeSize += child->subTreeSize;
			}
		}

	// read each node's file, and compute and print stats

	int maxBitVectors = 2;
	int numNodes = preOrder.size();

	cout << "#node"
	     << "\tdepth"
	     << "\theight"
	     << "\tsubtree"
	     << "\tsiblings"
	     << "\tniblings"
	     << "\tbf.items"
	     << "\tbf.fpRate";
	for (int bvIx=0 ; bvIx<maxBitVectors ; bvIx++)
		cout << "\t" << "bf" << bvIx << ".bytes"
		     << "\t" << "bf" << bvIx << ".bits"
		     << "\t" << "bf" << bvIx << ".ones";
	cout << endl;

	int nodeNum = 0;
	for (const auto& node : preOrder)
		{
		nodeNum++;
		node->load();
		BloomFilter* bf = node->bf;

		if (dbgTraversal)
			{
			cerr << "inspecting "
			     << "#" << nodeNum << " of " << numNodes
			     << " " << node->name
			     << " (";
			for (int bvIx=0 ; bvIx<bf->numBitVectors ; bvIx++)
				{
				BitVector* bv = bf->get_bit_vector(bvIx);
				if (bvIx > 0) cerr << "/";
				cerr << bv->num_bits();
				}
			cerr << " bits)" << endl;
			}

		// print the stats line for this node

		int siblings, niblings;
		if (node->parent == nullptr)	// note that roots in a forest *do*
			{							// .. have a parent; this case only
			siblings = niblings = 0;	// .. occurs when we have a single root
			}
		else
			{
			siblings = node->parent->num_children()-1;
			niblings = 0;
			for (const auto& child : node->parent->children)
				{ if (child != node) niblings += child->num_children(); }
			}

		cout << node->name
		     << "\t" << node->depth
		     << "\t" << node->height
		     << "\t" << node->subTreeSize
		     << "\t" << siblings
		     << "\t" << niblings;
		if (not bf->setSizeKnown)
			{
			cout << "\tNA\tNA";
			}
		else
			{
			double fpRate = BloomFilter::false_positive_rate(bf->numHashes,bf->numBits,bf->setSize);
			cout << "\t" << bf->setSize
			     << "\t" << fpRate;
			}

		for (int bvIx=0 ; bvIx<bf->numBitVectors ; bvIx++)
			{
			BitVector* bv = bf->get_bit_vector(bvIx);
			u64 numBits = bv->num_bits();

			cout << "\t" << bv->numBytes
			     << "\t" << numBits;

			if (not reportOccupancy)
				cout << "\tNA";
			else
				{
				u32 compressor = bv->compressor();
				u64 numOnes = 0;
				if ((compressor == bvcomp_uncompressed)
				 || (compressor == bvcomp_unc_rrr)
				 || (compressor == bvcomp_unc_roar))
					{
					sdslrank1 bvRanker1(bv->bits);
					numOnes = bvRanker1(bv->numBits);
					}
				else if (compressor == bvcomp_rrr)
					{
					RrrBitVector* rrrBv = (RrrBitVector*) bv;
					rrrrank1 bvRanker1(rrrBv->rrrBits);
					numOnes = bvRanker1(rrrBv->numBits);
					}
				else
					{
					for (u64 pos=0 ; pos<numBits ; pos++)
						{ if ((*bv)[pos] == 1) numOnes++; }
					}
				cout << "\t" << numOnes;
				}
			}

		for (int bvIx=bf->numBitVectors ; bvIx<maxBitVectors ; bvIx++)
			cout << "\tNA\tNA\tNA";
		cout << endl;

		node->unloadable();
		}

	// cleanup

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	delete root;
	return EXIT_SUCCESS;
	}

