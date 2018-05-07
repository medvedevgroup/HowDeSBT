// cmd_validate_tree.cc-- validate that a tree's filters all have consistent properties

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>
#include <unordered_map>

#include "utilities.h"
#include "bit_utilities.h"
#include "bloom_tree.h"
#include "file_manager.h"

#include "support.h"
#include "commands.h"
#include "cmd_validate_tree.h"

using std::string;
using std::pair;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u32 std::uint32_t
#define u64 std::uint64_t

void ValidateTreeCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- validate that a tree's filters all have consistent properties" << endl;
	}

void ValidateTreeCommand::usage
   (std::ostream& s,
	const string& message)
	{
	if (!message.empty())
		{
		s << message << endl;
		s << endl;
		}

	short_description(s);
	s << "usage: " << commandName << " <filename>" << endl;
	//    123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789
	s << "  <filename>  name of a topology file" << endl;
	s << "  --union     verify the node union property" << endl;
	s << "              (by default we only validate simple properties like the size of" << endl;
	s << "              bloom filters)" << endl;
	}

void ValidateTreeCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  topology" << endl;
	s << "  traversal" << endl;
	}

void ValidateTreeCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

	inTreeFilename = "";
	validateUnion  = false;

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

		// --tree=<filename>, etc.

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

		// --union

		if (arg == "--union")
			{ validateUnion = true;  continue; }

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
		chastise ("a topology filename is required");

	return;
	}


int ValidateTreeCommand::execute()
	{
	BloomTree* root = BloomTree::read_topology(inTreeFilename);
	if (contains(debug,"topology"))
		root->print_topology(cerr,/*level*/0,/*format*/topofmt_nodeNames);

	validate_consistency(root);
	if (validateUnion) validate_union(root);

	delete root;

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	// we assume that any failures prevented us from getting to this point

	cout << "TEST SUCCEEDED" << std::endl;
	return EXIT_SUCCESS;
	}


int ValidateTreeCommand::validate_consistency(BloomTree* root)
	{
	std::unordered_map<std::string,std::vector<std::string>*> filenameToNames;
	std::unordered_map<string,string>                         nameToFile;

	vector<BloomTree*> order;
	root->post_order(order);
	for (const auto& node : order)
		{
		if (nameToFile.count(node->name) > 0)
			fatal ("error: tree contains more than one node named \""
			     + node->name 
			     + " (in \"" + node->bfFilename + "\""
			     + " and \"" + nameToFile[node->name] + "\")");
		nameToFile[node->name] = node->bfFilename;

		if (filenameToNames.count(node->bfFilename) == 0)
			{
			// !!! $$$ this looks like a memory leak
			filenameToNames[node->bfFilename] = new vector<string>;
			}
		filenameToNames[node->bfFilename]->emplace_back(node->name);
		}

	BloomFilter* modelBf = nullptr;
	for (auto iter : filenameToNames) 
		{
		string filename = iter.first;

		std::ifstream* in = FileManager::open_file(filename,std::ios::binary|std::ios::in,
		                                           /* positionAtStart*/ true);
		if (not *in)
			fatal ("error: failed to open \"" + filename + "\"");

		vector<pair<string,BloomFilter*>> content
			= BloomFilter::identify_content(*in,filename);

		vector<string>* nodeNames = filenameToNames[filename];
		for (const auto& templatePair : content)
			{
			string       bfName     = templatePair.first;
			BloomFilter* templateBf = templatePair.second;
			if (not contains (*nodeNames, bfName))
				fatal ("error: \"" + filename + "\""
					 + " contains the bloom filter \"" + bfName + "\""
					 + ", in conflict with the tree's topology");

			// make sure all bloom filters in the tree are consistent

			if (modelBf == nullptr)
				{
				if (contains(debug,"traversal"))
					cerr << "using " << templateBf->filename << " as the consistency model" << endl;

				modelBf = templateBf;
				}
			else
				{
				if (contains(debug,"traversal"))
					cerr << "checking consistency of " << templateBf->filename << endl;

				templateBf->is_consistent_with (modelBf, /*beFatal*/ true);
				delete templateBf;
				}
			}

		FileManager::close_file(in);
		}

	delete modelBf;

	// we assume that any failures prevented us from getting to this point

	return EXIT_SUCCESS;
	}


int ValidateTreeCommand::validate_union(BloomTree* root)
	{
	vector<BloomTree*> order;
	root->pre_order(order);
	for (const auto& node : order)
		{
		if (node->is_dummy()) continue;
		if (node->is_leaf()) continue;

		if (contains(debug,"traversal"))
			cerr << "checking union at " << node->bfFilename << endl;

		// preload the node, so we know how many bit vectors it has

		node->load();
		if (node->bf == nullptr)
			fatal ("internal error: failed to load " + node->bfFilename);
		if (node->bf->numBitVectors != 1)
			fatal ("error: " + node->bfFilename + " contains more than one bit vector");

		// compute the union of the node's children

		BitVector* unionBv = nullptr;
		bool isFirstChild = true;
		for (const auto& child : node->children)
			{
			child->load();
			if (child->bf == nullptr)
				fatal ("internal error: failed to load " + child->bfFilename);
			if (child->bf->numBitVectors != 1)
				fatal ("error: " + child->bfFilename + " contains more than one bit vector");

			BitVector* childBv = child->bf->get_bit_vector(0);
			if (childBv == nullptr)
				fatal ("internal error: failed to load bit vector for " + child->bfFilename);
			if (childBv->compressor() != bvcomp_uncompressed)
				fatal ("error: " + child->bfFilename + " contains compressed bit vector(s)");

			if (isFirstChild) // incorporate first child's filters
				{
				unionBv = BitVector::bit_vector(bvcomp_uncompressed,childBv);
				isFirstChild = false;
				}
			else // union with later child's filter
				unionBv->union_with(childBv->bits);

			child->unloadable();
			}

		// compare the node to the union of its children

		node->load();
		if (node->bf == nullptr)
			fatal ("internal error: failed to load " + node->bfFilename);

		BitVector* nodeBv = node->bf->get_bit_vector(0);
		if (nodeBv == nullptr)
			fatal ("internal error: failed to load bit vector for " + node->bfFilename);
		if (nodeBv->compressor() != bvcomp_uncompressed)
			fatal ("error: " + node->bfFilename + " contains compressed bit vector(s)");

		unionBv->xor_with(nodeBv->bits);
		if (not unionBv->is_all_zeros())
			fatal ("error: \"" + node->bfFilename + "\""
				 + " does not match the union of its children");

		node->unloadable();
		delete unionBv;
		}

	// we assume that any failures prevented us from getting to this point

	return EXIT_SUCCESS;
	}

