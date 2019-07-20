// cmd_sabuhash_test.cc-- test sabuhash (nucleotide string hashing)

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "utilities.h"
#include "hash.h"
#include "jelly_kmers.h"

#include "support.h"
#include "commands.h"
#include "cmd_sabuhash_test.h"

using std::string;
using std::vector;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
#define u32 std::uint32_t
#define u64 std::uint64_t

void SabuhashTestCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- test the sabuhash nucleotide-string hashing function" << endl;
	}

void SabuhashTestCommand::usage
   (std::ostream& s,
	const string& message)
	{
	if (!message.empty())
		{
		s << message << endl;
		s << endl;
		}

	short_description(s);
	s << "usage: [cat <dna_strings> |] " << commandName << " <filename> [<filename>..] [options]" << endl;
	//    123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789
	s << "  <dna_strings>    nucleotide strings to hash; only used if no filenames are" << endl;
	s << "                   provided" << endl;
	s << "  <filename>       (cumulative) a sequence file, e.g. fasta or fastq" << endl;
	s << "  --k=<N>          kmer size; applies only when input is from sequence files" << endl;
	s << "                   (default is " << defaultKmerSize << ")" << endl;
	s << "  --strings        hash strings instead of bits; applies only when input is" << endl;
	s << "                   from sequence files" << endl;
	s << "                   (by default kmers are hashed in 2-bit encoded form)" << endl;
	s << "  --negate         negate hash values; replace values by their ones-complement" << endl;
	s << "  --modulus=<M>    set the hash modulus" << endl;
	s << "                   (by default, the hash values have no modulus)" << endl;
	s << "  --seed=<number>  set the hash function's 32-bit seed" << endl;
	s << "                   (the default seed is 0)" << endl;
	}

void SabuhashTestCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  jellybits" << endl;
	s << "  input" << endl;
	}

void SabuhashTestCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;
	vector<string> tempBvFilenames;

	// defaults

	kmerSize       = defaultKmerSize;
	useStringKmers = false;
	negateHash     = false;
	modulus        = 0;
	hashSeed       = 0;
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

		// --k=<N>

		if ((is_prefix_of (arg, "K="))
		 ||	(is_prefix_of (arg, "--K="))
		 ||	(is_prefix_of (arg, "k="))
		 ||	(is_prefix_of (arg, "--k="))
		 ||	(is_prefix_of (arg, "--kmer="))
		 ||	(is_prefix_of (arg, "--kmersize=")))
			{
			kmerSize = string_to_u32(argVal);
			if (kmerSize == 0)
				chastise ("(in \"" + arg + "\") kmer size cannot be zero");
			continue;
			}

		// --strings

		if (arg == "--strings")
		 	{ useStringKmers = true;  continue; }

		// --negate

		if (arg == "--negate")
		 	{ negateHash = true;  continue; }

		// --modulus=<M>

		if ((is_prefix_of (arg, "--modulus="))
		 ||	(is_prefix_of (arg, "M="))
		 ||	(is_prefix_of (arg, "--M=")))
			{ modulus = string_to_unitized_u32(argVal);  continue; }

		// --seed=<number>

		if ((is_prefix_of (arg, "--seed="))
		 || (is_prefix_of (arg, "S="))
		 || (is_prefix_of (arg, "--S=")))
			{ hashSeed = string_to_u64 (argVal);  continue; }

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

	return;
	}


SabuhashTestCommand::~SabuhashTestCommand()
	{
	if (hasher != nullptr) delete hasher;
	}


int SabuhashTestCommand::execute()
	{
	// $$$ add trackMemory to hash constructor/destructor
	hasher = new HashCanonical(kmerSize,hashSeed);

	// if the user gave us sequence files, process kmers from those files

	if (not seqFilenames.empty())
		{
		// create the hash table, with jellyfish defaults

		const u64 hashSize    = 10*1000*1000;
		const u32 numReprobes = 126;
		const u32 counterLen  = 7;

		unsigned int savedKmerSize = jellyfish::mer_dna::k();
		jellyfish::mer_dna::k(kmerSize);

		int numThreads = 1;
		mer_hash_type merHash (hashSize, kmerSize*2, counterLen, numThreads, numReprobes);

		// process the kmers
		// $$$ make a version of sabuhash that works on 2-bit-encoded strings,
		//     .. using keyValuePair.first.data()[0] and making sure that k<=32

		MerCounter counter(numThreads, merHash, seqFilenames.begin(), seqFilenames.end());
		counter.exec_join (numThreads);

		const auto jfAry = merHash.ary();
		const auto end   = jfAry->end();
		for (auto kmer=jfAry->begin() ; kmer!=end ; ++kmer)
			{
			auto& keyValuePair = *kmer;
			string merStr = keyValuePair.first.to_str();

			if (useStringKmers)
				{
				if (contains(debug,"jellybits"))
					{
					u64* merData = (u64*) keyValuePair.first.data();
					int merWords = (2*kmerSize + 63) / 64;
					cerr << merStr;
					for (int jIx=0 ; jIx<merWords ; jIx++)
						cout << " " << std::setfill('0') << std::setw(16) << std::hex << std::uppercase << merData[jIx];
					cout << endl;
					}
				perform_hash_test (merStr);
				}
			else
				{
				u64* merData = (u64*) keyValuePair.first.data();
				perform_hash_test (merStr, merData);
				}
			}

	    jellyfish::mer_dna::k(savedKmerSize);	// restore jellyfish kmer size
		}

	// otherwise, just process whole sequences from stdin

	else
		{
		string seq;
		while (std::getline (cin, seq))
			perform_hash_test (seq);
		}

	return EXIT_SUCCESS;
	}


void SabuhashTestCommand::perform_hash_test
   (const string&	seq,
	const u64*		data)
	{
	u32 h, hr;
	u64 h64, h64r;
	bool isMatch;

	h64 = h64r = 0; // placate compiler

	string revCompSeq = reverse_complement (seq);

	if (contains(debug,"input"))
		cerr << seq << " , " << revCompSeq << endl;

	if (data != nullptr)
		{
		h64  = hasher->hash(data);
		h64r = hasher->hash(revCompSeq);
		}
	else
		{
		h64  = hasher->hash(seq);
		h64r = hasher->hash(revCompSeq);
		}

	if (negateHash)
		{
		h64  = ~h64;
		h64r = ~h64r;
		}

	if (modulus == 0)
		isMatch = (h64 == h64r);
	else
		{
		h = h64 % modulus;  hr = h64r % modulus;
		isMatch = (h == hr);
		}

	std::ios::fmtflags saveCoutFlags(cout.flags());

	if (isMatch)
		{
		if (modulus == 0)
			cout << std::setfill('0') << std::setw(16) << std::hex << std::uppercase << h64 << " " << seq << endl;
		else
			{
			size_t w = std::to_string(modulus-1).length();
			cout << std::setw(w) << h << " " << seq << endl;
			}
		}
	else
		{
		if (modulus == 0)
			cout << "x " << std::setfill('0') << std::setw(16) << std::hex << std::uppercase << h64
				 << " "  << std::setfill('0') << std::setw(16) << std::hex << std::uppercase << h64r
				 << " "  << seq
				 << " "  << revCompSeq << endl;
		else
			{
			size_t w = std::to_string(modulus-1).length();
			cout << "x " << std::setw(w) << h
				 << " "  << std::setw(w) << hr
				 << " "  << seq
				 << " "  << revCompSeq << endl;
			}
		}

	cout.flags(saveCoutFlags);
	}
