// cmd_combine_bf.cc-- combine several bloom filters into a single file

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

#include "utilities.h"
#include "bloom_tree.h"
#include "file_manager.h"

#include "support.h"
#include "commands.h"
#include "cmd_combine_bf.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u32 std::uint32_t
#define u64 std::uint64_t

void CombineBFCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- combine several bloom filters into a single file" << endl;
	}

void CombineBFCommand::usage
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
	s << "  <filename>            (cumulative) a bloom filter file (usually .bf); one" << endl;
	s << "                        file is created, containing these bloom filters" << endl;
	s << "  --out=<filename>      name for the combined bloom filter file" << endl;
	s << "                        (by default this is derived from first filter filename)" << endl;
	s << "  --list=<filename>     file containing a list of sets of bloom filters to" << endl;
	s << "                        combine; this is used in place of the <filename>s on" << endl;
	s << "                        the command line" << endl;
	s << "  --siblings=<filename> name of a topology file; siblings from this file are" << endl;
	s << "                        combined into one file for each parent; this is used in" << endl;
	s << "                        place of the <filename>s or --list" << endl;
	s << "  --outtree=<filename>  name of topology file in which to write a tree" << endl;
	s << "                        incorporating the combined siblings" << endl;
	s << "                        (by default, when --siblings is used, we derive a name" << endl;
	s << "                        for the resulting topology from the input filename)" << endl;
	s << "  --noouttree           don't write the resulting topology file" << endl;
	s << "  --dryrun              report the files we'd combine, but don't do it" << endl;
	s << "  --quiet               don't report what files we're combining" << endl;
	s << endl;
	s << "When --list is used, each line of the file corresponds to a set of bloom" << endl;
	s << "filters. The format of each line is" << endl;
	s << "  <filename> [<filename>..] [--out=<filename>]" << endl;
	s << "with meaning the same as on the command line." << endl;
	}

void CombineBFCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  topology" << endl;
	s << "  trackmemory" << endl;
	}

void CombineBFCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

	listFilename    = "";
	inTreeFilename  = "";
	outTreeFilename = "";
	unityFilename   = "";
	dryRun  = false;
	beQuiet = false;

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

		// --out=<filename>

		if ((is_prefix_of (arg, "--out="))
		 ||	(is_prefix_of (arg, "--output=")))
			{ unityFilename = argVal;  continue; }

		// --list=<filename>

		if (is_prefix_of (arg, "--list="))
			{ listFilename = argVal;  continue; }

		// --tree=<filename>, etc.

		if ((is_prefix_of (arg, "--siblings="))
		 ||	(is_prefix_of (arg, "--intree="))
		 ||	(is_prefix_of (arg, "--topology=")))
			{ inTreeFilename = argVal;  continue; }

		// --outtree=<filename>, etc.

		if (is_prefix_of (arg, "--outtree="))
			{ outTreeFilename = argVal;  inhibitOutTree = false;  continue; }

		if (arg == "--noouttree")
			{ inhibitOutTree = true;  continue; }

		// --dryrun

		if (arg == "--dryrun")
			{ dryRun = true;  continue; }

		// --quiet

		if (arg == "--quiet")
			{ beQuiet = true;  continue; }

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
	else if (bfFilenames.size() == 1)
		chastise ("at least two bloom filter filename(s) are needed, to have anything to combine");

	if (not unityFilename.empty())
		{
		if (not listFilename.empty())
			chastise ("cannot use --list with an output filter filename (" + unityFilename + ") in the command");
		if (not inTreeFilename.empty())
			chastise ("cannot use an input tree with an output filter filename (" + unityFilename + ") in the command");
		}

	if ((not outTreeFilename.empty()) and (inTreeFilename.empty()))
		chastise ("cannot use --outtree unless you provide the input tree");

	if ((not inTreeFilename.empty()) and (outTreeFilename.empty()) and (not inhibitOutTree))
		{
		outTreeFilename = strip_file_path(inTreeFilename);
		string::size_type dotIx = outTreeFilename.find_last_of(".");
		if (dotIx == string::npos)
			outTreeFilename = outTreeFilename + ".siblings.sbt";
		else if (is_suffix_of(outTreeFilename,".sbt"))
			outTreeFilename = outTreeFilename.substr(0,dotIx) + ".siblings.sbt";
		else
			outTreeFilename = outTreeFilename + ".siblings.sbt";

		if (dryRun)
			cout << "topology would be written to \"" << outTreeFilename << "\"" << endl;
		else if (not beQuiet)
			cout << "topology will be written to \"" << outTreeFilename << "\"" << endl;
		}

	return;
	}


