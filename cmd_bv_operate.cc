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
	s << "  --or              output = a OR b" << endl;
	s << "  --squeeze         output = a SQUEEZE b" << endl;
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

		if ((arg == "--or") || (arg == "--OR") || (arg == "OR"))
			{ operation = "or";  continue; }

		if ((arg == "--squeeze") || (arg == "--SQUEEZE") || (arg == "SQUEEZE"))
			{ operation = "squeeze";  continue; }

		if ((arg == "--squeeze.long") || (arg == "--SQUEEZE.LONG") || (arg == "SQUEEZE.LONG"))
			{ operation = "squeeze long";  continue; }

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
			chastise ("and requires two input bit vectors");
		}
	else if (operation == "or")
		{
		if (bvFilenames.size() != 2)
			chastise ("or requires two input bit vectors");
		}
	else if ((operation == "squeeze") || (operation == "squeeze long"))
		{
		if (bvFilenames.size() != 2)
			chastise ("squeeze requires two input bit vectors");
		}


	return;
	}


int BVOperateCommand::execute()
	{
	if      (operation == "and")          op_and();
	else if (operation == "or")           op_or();
	else if (operation == "squeeze")      op_squeeze(false);
	else if (operation == "squeeze long") op_squeeze(true);

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

	FileManager::close_file();	// make sure the last bloom filter file we
								// .. opened for read gets closed

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
	cout << "result has " << numCopied << " bits" << endl;
	dstBv->save();

	delete srcBv;
	delete specBv;
	delete dstBv;
	}
