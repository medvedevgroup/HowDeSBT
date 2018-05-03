// cmd_make_bv.cc-- convert a sequence file to a bit vector

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

#include "utilities.h"
#include "hash.h"
#include "jelly_kmers.h"
#include "bit_vector.h"
#include "bloom_filter.h"

#include "support.h"
#include "commands.h"
#include "cmd_make_bv.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u32 std::uint32_t
#define u64 std::uint64_t


void MakeBVCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- convert a sequence file to a bit vector" << endl;
	}

void MakeBVCommand::usage
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
	s << "  <filename>         (cumulative) a sequence file, e.g. fasta or fastq" << endl;
	s << "                     (one bloom filter is created, for the union of the" << endl;
	s << "                     sequence files)" << endl;
	s << "  --kmersin          input files are kmers" << endl;
	s << "                     (by default input files are expected to be fasta or fastq)" << endl;
	s << "  --out=<filename>   name for bit vector file; the bit vector's compression" << endl;
	s << "                     type is determined by the file extension (e.g. .bv, .rrr" << endl;
	s << "                     or .roar)" << endl;
	s << "                     (by default this is derived from first sequence filename," << endl;
	s << "                     and an uncompressed bit vector is created)" << endl;
	s << "  --list=<filename>  file containing a list of bit vectors to create; this is" << endl;
	s << "                     used in place of the <filename>s on the command line; the" << endl;
	s << "                     file format is described below" << endl;
	s << "  --asper=<filename> name of an existing bloom filter file to extract settings" << endl;
	s << "                     from; that file's --k, --seed, and --bits will be used if" << endl;
	s << "                     they are not otherwise specified on the command line" << endl;
	s << "  --k=<N>            kmer size (number of nucleotides in a kmer)" << endl;
	s << "                     (default is " << defaultKmerSize << ")" << endl;
	s << "  --min=<N>          kmers occuring fewer than N times are left out of the" << endl;
	s << "                     bloom filter" << endl;
	s << "                     (default is " << defaultMinAbundance << ")" << endl;
	s << "  --threads=<N>      number of threads to use during kmerization" << endl;
	s << "                     (default is " << defaultNumThreads << ")" << endl;
	s << "  --seed=<number>    the hash function's 64-bit seed" << endl;
	s << "  --bits=<N>         number of bits in the bloom filter" << endl;
	s << "                     (default is " << defaultNumBits << ")" << endl;
	s << endl;
	s << "When --list is used, each line of the file corresponds to a bit vector. The" << endl;
	s << "format of each line is" << endl;
	s << "  <filename> [<filename>..] [--kmersin] [--out=<filename>]" << endl;
	s << "with meaning the same as on the command line. No other options (e.g. --k or" << endl;
	s << "--bits) are allowed in the file. These are specified on the command line and" << endl;
	s << "will affect all the bit vectors." << endl;
	s << endl;
	s << "When --kmersin is used, each line of the sequence input files is a single kmer," << endl;
	s << "as the first field in the line. Any additional fields on the line are ignored." << endl;
	s << "For example, with --k=20 this might be" << endl;
	s << "  ATGACCAGATATGTACTTGC" << endl;
	s << "  TCTGCGAACCCAGACTTGGT" << endl;
	s << "  CAAGACCTATGAGTAGAACG" << endl;
	s << "   ..." << endl;
	s << "Every kmer in the file(s) is added to the bit vector. No counting is performed," << endl;
	s << "and --min is not allowed." << endl;
	}

void MakeBVCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  settings" << endl;
	s << "  kmers" << endl;
	s << "  strings" << endl;
	s << "  count" << endl;
	}

void MakeBVCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;
	vector<string> tempBvFilenames;

	listFilename = "";
	inputIsKmers = false;
	bvFilename   = "";
	kmerSize     = defaultKmerSize;      bool kmerSizeSet     = false;
	minAbundance = defaultMinAbundance;       minAbundanceSet = false;
	numThreads   = defaultNumThreads;
	hashSeed     = 0;                    bool hashSeedSet     = false;
	numBits      = defaultNumBits;       bool numBitsSet      = false;

#ifdef useJellyHash
	hashSeed = JellyHashSeed;
