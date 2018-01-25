// cmd_cluster.cc-- determine a tree topology by clustering bloom filters

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <queue>

#include "utilities.h"
#include "bit_utilities.h"
#include "bloom_filter.h"

#include "support.h"
#include "commands.h"
#include "cmd_build_sbt.h"
#include "cmd_cluster.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u32 std::uint32_t
#define u64 std::uint64_t

void ClusterCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- determine a tree topology by clustering bloom filters" << endl;
	}

void ClusterCommand::usage
   (std::ostream& s,
	const string& message)
	{
	if (!message.empty())
		{
		s << message << endl;
		s << endl;
		}

	short_description(s);
	s << "usage: " << commandName << " [options]" << endl;
	//    123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789
	s << "  --list=<filename> file containing a list of bloom filters to cluster; only" << endl;
	s << "                    filters with uncompressed bit vectors are allowed" << endl;
	s << "  <filename>        same as --list=<filename>" << endl;
	s << "  --out=<filename>  name for tree toplogy file" << endl;
	s << "                    (by default this is derived from the list filename)" << endl;
	s << "  --tree=<filename> same as --out=<filename>" << endl;
	s << "  --node=<template> filename template for internal tree nodes" << endl;
	s << "                    this must contain the substring {node}" << endl;
	s << "                    (by default this is derived from the list filename)" << endl;
	s << "  <start>..<end>    interval of bits to use from each filter; the clustering" << endl;
	s << "                    algorithm only considers this subset of each filter's bits" << endl;
	s << "                    (by default we use the first " << defaultEndPosition << " bits)" << endl;
	s << "  --bits=<N>        number of bits to use from each filter; same as 0..<N>" << endl;
	s << "  --nobuild         perform the clustering but don't build the tree's nodes" << endl;
	s << "                    (this is the default)" << endl;
	s << "  --build           perform clustering, then build the uncompressed nodes" << endl;
	}

void ClusterCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  trackmemory" << endl;
	s << "  bvcreation" << endl;
	s << "  interval" << endl;
	s << "  offsets" << endl;
	s << "  console" << endl;
	s << "  bits" << endl;
	s << "  distances" << endl;
	s << "  queue" << endl;
	s << "  mergings" << endl;
	s << "  numbers" << endl;
	}

void ClusterCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

	startPosition = 0;
	endPosition   = defaultEndPosition;
	inhibitBuild  = true;

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

		// --out=<filename>, --tree=<filename>, etc.

		if ((is_prefix_of (arg, "--out="))
		 ||	(is_prefix_of (arg, "--output="))
		 ||	(is_prefix_of (arg, "--tree="))
		 ||	(is_prefix_of (arg, "--outtree="))
		 ||	(is_prefix_of (arg, "--topology=")))
			{ treeFilename = argVal;  continue; }

		// --node=<template>

		if ((is_prefix_of (arg, "--node="))
		 ||	(is_prefix_of (arg, "--nodes="))
		 ||	(is_prefix_of (arg, "--nodename="))
		 ||	(is_prefix_of (arg, "--nodenames=")))
			{
			nodeTemplate = argVal; 
			std::size_t fieldIx = nodeTemplate.find ("{node}");
			if (fieldIx == string::npos)
				chastise ("--node is required to contain the substring \"{node}\"");
			continue;
			}

		// --bits=<N>

		if ((is_prefix_of (arg, "--bits="))
		 ||	(is_prefix_of (arg, "B="))
		 ||	(is_prefix_of (arg, "--B=")))
			{
			startPosition = 0;
			endPosition   = string_to_unitized_u64(argVal);
			continue;
			}

		// --nobuild, --build

		if ((arg == "--nobuild")
		 ||	(arg == "--dontbuild"))
			{ inhibitBuild = true;  continue; }

		if (arg == "--build")
			{ inhibitBuild = false;  continue; }

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

		listFilename = arg;
		}

	// sanity checks

	if (startPosition % 8 != 0)
		chastise ("the bit interval's start (" + std::to_string(startPosition)
		        + ") has to be a multiple of 8");

	if (listFilename.empty())
		chastise ("you have to provide a file, listing the bloom filters for the tree");

	if (treeFilename.empty())
		{
		string::size_type dotIx = listFilename.find_last_of(".");
		if (dotIx == string::npos)
			treeFilename = listFilename + ".sbt";
		else
			treeFilename = listFilename.substr(0,dotIx) + ".sbt";
		}

	if (nodeTemplate.empty())
		{
		string::size_type dotIx = listFilename.find_last_of(".");
		if (dotIx == string::npos)
			nodeTemplate = listFilename + "{node}.bf";
		else
			nodeTemplate = listFilename.substr(0,dotIx) + "{node}.bf";
		}

	return;
	}


