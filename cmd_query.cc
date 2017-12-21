// cmd_query.cc-- query a sequence bloom tree

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

#include "utilities.h"
#include "bit_vector.h"
#include "bloom_tree.h"
#include "file_manager.h"
#include "query.h"

#include "support.h"
#include "commands.h"
#include "cmd_query.h"

using std::string;
using std::vector;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
#define u64 std::uint64_t


void QueryCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- query a sequence bloom tree" << endl;
	}

void QueryCommand::usage
   (std::ostream& s,
	const string& message)
	{
	if (!message.empty())
		{
		s << message << endl;
		s << endl;
		}

//$$$ add an option to limit the number of bits used in each BF
//$$$ .. that's to let us experiment with different reductions of BF fraction
//$$$ .. without having to generate every populated filter size; implementation
//$$$ .. would just act as a filter on the hashed position list for each query
	short_description(s);
	s << "usage: " << commandName << " [<queryfilename>] [options]" << endl;
	//    123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789
	s << "  --tree=<filename>  name of the tree toplogy file" << endl;
	s << "  <queryfilename>    (cumulative) name of a query file; this is either a fasta" << endl;
	s << "                     file or a file with one nucleotide sequence per line; if" << endl;
	s << "                     no query files are provided, queries are read from stdin" << endl;
	s << "  --threshold=<F>    fraction of query kmers that must be present in a leaf to" << endl;
	s << "                     be considered a match; this must be between 0 and 1" << endl;
	s << "                     (default is " << defaultQueryThreshold << ")" << endl;
	s << "  --leafonly         disregard internal tree nodes and perform the query only" << endl;
	s << "                     at the leaves" << endl;
	s << "  --distinctkmers    perform the query counting each distinct kmer only once" << endl;
	s << "                     (by default we count a query kmer each time it occurs)" << endl;
	s << "  --nomanager        don't use a file manager; generally this means each file" << endl;
	s << "                     can contain only one bloom filter" << endl;
	s << "  --noconsistency    (only with --nomanager) don't check that bloom filter" << endl;
	s << "                     properties are consistent across the tree" << endl;
	s << "  --justcountkmers   just report the number of kmers in each query, and quit" << endl;
	s << "  --countallkmerhits report the number of kmers that 'hit', for each query/leaf" << endl;
	s << "  --out=<filename>   file for query results; if this is not provided, results" << endl;
	s << "                     are written to stdout" << endl;
	}

void QueryCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  bvcreation" << endl;
	s << "  bvdestructor" << endl;
	s << "  bfconstructor" << endl;
	s << "  bfdestructor" << endl;
	s << "  topology" << endl;
	s << "  load" << endl;
	s << "  names" << endl;
	s << "  input" << endl;
	s << "  sort" << endl;
	s << "  kmerize" << endl;
	s << "  kmerizeall" << endl;
	s << "  traversal" << endl;
	s << "  lookups" << endl;
	s << "  positions" << endl;
	s << "  positionsbyhash" << endl;
	s << "  adjustposlist" << endl;
	s << "  rankselectlookup" << endl;
	}

void QueryCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

	queryThreshold       = defaultQueryThreshold;
	onlyLeaves           = false;
	distinctKmers        = false;
	useFileManager       = true;
	checkConsistency     = true;
	justReportKmerCounts = false;
	countAllKmerHits     = false;

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
			{ treeFilename = argVal;  continue; }

		// (unadvertised) --query=<filename>

		if (is_prefix_of (arg, "--query="))
			{ queryFilenames.emplace_back(strip_blank_ends(argVal));  continue; }

		// --threshold=<F>

		if ((is_prefix_of (arg, "--threshold="))
		 ||	(is_prefix_of (arg, "--query-threshold="))
		 ||	(is_prefix_of (arg, "--theta=")))
			{ queryThreshold = string_to_probability(argVal);  continue; }

		// --leafonly, etc.

		if ((arg == "--leafonly")
		 || (arg == "--leaf-only")
		 || (arg == "--leavesonly")
		 || (arg == "--leaves-only")
		 || (arg == "--onlyleaves")
		 || (arg == "--only-leaves"))
			{ onlyLeaves = true;  continue; }

		// --distinctkmers

		if ((arg == "--distinctkmers")
		 || (arg == "--distinct-kmers")
		 || (arg == "--distinct"))
			{ distinctKmers = true;  continue; }

		// --nomanager

		if ((arg == "--nomanager")
		 || (arg == "--nofilemanager"))
			{ useFileManager = false;  continue; }

		// --noconsistency

		if ((arg == "--noconsistency")
		 || (arg == "--noconsistencycheck"))
			{ checkConsistency = false;  continue; }

		// --justcountkmers

		if (arg == "--justcountkmers")
			{
			justReportKmerCounts = true;
			countAllKmerHits     = false;
			continue;
			}

		// --justcountkmers

		if (arg == "--countallkmerhits")
			{
			justReportKmerCounts = false;
			countAllKmerHits     = true;
			continue;
			}

		// --out=<filename>, etc.

		if ((is_prefix_of (arg, "--out="))
		 ||	(is_prefix_of (arg, "--output="))
		 ||	(is_prefix_of (arg, "--matches="))
		 ||	(is_prefix_of (arg, "--results=")))
			{ matchesFilename = argVal;  continue; }

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

		// <queryfilename>

		queryFilenames.emplace_back(strip_blank_ends(arg));
		}

	// sanity checks

	if (treeFilename.empty())
		chastise ("you have to provide a tree topology file");

	if (countAllKmerHits)
		onlyLeaves = true;

	return;
	}

QueryCommand::~QueryCommand()
	{
	for (const auto& q : queries)
		delete q;
	}

int QueryCommand::execute()
	{
	if (contains(debug,"bvcreation"))
		BitVector::reportCreation = true;
	if (contains(debug,"bvdestructor"))
		BitVector::reportDestructor = true;
	if (contains(debug,"bfconstructor"))
		BloomFilter::reportConstructor = true;
	if (contains(debug,"bfdestructor"))
		BloomFilter::reportDestructor = true;

	// read the tree

	BloomTree* root = BloomTree::read_topology(treeFilename,onlyLeaves);
	vector<BloomTree*> order;

	if (contains(debug,"topology"))
		root->print_topology(cerr);

	if (contains(debug,"load"))
		{
		if (order.size() == 0)
			root->post_order(order);
		for (const auto& node : order)
			node->reportLoad = true;
		}

	// leaf-only search doesn't work with certain types of filters
	//
	// nota bene: we like to reject the command if onlyLeaves is true and the
	// filters are not of type bfkind_simple; but we don't know the filter
	// types until we preload them, and we can't preload them until we find a
	// non-empty file (note that with a file manager we could hypothetically
	// have every filter in one big file); so we delay rejection until we get
	// into the actual query method (e.g. batch_query or batch_count_kmer_hits)

	// set up the file manager

	FileManager* manager = nullptr;
	if (useFileManager)
		{
		manager = new FileManager(root);
		if (contains(debug,"load"))
			manager->reportLoad = true;
		if (contains(debug,"names"))
			{
			for (auto iter : manager->filenameToNames) 
				{
				string          filename = iter.first;
				vector<string>* names    = iter.second;
				cout << filename << " contains:" << endl;
				for (const auto& name : *names)
					cout << "  " << name << endl;
				}
			}
		}

	// if we're not using a file manager, we still want to do a consistency
	// check before we start the search (we'd rather not run for a long time
	// and *then* report the problem)

	else if (checkConsistency)
		{
		BloomFilter* modelBf = nullptr;

		if (order.size() == 0)
			root->post_order(order);
		for (const auto& node : order)
			{
			node->preload();

			if (modelBf == nullptr)
				modelBf = node->bf;
			else
				node->bf->is_consistent_with (modelBf, /*beFatal*/ true);
			}
		}

	// read the queries

	read_queries ();

	if (contains(debug,"input"))
		{
		for (auto& q : queries)
			{
			cerr << ">" << q->name << endl;
			cerr << q->seq << endl;
			}
		}

	// propagate debug information into the queries and/or tree nodes

	if (contains(debug,"kmerize"))
		{
		for (auto& q : queries)
			q->dbgKmerize = true;
		}
	if (contains(debug,"kmerizeall"))
		{
		for (auto& q : queries)
			q->dbgKmerizeAll = true;
		}

	if ((contains(debug,"traversal"))
	 || (contains(debug,"lookups")))
		{
		if (order.size() == 0)
			root->post_order(order);
		for (const auto& node : order)
			{
			node->dbgTraversal = (contains(debug,"traversal"));
			node->dbgLookups   = (contains(debug,"lookups"));
			}
		}

	if (contains(debug,"sort"))
		{
		if (order.size() == 0)
			root->post_order(order);
		for (const auto& node : order)
			node->dbgSortKmerPositions = true;
		}

	if (contains(debug,"positions"))
		{
		if (order.size() == 0)
			root->post_order(order);
		for (const auto& node : order)
			node->dbgKmerPositions = true;
		}

	if (contains(debug,"positionsbyhash"))
		{
		if (order.size() == 0)
			root->post_order(order);
		for (const auto& node : order)
			node->dbgKmerPositionsByHash = true;
		}

	if (contains(debug,"adjustposlist"))
		{
		if (order.size() == 0)
			root->post_order(order);
		for (const auto& node : order)
			node->dbgAdjustPosList = true;
		}

	if (contains(debug,"rankselectlookup"))
		{
		if (order.size() == 0)
			root->post_order(order);
		for (const auto& node : order)
			node->dbgRankSelectLookup = true;
		}

	// perform the query (or just report kmer counts)

	if (justReportKmerCounts)
		{
		BloomFilter* bf = root->real_filter();
		for (auto& q : queries)
			{
			q->kmerize(bf,distinctKmers);
			cout << q->name << " " << q->kmerPositions.size() << endl;
			}
		}
	else if (countAllKmerHits)
		{
		// perform the query (sort of)

		root->batch_count_kmer_hits(queries,queryThreshold,onlyLeaves,distinctKmers);

		// report results

		if (matchesFilename.empty())
			print_kmer_hit_counts (cout);
		else
			{
			std::ofstream out(matchesFilename);
			print_kmer_hit_counts (out);
			}
		}
	else
		{
		// perform the query

		root->batch_query(queries,queryThreshold,onlyLeaves,distinctKmers);

		// report results

		if (matchesFilename.empty())
			print_matches (cout);
		else
			{
			std::ofstream out(matchesFilename);
			print_matches (out);
			}
		}

	if (manager != nullptr)
		delete manager;

	return EXIT_SUCCESS;
	}

