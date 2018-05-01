// cmd_compress_bf.cc-- copy bloom filters using a different compression format

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

#include "utilities.h"
#include "bloom_filter.h"
#include "file_manager.h"

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
	s << "  --out=<template>     filename template for resulting bloom filter files;" << endl;
	s << "                       this must contain the substring {in}, which is replaced" << endl;
	s << "                       by the root of the input filename; this option is" << endl;
	s << "                       usually only needed if the output filename would be the;" << endl;
	s << "                       same as the input filename otherwise" << endl;
	s << "                       (by default, we derive a filename from the input file;" << endl;
	s << "                       using simple rules)" << endl;
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
	s << "  bfsimplify" << endl;
	}

void CompressBFCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

	listFilename        = "";
	dstCompressor       = bvcomp_rrr;
	inhibitBvSimplify   = false;

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

		// --out=<template>

		if (is_prefix_of (arg, "--out="))
			{
			dstFilenameTemplate = argVal; 

			std::size_t fieldIx = dstFilenameTemplate.find ("{in}");
			if (fieldIx == string::npos)
				chastise ("--out is required to contain the substring \"{in}\", or a variant of it");

			dstFilenameTemplate = BloomFilter::strip_filter_suffix(strip_file_path(dstFilenameTemplate),false);
			continue;
			}

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
			{ dstCompressor = bvcomp_uncompressed;  continue; }

		if ((arg == "--rrr")
		 || (arg == "--RRR"))
			{ dstCompressor = bvcomp_rrr;  continue; }

		if ((arg == "--roar")
		 || (arg == "--roaring"))
			{ dstCompressor = bvcomp_roar;  continue; }

		// (unadvertised) special compressor types

		if (arg == "--uncrrr")
			{ dstCompressor = bvcomp_unc_rrr;  continue; }

		if (arg == "--uncroar")
			{ dstCompressor = bvcomp_unc_roar;  continue; }

		// (unadvertised) --nobvsimplify

		if (arg == "--nobvsimplify")
			{ inhibitBvSimplify = true;  continue; }

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
		string compressorStr = BitVector::compressor_to_string(dstCompressor);
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
	if (contains(debug,"bfsimplify"))
		BloomFilter::reportSimplify = true;

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

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	return EXIT_SUCCESS;
	}