int CombineBFCommand::execute()
	{
	if (contains(debug,"trackmemory"))
		{
		trackMemory              = true;
		FileManager::trackMemory = true;
		BloomFilter::trackMemory = true;
		BitVector::trackMemory   = true;
		}

	combinationsCounter = 0;

	// if we're to make a single unity, do so

	if (not bfFilenames.empty())
		combine_bloom_filters();

	// otherwise, if we have a list file, make a series of unities according to
	// each line specified in that file

	else if (not listFilename.empty())
		{
		std::ifstream in (listFilename);
	    if (not in)
			fatal ("error: failed to open \"" + listFilename + "\"");

		string line;
		int lineNum = 0;
		while (std::getline (in, line))
			{
			lineNum++;
			bfFilenames.clear();
			unityFilename = "";

			vector<string> tokens = tokenize(line);
			for (size_t argIx=0 ; argIx<tokens.size() ; argIx++)
				{
				string arg = tokens[argIx];
				string argVal;

				string::size_type argValIx = arg.find('=');
				if (argValIx == string::npos) argVal = "";
				                         else argVal = arg.substr(argValIx+1);

				if ((is_prefix_of (arg, "--out="))
				 ||	(is_prefix_of (arg, "--output=")))
					{ unityFilename = argVal;  continue; }

				if (is_prefix_of (arg, "--"))
					fatal ("unrecognized field: \"" + arg + "\""
					     + " at line " + std::to_string(lineNum)
					     + " in " + listFilename);

				bfFilenames.emplace_back(strip_blank_ends(arg));
				}
			
			combine_bloom_filters();
			}

		in.close();
		}

	// otherwise, if we have a tree file, make a unity for each set of siblings

	else // if (not inTreeFilename.empty())
		{
		bool makeOutTree = (not outTreeFilename.empty());

		std::string inTreePath;
		string::size_type slashIx = inTreeFilename.find_last_of("/");
		if (slashIx != string::npos)
			inTreePath = inTreeFilename.substr(0,slashIx);

		BloomTree* root = BloomTree::read_topology(inTreeFilename);
		if (root->nodesShareFiles)
			fatal("cannot combine siblings in " + inTreeFilename + ";"
			    + " it already contains some combined nodes");

		if (contains(debug,"topology"))
			root->print_topology(cerr,/*level*/0,/*format*/topofmt_nodeNames);

		vector<BloomTree*> order;
		root->post_order(order);  // (it's important we use this order)
		for (const auto& node : order)
			{
			// ignore this node unless it is a last child, with at least one
			// sibling
			//
			// nota bene: nodes we ignore hear remain in the tree in
			//   .. single-node-per-file form

			auto& parent = node->parent;
			if (parent == nullptr)                       continue;  // no parent means no siblings
			size_t numChildren = parent->num_children();
			if (numChildren < 2)                         continue;  // only child
			if (parent->children[numChildren-1] != node) continue;  // not last child

			bfFilenames.clear();
			for (const auto& child : parent->children)
				{
				string bfFilename = child->bfFilename;
				if ((not inTreePath.empty())
				 && (bfFilename.find_first_of("/") == string::npos))
					bfFilename = inTreePath + "/" + bfFilename;
				bfFilenames.emplace_back (bfFilename);
				}

			string unityTemplate;
			if (parent->is_dummy())
				{ // dummy is root of forest -- derive a name from last child
				string::size_type dotIx = node->name.find_first_of(".");
				if (dotIx == string::npos) dotIx = node->name.length();
				unityTemplate = "root" + node->bfFilename.substr(dotIx);
				}
			else
				unityTemplate = strip_file_path(parent->bfFilename);

			string unityPrefix = BloomFilter::strip_filter_suffix(unityTemplate);
			string unitySuffix = unityTemplate.substr(unityPrefix.length());
			unityFilename = unityPrefix + ".children" + unitySuffix;

			string dstFilename = combine_bloom_filters();

			if (makeOutTree)
				{
				for (const auto& child : parent->children)
					child->bfFilename = child->name + "[" + dstFilename + "]";
				}
			}

		if (makeOutTree)
			{
			if (dryRun)
				root->print_topology(cout);
			else
				{
			    std::ofstream out(outTreeFilename);
				root->print_topology(out);
				}
			}

		delete root;
		}

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	return EXIT_SUCCESS;
	}


