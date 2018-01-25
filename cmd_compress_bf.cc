// cmd_compress_bf.cc-- copy bloom filters using a different compression format

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

#include "utilities.h"
#include "bloom_filter.h"

#include "support.h"
#include "commands.h"
#include "cmd_compress_bf.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u32 std::uint32_t
#define u64 std::uint64_t

void CompressBFCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- copy bloom filters using a different compression format" << endl;
	}

void CompressBFCommand::usage
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
	s << "  <filename>           (cumulative) a bloom filter file (usually .bf)" << endl;
	s << "  --list=<filename>    file containing a list of bloom filters to compress;" << endl;
	s << "                       this is used in place of the <filename>s on the command" << endl;
	s << "                       line" << endl;
	s << "  --tree=<filename>    name of topology file for tree containing the filters;" << endl;
	s << "                       this is used in place of the <filename>s or --list" << endl;
	s << "  --outtree=<filename> name of topology file to write tree consisting of the" << endl;
	s << "                       compressed filters" << endl;
	s << "                       (by default, when --tree is given, we derive a name for" << endl;
	s << "                       the resulting topology from the input filename)" << endl;
	s << "  --noouttree          don't write the resulting topology file" << endl;
	s << "  --rrr                copy the filter(s) to rrr-compressed bit vector(s)" << endl;
	s << "                       (this is the default)" << endl;
	s << "  --roar               copy the filter(s) to roar-compressed bit vector(s)" << endl;
	s << "  --uncompressed       copy the filter(s) to uncompressed bit vector(s)" << endl;
	s << "                       (this may be very slow)" << endl;
	}

void CompressBFCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  trackmemory" << endl;
	}

void CompressBFCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

	listFilename = "";
	compressor   = bvcomp_rrr;

	bool inhibitOutTree = false;

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

		// --tree=<filename>, etc.

		if ((is_prefix_of (arg, "--tree="))
		 ||	(is_prefix_of (arg, "--intree="))
		 ||	(is_prefix_of (arg, "--topology=")))
			{ inTreeFilename = argVal;  continue; }

		// --outtree=<filename>, etc.

		if (is_prefix_of (arg, "--outtree="))
			{ outTreeFilename = argVal;  inhibitOutTree = false;  continue; }

		if (arg == "--noouttree")
			{ inhibitOutTree = true;  continue; }

		// compression type

		if (arg == "--uncompressed")
			{ compressor = bvcomp_uncompressed;  continue; }

		if ((arg == "--rrr")
		 || (arg == "--RRR"))
			{ compressor = bvcomp_rrr;  continue; }

		if ((arg == "--roar")
		 || (arg == "--roaring"))
			{ compressor = bvcomp_roar;  continue; }

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

		bfFilenames.emplace_back(strip_blank_ends(arg));
		}

	// sanity checks

	int numSourceTypes = 0;
	if (not bfFilenames.empty())    numSourceTypes++;
	if (not listFilename.empty())   numSourceTypes++;
	if (not inTreeFilename.empty()) numSourceTypes++;

	if (numSourceTypes == 0)
		chastise ("at least one bloom filter filename is required");
	else if (numSourceTypes > 1)
		{
		if (not bfFilenames.empty())
			chastise ("cannot use --list or --tree with bloom filter filename(s)"
			          " (e.g. " + bfFilenames[0] + ") in the command");
		else
			chastise ("cannot use both --list and --tree");
		}

	if ((not outTreeFilename.empty()) and (inTreeFilename.empty()))
		chastise ("cannot use --outtree unless you provide the input tree");

	if ((not inTreeFilename.empty()) and (outTreeFilename.empty()) and (not inhibitOutTree))
		{
		string compressorStr = BitVector::compressor_to_string(compressor);
		outTreeFilename = strip_file_path(inTreeFilename);
		string::size_type dotIx = outTreeFilename.find_last_of(".");
		if (dotIx == string::npos)
			outTreeFilename = outTreeFilename + "." + compressorStr + ".sbt";
		else if (is_suffix_of(outTreeFilename,".sbt"))
			outTreeFilename = outTreeFilename.substr(0,dotIx) + "." + compressorStr + ".sbt";
		else
			outTreeFilename = outTreeFilename + "." + compressorStr + ".sbt";

		cout << "topology will be written to \"" << outTreeFilename << "\"" << endl;
		}

	return;
	}


