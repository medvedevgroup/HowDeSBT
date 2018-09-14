// cmd_bv_operate.cc-- perform some user-specified operation on bit vectors

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
#include "cmd_bv_operate.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u8  std::uint8_t
#define u32 std::uint32_t
#define u64 std::uint64_t


void BVOperateCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- perform some user-specified operation on bit vectors" << endl;
	}

void BVOperateCommand::usage
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
	s << "  <filename>        (cumulative) a bit vector file, either .bv, .rrr or .roar;" << endl;
	s << "                    There should be as many bit vectors as the operation needs," << endl;
	s << "                    usually 2." << endl;
	s << "  <filename>:<type>[:<offset>] bit vector is embedded in another file; <type>" << endl;
	s << "                    is bv, rrr or roar; <offset> is location within the file" << endl;
	s << "  --out=<filename>  name for the resulting bit vector file" << endl;
	s << "  --and             output = a AND b" << endl;
	s << "  --mask            output = a MASK b  (i.e. a AND NOT b)" << endl;
	s << "  --or              output = a OR b" << endl;
	s << "  --ornot           output = a OR NOT b" << endl;
	s << "  --xor             output = a XOR b" << endl;
	s << "  --eq              output = a EQ b" << endl;
	s << "  --not             output = NOT a  (i.e. 1s complement)" << endl;
	s << "  --squeeze         output = a SQUEEZE b" << endl;
	s << "  --unsqueeze       output = a UNSQUEEZE b" << endl;
	s << "  --rrr             output = RRR a" << endl;
	s << "  --unrrr           output = UNRRR a" << endl;
	s << "  --quiet           don't report information about the result" << endl;
	}

void BVOperateCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  (none, yet)" << endl;
	}

void BVOperateCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

	beQuiet = false;

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

		// operations (some unadvertised)

		if ((arg == "--and") || (arg == "--AND") || (arg == "AND"))
			{ operation = "and";  continue; }

		if ((arg == "--mask")   || (arg == "--MASK")   || (arg == "MASK")
		 || (arg == "--andnot") || (arg == "--ANDNOT") || (arg == "ANDNOT"))
			{ operation = "mask";  continue; }

		if ((arg == "--or") || (arg == "--OR") || (arg == "OR"))
			{ operation = "or";  continue; }

		if ((arg == "--ornot") || (arg == "--ORNOT") || (arg == "ORNOT"))
			{ operation = "or not";  continue; }

		if ((arg == "--xor") || (arg == "--XOR") || (arg == "XOR"))
			{ operation = "xor";  continue; }

		if ((arg == "--eq") || (arg == "--EQ") || (arg == "EQ") || (arg == "=="))
			{ operation = "eq";  continue; }

		if ((arg == "--not") || (arg == "--NOT") || (arg == "NOT") || (arg == "--complement"))
			{ operation = "complement";  continue; }

		if ((arg == "--squeeze") || (arg == "--SQUEEZE") || (arg == "SQUEEZE"))
			{ operation = "squeeze";  continue; }

		if ((arg == "--squeeze.long") || (arg == "--SQUEEZE.LONG") || (arg == "SQUEEZE.LONG"))
			{ operation = "squeeze long";  continue; }

		if ((arg == "--unsqueeze") || (arg == "--UNSQUEEZE") || (arg == "UNSQUEEZE"))
			{ operation = "unsqueeze";  continue; }

		if ((arg == "--rrr") || (arg == "--RRR") || (arg == "RRR"))
			{ operation = "rrr compress";  continue; }

		if ((arg == "--unrrr") || (arg == "--UNRRR") || (arg == "UNRRR"))
			{ operation = "rrr decompress";  continue; }

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

		if (BitVector::valid_filename (arg))
			{ bvFilenames.emplace_back(strip_blank_ends(arg));  continue; }

		// <filename>:<type>[:<offset>]

		if (arg.find(':') != string::npos)
			{ bvFilenames.emplace_back(strip_blank_ends(arg));  continue; }

		// unrecognized argument

		chastise ("unrecognized argument: \"" + arg + "\"");
		}

	// sanity checks

	if (outputFilename.empty())
		chastise ("an output bit vector filename is required (--out)");

	if (operation.empty())
		chastise ("an operation is required (e.g. --AND)");

	if (operation == "and")
		{
		if (bvFilenames.size() != 2)
			chastise ("AND requires two input bit vectors");
		}
	if (operation == "mask")
		{
		if (bvFilenames.size() != 2)
			chastise ("MASK requires two input bit vectors");
		}
	else if (operation == "or")
		{
		if (bvFilenames.size() != 2)
			chastise ("OR requires two input bit vectors");
		}
	else if (operation == "xor")
		{
		if (bvFilenames.size() != 2)
			chastise ("XOR requires two input bit vectors");
		}
	else if (operation == "eq")
		{
		if (bvFilenames.size() != 2)
			chastise ("EQ requires two input bit vectors");
		}
	else if (operation == "complement")
		{
		if (bvFilenames.size() != 1)
			chastise ("NOT requires one input bit vector");
		}
	else if ((operation == "squeeze") || (operation == "squeeze long"))
		{
		if (bvFilenames.size() != 2)
			chastise ("SQUEEZE requires two input bit vectors");
		}
	else if (operation == "unsqueeze")
		{
		if (bvFilenames.size() != 2)
			chastise ("UNSQUEEZE requires two input bit vectors");
		}
	else if (operation == "rrr compress")
		{
		if (bvFilenames.size() != 1)
			chastise ("RRR requires one input bit vector");
		}
	else if (operation == "rrr decompress")
		{
		if (bvFilenames.size() != 1)
			chastise ("UNRRR requires one input bit vector");
		}

	return;
	}


