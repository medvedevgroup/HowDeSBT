// cmd_tree_stats.cc-- report file size and occupancy stats for a sequence
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
#include "cmd_tree_stats.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;


void TreeStatsCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- report file sizes and node occupancy stats for a tree" << endl;
	}

void TreeStatsCommand::usage
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
	}

void TreeStatsCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  trackmemory" << endl;
	s << "  topology" << endl;
	s << "  load" << endl;
	s << "  traversal" << endl;
	}

void TreeStatsCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

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

		// (unadvertised) --tree=<filename>, --topology=<filename>

		if ((is_prefix_of (arg, "--tree="))
		 ||	(is_prefix_of (arg, "--intree="))
		 ||	(is_prefix_of (arg, "--topology=")))
			{ inTreeFilename = argVal;  continue; }

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

		inTreeFilename = arg;
		}

	// sanity checks

	if (inTreeFilename.empty())
		chastise ("you have to provide a tree topology file");

	return;
	}

int TreeStatsCommand::execute()
	{
	if (contains(debug,"trackmemory"))
		{
		BloomTree::trackMemory   = true;
		BloomFilter::trackMemory = true;
		BitVector::trackMemory   = true;
		}

	BloomTree* root = BloomTree::read_topology(inTreeFilename);

	if (contains(debug,"topology"))
		root->print_topology(cerr);

	vector<BloomTree*> order;
	root->post_order(order);
	if (contains(debug,"load"))
		{
		for (const auto& node : order)
			node->reportLoad = true;
		}

	bool hasOnlyChildren = false;
	for (const auto& node : order)
		{
		node->dbgTraversal = (contains(debug,"traversal"));

		if (node->num_children() == 1)
			{
			hasOnlyChildren = true;
			cerr << "warning: " << node->child(0)->bfFilename
				<< " is an only child" << endl;
			}
		}
	if (hasOnlyChildren)
		fatal ("error: tree contains at least one only child");

// $$$ compute stats here

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	delete root;
	return EXIT_SUCCESS;
	}