int CompressBFCommand::execute()
	{
	if (contains(debug,"trackmemory"))
		trackMemory = true;

	if (not bfFilenames.empty())
		{
		for (const auto& bfFilename : bfFilenames)
			process_bloom_filter(bfFilename);
		}
	else if (not listFilename.empty())
		{
		std::ifstream in (listFilename);
	    if (not in)
			fatal ("error: failed to open \"" + listFilename + "\"");

		string line;
		int lineNum = 0;
		while (std::getline(in,line))
			{
			lineNum++;
			string bfFilename = strip_blank_ends(line);
			process_bloom_filter(bfFilename);
			}

		in.close();
		}
	else // if (not inTreeFilename.empty())
		{
	    std::ofstream* treeOut = nullptr;
		if (not outTreeFilename.empty())
			{
			treeOut = new std::ofstream(outTreeFilename);
			if (treeOut == nullptr)
				fatal ("error: failed to open ofstream \"" + outTreeFilename + "\"");
			if (trackMemory)
				cerr << "@+" << treeOut << " creating ofstream \"" << outTreeFilename << "\"" << endl;
			}

		std::string inTreePath;
		string::size_type slashIx = inTreeFilename.find_last_of("/");
		if (slashIx != string::npos)
			inTreePath = inTreeFilename.substr(0,slashIx);

		std::ifstream in (inTreeFilename);
	    if (not in)
			fatal ("error: failed to open \"" + inTreeFilename + "\"");

		string line;
		int lineNum = 0;
		while (std::getline(in,line))
			{
			lineNum++;
			std::size_t level = line.find_first_not_of("*");
			string bfFilename = strip_blank_ends(line.substr (level));
			string::size_type brackIx = bfFilename.find_first_of("[]");
			if (brackIx != string::npos)
				bfFilename = bfFilename.substr(0,brackIx);
			if ((not inTreePath.empty())
			 && (bfFilename.find_first_of("/") == string::npos))
				bfFilename = inTreePath + "/" + bfFilename;
			string dstFilename = process_bloom_filter(bfFilename);
			if (treeOut != nullptr)
				*treeOut << string(level,'*') << dstFilename << endl;
			}

		in.close();

		if (treeOut != nullptr)
			{
			treeOut->close();
			if (trackMemory)
				cerr << "@-" << treeOut << " deleting ofstream \"" << outTreeFilename << "\"" << endl;
			delete treeOut;
			}
		}

	return EXIT_SUCCESS;
	}


string CompressBFCommand::process_bloom_filter(const string& filename)
	{
	// determine the name for the compressed filter file; note that we will
	// always create this in the current directory, even if the source filter
	// is from another directory

	string dstFilename     = BloomFilter::strip_filter_suffix(strip_file_path(filename));
	string compressionDesc = BitVector::compressor_to_string(compressor);
	if (compressionDesc == "uncompressed")
		dstFilename += ".bf";
	else if ((compressionDesc == "zeros")
	      || (compressionDesc == "ones"))
		dstFilename += "." + compressionDesc + ".bf";
	else
		{
		dstFilename += "." + compressionDesc + ".bf";
		compressionDesc += "-compressed";
		}

	if (dstFilename == filename)
		{
		cerr << "warning: not converting \"" << filename << "\""
		     << " (new filename would be the same)" << endl;
		return dstFilename;
		}

	// load the source filter
 	// $$$ if the file contains multiple filters we should compress each
 	//     .. separately and combine them into a single resultant file;  as it
 	//     .. stands, we'll get an internal error in BloomFilter::preload()
 
	BloomFilter* srcBf = new BloomFilter (filename);
	srcBf->load();

	// øøø this needs to loop over bvs[]	
	u32 srcCompressor = srcBf->bvs[0]->compressor();
	if (compressor == srcCompressor)
		{
		cerr << "warning: not converting \"" << filename << "\""
		     << " (it is already " << compressionDesc << ")" << endl;
		return dstFilename;
		}

	// create the destination filter, either copying the bits en masse (if the
	// source is uncompressed) or copying bits one-by-one (if the source is
	// compressed)

	BloomFilter* dstBf = new BloomFilter(srcBf, dstFilename);

	// øøø this need to loop over bvs[]	
	if (srcCompressor == bvcomp_uncompressed)
		dstBf->new_bits (srcBf->bvs[0], compressor);
	else
		{
		// øøø this needs to loop over bvs[]	
		// ……… improve this for RRR by decoding chunk by chunk
		dstBf->new_bits (compressor);
		BitVector* srcBv = srcBf->bvs[0];
		BitVector* dstBv = dstBf->bvs[0];
		for (u64 pos=0 ; pos<srcBf->numBits ; pos++)
			{ if ((*srcBv)[pos] == 1) dstBv->write_bit(pos); }
		}

	// save the destination filter; note that the bit vector will automatically
	// be compressed (if necessary) as part of the save process

	dstBf->reportSave = true;
	dstBf->save();
	delete srcBf;
	delete dstBf;

	return dstFilename;
	}