int BVOperateCommand::execute()
	{
	if      (operation == "and")            op_and();
	else if (operation == "mask")           op_mask();
	else if (operation == "or")             op_or();
	else if (operation == "or not")         op_or_not();
	else if (operation == "xor")            op_xor();
	else if (operation == "eq")             op_eq();
	else if (operation == "complement")     op_complement();
	else if (operation == "squeeze")        op_squeeze(false);
	else if (operation == "squeeze long")   op_squeeze(true);
	else if (operation == "unsqueeze")      op_unsqueeze();
	else if (operation == "rrr compress")   op_rrr();
	else if (operation == "rrr decompress") op_unrrr();

	return EXIT_SUCCESS;
	}


void BVOperateCommand::op_and()
	{
	BitVector* bvA = BitVector::bit_vector (bvFilenames[0]);
	BitVector* bvB = BitVector::bit_vector (bvFilenames[1]);

	bvA->load();
	bvB->load();

	u64 numBits = bvA->num_bits();
	if (bvB->num_bits() != numBits)
		fatal ("error: \"" + bvFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
			 + ", but  \"" + bvFilenames[1] + "\" has " + std::to_string(bvB->num_bits()));

	BitVector* dstBv = BitVector::bit_vector (outputFilename);
	dstBv->new_bits (numBits);

	bitwise_and (bvA->bits->data(), bvB->bits->data(), dstBv->bits->data(), numBits);
	dstBv->save();

	delete bvA;
	delete bvB;
	delete dstBv;
	}


void BVOperateCommand::op_mask()
	{
	BitVector* bvA = BitVector::bit_vector (bvFilenames[0]);
	BitVector* bvB = BitVector::bit_vector (bvFilenames[1]);

	bvA->load();
	bvB->load();

	u64 numBits = bvA->num_bits();
	if (bvB->num_bits() != numBits)
		fatal ("error: \"" + bvFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
			 + ", but  \"" + bvFilenames[1] + "\" has " + std::to_string(bvB->num_bits()));

	BitVector* dstBv = BitVector::bit_vector (outputFilename);
	dstBv->new_bits (numBits);

	bitwise_mask (bvA->bits->data(), bvB->bits->data(), dstBv->bits->data(), numBits);
	dstBv->save();

	delete bvA;
	delete bvB;
	delete dstBv;
	}


void BVOperateCommand::op_or()
	{
	BitVector* bvA = BitVector::bit_vector (bvFilenames[0]);
	BitVector* bvB = BitVector::bit_vector (bvFilenames[1]);

	bvA->load();
	bvB->load();

	u64 numBits = bvA->num_bits();
	if (bvB->num_bits() != numBits)
		fatal ("error: \"" + bvFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
			 + ", but  \"" + bvFilenames[1] + "\" has " + std::to_string(bvB->num_bits()));

	BitVector* dstBv = BitVector::bit_vector (outputFilename);
	dstBv->new_bits (numBits);

	bitwise_or (bvA->bits->data(), bvB->bits->data(), dstBv->bits->data(), numBits);
	dstBv->save();

	delete bvA;
	delete bvB;
	delete dstBv;
	}


void BVOperateCommand::op_or_not()
	{
	BitVector* bvA = BitVector::bit_vector (bvFilenames[0]);
	BitVector* bvB = BitVector::bit_vector (bvFilenames[1]);

	bvA->load();
	bvB->load();

	u64 numBits = bvA->num_bits();
	if (bvB->num_bits() != numBits)
		fatal ("error: \"" + bvFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
			 + ", but  \"" + bvFilenames[1] + "\" has " + std::to_string(bvB->num_bits()));

	BitVector* dstBv = BitVector::bit_vector (outputFilename);
	dstBv->new_bits (numBits);

	bitwise_or_not (bvA->bits->data(), bvB->bits->data(), dstBv->bits->data(), numBits);
	dstBv->save();

	delete bvA;
	delete bvB;
	delete dstBv;
	}


void BVOperateCommand::op_xor()
	{
	BitVector* bvA = BitVector::bit_vector (bvFilenames[0]);
	BitVector* bvB = BitVector::bit_vector (bvFilenames[1]);

	bvA->load();
	bvB->load();

	u64 numBits = bvA->num_bits();
	if (bvB->num_bits() != numBits)
		fatal ("error: \"" + bvFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
			 + ", but  \"" + bvFilenames[1] + "\" has " + std::to_string(bvB->num_bits()));

	BitVector* dstBv = BitVector::bit_vector (outputFilename);
	dstBv->new_bits (numBits);

	bitwise_xor (bvA->bits->data(), bvB->bits->data(), dstBv->bits->data(), numBits);
	dstBv->save();

	delete bvA;
	delete bvB;
	delete dstBv;
	}


