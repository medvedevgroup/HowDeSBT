// cmd_query_bf.cc-- query a bloom filter

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <vector>

#include "utilities.h"
#include "bloom_filter.h"
#include "query.h"

#include "support.h"
#include "commands.h"
#include "cmd_query.h"
#include "cmd_query_bf.h"

using std::string;
using std::vector;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
#define u64 std::uint64_t


void QueryBFCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- query a bloom filter, listing the kmers that \"hit\"" << endl;
	}

void QueryBFCommand::usage
   (std::ostream& s,
	const string& message)
	{
	if (!message.empty())
		{
		s << message << endl;
		s << endl;
		}

	short_description(s);
	s << "usage: " << commandName << " [<queryfilename>[=<F>]] [options]" << endl;
	//    123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789
	s << "  --filter=<filename>  (cumulative) a bloom filter file (usually .bf)" << endl;
	s << "  <queryfilename>      (cumulative) name of a query file; this is either a" << endl;
	s << "                       fasta file or a file with one nucleotide sequence per" << endl;
	s << "                       line; if no query files are provided, queries are read" << endl;
	s << "                       from stdin" << endl;
	s << "  <queryfilename>=<F>  query file with associated threshold; <F> has the same" << endl;
	s << "                       meaning as in --threshold=<F> but applies only to this" << endl;
	s << "                       query file" << endl;
	s << "  --threshold=<F>      fraction of query kmers that must be present in a filter" << endl;
	s << "                       to be considered a match; this must be between 0 and 1;" << endl;
	s << "                       this only applies to query files for which <F> is not" << endl;
	s << "                       otherwise specified (by <queryfilename>=<F>)" << endl;
	s << "                       (default is " << QueryCommand::defaultQueryThreshold << ")" << endl;
	s << "  --distinctkmers      perform the query counting each distinct kmer only once" << endl;
	s << "                       (by default we count a query kmer each time it occurs)" << endl;
	s << "  --distinctkmers      perform the query counting each distinct kmer only once" << endl;
	s << "                       (by default we count a query kmer each time it occurs)" << endl;
	s << "  --report:all         report both present and absent kmers" << endl;
	s << "                       (by default we only report kmers that are present)" << endl;
	}

void QueryBFCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  input" << endl;
	s << "  kmerize" << endl;
	s << "  kmerizeall" << endl;
	}

void QueryBFCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

	generalQueryThreshold = -1.0;		// (unassigned threshold)
	distinctKmers         = false;
	reportAllKmers          = false;

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

		// --filter=<filename>, etc.

		if ((is_prefix_of (arg, "--filter="))
		 ||	(is_prefix_of (arg, "--bf=")))
			{ bfFilenames.emplace_back(strip_blank_ends(argVal));  continue; }

		// (unadvertised) --query=<filename>
		//             or --query=<filename>=<F> or --query=<filename>:<F>

		if (is_prefix_of (arg, "--query="))
			{
			string::size_type threshIx = argVal.find('=');
			if (threshIx == string::npos) threshIx = argVal.find(':');

			if (threshIx == string::npos)
				{
				queryFilenames.emplace_back(strip_blank_ends(argVal));
				queryThresholds.emplace_back(-1.0);  // (unassigned threshold)
				}
			else
				{
				double thisQueryThreshold = string_to_probability(arg.substr(threshIx+1));
				queryFilenames.emplace_back(strip_blank_ends(argVal));
				queryThresholds.emplace_back(thisQueryThreshold);
				}
			continue;
			}

		// --threshold=<F>

		if ((is_prefix_of (arg, "--threshold="))
		 ||	(is_prefix_of (arg, "--query-threshold="))
		 ||	(is_prefix_of (arg, "--theta="))
		 ||	(is_prefix_of (arg, "--specificity=")))
			{
			if (generalQueryThreshold >= 0.0)
				{
				cerr << "warning: --threshold=<F> used more that once; only final setting will apply" << endl;
				cerr << "(to use different thresholds for different files, use <queryfilename>=<F> form)" << endl;
				}
			generalQueryThreshold = string_to_probability(argVal);
			continue;
			}

		// --distinctkmers

		if ((arg == "--distinctkmers")
		 || (arg == "--distinct-kmers")
		 || (arg == "--distinct"))
			{ distinctKmers = true;  continue; }

		// --report:all

		if (arg == "--report:all")
			{ reportAllKmers = true;  continue; }

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

		// <queryfilename>=<F> or <queryfilename>:<F>

		string::size_type threshIx = argValIx;
		if (threshIx == string::npos) threshIx = arg.find(':');

		if (threshIx != string::npos)
			{
			double thisQueryThreshold = string_to_probability(arg.substr(threshIx+1));
			queryFilenames.emplace_back(strip_blank_ends(arg.substr(0,threshIx)));
			queryThresholds.emplace_back(thisQueryThreshold);
			continue;
			}

		// <queryfilename>

		queryFilenames.emplace_back(strip_blank_ends(arg));
		queryThresholds.emplace_back(-1.0);  // (unassigned threshold)
		}

	// sanity checks

	if (bfFilenames.empty())
		chastise ("at least one bloom filter filename is required");

	// assign threshold to any unassigned queries

	if (generalQueryThreshold < 0.0)
		generalQueryThreshold = QueryCommand::defaultQueryThreshold;

	int numQueryFiles = queryFilenames.size();
	for (int queryIx=0 ; queryIx<numQueryFiles ; queryIx++)
		{
		if (queryThresholds[queryIx] < 0)
			queryThresholds[queryIx] = generalQueryThreshold;
		}

	return;
	}