string CombineBFCommand::combine_bloom_filters ()
	{
	// bfFilenames is a list of files to combine
	// unityFilename is the file to create (may be blank)
	// returns the name of the created file

	// determine the name for the unity file, making sure we won't overwrite
	// one of the components; note if we have to build the unity filename from
	// the source filter names, we will always create this in the current
	// directory, even if the source filters are from another directory

	string dstFilename = unityFilename;

	if (dstFilename.empty())
		{
		dstFilename = strip_file_path(bfFilenames[0]);
		string::size_type dotIx = dstFilename.find_last_of(".");
		if (dotIx == string::npos)
			dstFilename = dstFilename + ".unity.bf";
		else if (is_suffix_of(dstFilename,".bf"))
			dstFilename = dstFilename.substr(0,dotIx) + ".unity.bf";
		else
			dstFilename = dstFilename + ".unity.bf";
		}

	bool badComponent = false;
	for (const auto& componentFilename : bfFilenames)
		{
		if (dstFilename == componentFilename)
			{ badComponent = true;  break; }
		}

	if (badComponent)
		{
		string components = "";
		for (const auto& componentFilename : bfFilenames)
			{
			if (not components.empty()) components = components + ", ";
			components = components + "\"" + componentFilename + "\"";
			}

		fatal("error: not combining \"" + dstFilename + "\""
		    + ", one of the component files has the same name"
		    + "\ncomponents: " + components);
		}

	if ((dryRun) or (not beQuiet))
		{
		bool isFirst = true;
		cout << "(#" << (++combinationsCounter) << ") ";
		if (dryRun) cout << "would combine " << dstFilename << " from ";
		       else cout << "combining " << dstFilename << " from ";
		for (const auto& componentFilename : bfFilenames)
			{
			if (isFirst) isFirst = false;
					else cout << ",";
			cout << componentFilename;
			}
		cout << endl;
		}

	if (dryRun)
		return dstFilename;

	// preload the components; we make sure that they have consistent properties

	vector<BloomFilter*> componentBfs;
	vector<string> componentNames;

	BloomFilter* modelBf = nullptr;
	for (const auto& componentFilename : bfFilenames)
		{
		BloomFilter* bf = BloomFilter::bloom_filter(componentFilename);
		bool ok = bf->preload(/*bypassManager*/false,/*stopOnMultipleContent*/true);

		if (not ok)
			fatal ("error: " + componentFilename + " contains multiple bloom filters");

		if (modelBf == nullptr) modelBf = bf;
		                   else bf->is_consistent_with (modelBf, /*beFatal*/ true);

		componentBfs.emplace_back(bf);

		string name = BloomFilter::default_filter_name(componentFilename);
		componentNames.emplace_back(name);
		}

	//===== create the combined file =====
	// $$$ this should be merged into BloomFilter::save()

	int totalBitVectors = 0;
	for (const auto& bf : componentBfs)
		{
		bf->preload();
		totalBitVectors += bf->numBitVectors;
		}

	// allocate the header, with enough room for a bfvectorinfo record for all
	// of the component's bit vectors, and for the components' names
	//
	// nota bene: correctness of the file is sensitive to the order in which
	//   .. we write the bit vectors and the names we give them; at read time,
	//   .. these are interpreted by BloomFilter::identify_content()

	u64 headerBytesNeeded = bffileheader_size(totalBitVectors);
	u32 namesStart = (u32) headerBytesNeeded;
	u32 bfIx = 0;
	for (const auto& bf : componentBfs)
		{
		bf->preload();
		for (int whichBv=0 ; whichBv<bf->numBitVectors ; whichBv++)
			headerBytesNeeded += componentNames[bfIx].length() + 1;
		bfIx++;
		}
	headerBytesNeeded = round_up_16(headerBytesNeeded);

	u32 headerSize = (u32) headerBytesNeeded;   // (check for
	if (headerSize != headerBytesNeeded)        //  .. size overflow)
		fatal ("error: header record for \"" + dstFilename + "\""
		       " would be too large (" + std::to_string(headerSize) + " bytes)");

	bffileheader* header = (bffileheader*) new char[headerSize];
	if (header == nullptr)
		fatal ("error:"
		       " failed to allocate " + std::to_string(headerSize) + " bytes"
		     + " for header record for \"" + dstFilename + "\"");
	if (trackMemory)
		cerr << "@+" << header << " allocating bf file header for \"" << dstFilename << "\"" << endl;

	// write a fake header to the file; after we write the rest of the file
	// we'll rewind and write the real header; we do this because we don't know
	// the component offsets and sizes until after we've written them

	memset (header, 0, headerSize);
	header->magic      = bffileheaderMagicUn;
	header->headerSize = (u32) sizeof(bffileprefix);

	std::ofstream out (dstFilename, std::ios::binary | std::ios::trunc | std::ios::out);
	out.write ((char*)header, headerSize);
	size_t bytesWritten = headerSize; // (based on assumption of success)
	if (not out)
		fatal ("error: failed to open \"" + dstFilename + "\"");

	// start the real header

	header->magic        = bffileheaderMagic;
	header->headerSize   = headerSize;
	header->version      = bffileheaderVersion;
	header->bfKind       = modelBf->kind();
	header->padding1     = 0;
	header->kmerSize     = modelBf->kmerSize;
	header->numHashes    = modelBf->numHashes;
	header->hashSeed1    = modelBf->hashSeed1;
	header->hashSeed2    = modelBf->hashSeed2;
	header->hashModulus  = modelBf->hashModulus;
	header->numBits      = modelBf->numBits;
	header->numVectors   = totalBitVectors;
	header->setSizeKnown = false;  // note: setSize is stored with the *file*
	header->setSize      = 0;      //       not with the vector; this is a
	                               //       design shortcoming

	// write the component(s)

	size_t nameOffset = (u64) namesStart;

	u32 bvIx = 0;
	bfIx = 0;
	for (const auto& bf : componentBfs)
		{
		bf->load();
		for (int whichBv=0 ; whichBv<bf->numBitVectors ; whichBv++)
			{
			BitVector* bv = bf->bvs[whichBv];

			header->info[bvIx].compressor = bv->compressor();
			if ((header->info[bvIx].compressor == bvcomp_rrr)
			 || (header->info[bvIx].compressor == bvcomp_unc_rrr))
				header->info[bvIx].compressor |= (RRR_BLOCK_SIZE << 8);
			header->info[bvIx].offset = bytesWritten;

			// $$$ we *could* just copy the bytes, instead of loading the bv and
			// $$$ .. then re-serializing it
			size_t numBytes = bv->serialized_out (out);
			bytesWritten += numBytes;

			header->info[bvIx].numBytes   = numBytes;
			header->info[bvIx].filterInfo = bv->filterInfo;

			header->info[bvIx].name = nameOffset;
			string name = componentNames[bfIx];
			strcpy (/*to*/ ((char*)header)+nameOffset , /*from*/ name.c_str());
			nameOffset += name.length() + 1;

			bvIx++;
			}
		bfIx++;
		}

	// rewind and overwrite the header

	out.seekp(std::ios::beg);
	out.write ((char*)header, headerSize);
	out.close();

	// clean up

	for (const auto& bf : componentBfs)
		delete bf;

	if (header != nullptr)
		{
		if (trackMemory)
			cerr << "@-" << header << " discarding bf file header for \"" << dstFilename << "\"" << endl;
		delete[] header;
		}

	return dstFilename;
	}
