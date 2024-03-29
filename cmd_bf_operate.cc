// cmd_bf_operate.cc-- perform some user-specified operation on bloom filters

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

#include "utilities.h"
#include "bit_utilities.h"
#include "bit_vector.h"
#include "file_manager.h"

#include "support.h"
#include "commands.h"
#include "cmd_bf_operate.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u8  std::uint8_t
#define u32 std::uint32_t
#define u64 std::uint64_t

static u64 num_one_bits (BloomFilter* dstBf);


void BFOperateCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- perform some user-specified operation on bloom filters" << endl;
	}

void BFOperateCommand::usage
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
	s << "  <filename>        (cumulative) a bloom filter file, extension .bf; only" << endl;
	s << "                    simple uncompressed bloom filters are supported (except for" << endl;
	s << "                    --unrrr); with these and --list, there should be as many" << endl;
	s << "                    bloom filters as the operation needs." << endl;
	s << "  --list=<filename> file containing a list of bloom filter files; only" << endl;
	s << "                    filters with uncompressed bit vectors are allowed." << endl;
	s << "  --out=<filename>  name for the resulting bloom filter file" << endl;
	s << "  --noout           don't write the resulting bloom filter to a file" << endl;
	s << "  --and             output = a AND b [AND c ..]" << endl;
	s << "  --or              output = a OR b [OR c ..]" << endl;
	s << "  --xor             output = a XOR b [XOR c ..]" << endl;
	s << "  --eq              output = a EQ b" << endl;
	s << "  --not             output = NOT a  (i.e. 1s complement)" << endl;
	s << "  --rrr             output = RRR a" << endl;
	s << "  --unrrr           output = UNRRR a" << endl;
	s << "  --report:counts   report the number of active bits in the bloom filters" << endl;
	s << "                    (inputs and result); only applicable for --and, --or," << endl;
	s << "                    --xor, --eq, or --not" << endl;
	// $$$ consider adding ROAR and UNROAR
	}

void BFOperateCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  (none, yet)" << endl;
	}

void BFOperateCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

	saveToFile   = true;
	reportCounts = false;

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
			{ outputFilename = argVal;  continue; }

		// --noout

		if ((arg == "--noout") || (arg == "--nooutput"))
			{ saveToFile = false;  continue; }

		// --list=<filename>
		// <filename> is supposed to contain a list of input bloom filter files

		if (is_prefix_of (arg, "--list="))
			{
			std::string bfFilelist = strip_blank_ends(argVal);
			std::ifstream in (bfFilelist);
			string bfFilename;

			int lineNum = 0;
			while (std::getline (in, bfFilename))
				{
				lineNum++;
				if (is_suffix_of (strip_blank_ends(bfFilename), ".bf"))
					bfFilenames.emplace_back(strip_blank_ends(bfFilename));
				else
					chastise ("(at line " + std::to_string(lineNum) + " of \"" + argVal + "\")"
					        + " \"" + strip_blank_ends(bfFilename) + "\" isn't a \".bf\" file");
				}

			continue;
			}

		// operations (some unadvertised)

		if ((arg == "--and") || (arg == "--AND") || (arg == "AND"))
			{ operation = "and";  continue; }

		if ((arg == "--or") || (arg == "--OR") || (arg == "OR"))
			{ operation = "or";  continue; }

		if ((arg == "--xor") || (arg == "--XOR") || (arg == "XOR"))
			{ operation = "xor";  continue; }

		if ((arg == "--eq") || (arg == "--EQ") || (arg == "EQ") || (arg == "=="))
			{ operation = "eq";  continue; }

		if ((arg == "--not") || (arg == "--NOT") || (arg == "NOT") || (arg == "--complement"))
			{ operation = "complement";  continue; }

		if ((arg == "--rrr") || (arg == "--RRR") || (arg == "RRR"))
			{ operation = "rrr compress";  continue; }

		if ((arg == "--unrrr") || (arg == "--UNRRR") || (arg == "UNRRR"))
			{ operation = "rrr decompress";  continue; }

		// $$$ consider adding ROAR and UNROAR

		// --report:counts

		if ((arg == "--report:counts") || (arg == "--report=counts")
		  || (arg == "--report:count") || (arg == "--report=count")
		  || (arg == "--counts") || (arg == "--count"))
			{ reportCounts = true;  continue; }

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

		if (is_suffix_of (arg, ".bf"))
			{ bfFilenames.emplace_back(strip_blank_ends(arg));  continue; }

		// unrecognized argument

		chastise ("unrecognized argument: \"" + arg + "\"");
		}

	// sanity checks; note that the operator functions (op_and, etc.) will
	// perform additional validation of the filters

	if ((saveToFile) && (outputFilename.empty()))
		chastise ("an output bloom filter filename is required (--out)");

	if ((not saveToFile) && (not outputFilename.empty()))
		chastise ("an output bloom filter filename was given, inconsistent with --noout");

	if (operation.empty())
		chastise ("an operation is required (e.g. --AND)");

	if (operation == "and")
		{
		if (bfFilenames.size() < 2)
			chastise ("AND requires at least two input bloom filters");
		}
	else if (operation == "or")
		{
		if (bfFilenames.size() < 2)
			chastise ("OR requires at least two input bloom filters");
		}
	else if (operation == "xor")
		{
		if (bfFilenames.size() < 2)
			chastise ("XOR requires at least two input bloom filters");
		}
	else if (operation == "eq")
		{
		if (bfFilenames.size() != 2)
			chastise ("EQ requires exactly two input bloom filters");
		}
	else if (operation == "complement")
		{
		if (bfFilenames.size() != 1)
			chastise ("NOT requires exactly one input bloom filter");
		}
	else if (operation == "rrr compress")
		{
		if (bfFilenames.size() != 1)
			chastise ("RRR requires exactly one input bloom filter");
		if (reportCounts)
			chastise ("--report:count is not implemented for --rrr");
		}
	else if (operation == "rrr decompress")
		{
		if (bfFilenames.size() != 1)
			chastise ("UNRRR requires exactly one input bloom filter, rrr-compressed");
		if (reportCounts)
			chastise ("--report:count is not implemented for --unrrr");
		}

	return;
	}