#endif

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

		// --kmersin

		if ((arg == "--kmersin")
		 ||	(arg == "--askmers="))
			{ inputIsKmers = true;  continue; }

		// --out=<filename>

		if ((is_prefix_of (arg, "--out="))
		 ||	(is_prefix_of (arg, "--output=")))
			{ bvFilename = argVal;  continue; }

		// --list=<filename>

		if (is_prefix_of (arg, "--list="))
			{ listFilename = argVal;  continue; }

		// --k=<N>

		if ((is_prefix_of (arg, "K="))
		 ||	(is_prefix_of (arg, "--K="))
		 ||	(is_prefix_of (arg, "k="))
		 ||	(is_prefix_of (arg, "--k="))
		 ||	(is_prefix_of (arg, "--kmer="))
		 ||	(is_prefix_of (arg, "--kmersize=")))
			{
			kmerSize    = string_to_u32(argVal);
			kmerSizeSet = true;
			continue;
			}

		// --min=<N>

		if ((is_prefix_of (arg, "--min="))
		 ||	(is_prefix_of (arg, "--abundance=")))
			{
			minAbundance = string_to_u32(argVal);
			if (minAbundance < 1) minAbundance = 1;
			minAbundanceSet = (minAbundance > 1);
			continue;
			}

		// --threads=<N>

		if ((is_prefix_of (arg, "--threads="))
		 ||	(is_prefix_of (arg, "T="))
		 ||	(is_prefix_of (arg, "--T=")))
			{
			numThreads = string_to_u32(argVal);
			if (numThreads == 0)
				chastise ("(in \"" + arg + "\") number of threads cannot be zero");
			continue;
			}

		// --seed=<number>

		if ((is_prefix_of (arg, "--seed="))
		 || (is_prefix_of (arg, "S="))
		 || (is_prefix_of (arg, "--S=")))
			{
			hashSeed    = string_to_u64 (argVal);
			hashSeedSet = true;
			continue;
			}

		// --bits=<N>

		if ((is_prefix_of (arg, "--bits="))
		 ||	(is_prefix_of (arg, "B="))
		 ||	(is_prefix_of (arg, "--B=")))
			{
			numBits    = string_to_unitized_u64(argVal);
			numBitsSet = true;
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

		seqFilenames.emplace_back(strip_blank_ends(arg));
		}

	// if an "as per" file was given, modify settings that weren't otherwise
	// specified

	if (not asPerFilename.empty())
		{
		BloomFilter bf(asPerFilename);
		bf.load();

		if (!kmerSizeSet) { kmerSize = bf.kmerSize;     kmerSizeSet = true; }
		if (!hashSeedSet) { hashSeed = bf.hashSeed1;    hashSeedSet = true; }
		if (!numBitsSet)  { numBits  = bf.hashModulus;  numBitsSet  = true; }  // (yes, hashModulus)
		}

	// sanity checks

	if (listFilename.empty())
		{
		if (seqFilenames.empty())
			chastise ("at least one sequence filename is required");
		}
	else
		{
		if (not seqFilenames.empty())
			chastise ("cannot use --list with sequence filenames (e.g. " + seqFilenames[0] + ") in the command");

		if (not bvFilename.empty())
			chastise ("cannot use --list with a vector filename (" + bvFilename + ")  in the command");
		}

	if (kmerSize == 0)
		chastise ("kmer size cannot be zero");

	if (numBits < 2)
		chastise ("number of bits must be at least 2");

	if ((inputIsKmers) and (minAbundanceSet))
		chastise ("cannot use --kmersin with  --min");

	if (contains(debug,"settings"))
		{
		cerr << "kmerSize = " << kmerSize << endl;
		cerr << "hashSeed = " << hashSeed << endl;
		cerr << "numBits  = " << numBits  << endl;
		}

	return;
	}


MakeBVCommand::~MakeBVCommand()
	{
	if (hasher != nullptr) delete hasher;
	}


int MakeBVCommand::execute()
	{
	// $$$ add trackMemory to hash constructor/destructor
	hasher = new HashCanonical(kmerSize,hashSeed);

	// if we're to make a single vector, do so

	if (listFilename.empty())
		{
		if (inputIsKmers) make_bit_vector_kmers ();
		             else make_bit_vector_fasta ();
		}

	// otherwise, make a series of vectors according to each line specified in
	// a file

	else
		{
		std::ifstream in (listFilename);
	    if (not in)
			fatal ("error: failed to open \"" + listFilename + "\"");

		string line;
		int lineNum = 0;
		while (std::getline (in, line))
			{
			lineNum++;
			seqFilenames.clear();
			inputIsKmers = false;
			bvFilename   = "";

			vector<string> tokens = tokenize(line);
			for (size_t argIx=0 ; argIx<tokens.size() ; argIx++)
				{
				string arg = tokens[argIx];
				string argVal;

				string::size_type argValIx = arg.find('=');
				if (argValIx == string::npos) argVal = "";
				                         else argVal = arg.substr(argValIx+1);

				if ((arg == "--kmersin")
				 ||	(arg == "--askmers="))
					{
					if (minAbundanceSet)
						fatal ("cannot use --kmersin, with --min on the command line"
						       " (at line " + std::to_string(lineNum)
						     + " in " + listFilename + ")");
					inputIsKmers = true;
					continue;
					}

				if ((is_prefix_of (arg, "--out="))
				 ||	(is_prefix_of (arg, "--output=")))
					{ bvFilename = argVal;  continue; }

				if (is_prefix_of (arg, "--"))
					fatal ("unrecognized field: \"" + arg + "\""
					     + " at line " + std::to_string(lineNum)
					     + " in " + listFilename);

				seqFilenames.emplace_back(strip_blank_ends(arg));
				}
			
			if (inputIsKmers) make_bit_vector_kmers ();
			             else make_bit_vector_fasta ();
			}

		in.close();
		}

	return EXIT_SUCCESS;
	}