ClusterCommand::~ClusterCommand()
	{
	for (const auto& bf : leafVectors)
		delete bf;
	if (treeRoot != NULL) delete treeRoot;
	}


int ClusterCommand::execute()
	{
	if (contains(debug,"trackmemory"))
		{
		trackMemory              = true;
		BloomFilter::trackMemory = true;
		BitVector::trackMemory   = true;
		}
	if (contains(debug,"bvcreation"))
		BitVector::reportCreation = true;

	if (contains(debug,"interval"))
		cerr << "interval is " << startPosition << ".." << endPosition << endl;

	find_leaf_vectors ();

	if (contains(debug,"offsets"))
		{
		for (const auto& bv : leafVectors)
			cerr << "bit vector " << bv->filename << " " << bv->offset << endl;
		}

	cluster_greedily ();

	if (contains(debug,"console"))
		print_topology(cout,treeRoot,0);
	else
		{
	    std::ofstream out(treeFilename);
		print_topology(out,treeRoot,0);
		}

	// build the tree (we defer this to the "build" command)

	string commandLine = "sabutan build \"" + treeFilename + "\"";

	if (inhibitBuild)
		{
		cerr << treeFilename << " has been created"
		     << ", but the internal nodes have not been built." << endl;
		cerr << "You can use this command to build them:" << endl;
		cerr << commandLine << endl;
		}
	else
		deferredCommands.emplace_back(commandLine);

	return EXIT_SUCCESS;
	}

//----------
//
// find_leaf_vectors--
//	Determine the bit vectors that will be the leaves of the tree.
//
// We don't *load* the vectors, but establish a list of BitVector objects that
// point to the subset interval within the corresponding bloom filter file.
//	
//----------

void ClusterCommand::find_leaf_vectors()
	{
	// read the filter headers; validate that they all have bit vectors of the
	// correct type, and that they all have the same parameters

	std::ifstream in (listFilename);
	if (not in)
		fatal ("error: failed to open \"" + listFilename + "\"");

	BloomFilter* firstBf = nullptr;
	bool listIsEmpty = true;

	string bfFilename;
	int lineNum = 0;
	while (std::getline (in, bfFilename))
		{
		lineNum++;

		// read the filter's header and verify filter consistency and vector
		// types; note that this does *not* load the bit vector
		// note bene: we keep the first filter open (until we're done) so we
		// can check that all the other vectors are consistent with it
		// $$$ MULTI_VECTOR what if the filter contains more than one bit vector!

		BloomFilter* bf = new BloomFilter (strip_blank_ends(bfFilename));
		bf->preload();
		BitVector* bv = bf->get_bit_vector();
		if (bv->compressor() != bvcomp_uncompressed)
			fatal ("error: bit vectors in \"" + bfFilename + "\" are not uncompressed");

		if (firstBf == nullptr)
			{
			firstBf = bf;
			listIsEmpty = false;
			if (bf->numBits <= startPosition)
				fatal ("error: " + bfFilename + " has only " + std::to_string(bf->numBits) + " bits"
				     + ", so the bit interval "
				     + std::to_string(startPosition) + ".."  + std::to_string(endPosition)
				     + " would be empty");
			if (bf->numBits < endPosition)
				{
				endPosition = bf->numBits;
				cerr << "warning: reducing bit interval to " << startPosition << ".." << endPosition << endl;
				}
			}
		else
			bf->is_consistent_with (firstBf, /*beFatal*/ true);

		// discard the bloom filter (and its bit vector) and create a new "raw"
		// bit vector with the desired bit subset interval

		string bvFilename = bv->filename;
		size_t offset = bv->offset;
		if (bf != firstBf) delete bf;

		size_t startOffset = offset + sdslbitvectorHeaderBytes + startPosition/8;
		string rawFilename = bvFilename
		                   + ":raw"
		                   + ":" + std::to_string(startOffset)
		                   + ":" + std::to_string(endPosition-startPosition);

		BitVector* rawBv = BitVector::bit_vector(rawFilename);
		leafVectors.emplace_back(rawBv);
		}

	if (firstBf != nullptr) delete firstBf;

	in.close();

	// make sure the list wasn't empty

	if (listIsEmpty)
		fatal ("error: \"" + listFilename + "\" contains no bloom filters");
	}