string CompressBFCommand::process_bloom_filter(const string& filename)
	{
	// determine the name for the compressed filter file; note that we will
	// always create this in the current directory, even if the source filter
	// is from another directory

	string dstFilename;
	string rootName = BloomFilter::strip_filter_suffix(strip_file_path(filename),2);
	if (dstFilenameTemplate.empty())
		dstFilename = rootName;
	else
		{
		string nakedName = BloomFilter::strip_filter_suffix(strip_file_path(filename),3);
		dstFilename = dstFilenameTemplate;
		string field = "{in}";
		std::size_t fieldIx = dstFilename.find(field);
		dstFilename.replace(fieldIx,field.length(),nakedName);
		dstFilename += rootName.substr(nakedName.length());
		}

	string compressionDesc = BitVector::compressor_to_string(dstCompressor);
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
		     << " (the new filename would be the same; use --out)" << endl;
		return dstFilename;
		}

	// load the source filter
 
	BloomFilter* srcBf = BloomFilter::bloom_filter(filename);
	srcBf->load();

	// make sure all vectors in the source filter have the same compression
	// type excluding any super-compressed vectors (all-zeros or all-ones) from
	// this test;  if any of them is rrr or roar in uncompressed form, modify
	// the compression type to reflect that fact

	if (srcBf->numBitVectors == 0)
		fatal ("error: \"" + filename + "\" contains no bit vectors");

	u32 srcCompressor = bvcomp_unknown;
	for (int whichBv=0 ; whichBv<srcBf->numBitVectors ; whichBv++)
		{
		u32 bvCompressor = srcBf->bvs[whichBv]->compressor();
		if ((bvCompressor == bvcomp_zeros) || (bvCompressor == bvcomp_ones))
			continue;

		if (srcCompressor == bvcomp_unknown)
			srcCompressor = bvCompressor;
		else if (bvCompressor != srcCompressor)
			fatal ("error: not converting \"" + filename + "\""
			    + " (its bit vectors are inconsistently compressed)");
		}

	if (srcCompressor == bvcomp_rrr)
		{
		for (int whichBv=0 ; whichBv<srcBf->numBitVectors ; whichBv++)
			{
			u32 bvCompressor = srcBf->bvs[whichBv]->compressor();
			if ((bvCompressor == bvcomp_zeros) || (bvCompressor == bvcomp_ones))
				continue;
			if (srcBf->bvs[whichBv]->bits != nullptr)
				{ srcCompressor = bvcomp_unc_rrr;  break; }
			}
		}
	else if (srcCompressor == bvcomp_roar)
		{
		for (int whichBv=1 ; whichBv<srcBf->numBitVectors ; whichBv++)
			{
			u32 bvCompressor = srcBf->bvs[whichBv]->compressor();
			if ((bvCompressor == bvcomp_zeros) || (bvCompressor == bvcomp_ones))
				continue;
			if (srcBf->bvs[whichBv]->bits != nullptr)
				{ srcCompressor = bvcomp_unc_roar;  break; }
			}
		}

	// create the destination filter, either copying the bits en masse (if the
	// source is uncompressed) or copying bits one-by-one (if the source is
	// compressed)

	BloomFilter* dstBf = BloomFilter::bloom_filter(srcBf,dstFilename);

	if (srcCompressor == bvcomp_uncompressed)
		{
		for (int whichBv=0 ; whichBv<srcBf->numBitVectors ; whichBv++)
			{
			u32 bvCompressor = srcBf->bvs[whichBv]->compressor();
			if ((bvCompressor == bvcomp_zeros) || (bvCompressor == bvcomp_ones))
				dstBf->new_bits(bvCompressor,whichBv);
			else
				dstBf->new_bits(srcBf->bvs[whichBv],dstCompressor,whichBv);
			}
		}
	else
		{
		for (int whichBv=0 ; whichBv<srcBf->numBitVectors ; whichBv++)
			{
			BitVector* srcBv = srcBf->bvs[whichBv];
			u32 bvCompressor = srcBv->compressor();
			if ((bvCompressor == bvcomp_zeros) || (bvCompressor == bvcomp_ones))
				dstBf->new_bits(bvCompressor,whichBv);
			else if (bvCompressor == dstCompressor)
				dstBf->bvs[whichBv] = BitVector::bit_vector(dstCompressor,srcBv);
			else
				{
				// $$$ improve this for RRR by decoding chunk by chunk
				dstBf->new_bits(dstCompressor,whichBv);
				BitVector* dstBv = dstBf->bvs[whichBv];
				for (u64 pos=0 ; pos<srcBf->numBits ; pos++)
					{ if ((*srcBv)[pos] == 1) dstBv->write_bit(pos); }
				}
			}
		}

	// save the destination filter; note that the bit vector will automatically
	// be compressed (if necessary) as part of the save process

	if ((dstCompressor == bvcomp_unc_rrr)    // non-yet-compressed rrr
	 || (dstCompressor == bvcomp_unc_roar))  // non-yet-compressed roar
		{
		for (int whichBv=0 ; whichBv<dstBf->numBitVectors ; whichBv++)
			{
			BitVector* dstBv = dstBf->bvs[whichBv];
			dstBv->unfinished();
			}
		}
	else if (not inhibitBvSimplify)
		{
		for (int whichBv=0 ; whichBv<dstBf->numBitVectors ; whichBv++)
			dstBf->simplify_bit_vector(whichBv);
		}

	dstBf->reportSave = true;
	dstBf->save();
	delete srcBf;
	delete dstBf;

	return dstFilename;
	}