int BFOperateCommand::execute()
	{
	if      (operation == "and")          	op_and();
	else if (operation == "or")           	op_or();
	else if (operation == "xor")          	op_xor();
	else if (operation == "eq")           	op_eq();
	else if (operation == "complement")   	op_complement();
	else if (operation == "rrr compress")   op_rrr();
	else if (operation == "rrr decompress") op_unrrr();

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

	return EXIT_SUCCESS;
	}


void BFOperateCommand::op_and()
	{
	BloomFilter* dstBf = nullptr;
	std::vector<u64> bfCounts;

	// process the input bloom filter files in the order they appear in the array
	//
	// $$$ it'd be nicer to validate all the bf filenames and whether they are
	//     uncompressed, before performing any of the bitwise operations, but
	//     for now, this is adequate

	int bfNum = 0;
	for (const string &bfFilename : bfFilenames)
		{
		BloomFilter* bf = BloomFilter::bloom_filter(bfFilename);
		bf->load();
		BitVector* bv = bf->bvs[0];

		if (bf->numBitVectors > 1)
			fatal ("error: \"" + bfFilename + "\" contains more than one bit vector");
		if (bv->compressor() != bvcomp_uncompressed)
			fatal ("error: \"" + bfFilename + "\" doesn't contain an uncompressed bit vector");

		if (bfNum == 0)
			{
			// simply make a copy of the first bloom filter into the dstBf
			dstBf = BloomFilter::bloom_filter(bf,outputFilename);
			dstBf->new_bits(bv,bvcomp_uncompressed,0);
			}
		else
			{
			u64 numBits = dstBf->num_bits();
			if (bf->num_bits() != numBits)
				fatal ("error: \"" + bfFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
					 + ", but  \"" + bfFilename + "\" has " + std::to_string(bf->num_bits()));
			dstBf->intersect_with(bv);
			}

		if (reportCounts)
			bfCounts.emplace_back(num_one_bits(bf));

		delete bf;
		bfNum++;
		}

	assert (dstBf != nullptr);
	if (reportCounts)
		{
		for (int bfIx=0 ; bfIx<bfFilenames.size() ; bfIx++)
			cout << bfFilenames[bfIx] << " has " << bfCounts[bfIx] << " 'active' bits" << endl;
		cout << ((saveToFile)? outputFilename : "result") << " has " << num_one_bits(dstBf) << " 'active' bits" << endl;
		}
	if (saveToFile) dstBf->save();

	delete dstBf;
	}