//----------
//
// cluster_greedily--
//	Determine a binary tree structure by greedy clustering.
//
// The clustering process consist of repeatedly (a) choosing the closest pair
// of nodes, and (b) replacing those nodes with a new node that is their union.
//
//----------
//
// Implementation notes:
//	(1)	We use a STL priority queue to keep track of node-to-node distances.
//		Note that by using greater-than as the comparison operator means the
//		smallest distances are on the top of the queue.
//	(2)	We involve the subtree height in the comparison, as a tie breaker. This
//		is to prevent a known degenerate case where a batch of empty nodes (all
//		of which have distance zero to each other) would cluster like a ladder.
//		In a more general case it may keep the overall tree height shorter, but
//		such cases are probably rare.
// 
//----------

struct MergeCandidate
	{
public:
	u64 d;		// distance between u and v
	u32 height;	// height of a subtree containing the u-v merger as its root
	u32 u;		// one node (index into node)
	u32 v;		// other node (index into node)
	};

bool operator>(const MergeCandidate& lhs, const MergeCandidate& rhs)
	{
	if (lhs.d      != rhs.d)      return (lhs.d      > rhs.d);
	if (lhs.height != rhs.height) return (lhs.height > rhs.height);
	if (lhs.u      != rhs.u)      return (lhs.u      > rhs.u);
	return (lhs.v > rhs.v);
	}

void ClusterCommand::cluster_greedily()
	{
	u64 numBits = endPosition - startPosition;
	u64 numBytes = (numBits + 7) / 8;
	u32 numLeaves = leafVectors.size();

	if (numLeaves == 0)
		fatal ("internal error: cluster_greedily() asked to cluster an empty nodelist");

	u32 numNodes = 2*numLeaves - 1;  // nodes in tree, including leaves
	BinaryTree* node[numNodes];

	// load the bit arrays for the leaves

	for (u32 u=0 ; u<numLeaves ; u++)
		{
		BitVector* bv = leafVectors[u];
		bv->load();
		node[u] = new BinaryTree(u,bv->bits->data());
		if (node[u] == nullptr)
			fatal ("error: failed to create BinaryTree for node[" + std::to_string(u) + "]");
		if (trackMemory)
			cerr << "@+" << node[u] << " creating BinaryTree for node[" << u << "]" << endl;
		node[u]->trackMemory = trackMemory;

		if (contains(debug,"bits"))
			{ cerr << u << ": ";  dump_bits (cerr, node[u]->bits);  cerr << endl; }
		}

	// fill the priority queue with all-vs-all distances among the leaves

	std::priority_queue<MergeCandidate, vector<MergeCandidate>, std::greater<MergeCandidate>> q;

	for (u32 u=0 ; u<numLeaves-1 ; u++)
		{
		for (u32 v=u+1 ; v<numLeaves ; v++)
			{
			u64 d = hamming_distance (node[u]->bits, node[v]->bits, numBits);
			if (contains(debug,"distances"))
				cerr << "node " << u << " vs " << "node " << v << " d=" << d << " h=" << 2 << endl;
			if (contains(debug,"queue"))
				cerr << "pushing (" << d << "," << 2 << "," << u << "," << v << ")" << endl;
			MergeCandidate c = { d,2,u,v };
			q.push (c);
			}
		}

	// for each new node,
	//	- pop the closest active pair (u,v) from the queue
	//	- create a new node w = union of (u,v)
	//	- deactivate u and v by removing their bit arrays
	//	- add the distance to w from each active node

	for (u32 w=numLeaves ; w<numNodes ; w++)
		{
		// pop the closest active pair (u,v) from the queue; nodes that have
		// been deactivate have a null bit array, but still have entries in
		// the queue

		u64 d;
		u32 height, u, v;

		while (true)
			{
			if (q.empty())
				fatal ("internal error: cluster_greedily() queue is empty");

			MergeCandidate cand = q.top();
			q.pop();
			if (contains(debug,"queue"))
				cerr << "popping (" << cand.d << "," << cand.height << "," << cand.u << "," << cand.v << ")"
				     << " q.size()=" << q.size() << endl;
			if (node[cand.u]->bits == nullptr) continue; // u isn't active
			if (node[cand.v]->bits == nullptr) continue; // v isn't active
			d      = cand.d;
			height = cand.height;
			u      = cand.u;
			v      = cand.v;
			break;
			}

		if (contains(debug,"mergings"))
			cerr << "merge " << u << " and " << v << " to make " << w
			     << " (hamming distance " << d << ")" << endl;

		// create a new node w = union of (u,v)

		u64* wBits = (u64*) new char[numBytes];
		if (wBits == nullptr)
			fatal ("error: failed to allocate " + std::to_string(numBytes) + " bytes"
			     + " for node " + std::to_string(w) + "'s bit array");
		if (trackMemory)
			cerr << "@+" << wBits << " allocating bits for node[" << w << "]"
			     << " (merges node[" << u << " and node[" << v << "])" << endl;

		bitwise_or (node[u]->bits, node[v]->bits, /*dst*/ wBits, numBits);

		node[w] = new BinaryTree(w,wBits,node[u],node[v]);
		if (node[w] == nullptr)
			fatal ("error: failed to create BinaryTree for node[" + std::to_string(w) + "]");
		if (trackMemory)
			cerr << "@+" << node[w] << " creating BinaryTree for node[" << w << "]" << endl;
		node[w]->trackMemory = trackMemory;

		if (contains(debug,"bits"))
			{ cerr << w << ": ";  dump_bits (cerr, wBits);  cerr << endl; }

		// deactivate u and v by removing their bit arrays; if either was a
		// leaf tell the corresonding bit vector it can get rid of its bits

		if (u < numLeaves)
			leafVectors[u]->discard_bits();
		else
			{
			if (trackMemory)
				cerr << "@-" << node[u]->bits << " discarding bits for node[" << u << "]" << endl;
			delete[] node[u]->bits;
			}

		if (v < numLeaves)
			leafVectors[v]->discard_bits();
		else
			{
			if (trackMemory)
				cerr << "@-" << node[v]->bits << " discarding bits for node[" << v << "]" << endl;
			delete[] node[v]->bits;
			}

		node[u]->bits = nullptr;
		node[v]->bits = nullptr;

		// add the distance to w from each active node

		for (u32 x=0 ; x<w ; x++)
			{
			if (node[x]->bits == nullptr) continue; // x isn't active
			u64 d = hamming_distance (node[x]->bits, wBits, numBits);
			u32 h = 1 + std::max (height,node[x]->height);
			if (contains(debug,"distances"))
				cerr << "node " << x << " vs " << "node " << w << " d=" << d << " h=" << h << endl;
			if (contains(debug,"queue"))
				cerr << "pushing (" << d << "," << h << "," << x << "," << w << ")" << endl;
			MergeCandidate c = { d,h,x,w };
			q.push (c);
			}
		}

	// get rid of the root

	u32 root = numNodes-1;
	if (trackMemory)
		cerr << "@-" << node[root]->bits << " discarding bits for node[" << root << "]" << endl;
	delete[] node[root]->bits;
	node[root]->bits = nullptr;

	// sanity check -- the only thing left in node list should be the root

	bool failure = false;
	for (u32 x=0 ; x<numNodes ; x++)
		{
		if (node[x]->bits == nullptr) continue; // x isn't active
		cerr << "uh-oh: node " << x << " was never merged" << endl;
		failure = true;
		}

	if (failure)
		fatal ("internal error: cluster_greedily() sanity check failed");

	treeRoot = node[root];
	}

