// bit_vector.cc-- classes representing bit vectors.

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <sdsl/bit_vectors.hpp>
#include <sdsl/sfstream.hpp>

#include "utilities.h"
#include "bit_utilities.h"
#include "bit_vector.h"

using std::string;
using std::cerr;
using std::endl;
#define u32 std::uint32_t
#define u64 std::uint64_t

//----------
//
// initialize class variables
//
//----------

bool BitVector::trackMemory    = false;
bool BitVector::reportCreation = false;

bool BitVector::reportFileBytes    = false;
bool BitVector::countFileBytes     = false;
u64  BitVector::totalFileReads     = 0;
u64  BitVector::totalFileBytesRead = 0;

//----------
//
// BitVector--
//	Bit vector with an uncompressed underlying file.
//
//----------

BitVector::BitVector
   (const string& _filename,
	const size_t _offset,
	const size_t _numBytes)
	  :	isResident(false),
		filename(_filename),
		offset(_offset),
		numBytes(_numBytes),
		bits(nullptr),
		numBits(0),
		ranker1(nullptr),
		selector0(nullptr),
		filterInfo(0)
	{
	if (trackMemory)
		cerr << "@+" << this << " constructor BitVector(" << identity() << "), variant 1" << endl;
	}

BitVector::BitVector
   (const BitVector* srcBv)
	  :	isResident(false),
		filename(""),
		offset(0),
		ranker1(nullptr),
		selector0(nullptr),
		filterInfo(0)
	{
	bits = nullptr;
	if (srcBv == nullptr) return;

	// we copy from any subclass that is in the uncompressed state, or from an
	// all-ones or all-zeros class

	if (srcBv->bits != nullptr)
		copy_from (srcBv->bits);
	else if ((srcBv->numBits != 0)
	      && ((srcBv->compressor() == bvcomp_zeros)
	       || (srcBv->compressor() == bvcomp_ones)))
		{
		new_bits (srcBv->numBits);
		if (srcBv->compressor() == bvcomp_ones)
			bitwise_complement (/*dst*/ bits->data(), numBits);
		}

	if (trackMemory)
		cerr << "@+" << this << " constructor BitVector(" << identity() << "), variant 2" << endl;
	}

BitVector::BitVector
   (const u64 _numBits)
	  :	isResident(false),
		filename(""),
		offset(0),
		ranker1(nullptr),
		selector0(nullptr),
		filterInfo(0)
	{
	bits = nullptr;
	numBits = 0;
	if (_numBits != 0) new_bits (_numBits);

	if (trackMemory)
		cerr << "@+" << this << " constructor BitVector(" << identity() << "), variant 3" << endl;
	}

BitVector::~BitVector()
	{
	if (trackMemory)
		cerr << "@-" << this << " destructor BitVector(" << identity() << ")" << endl;

	if ((trackMemory) && (bits != nullptr))
		cerr << "@-" << bits << " discarding bits for BitVector(" << identity() << " " << this << ")" << endl;

	if (bits != nullptr) delete bits;
	discard_rank_select();
	}

string BitVector::identity() const
	{
	string id = class_identity() + ":\"" + filename + "\"";
	if (offset != 0) id += ":" + std::to_string(offset);
	return id;
	}

void BitVector::load()
	{
	if (isResident) return;

	if (reportLoad)
		cerr << "loading " << identity() << endl;

	auto startTime = get_wall_time();
	std::ifstream in (filename, std::ios::binary | std::ios::in);
	if (not in)
		fatal ("error: BitVector::load(" + identity() + ")"
		     + " failed to open \"" + filename + "\"");

	if (offset != 0)
		{
		in.seekg (offset, in.beg);
		if (not in)
			fatal ("error: BitVector::load(" + identity() + ")"
			     + " failed to seek to " + std::to_string(offset)
			     + " in \"" + filename + "\"");
		}
	fileLoadTime = elapsed_wall_time(startTime);

	serialized_in (in);
	in.close();
	if (reportLoadTime)
		cerr << "[" << class_identity() << " load] " << fileLoadTime << " secs " << filename << endl;
	fileLoadTime = 0;
	}