//----------
//
// read_queries--
//	Read the query file(s), populating the queries list.
//	
//----------

void QueryCommand::read_queries()
	{
	// if no query files are provided, read from stdin

	if (queryFilenames.empty())
		Query::read_query_file (cin, /*filename*/ "", queries);

	// otherwise, read each query file

	else
		{
		for (auto& filename : queryFilenames)
			{
			std::ifstream in (filename);
			if (not in)
				fatal ("error: failed to open \"" + filename + "\"");
			Query::read_query_file (in, filename, queries);
			in.close();
			}
		}

	}

//----------
//
// print_matches--
//	
//----------

void QueryCommand::print_matches
   (std::ostream& out) const
	{
	for (auto& q : queries)
		{
		out << "*" << q->name << " " << q->matches.size() << endl;
        out << "# " << q->nodesExamined << " nodes examined" << endl;
		for (auto& name : q->matches)
        	out << name << endl;
        }
	}

//----------
//
// print_kmer_hit_counts--
//	
//----------

void QueryCommand::print_kmer_hit_counts
   (std::ostream& out) const
	{
	for (auto& q : queries)
		{
		int matchCount = 0;
		for (size_t ix=0 ; ix<q->matches.size() ; ix++)
			{
			u64 numPassed = q->matchesNumPassed[ix];
			bool queryPasses = (numPassed >= q->neededToPass);
			if (queryPasses) matchCount++;
        	}

		out << "*" << q->name << " " << matchCount << endl;

        int ix = 0;
		for (auto& name : q->matches)
			{
			u64 numPassed = q->matchesNumPassed[ix];
			bool queryPasses = (numPassed >= q->neededToPass);

			out << q->name << " vs " << name
	            << " " << numPassed << "/" << q->numPositions
	            << " " << (numPassed/float(q->numPositions));
			if (queryPasses) out << " hit";
		    out << endl;
	        ix++;
        	}
        }
	}