//----------
//
// print_topology--
//	
//----------

void ClusterCommand::print_topology
   (std::ostream&	out,
	BinaryTree*		root,
	int				level)
	{
	u32				numLeaves = leafVectors.size();
	u32				nodeNum = root->nodeNum;
	string			nodeName;

	if (nodeNum < numLeaves)
		nodeName = leafVectors[nodeNum]->filename;
	else
		{
		nodeName = nodeTemplate;
		string field = "{node}";
		std::size_t fieldIx = nodeName.find (field);
		if (fieldIx == string::npos)
			fatal ("internal error: nodeTemplate=\"" + nodeTemplate + "\""
			     + "does not contain \"{node}\"");
		nodeName.replace (fieldIx, field.length(), std::to_string(1+nodeNum));
		}

	if (not contains(debug,"numbers"))
		out << string(level,'*');
	else if (level == 0)
		out << "- (" << nodeNum << ") ";
	else
		out << string(level,'*') << " (" << nodeNum << ") ";
	out << nodeName << endl;

	if (root->children[0] != nullptr) print_topology (out, root->children[0], level+1);
	if (root->children[1] != nullptr) print_topology (out, root->children[1], level+1);
	}

//----------
//
// dump_bits--
//	Write a bit array to a steam, in human-readable form (for debugging).
//	
//----------

void ClusterCommand::dump_bits
   (std::ostream&	out,
	const u64*		bits)
	{
	u64		numBits = endPosition - startPosition;
	string	bitsString(numBits,'-');
	u64*	scan = (u64*) bits;

	u64 chunk = *(scan++);
	int bitInChunk = -1;
	for (u64 ix=0 ; ix<numBits ; ix++)
		{
		if (++bitInChunk == 64)
			{ chunk = *(scan++);  bitInChunk = 0; }
		if (((chunk >> bitInChunk) & 1) == 1) bitsString[ix] = '+';
		}

	out << bitsString;
	}