void BFOperateCommand::op_or()
	{
	BloomFilter* dstBf = nullptr;
	std::vector<u64> bfCounts;

	// process the input bloom filter files in the order they appear in the array
	//
	// $$$ it'd be nicer to validate all the bf filenames and whether they are
	//     uncompressed, before performing any of the bitwise operations, but
	//     for now, this is adequate

	int bfNum = 0;
	for (const string &bfFilename : bfFilenames)
		{
		BloomFilter* bf = BloomFilter::bloom_filter(bfFilename);
		bf->load();
		BitVector* bv = bf->bvs[0];

		if (bf->numBitVectors > 1)
			fatal ("error: \"" + bfFilename + "\" contains more than one bit vector");
		if (bv->compressor() != bvcomp_uncompressed)
			fatal ("error: \"" + bfFilename + "\" doesn't contain an uncompressed bit vector");

		if (bfNum == 0)
			{
			// simply make a copy of the first bloom filter into the dstBf
			dstBf = BloomFilter::bloom_filter(bf,outputFilename);
			dstBf->new_bits(bv,bvcomp_uncompressed,0);
			}
		else
			{
			u64 numBits = dstBf->num_bits();
			if (bf->num_bits() != numBits)
				fatal ("error: \"" + bfFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
					 + ", but  \"" + bfFilename + "\" has " + std::to_string(bf->num_bits()));
			dstBf->union_with(bv);
			}

		if (reportCounts)
			bfCounts.emplace_back(num_one_bits(bf));

		delete bf;
		bfNum++;
		}

	assert (dstBf != nullptr);
	if (reportCounts)
		{
		for (int bfIx=0 ; bfIx<bfFilenames.size() ; bfIx++)
			cout << bfFilenames[bfIx] << " has " << bfCounts[bfIx] << " 'active' bits" << endl;
		cout << ((saveToFile)? outputFilename : "result") << " has " << num_one_bits(dstBf) << " 'active' bits" << endl;
		}
	if (saveToFile) dstBf->save();

	delete dstBf;
	}


void BFOperateCommand::op_xor()
	{
	BloomFilter* dstBf = nullptr;
	std::vector<u64> bfCounts;

	// process the input bloom filter files in the order they appear in the array
	//
	// $$$ it'd be nicer to validate all the bf filenames and whether they are
	//     uncompressed, before performing any of the bitwise operations, but
	//     for now, this is adequate

	int bfNum = 0;
	for (const string &bfFilename : bfFilenames)
		{
		BloomFilter* bf = BloomFilter::bloom_filter(bfFilename);
		bf->load();
		BitVector* bv = bf->bvs[0];

		if (bf->numBitVectors > 1)
			fatal ("error: \"" + bfFilename + "\" contains more than one bit vector");
		if (bv->compressor() != bvcomp_uncompressed)
			fatal ("error: \"" + bfFilename + "\" doesn't contain an uncompressed bit vector");

		if (bfNum == 0)
			{
			// simply make a copy of the first bloom filter into the dstBf
			dstBf = BloomFilter::bloom_filter(bf,outputFilename);
			dstBf->new_bits(bv,bvcomp_uncompressed,0);
			}
		else
			{
			u64 numBits = dstBf->num_bits();
			if (bf->num_bits() != numBits)
				fatal ("error: \"" + bfFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
					 + ", but  \"" + bfFilename + "\" has " + std::to_string(bf->num_bits()));
			dstBf->xor_with(bv);
			}

		if (reportCounts)
			bfCounts.emplace_back(num_one_bits(bf));

		delete bf;
		bfNum++;
		}

	assert (dstBf != nullptr);
	if (reportCounts)
		{
		for (int bfIx=0 ; bfIx<bfFilenames.size() ; bfIx++)
			cout << bfFilenames[bfIx] << " has " << bfCounts[bfIx] << " 'active' bits" << endl;
		cout << ((saveToFile)? outputFilename : "result") << " has " << num_one_bits(dstBf) << " 'active' bits" << endl;
		}
	if (saveToFile) dstBf->save();

	delete dstBf;
	}