void MakeBVCommand::make_bit_vector_fasta()  // this also supports fastq
	{
	string bvOutFilename = bvFilename;

	if (bvOutFilename.empty())
		{
		string seqFilename = seqFilenames[0];
		string::size_type dotIx = seqFilename.find_last_of(".");
		if (dotIx == string::npos)
			bvOutFilename = seqFilename + ".bv";
		else
			bvOutFilename = seqFilename.substr(0,dotIx) + ".bv";
		}

	// create the hash table, with jellyfish defaults

	const u64 hashSize    = 10*1000*1000;
	const u32 numReprobes = 126;
	const u32 counterLen  = 7;

	unsigned int savedKmerSize = jellyfish::mer_dna::k();
	jellyfish::mer_dna::k(kmerSize);

	mer_hash_type merHash (hashSize, kmerSize*2, counterLen, numThreads, numReprobes);

	// count the kmers
	// nota bene: MerCounter internally discards kmers containing any non-ACGT
	// $$$ ERROR_CHECK need to trap exceptions from the jellyfish stuff
	// $$$ ERROR_CHECK does jellyfish give us any indication if one of the sequence files doesn't exist?

	MerCounter counter(numThreads, merHash, seqFilenames.begin(), seqFilenames.end());
	counter.exec_join (numThreads);

	// build the bit vector

	BitVector* bv = BitVector::bit_vector (bvOutFilename);
	bv->new_bits (numBits);
	u64 onesCount = 0;

	const auto jfAry = merHash.ary();
	const auto end   = jfAry->end();
	for (auto kmer=jfAry->begin() ; kmer!=end ; ++kmer)
		{
		auto& keyValuePair = *kmer;
		if (keyValuePair.second >= minAbundance)
			{
			if (contains(debug,"kmers"))
				cerr << keyValuePair.first << " " << keyValuePair.second << endl;

			u64 pos;
			if (contains(debug,"strings"))
				{
				string key = keyValuePair.first.to_str();
				pos = hasher->hash(key) % numBits;
				}
			else
				{
				u64* merData = (u64*) keyValuePair.first.data();
				pos = hasher->hash(merData) % numBits;
				}

			if ((*bv)[pos] == 0)
				{ bv->write_bit (pos);  onesCount++; }
			}
		}

	if (contains(debug,"count"))
		cerr << "generated " << bv->identity() << " with " << onesCount << " 1s"<< endl;

	jellyfish::mer_dna::k(savedKmerSize);	// restore jellyfish kmer size

	bv->reportSave = true;
	bv->save();
	delete bv;
	}

void MakeBVCommand::make_bit_vector_kmers()
	{
	string bvOutFilename = bvFilename;

	if (bvOutFilename.empty())
		{
		string seqFilename = seqFilenames[0];
		string::size_type dotIx = seqFilename.find_last_of(".");
		if (dotIx == string::npos)
			bvOutFilename = seqFilename + ".bv";
		else
			bvOutFilename = seqFilename.substr(0,dotIx) + ".bv";
		}

	// build the bit vector

	BitVector* bv = BitVector::bit_vector (bvOutFilename);
	bv->new_bits (numBits);
	u64 onesCount = 0;

	for (const auto& kmersFilename : seqFilenames)
		{
		std::ifstream in (kmersFilename);
		if (not in)
			fatal ("error: failed to open \"" + kmersFilename + "\"");

		string line, kmer;
		int lineNum = 0;
		while (std::getline (in, line))
			{
			lineNum++;
			line = strip_blank_ends (line);
			if (line.empty()) continue;

			string::size_type whitespaceIx = line.find_first_of(" \t");
			if (whitespaceIx == string::npos) kmer = line;
			                             else kmer = line.substr(0,whitespaceIx);

			if (kmer.length() != kmerSize)
				fatal ("error: expected " + std::to_string(kmerSize) + "-mer"
				     + " but encountered " + std::to_string(kmer.length()) + "-mer"
				     + " (at line " + std::to_string(lineNum)
				     + " in " + kmersFilename + ")");

			string::size_type badIx = kmer.find_first_not_of("ACGTacgt");
			if (badIx != string::npos) continue;

			if (contains(debug,"kmers"))
				cerr << kmer << endl;

			u64 pos = hasher->hash(kmer) % numBits;
			if ((*bv)[pos] == 0)
				{ bv->write_bit (pos);  onesCount++; }
			}
		}

	if (contains(debug,"count"))
		cerr << "generated " << bv->identity() << " with " << onesCount << " 1s"<< endl;

	bv->reportSave = true;
	bv->save();
	delete bv;
	}