void BitVector::serialized_in
   (std::ifstream& in)
	{
	if (bits != nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to serialized_in onto non-null bit vector");

	bits = new sdslbitvector();
	if (trackMemory)
		cerr << "@+" << bits << " creating bits for BitVector(" << identity() << " " << this << ")" << endl;

	auto startTime = get_wall_time();
	sdsl::load (*bits, in);  // $$$ ERROR_CHECK we need to check for errors inside sdsl
	fileLoadTime += elapsed_wall_time(startTime);
	if (reportFileBytes)
		cerr << "read " << numBytes << " for BitVector::serialized_in(" << filename << ")" << endl;
	if (countFileBytes)
		{ totalFileReads++;  totalFileBytesRead += numBytes; }
	numBits = bits->size();
	isResident = true;
	}

void BitVector::save()
	{
	if (reportSave)
		cerr << "Saving " << filename << endl;

	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to save null bit vector");

	if (offset != 0)
		fatal ("internal error for " + identity()
		     + "; attempt to save bit vector to non-zero file offset");

	auto startTime = get_wall_time();
	std::ofstream out (filename, std::ios::binary | std::ios::trunc | std::ios::out);
	if (not out)
		fatal ("error: " + class_identity() + "::save(" + identity() + ")"
		     + " failed to open \"" + filename + "\"");
	serialized_out (out);
	out.close();
	auto elapsedTime = elapsed_wall_time(startTime);
	if (reportSaveTime)
		cerr << "[" + class_identity() + " save] " << elapsedTime << " secs " << filename << endl;
	}

size_t BitVector::serialized_out
   (std::ofstream& out,
	const string& _filename,
	const size_t _offset)
	{
	size_t bytesWritten = serialized_out (out);
	filename = _filename;
	offset   = _offset;
	return bytesWritten;
	}

size_t BitVector::serialized_out
   (std::ofstream& out)
	{
	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to serialize null bit vector");

	// $$$ ERROR_CHECK we need to check for errors inside sdsl
	return (size_t) bits->serialize (out, nullptr, "");
	}

void BitVector::discard_bits()
	{
	if ((trackMemory) && (bits != nullptr))
		cerr << "@-" << bits << " discarding bits for BitVector(" << identity() << " " << this << ")" << endl;

	if (bits != nullptr)
		{
		delete bits;  bits = nullptr;
		discard_rank_select();
		}
	isResident = false;
	}

void BitVector::new_bits
   (u64 _numBits)
	{
	if ((trackMemory) && (bits != nullptr))
		cerr << "@-" << bits << " discarding bits for BitVector(" << identity() << " " << this << ")" << endl;

	if (bits != nullptr)
		{
		delete bits;
		discard_rank_select();
		}

	bits = new sdslbitvector (_numBits, 0);
	if (trackMemory)
		cerr << "@+" << bits << " creating bits for BitVector(" << identity() << " " << this << ")" << endl;

	numBits = _numBits;
	isResident = true;
	}

void BitVector::replace_bits
   (sdslbitvector* srcBits)
	{
	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to replace null bit vector");

	if (trackMemory)
		{
		cerr << "replacing bits for BitVector(" << identity() << " " << this << ")"
		     << " old=" << bits << " new=" << srcBits << endl;
		cerr << "@-" << bits << " discarding bits for BitVector(" << identity() << " " << this << ")" << endl;
		}

	delete bits;
	discard_rank_select();

	bits = srcBits;       // note that we assume, intentionally, that numBits
	isResident = true;    // .. should not be changed
	}

void BitVector::copy_from
   (const sdslbitvector* srcBits)
	{
	if ((trackMemory) && (bits != nullptr))
		cerr << "@-" << bits << " discarding bits for BitVector(" << identity() << " " << this << ")" << endl;

	if (bits != nullptr)
		{
		delete bits;
		discard_rank_select();
		}

	bits = new sdslbitvector (*srcBits);
	if (trackMemory)
		cerr << "@+" << bits << " creating bits for BitVector(" << identity() << " " << this << ")" << endl;

	numBits = bits->size();
	isResident = true;
	}

void BitVector::fill
   (int bitVal)
	{
	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to fill null bit vector");

	bitwise_fill (/*dst*/ bits->data(), bitVal, numBits);
	}

void BitVector::complement ()
	{
	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to complement null bit vector");

	bitwise_complement (/*dst*/ bits->data(), numBits);
	}

void BitVector::union_with
   (const sdslbitvector* srcBits)
	{
	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to union into null bit vector");
	if (srcBits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to union from null bit vector");

	u64 commonNumBits = std::min(numBits,srcBits->size());
	bitwise_or (/*dst*/ bits->data(), srcBits->data(), commonNumBits);
	}

void BitVector::union_with_complement
   (const sdslbitvector* srcBits)
	{
	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to union into null bit vector");
	if (srcBits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to union from null bit vector");

	u64 commonNumBits = std::min(numBits,srcBits->size());
	bitwise_or_not (/*dst*/ bits->data(), srcBits->data(), commonNumBits);
	if (numBits > commonNumBits)
		{
		fatal ("internal error for " + identity()
		     + "; union-not of unequal-length bit vectors is not implemented");
		// $$$ treating a shorter src vector as having been padded with zeros,
		//     .. we should set any extra bits in the destination to all 1s;
		//     .. but this gets messy if commonNumBits is not a multiple of 64;
		//     .. best option would be to give bitwise_or_not the length of
		//     .. both vectors
		}
	}

void BitVector::intersect_with
   (const sdslbitvector* srcBits)
	{
	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to intersect into null bit vector");
	if (srcBits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to intersect from null bit vector");

	u64 commonNumBits = std::min(numBits,srcBits->size());
	bitwise_and (/*dst*/ bits->data(), srcBits->data(), commonNumBits);
	if (numBits > commonNumBits)
		{
		fatal ("internal error for " + identity()
		     + "; intersection of unequal-length bit vectors is not implemented");
		// $$$ treating a shorter src vector as having been padded with zeros,
		//     .. we should zero any extra bits in the destination; but this
		//     .. gets messy if commonNumBits is not a multiple of 64; best
		//     .. option would be to give bitwise_and the length of both
		//     .. vectors
		}
	}

void BitVector::mask_with
   (const sdslbitvector* srcBits)
	{
	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to intersect into null bit vector");
	if (srcBits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to intersect from null bit vector");

	u64 commonNumBits = std::min(numBits,srcBits->size());
	bitwise_mask (/*dst*/ bits->data(), srcBits->data(), commonNumBits);
	}

void BitVector::squeeze_by
   (const sdslbitvector* srcBits)
	{
	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to squeeze null bit vector");
	if (srcBits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to squeeze by a null bit vector");

	u64 commonNumBits = std::min(numBits,srcBits->size());
	u64 expectedNumBits = bitwise_count(srcBits->data(),numBits);
	sdslbitvector* resultBits = new sdslbitvector(expectedNumBits,0);
	if (trackMemory)
		cerr << "@+" << resultBits << " creating squeezeBits for BitVector(" << identity() << " " << this << ")" << endl;

	u64 reportedNumBits = bitwise_squeeze
	                        (/*src*/ bits->data(), /*spec*/ srcBits->data(), commonNumBits,
	                         /*dst*/ resultBits->data(), expectedNumBits);
	if (reportedNumBits != expectedNumBits)
		fatal ("internal error for " + identity()
		     + "; expected squeeze to result in "  + std::to_string(expectedNumBits) + " bits"
		     + ", but bitwise_squeeze() reported " + std::to_string(reportedNumBits) + " bits" );

	replace_bits(resultBits);
	}

int BitVector::operator[]
   (u64 pos) const
	{
	return (*bits)[pos];
	}

void BitVector::write_bit
   (u64	pos,
	int	val)
	{
	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to modify position " + std::to_string(pos)
		     + " in null bit vector");

	// cerr << "setting " << identity() << "[" << pos << "] to " << val << endl;
	(*bits)[pos] = val;
	}

u64 BitVector::rank1
   (u64 pos)
	{
	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; request for rank1(" + std::to_string(pos) + ")"
		     + " in null bit vector");

	if (ranker1 == nullptr)
		{
		ranker1 = new sdslrank1(bits);
		if (trackMemory)
			cerr << "@+" << ranker1 << " creating ranker1 for BitVector(" << identity() << " " << this << ")" << endl;
		}

	return ranker1->rank(pos);
	}

u64 BitVector::select0
   (u64 pos)
	{
	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; request for select0(" + std::to_string(pos) + ")"
		     + " in null bit vector");

	if (selector0 == nullptr)
		{
		selector0 = new sdslselect0(bits);
		if (trackMemory)
			cerr << "@+" << selector0 << " creating selector0 for BitVector(" << identity() << " " << this << ")" << endl;
		}

	return selector0->select(pos+1);  // (pos+1 compensates for SDSL API)
	}

void BitVector::discard_rank_select ()
	{
	if ((trackMemory) && (ranker1 != nullptr))
		cerr << "@-" << ranker1 << " discarding ranker1 for BitVector(" << identity() << " " << this << ")" << endl;
	if ((trackMemory) && (selector0 != nullptr))
		cerr << "@-" << selector0 << " discarding selector0 for BitVector(" << identity() << " " << this << ")" << endl;

	if (ranker1   != nullptr) { delete ranker1;    ranker1   = nullptr; }
	if (selector0 != nullptr) { delete selector0;  selector0 = nullptr; }
	}

string BitVector::to_string () const
	{ // intended to aid in debugging things involving *short* bit vectors
	string s = "";
	for (u64 pos=0 ; pos<numBits ; pos++)
		s += ((*this)[pos] == 0)? "-" : "+";
	return s;
	}

string BitVector::to_complement_string () const
	{ // intended to aid in debugging things involving *short* bit vectors
	string s = "";
	for (u64 pos=0 ; pos<numBits ; pos++)
		s += ((*this)[pos] == 0)? "+" : "-";
	return s;
	}

//----------
//
// RrrBitVector--
//	Bit vector with an rrr-compressed underlying file.
//
//----------
//
// Implementation notes:
//	(1)	We allow creation with bits in either compressed or uncompressed form.
//		In the uncompressed case, we'll convert to compressed upon save(), or
//		when compress() is explicitly called. While is the uncompressed form,
//		bits in the vector can be modified.
//	(2)	We'll never have both compressed or uncompressed forms at the same time.
//	(3)	copy_from() automatically compresses.
//
//----------

RrrBitVector::RrrBitVector
   (const string& _filename,
	const size_t _offset,
	const size_t _numBytes)
	  :	BitVector(_filename, _offset, _numBytes),
		rrrBits(nullptr),
		rrrRanker1(nullptr),
		rrrSelector0(nullptr)
	{
	if (trackMemory)
		cerr << "@+" << this << " constructor RrrBitVector(" << identity() << "), variant 1" << endl;
	}

RrrBitVector::RrrBitVector
   (const BitVector* srcBv)
	  :	BitVector(nullptr),
		rrrBits(nullptr),
		rrrRanker1(nullptr),
		rrrSelector0(nullptr)
	{
	bits = nullptr;
	if (srcBv == nullptr) return;

	// we copy from any subclass that is in the uncompressed state, or from an
	// RrrBitVector in the compressed state, or from an all-ones or all-zeros
	// class

	if (srcBv->bits != nullptr)
		copy_from (srcBv->bits);
	else if (srcBv->compressor() == bvcomp_rrr)
		{
		RrrBitVector* srcRrrBv = (RrrBitVector*) srcBv;
		if (srcRrrBv->rrrBits != nullptr)
			copy_from (srcRrrBv->rrrBits);
		}
	else if ((srcBv->numBits != 0)
	      && ((srcBv->compressor() == bvcomp_zeros)
	       || (srcBv->compressor() == bvcomp_ones)))
		{
		new_bits (srcBv->numBits);
		if (srcBv->compressor() == bvcomp_ones)
			bitwise_complement (/*dst*/ bits->data(), numBits);
		}

	if (trackMemory)
		cerr << "@+" << this << " constructor RrrBitVector(" << identity() << "), variant 2" << endl;
	}

RrrBitVector::RrrBitVector
   (const u64 _numBits)
	  :	BitVector(_numBits),
		rrrBits(nullptr),
		rrrRanker1(nullptr),
		rrrSelector0(nullptr)
	{
	if (trackMemory)
		cerr << "@+" << this << " constructor RrrBitVector(" << identity() << "), variant 3" << endl;
	}

RrrBitVector::~RrrBitVector()
	{
	if (trackMemory)
		cerr << "@-" << this << " destructor RrrBitVector(" << identity() << ")" << endl;
	if ((trackMemory) && (rrrBits != nullptr))
		cerr << "@-" << rrrBits << " discarding rrrBits for RrrBitVector(" << identity() << " " << this << ")" << endl;

	if (rrrBits != nullptr) delete rrrBits;
	// bits will get deleted in BitVector's destructor
	discard_rank_select();
	}

void RrrBitVector::serialized_in
   (std::ifstream& in)
	{
	assert (bits    == nullptr);
	assert (rrrBits == nullptr);

	rrrBits = new rrrbitvector();
	auto startTime = get_wall_time();
	sdsl::load (*rrrBits, in);  // $$$ ERROR_CHECK we need to check for errors inside sdsl
	fileLoadTime = elapsed_wall_time(startTime);
	if (reportFileBytes)
		cerr << "read " << numBytes << " for RrrBitVector::serialized_in(" << filename << ")" << endl;
	if (countFileBytes)
		{ totalFileReads++;  totalFileBytesRead += numBytes; }
	numBits = rrrBits->size();
	isResident = true;

	if (trackMemory)
		cerr << "@+" << rrrBits << " creating rrrBits for RrrBitVector(" << identity() << " " << this << ")" << endl;
	}

void RrrBitVector::save()
	{
	if (reportSave)
		cerr << "Saving " << filename << endl;

	if ((rrrBits == nullptr) and (bits == nullptr))
		fatal ("internal error for " + identity()
		     + "; attempt to save null bit vector");

	if ((rrrBits == nullptr) and (bits != nullptr))
		compress();

	if (offset != 0)
		fatal ("internal error for " + identity()
		     + "; attempt to save bit vector to non-zero file offset");

	auto startTime = get_wall_time();
	std::ofstream out (filename, std::ios::binary | std::ios::trunc | std::ios::out);
	if (not out)
		fatal ("error: " + class_identity() + "::save(" + identity() + ")"
		     + " failed to open \"" + filename + "\"");
	serialized_out (out);
	out.close();
	auto elapsedTime = elapsed_wall_time(startTime);
	if (reportSaveTime)
		cerr << "[" + class_identity() + " save] " << elapsedTime << " secs " << filename << endl;
	}

size_t RrrBitVector::serialized_out
   (std::ofstream& out)
	{
	if ((rrrBits == nullptr) and (bits == nullptr))
		fatal ("internal error for " + identity()
		     + "; attempt to serialize null bit vector");

	if ((rrrBits == nullptr) and (bits != nullptr))
		compress();

	return (size_t) rrrBits->serialize (out, nullptr, "");
	}

void RrrBitVector::discard_bits()
	{
	if ((trackMemory) && (bits != nullptr))
		cerr << "@-" << bits << " discarding bits for RrrBitVector(" << identity() << " " << this << ")" << endl;
	if ((trackMemory) && (rrrBits != nullptr))
		cerr << "@-" << rrrBits << " discarding rrrBits for RrrBitVector(" << identity() << " " << this << ")" << endl;

	if      (bits    != nullptr) { delete bits;     bits    = nullptr; }
	else if (rrrBits != nullptr) { delete rrrBits;  rrrBits = nullptr; }
	isResident = false;
	}

void RrrBitVector::new_bits
   (u64 _numBits)
	{
	if ((trackMemory) && (rrrBits != nullptr))
		cerr << "@-" << rrrBits << " discarding rrrBits for RrrBitVector(" << identity() << " " << this << ")" << endl;

	if (rrrBits != nullptr)
		{
		delete rrrBits;  rrrBits = nullptr;
		discard_rank_select();
		}
	BitVector::new_bits (_numBits);
	isResident = true;
	}

void RrrBitVector::copy_from
   (const sdslbitvector* srcBits)
	{
	if ((trackMemory) && (bits != nullptr))
		cerr << "@-" << bits << " discarding bits for RrrBitVector(" << identity() << " " << this << ")" << endl;
	if ((trackMemory) && (rrrBits != nullptr))
		cerr << "@-" << rrrBits << " discarding rrrBits for RrrBitVector(" << identity() << " " << this << ")" << endl;

	if (bits != nullptr)
		{
		delete bits;  bits = nullptr;
		discard_rank_select();
		}
	if (rrrBits != nullptr)
		{
		delete rrrBits;
		discard_rank_select();
		}

	rrrBits = new rrrbitvector (*srcBits);
	numBits = rrrBits->size();
	isResident = true;

	if (trackMemory)
		cerr << "@+" << rrrBits << " creating rrrBits for RrrBitVector(" << identity() << " " << this << ")" << endl;
	}

void RrrBitVector::copy_from
   (const rrrbitvector* srcRrrBits)
	{
	if ((trackMemory) && (bits != nullptr))
		cerr << "@-" << bits << " discarding bits for RrrBitVector(" << identity() << " " << this << ")" << endl;
	if ((trackMemory) && (rrrBits != nullptr))
		cerr << "@-" << rrrBits << " discarding rrrBits for RrrBitVector(" << identity() << " " << this << ")" << endl;

	if (bits != nullptr)
		{
		delete bits;  bits = nullptr;
		discard_rank_select();
		}
	if (rrrBits != nullptr)
		{
		delete rrrBits;
		discard_rank_select();
		}

	rrrBits = new rrrbitvector (*srcRrrBits);
	numBits = rrrBits->size();
	isResident = true;

	if (trackMemory)
		cerr << "@+" << rrrBits << " creating rrrBits for RrrBitVector(" << identity() << " " << this << ")" << endl;
	}

void RrrBitVector::compress
   ()
	{
	if (rrrBits != nullptr)
		return;	// compressing already compressed vector is benign

	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to compress null bit vector");

	rrrBits = new rrrbitvector (*bits);
	numBits = rrrBits->size();

	if (trackMemory)
		cerr << "@+" << rrrBits << " creating rrrBits for RrrBitVector(" << identity() << " " << this << ")" << endl;
	if (trackMemory)
		cerr << "@-" << bits << " discarding bits for RrrBitVector(" << identity() << " " << this << ")" << endl;

	delete bits;  bits = nullptr;
	discard_rank_select();
	}

int RrrBitVector::operator[]
   (u64 pos) const
	{
	if (rrrBits != nullptr) return (*rrrBits)[pos];
	                   else return BitVector::operator[](pos);
	}

void RrrBitVector::write_bit
   (u64	pos,
	int	val)
	{
	if (rrrBits != nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to modify position " + std::to_string(pos));

	BitVector::write_bit (pos, val);
	}

u64 RrrBitVector::rank1
   (u64 pos)
	{
	if (rrrBits == nullptr)
		fatal ("internal error for " + identity()
		     + "; request for rank1(" + std::to_string(pos) + ")"
		     + " in null bit vector");

	if (rrrRanker1 == nullptr)
		rrrRanker1 = new rrrrank1(rrrBits);
	return rrrRanker1->rank(pos);
	}

u64 RrrBitVector::select0
   (u64 pos)
	{
	if (rrrBits == nullptr)
		fatal ("internal error for " + identity()
		     + "; request for select0(" + std::to_string(pos) + ")"
		     + " in null bit vector");

	if (rrrSelector0 == nullptr)
		rrrSelector0 = new rrrselect0(rrrBits);
	return rrrSelector0->select(pos+1);  // (pos+1 compensates for SDSL API)
	}

void RrrBitVector::discard_rank_select ()
	{
	if (rrrRanker1   != nullptr) { delete rrrRanker1;    rrrRanker1   = nullptr; }
	if (rrrSelector0 != nullptr) { delete rrrSelector0;  rrrSelector0 = nullptr; }
	BitVector::discard_rank_select();
	}

//----------
//
// RoarBitVector--
//	Bit vector with a roaring bits-compressed underlying file.
//
//----------
//
// Implementation notes:
//	(1)	We allow creation with bits in either compressed or uncompressed form.
//		In the uncompressed case, we'll convert to compressed upon save(), or
//		when compress() is explicitly called. While is the uncompressed form,
//		bits in the vector can be modified.
//	(2)	We'll never have both compressed or uncompressed forms at the same time.
//	(3)	copy_from() automatically compresses.
//	(4) We add two 8-byte header fields to the native roar format. The file
//		contains
//			(8 bytes) object size        N
//			(8 bytes) number of bits     number of bits in the bit vector
//			(N bytes) native roar data
//
//----------

struct roarfile
	{
	u64	 roarBytes;
	u64	 numBits;
	char nativeData[1];
	};
#define roarHeaderBytes (2*sizeof(u64))


RoarBitVector::RoarBitVector
   (const string& _filename,
	const size_t _offset,
	const size_t _numBytes)
	  :	BitVector(_filename, _offset, _numBytes),
		roarBits(nullptr)
	{
	if (trackMemory)
		cerr << "@+" << this << " constructor RoarBitVector(" << identity() << "), variant 1" << endl;
	}

RoarBitVector::RoarBitVector
   (const BitVector* srcBv)
	  :	BitVector(nullptr),
		roarBits(nullptr)
	{
	bits = nullptr;
	if (srcBv == nullptr) return;

	// we copy from any subclass that is in the uncompressed state, or from an
	// RoarBitVector in the compressed state, or from all-ones or all-zeros
	// class

	if (srcBv->bits != nullptr)
		copy_from (srcBv->bits);
	else if (srcBv->compressor() == bvcomp_roar)
		{
		RoarBitVector* srcRoarBv = (RoarBitVector*) srcBv;
		if (srcRoarBv->roarBits != nullptr)
			copy_from (srcRoarBv->roarBits);
		}
	else if ((srcBv->numBits != 0)
	      && ((srcBv->compressor() == bvcomp_zeros)
	       || (srcBv->compressor() == bvcomp_ones)))
		{
		new_bits (srcBv->numBits);
		if (srcBv->compressor() == bvcomp_ones)
			bitwise_complement (/*dst*/ bits->data(), numBits);
		}

	if (trackMemory)
		cerr << "@+" << this << " constructor RoarBitVector(" << identity() << "), variant 2" << endl;
	}

RoarBitVector::RoarBitVector
   (const u64 _numBits)
	  :	BitVector(_numBits),
		roarBits(nullptr)
	{
	if (trackMemory)
		cerr << "@+" << this << " constructor RoarBitVector(" << identity() << "), variant 3" << endl;
	}

RoarBitVector::~RoarBitVector()
	{
	if (trackMemory)
		cerr << "@-" << this << " destructor RoarBitVector(" << identity() << ")" << endl;
	if ((trackMemory) && (roarBits != nullptr))
		cerr << "@-" << roarBits << " discarding roarBits for RoarBitVector(" << identity() << " " << this << ")" << endl;

	if (roarBits != nullptr) roaring_bitmap_free(roarBits);
	// bits will get deleted in BitVector's destructor
	}

void RoarBitVector::serialized_in
   (std::ifstream& in)
	{
	assert (bits     == nullptr);
	assert (roarBits == nullptr);

	auto startTime = get_wall_time();

	roarfile header;
	in.read ((char*) &header, roarHeaderBytes);
	if (!in.good())
		fatal ("error: " + class_identity() + "::serialized_in(" + identity() + ")"
		     + " problem reading header from \"" + filename + "\"");
	if (reportFileBytes)
		cerr << "read " << roarHeaderBytes << " for RoarBitVector::serialized_in(" << filename << ")" << endl;
	if (countFileBytes)
		{ totalFileReads++;  totalFileBytesRead += roarHeaderBytes; }
	size_t roarBytes = header.roarBytes;

	char* serializedData = new char[roarBytes];
	if (serializedData == nullptr)
		fatal ("error: " + class_identity() + "::serialized_in(" + identity() + ")"
		     + " failed to allocate " + std::to_string(roarBytes) + " bytes"
		     + " for \"" + filename + "\"");
	if (trackMemory)
		cerr << "@+" << serializedData << " allocating serializedData for RoarBitVector(" << identity() << " " << this << ")" << endl;

	in.read (serializedData, roarBytes);
	if (!in.good())
		fatal ("error: " + class_identity() + "::serialized_in(" + identity() + ")"
		     + " problem reading " + std::to_string(roarBytes) + " bytes"
		     + " from \"" + filename + "\"");
	if (reportFileBytes)
		cerr << "read " << roarBytes << " for RoarBitVector::serialized_in(" << filename << ")" << endl;
	if (countFileBytes)
		{ totalFileReads++;  totalFileBytesRead += roarBytes; }

    roarBits = roaring_bitmap_portable_deserialize(serializedData);
	fileLoadTime = elapsed_wall_time(startTime);

	if (trackMemory)
		cerr << "@+" << roarBits << " creating roarBits for RoarBitVector(" << identity() << " " << this << ")" << endl;
	if (trackMemory)
		cerr << "@-" << serializedData << " discarding serializedData for RoarBitVector(" << identity() << " " << this << ")" << endl;

	delete[] serializedData;
	numBits = header.numBits;
	isResident = true;
	}

void RoarBitVector::save()
	{
	if (reportSave)
		cerr << "Saving " << filename << endl;

	if ((roarBits == nullptr) and (bits == nullptr))
		fatal ("internal error for " + identity()
		     + "; attempt to save null bit vector");

	if ((roarBits == nullptr) and (bits != nullptr))
		compress();

	if (offset != 0)
		fatal ("internal error for " + identity()
		     + "; attempt to save bit vector to non-zero file offset");

	auto startTime = get_wall_time();
	std::ofstream out (filename, std::ios::binary | std::ios::trunc | std::ios::out);
	if (not out)
		fatal ("error: " + class_identity() + "::save(" + identity() + ")"
		     + " failed to open \"" + filename + "\"");
	serialized_out (out);
	out.close();
	auto elapsedTime = elapsed_wall_time(startTime);
	if (reportSaveTime)
		cerr << "[" + class_identity() + " save] " << elapsedTime << " secs " << filename << endl;
	}

size_t RoarBitVector::serialized_out
   (std::ofstream& out)
	{
	if ((roarBits == nullptr) and (bits == nullptr))
		fatal ("internal error for " + identity()
		     + "; attempt to serialize null bit vector");

	if ((roarBits == nullptr) and (bits != nullptr))
		compress();

	roarfile* serializedData;
	size_t headerBytes = roarHeaderBytes;
	size_t roarBytes   = roaring_bitmap_portable_size_in_bytes (roarBits);
	size_t totalBytes  = headerBytes + roarBytes;
	serializedData = (roarfile*) new char[totalBytes];
	if (serializedData == nullptr)
		fatal ("error: " + class_identity() + "::serialized_out(" + identity() + ")"
		     + " failed to allocate " + std::to_string(totalBytes) + " bytes"
		     + " for \"" + filename + "\"");
	if (trackMemory)
		cerr << "@+" << serializedData << " allocating serializedData for RoarBitVector(" << identity() << " " << this << ")" << endl;

	serializedData->roarBytes = roarBytes;
	serializedData->numBits   = numBits;
	// $$$ ERROR_CHECK would like to check for errors in this
	roaring_bitmap_portable_serialize(roarBits, (char*) &serializedData->nativeData);
	// $$$ ERROR_CHECK would like to check for errors in this
	out.write ((char*) serializedData, totalBytes);

	if (trackMemory)
		cerr << "@-" << serializedData << " discarding serializedData for RoarBitVector(" << identity() << " " << this << ")" << endl;

	delete[] serializedData;
	return totalBytes;
	}

void RoarBitVector::discard_bits()
	{
	if ((trackMemory) && (bits != nullptr))
		cerr << "@-" << bits << " discarding bits for RoarBitVector(" << identity() << " " << this << ")" << endl;
	if ((trackMemory) && (roarBits != nullptr))
		cerr << "@-" << roarBits << " discarding roarBits for RoarBitVector(" << identity() << " " << this << ")" << endl;

	if      (bits     != nullptr) { delete bits;                    bits     = nullptr; }
	else if (roarBits != nullptr) { roaring_bitmap_free(roarBits);  roarBits = nullptr; }
	isResident = false;
	}

void RoarBitVector::new_bits
   (u64 _numBits)
	{
	if ((trackMemory) && (roarBits != nullptr))
		cerr << "@-" << roarBits << " discarding roarBits for RoarBitVector(" << identity() << " " << this << ")" << endl;

	if (roarBits != nullptr)
		{ roaring_bitmap_free(roarBits);  roarBits = nullptr; }
	BitVector::new_bits (_numBits);
	isResident = true;
	}

void RoarBitVector::copy_from
   (const sdslbitvector* srcBits)
	{
	if ((trackMemory) && (bits != nullptr))
		cerr << "@-" << bits << " discarding bits for RoarBitVector(" << identity() << " " << this << ")" << endl;
	if ((trackMemory) && (roarBits != nullptr))
		cerr << "@-" << roarBits << " discarding roarBits for RoarBitVector(" << identity() << " " << this << ")" << endl;

	if (bits     != nullptr) { delete bits;                    bits     = nullptr; }
	if (roarBits != nullptr) { roaring_bitmap_free(roarBits);  roarBits = nullptr; }

	bits = new sdslbitvector (*srcBits);
	if (trackMemory)
		cerr << "@+" << bits << " creating bits for RoarBitVector(" << identity() << " " << this << ")" << endl;

	numBits = bits->size();
	isResident = true;

	compress();
	}

void RoarBitVector::copy_from
   (const roaring_bitmap_t*	srcRoarBits)
	{
	if ((trackMemory) && (bits != nullptr))
		cerr << "@-" << bits << " discarding bits for RoarBitVector(" << identity() << " " << this << ")" << endl;
	if ((trackMemory) && (roarBits != nullptr))
		cerr << "@-" << roarBits << " discarding roarBits for RoarBitVector(" << identity() << " " << this << ")" << endl;

	if (bits     != nullptr) { delete bits;  bits = nullptr; }
	if (roarBits != nullptr) { roaring_bitmap_free(roarBits);  roarBits = nullptr; }

	roarBits = roaring_bitmap_copy(srcRoarBits);
	isResident = true;
	}

void RoarBitVector::compress
   ()
	{
	if (roarBits != nullptr)
		return;	// compressing already compressed vector is benign

	if (bits == nullptr)
		fatal ("internal error for " + identity()
		     + "; attempt to compress null bit vector");

	roarBits = roaring_bitmap_create();
	for (u64 pos=0 ; pos<numBits; pos++)
		{ if ((*bits)[pos]) roaring_bitmap_add (roarBits, pos); }

	if (trackMemory)
		cerr << "@+" << roarBits << " creating roarBits for RoarBitVector(" << identity() << " " << this << ")" << endl;
	if (trackMemory)
		cerr << "@-" << bits << " discarding bits for RoarBitVector(" << identity() << " " << this << ")" << endl;

	delete bits;  bits = nullptr;
	// note that numBits does not change
	}

int RoarBitVector::operator[]
   (u64 pos) const
	{
	if (roarBits != nullptr) return roaring_bitmap_contains (roarBits, pos);
	                    else return BitVector::operator[](pos);
	}

void RoarBitVector::write_bit
   (u64	pos,
	int	val)
	{
	if (roarBits != nullptr)
		roaring_bitmap_add (roarBits, pos);
	else
		BitVector::write_bit (pos, val);
	}

u64 RoarBitVector::rank1
   (u64 pos)
	{
	fatal ("internal error for " + identity()
	     + "; request for rank1(" + std::to_string(pos) + ")"
	     + " in roar-compressed bit vector");
	return 0;  // execution never reaches here
	}

u64 RoarBitVector::select0
   (u64 pos)
	{
	fatal ("internal error for " + identity()
	     + "; request for select0(" + std::to_string(pos) + ")"
	     + " in roar-compressed bit vector");
	return 0;  // execution never reaches here
	}

//----------
//
// RawBitVector--
//	Bit vector whose underlying file is an uncompressed segment of any file
//	type. The primary motivation is to allow us to read a segment of a
//	BitVector's file without having to read the entire file.
//
//----------
//
// Implementation notes:
//	(1)	Other than input, behavior is identical to a BitVector.
//	(2)	We usually construct this with numBits != 0, in anticipation of the
//		number of bits to be populated upon input.
//	(3)	Input interprets bits in the same order as a sdslbitvector.
//	(4)	We intentionally omit numBytes from the constructor here, as that is
//		a mechanism for reading *entire* bit vectors.
//
//----------

RawBitVector::RawBitVector
   (const string& _filename,
	const size_t _offset,
	const u64 _numBits)
	  :	BitVector(_filename, _offset)
	{
	numBits = _numBits;

	if (trackMemory)
		cerr << "@+" << this << " constructor RawBitVector(" << identity() << "), variant 1" << endl;
	}

RawBitVector::RawBitVector
   (const u64 _numBits)
	  :	BitVector(_numBits)
	{
	if (trackMemory)
		cerr << "@+" << this << " constructor RawBitVector(" << identity() << "), variant 2" << endl;
	}

RawBitVector::~RawBitVector()
	{
	if (trackMemory)
		cerr << "@-" << this << " destructor RawBitVector(" << identity() << ")" << endl;
	// bits will get deleted in BitVector's destructor
	}

void RawBitVector::serialized_in
   (std::ifstream& in)
	{
	assert (bits == nullptr);
	assert (numBits != 0);

	bits = new sdslbitvector(numBits,0);
	if (trackMemory)
		cerr << "@+" << bits << " creating bits for RawBitVector(" << identity() << " " << this << ")" << endl;

	u64* rawBits = bits->data();
	u64 numBytes = (numBits+7) / 8;

	auto startTime = get_wall_time();
	in.read ((char*) rawBits, numBytes);  // $$$ ERROR_CHECK we need to check for errors in the read
	fileLoadTime = elapsed_wall_time(startTime);
	if (reportFileBytes)
		cerr << "read " << numBytes << " for RawBitVector::serialized_in(" << filename << ")" << endl;
	if (countFileBytes)
		{ totalFileReads++;  totalFileBytesRead += numBytes; }
	numBits = bits->size();
	isResident = true;
	}

//----------
//
// ZerosBitVector--
//	Bit vector with no underlying file.  All bit positions are zero.
//
//----------
//
// Implementation notes:
//	(1)	Attempts to write bits are fatal errors.
//
//----------

ZerosBitVector::ZerosBitVector
   (const string& _filename,
	const size_t _offset,
	const size_t _numBytes)
	  :	BitVector(_filename, _offset, _numBytes)
	{
	if (trackMemory)
		cerr << "@+" << this << " constructor ZerosBitVector(" << identity() << "), variant 1" << endl;
	}

ZerosBitVector::ZerosBitVector
   (const u64 _numBits)
	  :	BitVector((u64)0) // casting to u64 tells the compiler which constructor I want
	{
	// nota bene: it would make sense to defer to the constructor
	//   .. BitVector(_numBits); but (apparently), when that constructor calls
	//   .. new_bits() it ends up at BitVector::new_bits() instead of at
	//   .. ZerosBitVector::new_bits()

	if (_numBits != 0) new_bits (_numBits);

	if (trackMemory)
		cerr << "@+" << this << " constructor ZerosBitVector(" << identity() << "), variant 2" << endl;
	}

ZerosBitVector::~ZerosBitVector()
	{
	if (trackMemory)
		cerr << "@-" << this << " destructor ZerosBitVector(" << identity() << ")" << endl;
	if (bits != nullptr)
		fatal ("internal error for " + identity()
		     + "; destructor encountered non-null bit vector");
	}

void ZerosBitVector::serialized_in
   (std::ifstream& in)
	{
	// do nothing
	}

void ZerosBitVector::save()
	{
	// do nothing
	}

size_t ZerosBitVector::serialized_out
   (std::ofstream& out,
	const string& _filename,
	const size_t _offset)
	{
	filename = _filename;
	offset   = _offset;
	return 0;
	}

size_t ZerosBitVector::serialized_out
   (std::ofstream& out)
	{
	return 0;
	}

void ZerosBitVector::discard_bits()
	{
	if (bits != nullptr)
		fatal ("internal error for " + identity()
		     + "; destructor encountered non-null bit vector");
	isResident = false;
	}

void ZerosBitVector::new_bits
   (u64 _numBits)
	{
	if (bits != nullptr)
		fatal ("internal error for " + identity()
		     + "; new_bits() encountered non-null bit vector");

	numBits = _numBits;
	isResident = true;
	}

void ZerosBitVector::copy_from
   (const sdslbitvector* srcBits)
	{
	fatal ("internal error for " + identity()
	     + "; attempt to install a bit vector");
	}

void ZerosBitVector::copy_from
   (const rrrbitvector* srcRrrBits)
	{
	fatal ("internal error for " + identity()
	     + "; attempt to install an RRR bit vector");
	}

void ZerosBitVector::copy_from
   (const roaring_bitmap_t* srcRoarBits)
	{
	fatal ("internal error for " + identity()
	     + "; attempt to install a roar bit vector");
	}

void ZerosBitVector::fill
   (int bitVal)
	{
	fatal ("internal error for " + identity()
	     + "; attempt to fill write-protected bit vector");
	}

void ZerosBitVector::complement ()
	{
	fatal ("internal error for " + identity()
	     + "; attempt to complement write-protected bit vector");
	}

void ZerosBitVector::union_with
   (const sdslbitvector* srcBits)
	{
	fatal ("internal error for " + identity()
	     + "; attempt to union into write-protected bit vector");
	}

void ZerosBitVector::intersect_with
   (const sdslbitvector* srcBits)
	{
	fatal ("internal error for " + identity()
	     + "; attempt to intersect into write-protected bit vector");
	}

void ZerosBitVector::mask_with
   (const sdslbitvector* srcBits)
	{
	fatal ("internal error for " + identity()
	     + "; attempt to mask write-protected bit vector");
	}

void ZerosBitVector::write_bit
   (u64	pos,
	int	val)
	{
	fatal ("internal error for " + identity()
	     + "; attempt to modify position " + std::to_string(pos)
	     + " in write-protected bit vector");
	}

u64 ZerosBitVector::rank1
   (u64 pos)
	{
	fatal ("internal error for " + identity()
	     + "; request for rank1(" + std::to_string(pos) + ")"
	     + " in write-protected bit vector");
	return 0;  // execution never reaches here
	}

u64 ZerosBitVector::select0
   (u64 pos)
	{
	fatal ("internal error for " + identity()
	     + "; request for select0(" + std::to_string(pos) + ")"
	     + " in write-protected bit vector");
	return 0;  // execution never reaches here
	}

//----------
//
// OnesBitVector--
//	Bit vector with no underlying file.  All bit positions are one.
//
//----------
//
// Implementation notes:
//	(1)	Most behavior is inherited from the ZerosBitVector superclass.
//
//----------

OnesBitVector::OnesBitVector
   (const string& _filename,
	const size_t _offset,
	const size_t _numBytes)
	  :	ZerosBitVector(_filename, _offset, _numBytes)
	{
	if (trackMemory)
		cerr << "@+" << this << " constructor OnesBitVector(" << identity() << "), variant 1" << endl;
	}

OnesBitVector::OnesBitVector
   (const u64 _numBits)
	  :	ZerosBitVector(_numBits)
	{
	if (trackMemory)
		cerr << "@+" << this << " constructor OnesBitVector(" << identity() << "), variant 2" << endl;
	}

OnesBitVector::~OnesBitVector()
	{
	if (trackMemory)
		cerr << "@-" << this << " destructor OnesBitVector(" << identity() << ")" << endl;
	if (bits != nullptr)
		fatal ("internal error for " + identity()
		     + "; destructor encountered non-null bit vector");
	}

//----------
//
// valid_filename--
//	Check whether a filename is appropriate for a bit_vector file.
//
//----------
//
// Arguments:
//	const string&	filename:	The name to check.
//
// Returns:
//	true if the filename could be a bit vector;  false otherwise.
//
//----------
//
// Notes:
//	(1)	This does check the file's contents in any way;  or even whether the
//		file exists.
//
//----------

bool BitVector::valid_filename
   (const string& filename)
	{
	// note that we don't recognize .zeros and .ones here, since we'll never
	// .. store a bitvector file with that type (as opposed to a bloom filter
	// .. file)
	if (is_suffix_of (filename, ".bv"))   return true;
	if (is_suffix_of (filename, ".rrr"))  return true;
	if (is_suffix_of (filename, ".roar")) return true;
	return false;
	}

//----------
//
// compressor_to_string--
//	Convert a bitvector compression type to a string, often used for
//	constructing filenames.
//
//----------
//
// Arguments:
//	const u32	compressor:	The type of bitvector; one of bvcomp_xxx.
//
// Returns:
//	A short string representing the compression type.
//
//----------

string BitVector::compressor_to_string
   (u32	compressor)
	{
	switch (compressor)
		{
		case bvcomp_rrr:          return "rrr";
		case bvcomp_roar:         return "roar";
		case bvcomp_zeros:        return "zeros";
		case bvcomp_ones:         return "ones";
		default: // ……… should default be an error?
		case bvcomp_uncompressed: return "uncompressed";
		}

	return "";  // should never get here
	}

//----------
//
// bit_vector--
//	Create a BitVector object of subclass appropriate for a filename.
//
//----------
//
// Arguments (variant 1):
//	const string&	filename:	The name of the file that contains the vector's
//								.. bits. This can also be of the special forms
//								.. discussed in note 2.
//	const string&	kind:		The type of bit vector (.e.g. "bv", "rrr",
//								.. "roar", or "raw"); an empty string means we
//								.. should determine the type from the filename.
//	const size_t	offset:		Offset from start of file to vector's data.
//	const size_t	numBytes:	Number of bytes of data occupied in the file;
//								.. zero means this fact is unknown.
//
// Arguments (variant 2):
//	const string&	filename:	The name of the file that contains the vector's
//								.. bits. This is just the filename, and should
//								.. *not* include other fields.
//	const u32		compressor:	The type of bit vector; one of bvcomp_xxx.
//	const size_t	offset:		(same as for variant 1).
//	const size_t	numBytes:	(same as for variant 1).
//
// Arguments (variant 3):
//	const u32		compressor:	The type of bit vector; one of bvcomp_xxx.
//	const u64		numBits:	Total number of bits in the bit vector.
//
// Arguments (variant 4):
//	const u32		compressor:	The type of bit vector; one of bvcomp_xxx.
//	const BitVector* srcBv:		A bit vector to copy from.
//
// Returns:
//	A BitVector (or subclass of BitVector) object;  a "bad" filename will
//	result in program termination.
//
//----------
//
// Notes:
//	(1)	This does *not* load the bits from the file, or even check that the
//		file exists.
//	(2)	Filename parsing supports special forms that allow a bitvector to be
//		extracted from a segment of the file.  The three special forms are
//		  <filename>:<type>[:<offset>[..<end>]]
//		  <filename>:<type>[:<offset>[:<bytes>]]
//		  <filename>:raw[:<offset>[:<bits>]]
//		where
//		  <type>   is e.g. "bv", "rrr", etc.
//		  <offset> gives the first byte of the segment; this can be decimal,
//		           or hexadecimal if it begins with "0x"
//		  <end>    gives the first byte *not* in the segment (decimal or hex)
//		  <bytes>  gives the number of bytes in the segment (decimal or hex)
//		  <bits>   gives the number of *bits* in the segment; this only applies
//		           to "raw" type
//
//----------

#define numBytesSanityLimit  ((size_t) (1*1000*1000*1000))

//=== variant 1 ===

BitVector* BitVector::bit_vector
   (const string&	_filename,
	const string&	_kind,
	const size_t	_offset,
	const size_t	_numBytes)
	{
	string filename = _filename;
	string kind     = _kind;
	size_t offset   = _offset;
	size_t numBytes = _numBytes;
	u64 numBits = 0;

	if (numBytes > numBytesSanityLimit)
		fatal ("internal error: request for " + std::to_string(numBytes)
		    + "for bit vector \"" + filename + "\" exceeds sanity limit");

	// if no kind has been specified, see if the filename is one of the forms
	//    <filename>:<type>[:<offset>[..<end>]]
	//  or
	//    <filename>:<type>[:<offset>[:<bytes>]]
	//  or
	//    <filename>:raw[:<offset>[:<bits>]]

	if (kind.empty())
		{
		string::size_type colonIx = filename.find(':');
		if (colonIx != string::npos)
			{
			kind     = filename.substr(colonIx+1);
			filename = filename.substr(0,colonIx);
			colonIx  = kind.find(':');
			if (colonIx != string::npos)
				{
				string offsetStr = kind.substr(colonIx+1);
				kind    = kind.substr(0,colonIx);
				size_t endOffset = 0;
				colonIx = offsetStr.find(':');
				if (colonIx != string::npos)
					{
					if (kind == "raw")
						{
						numBits   = string_to_u64 (offsetStr.substr(colonIx+1), /*allowHex*/ true);
						offsetStr = offsetStr.substr(0,colonIx);
						}
					else if (numBytes == 0)
						{
						numBytes  = string_to_u64 (offsetStr.substr(colonIx+1), /*allowHex*/ true);
						offsetStr = offsetStr.substr(0,colonIx);
						}
					else
						fatal ("error: can't decipher \"" + _filename + "\" as a bit vector");
					}
				else
					{
					string::size_type dotsIx = offsetStr.find("..");
					if (dotsIx != string::npos)
						{
						if ((kind != "raw") && (numBytes == 0))
							{
							endOffset = string_to_u64 (offsetStr.substr(dotsIx+2), /*allowHex*/ true);
							offsetStr = offsetStr.substr(0,dotsIx);
							}
						if (endOffset == 0)
							fatal ("error: can't decipher \"" + _filename + "\" as a bit vector");
						}
					}
				offset = string_to_u64 (offsetStr, /*allowHex*/ true);
				if (endOffset != 0)
					{
					if (endOffset <= offset)
						fatal ("error: can't decipher \"" + _filename + "\" as a bit vector");
					numBytes = endOffset - offset;
					}
				}
			}
		if (kind[0] == '.')
			kind = kind.substr(1); // allow, e.g., ".bv" and ".rrr"
		}

	// if we still don't know the kind, get it from the file extension
	//
	// note that we don't recognize .zeros and .ones here, since we'll never
	// .. store a bitvector file with that type

	if (kind.empty())
		{
		// nota bene: if more types are added to this list, be sure to also add
		//            them to the error message
		if      (is_suffix_of (filename, ".bv"))    kind = "bv";
		else if (is_suffix_of (filename, ".rrr"))   kind = "rrr";
		else if (is_suffix_of (filename, ".roar"))  kind = "roar";
		else
			fatal ("\"" + filename + "\" is of an unknown bit vector filetype"
			     + " (.bv, .rrr, and .roar are acceptible)");
		}

	// create the bit vector
	//
	// note that we *do* recognize zeros and ones here, since we can store a
	// .. bitvector of that type within a bloom filter file

	if (reportCreation)
		cerr << "creating bit_vector type \"" << kind << "\""
		     << " at offset " << offset << " in \"" << filename << "\"" << endl;

	if      (kind == "bv")    return new BitVector      (filename, offset, numBytes);
	else if (kind == "rrr")   return new RrrBitVector   (filename, offset, numBytes);
	else if (kind == "roar")  return new RoarBitVector  (filename, offset, numBytes);
	else if (kind == "raw")   return new RawBitVector   (filename, offset, numBits); // (numBits is correct)
	else if (kind == "zeros") return new ZerosBitVector (filename, offset, numBytes);
	else if (kind == "ones")  return new OnesBitVector  (filename, offset, numBytes);
	else
		fatal ("(for \"" + filename + "\")"
		     + " bad compression type: \"" + kind + "\"");

	return nullptr;  // execution never reaches here
	}


//=== variant 2 ===

BitVector* BitVector::bit_vector
   (const string&	filename,
	const u32		compressor,
	const size_t	offset,
	const size_t	numBytes)
	{
	// create the bit vector

	if (reportCreation)
		cerr << "creating bit_vector type " << compressor
		     << " at offset " << offset << " in \"" << filename << "\"" << endl;

	switch (compressor)
		{
		case bvcomp_uncompressed:
			return new BitVector (filename, offset, numBytes);
		case bvcomp_rrr:
			return new RrrBitVector (filename, offset, numBytes);
		case bvcomp_roar:
			return new RoarBitVector (filename, offset, numBytes);
		case bvcomp_zeros:
			return new ZerosBitVector (filename, offset, numBytes);
		case bvcomp_ones:
			return new OnesBitVector  (filename, offset, numBytes);
		default:
			fatal ("(for \"" + filename + "\")"
			     + " bad compressor code: \"" + std::to_string(compressor) + "\"");
		}

	return nullptr;  // execution never reaches here
	}


//=== variant 3 ===

BitVector* BitVector::bit_vector
   (const u32 compressor,
	const u64 numBits)
	{
	switch (compressor)
		{
		case bvcomp_uncompressed: return new BitVector      (numBits);
		case bvcomp_rrr:          return new RrrBitVector   (numBits);
		case bvcomp_roar:         return new RoarBitVector  (numBits);
		case bvcomp_zeros:        return new ZerosBitVector (numBits);
		case bvcomp_ones:         return new OnesBitVector  (numBits);
		default:
			fatal ("error: BitVector::bit_vector(\"" + std::to_string(compressor) + "\",numBits)"
			     + " is not implemented");
		}

	return nullptr;  // execution never reaches here
	}


//=== variant 4 ===

BitVector* BitVector::bit_vector
   (const u32			compressor,
	const BitVector*	srcBv)
	{
	// note that we don't support cloning of a bitvector into ZerosBitVector
	// or OnesBitVector

	switch (compressor)
		{
		case bvcomp_uncompressed: return new BitVector      (srcBv);
		case bvcomp_rrr:          return new RrrBitVector   (srcBv);
		case bvcomp_roar:         return new RoarBitVector  (srcBv);
		default:
			fatal ("error: BitVector::bit_vector(\"" + std::to_string(compressor) + "\",srcBv)"
			     + " is not implemented");
		}

	return nullptr;  // execution never reaches here
	}