void BFOperateCommand::op_eq()
	{
	BloomFilter* bfA = BloomFilter::bloom_filter(bfFilenames[0]);
	BloomFilter* bfB = BloomFilter::bloom_filter(bfFilenames[1]);
	bfA->load();
	bfB->load();
	BitVector* bvA = bfA->bvs[0];
	BitVector* bvB = bfB->bvs[0];

	if (bfA->numBitVectors > 1)
		fatal ("error: \"" + bfFilenames[0] + "\" contains more than one bit vector");
	if (bfB->numBitVectors > 1)
		fatal ("error: \"" + bfFilenames[1] + "\" contains more than one bit vector");
	if (bvA->compressor() != bvcomp_uncompressed)
		fatal ("error: \"" + bfFilenames[0] + "\" doesn't contain an uncompressed bit vector");
	if (bvB->compressor() != bvcomp_uncompressed)
		fatal ("error: \"" + bfFilenames[1] + "\" doesn't contain an uncompressed bit vector");

	u64 numBits = bfA->num_bits();
	if (bfB->num_bits() != numBits)
		fatal ("error: \"" + bfFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
			 + ", but  \"" + bfFilenames[1] + "\" has " + std::to_string(bfB->num_bits()));

	BloomFilter* dstBf = BloomFilter::bloom_filter(bfA,outputFilename);
	dstBf->new_bits(bvA,bvcomp_uncompressed,0);

	dstBf->xor_with(bvB);
	dstBf->complement();
	if (reportCounts)
		{
		cout << bfFilenames[0] << " has " << num_one_bits(bfA) << " 'active' bits" << endl;
		cout << bfFilenames[1] << " has " << num_one_bits(bfB) << " 'active' bits" << endl;
		cout << ((saveToFile)? outputFilename : "result") << " has " << num_one_bits(dstBf) << " 'active' bits" << endl;
		}
	if (saveToFile) dstBf->save();

	delete bfA;
	delete bfB;
	delete dstBf;
	}


void BFOperateCommand::op_complement()
	{
	BloomFilter* bf = BloomFilter::bloom_filter(bfFilenames[0]);
	bf->load();
	BitVector* bv = bf->bvs[0];

	if (bf->numBitVectors > 1)
		fatal ("error: \"" + bfFilenames[0] + "\" contains more than one bit vector");
	if (bv->compressor() != bvcomp_uncompressed)
		fatal ("error: \"" + bfFilenames[0] + "\" doesn't contain an uncompressed bit vector");

	BloomFilter* dstBf = BloomFilter::bloom_filter(bf,outputFilename);
	dstBf->new_bits(bv,bvcomp_uncompressed,0);

	dstBf->complement();
	if (reportCounts)
		{
		cout << bfFilenames[0] << " has " << num_one_bits(bf) << " 'active' bits" << endl;
		cout << ((saveToFile)? outputFilename : "result") << " has " << num_one_bits(dstBf) << " 'active' bits" << endl;
		}
	if (saveToFile) dstBf->save();

	delete bf;
	delete dstBf;
	}


void BFOperateCommand::op_rrr()
	{
	BloomFilter* bf = BloomFilter::bloom_filter(bfFilenames[0]);
	bf->load();
	BitVector* bv = bf->bvs[0];

	if (bf->numBitVectors > 1)
		fatal ("error: \"" + bfFilenames[0] + "\" contains more than one bit vector");
	if (bv->compressor() != bvcomp_uncompressed)
		fatal ("error: \"" + bfFilenames[0] + "\" doesn't contain an uncompressed bit vector");

	BloomFilter* dstBf = BloomFilter::bloom_filter(bf,outputFilename);
	dstBf->bvs[0] = new RrrBitVector (bv);

	if (saveToFile) dstBf->save();

	delete bf;
	delete dstBf;
	}


void BFOperateCommand::op_unrrr()
	{
	BloomFilter* bf = BloomFilter::bloom_filter(bfFilenames[0]);
	bf->load();
	BitVector* bv = bf->bvs[0];

	if (bf->numBitVectors > 1)
		fatal ("error: \"" + bfFilenames[0] + "\" contains more than one bit vector");
	if (bv->compressor() != bvcomp_rrr)
		fatal ("error: \"" + bfFilenames[0] + "\" doesn't contain an rrr bit vector");
	RrrBitVector* rrrBv = (RrrBitVector*) bv;

	u64 numBits = rrrBv->num_bits();

	BloomFilter* dstBf = BloomFilter::bloom_filter(bf,outputFilename);
	dstBf->bvs[0] = BitVector::bit_vector(bvcomp_uncompressed,numBits);

	decompress_rrr(rrrBv->rrrBits, dstBf->bvs[0]->bits->data(), numBits);
	if (saveToFile) dstBf->save();

	delete bf;
	delete dstBf;
	}


static u64 num_one_bits (BloomFilter* dstBf)
	{
	// nota bene: we support filters with more than one bit vector here, even
	//            though, as of this writing, other parts of this module do not
	u64 numOnes = 0;
	for (int bvIx=0 ; bvIx<dstBf->numBitVectors ; bvIx++)
		{
		BitVector* dstBv = dstBf->bvs[bvIx];
		numOnes += dstBv->rank1(dstBf->num_bits());
		}

	return numOnes;
	}
	