void BVOperateCommand::op_eq()
	{
	BitVector* bvA = BitVector::bit_vector (bvFilenames[0]);
	BitVector* bvB = BitVector::bit_vector (bvFilenames[1]);

	bvA->load();
	bvB->load();

	u64 numBits = bvA->num_bits();
	if (bvB->num_bits() != numBits)
		fatal ("error: \"" + bvFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
			 + ", but  \"" + bvFilenames[1] + "\" has " + std::to_string(bvB->num_bits()));

	BitVector* dstBv = BitVector::bit_vector (outputFilename);
	dstBv->new_bits (numBits);

	bitwise_xor (bvA->bits->data(), bvB->bits->data(), dstBv->bits->data(), numBits);
	bitwise_complement (dstBv->bits->data(), numBits);
	dstBv->save();

	delete bvA;
	delete bvB;
	delete dstBv;
	}


void BVOperateCommand::op_complement()
	{
	BitVector* bv = BitVector::bit_vector (bvFilenames[0]);

	bv->load();

	u64 numBits = bv->num_bits();

	BitVector* dstBv = BitVector::bit_vector (outputFilename);
	dstBv->new_bits (numBits);

	bitwise_complement (bv->bits->data(), dstBv->bits->data(), numBits);
	dstBv->save();

	delete bv;
	delete dstBv;
	}


void BVOperateCommand::op_squeeze
   (bool fullLengthResult)
	{
	BitVector* srcBv  = BitVector::bit_vector (bvFilenames[0]);
	BitVector* specBv = BitVector::bit_vector (bvFilenames[1]);

	srcBv->load();
	specBv->load();

	u64 numBits = srcBv->num_bits();
	if (specBv->num_bits() != numBits)
		fatal ("error: \"" + bvFilenames[0] + "\" has " + std::to_string(numBits) + " bits"
			 + ", but  \"" + bvFilenames[1] + "\" has " + std::to_string(specBv->num_bits()));

	BitVector* dstBv = BitVector::bit_vector (outputFilename);
	u64 dstNumBits;
	if (fullLengthResult) dstNumBits = numBits;
	                 else dstNumBits = bitwise_count(specBv->bits->data(),numBits);
	dstBv->new_bits (dstNumBits);

	u64 numCopied = bitwise_squeeze (srcBv->bits->data(), specBv->bits->data(), numBits,
	                                 dstBv->bits->data(), dstNumBits);
	if (not beQuiet) cout << "result has " << numCopied << " bits" << endl;
	dstBv->save();

	delete srcBv;
	delete specBv;
	delete dstBv;
	}


void BVOperateCommand::op_unsqueeze()
	{
	BitVector* srcBv  = BitVector::bit_vector (bvFilenames[0]);
	BitVector* specBv = BitVector::bit_vector (bvFilenames[1]);

	srcBv->load();
	specBv->load();

	u64 numBits     = srcBv->num_bits();
	u64 specNumBits = specBv->num_bits();
	u64 specOneBits = bitwise_count(specBv->bits->data(),specNumBits);
	if (specOneBits > numBits)
		fatal ("error: \"" + bvFilenames[1] + "\" has " + std::to_string(specOneBits) + " ones"
			 + ", but  \"" + bvFilenames[0] + "\" only has " + std::to_string(numBits) + " bits");

	BitVector* dstBv = BitVector::bit_vector (outputFilename);
	u64 dstNumBits = specNumBits;
	dstBv->new_bits (dstNumBits);

	u64 resultNumBits = bitwise_unsqueeze (srcBv->bits->data(),  numBits,
	                                       specBv->bits->data(), specNumBits,
	                                       dstBv->bits->data(),  dstNumBits);
	if (not beQuiet) cout << "result has " << resultNumBits << " bits" << endl;
	dstBv->save();

	delete srcBv;
	delete specBv;
	delete dstBv;
	}


void BVOperateCommand::op_rrr()
	{
	BitVector* bv = BitVector::bit_vector (bvFilenames[0]);

	bv->load();

	RrrBitVector* rrrBv = new RrrBitVector (bv);
	rrrBv->filename = outputFilename;
	rrrBv->save();

	delete bv;
	delete rrrBv;
	}


void BVOperateCommand::op_unrrr()
	{
	RrrBitVector* rrrBv = new RrrBitVector (bvFilenames[0]);

	rrrBv->load();

	u64 numBits = rrrBv->num_bits();

	BitVector* dstBv = BitVector::bit_vector (outputFilename);
	dstBv->new_bits (numBits);

	decompress_rrr (rrrBv->rrrBits, dstBv->bits->data(), numBits);
	dstBv->save();

	delete rrrBv;
	delete dstBv;
	}