QueryBFCommand::~QueryBFCommand()
	{
	for (const auto& q : queries)
		delete q;
	}

int QueryBFCommand::execute()
	{
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

	// perform the queries
	//
	// we read the bloom filters one-by-one, and perform all queries on one
	// bloom filter before moving on to the next bloom filter
	//
	// note that we reject bloom filters that don't support leaf-only operation
	//
	// note that we re-kmerize the queries for each bloom filter; though this
	// may be wasteful, it allows for the possibility that the bloom filters
	// don't have consistent build parameters

	for (const auto& bfFilename : bfFilenames)
		{
		BloomFilter* bf = BloomFilter::bloom_filter(bfFilename);
		bf->load();

		if (bf->kind() != bfkind_simple)
			fatal (commandName + " can't use \"" + bfFilename + "\""
			     + "\n(it can't work for "
			     + BloomFilter::filter_kind_to_string(bf->kind(),false) + " filters)");

		for (auto& q : queries)
			{
			q->kmerize(bf,distinctKmers,/*populateKmers*/ true);

			for (u64 posIx=0 ; posIx<q->kmerPositions.size() ; posIx++)
				{
				u64 pos = q->kmerPositions[posIx];

				int resolution = bf->lookup(pos);

				if (reportAllKmers)
					{
					if (resolution == BloomFilter::absent)
						cout << bfFilename << " " << q->name << " " << q->kmers[posIx] << " absent" << endl;
					else
						cout << bfFilename << " " << q->name << " " << q->kmers[posIx] << " present" << endl;
					}
				else
					{
					if (resolution != BloomFilter::absent)
						cout << bfFilename << " " << q->name << " " << q->kmers[posIx] << endl;
					}
				}
			}

		delete bf;
		}

	return EXIT_SUCCESS;
	}

//----------
//
// read_queries--
//	Read the query file(s), populating the queries list.
//	
//----------

void QueryBFCommand::read_queries()
	{
	// if no query files are provided, read from stdin

	if (queryFilenames.empty())
		Query::read_query_file (cin, /*filename*/ "", generalQueryThreshold, queries);

	// otherwise, read each query file

	else
		{
		int numQueryFiles = queryFilenames.size();
		for (int queryIx=0 ; queryIx<numQueryFiles ; queryIx++)
			{
			string filename = queryFilenames[queryIx];
			std::ifstream in (filename);
			if (not in)
				fatal ("error: failed to open \"" + filename + "\"");
			Query::read_query_file (in, filename, queryThresholds[queryIx], queries);
			in.close();
			}
		}

	}

