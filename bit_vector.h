#ifndef bit_vector_H
#define bit_vector_H

#include <cstdint>
#include <string>
#include <sdsl/bit_vectors.hpp>
#include <roaring/roaring.h>
#include "bloom_filter_file.h"

// Honor the deployer's choice of custom RRR block size; otherwise use 255.
// RRR_block_SIZE is typically one of 255, 127, 63, etc. We prefer 255, but
// past versions of sdsl-lite (before April 2017) when compiled with clang had
// a bug that made RRR fail silently with block sizes above 127.
//
// To override this default of 255, add -DRRR_BLOCK_SIZE=127 to the Makefile

#ifndef RRR_BLOCK_SIZE
#define RRR_BLOCK_SIZE 255
#endif

using sdslbitvector = sdsl::bit_vector;
using sdslrank0     = sdsl::rank_support_v<0>;
using sdslrank1     = sdsl::rank_support_v<1>;
using sdslselect0   = sdsl::select_support_mcl<0>;
using sdslselect1   = sdsl::select_support_mcl<1>;

using rrrbitvector  = sdsl::rrr_vector<RRR_BLOCK_SIZE>;
using rrrrank0      = sdsl::rank_support_rrr<0,RRR_BLOCK_SIZE>;
using rrrrank1      = sdsl::rank_support_rrr<1,RRR_BLOCK_SIZE>;
using rrrselect0    = sdsl::select_support_rrr<0,RRR_BLOCK_SIZE>;
using rrrselect1    = sdsl::select_support_rrr<1,RRR_BLOCK_SIZE>;

//using    roarbitvector = roaring_bitmap_t

#define sdslbitvectorHeaderBytes 8	// to grab "raw" bits, skip this many bytes
									// at the start of an sdslbitvector file

//----------
//
// classes in this module--
//
//----------

class BitVector
	{
public:
	BitVector(const std::string& filename, const size_t offset=0, size_t numBytes=0);
	BitVector(const BitVector* srcBv);
	BitVector(std::uint64_t numBits=0);
	virtual ~BitVector();

	virtual std::string class_identity() const { return "BitVector"; }
	virtual std::string identity() const;
	virtual bool modifiable() { return (bits != nullptr); }
	virtual std::uint32_t compressor() const { return bvcomp_uncompressed; }
	virtual void load();
	virtual void serialized_in(std::ifstream& in);
	virtual void save();
	virtual size_t serialized_out(std::ofstream& out, const std::string& filename, const size_t offset=0);
	virtual size_t serialized_out(std::ofstream& out);

	virtual void discard_bits();
	virtual void new_bits(std::uint64_t numBits);
	virtual void replace_bits(sdslbitvector* srcBits);
	virtual void copy_from(const sdslbitvector* srcBits);

	virtual void fill(int bitVal);
	virtual void complement();
	virtual void union_with(const sdslbitvector* srcBits);
	virtual void union_with_complement(const sdslbitvector* srcBits);
	virtual void intersect_with(const sdslbitvector* srcBits);
	virtual void mask_with(const sdslbitvector* srcBits);
	virtual void squeeze_by(const sdslbitvector* srcBits);

	virtual int operator[](std::uint64_t pos) const;
	virtual void write_bit(std::uint64_t pos, int val=1);

	virtual std::uint64_t rank1(std::uint64_t pos);
	virtual std::uint64_t select0(std::uint64_t pos);
	virtual void discard_rank_select();

	virtual std::uint64_t num_bits() const { return numBits; }

	virtual std::string to_string() const;
	virtual std::string to_complement_string() const;

public:
	bool isResident;
	std::string filename;
	size_t offset;          // offset from start of file to vector's data
	size_t numBytes;        // number of bytes of data; zero means unknown
	sdslbitvector* bits;
	std::uint64_t numBits;  // number of (conceptual) bits in the bit vector
	sdslrank1*   ranker1;
	sdslselect0* selector0;
	std::uint64_t filterInfo; // filter-dependent info for this bit vector;
							// .. typically zero

public:
	bool reportLoad     = false;
	bool reportSave     = false;
	bool reportLoadTime = false;
	bool reportSaveTime = false;
	double fileLoadTime = 0.0;	// (internal use) time (in secs) of latest load
	static bool reportCreation;
	static bool reportDestructor;
	static bool reportBits;
	static bool reportRankSelect;

public:
	static bool       valid_filename (const std::string& filename);
	static std::string compressor_to_string(std::uint32_t compressor);
	static BitVector* bit_vector     (const std::string& filename,
	                                  const std::string& kind="",
	                                  const size_t offset=0,
	                                  const size_t numBytes=0);
	static BitVector* bit_vector     (const std::string& filename,
	                                  const std::uint32_t compressor,
	                                  const size_t offset=0,
	                                  const size_t numBytes=0);
	static BitVector* bit_vector     (const std::uint32_t compressor,
	                                  const std::uint64_t numBits);
	static BitVector* bit_vector     (const std::uint32_t compressor,
	                                  const BitVector* srcBv);
	};


class RrrBitVector: public BitVector
	{
public:
	RrrBitVector(const std::string& filename, const size_t offset=0, size_t numBytes=0);
	RrrBitVector(const BitVector* srcBv);
	RrrBitVector(std::uint64_t numBits=0);
	virtual ~RrrBitVector();

	virtual std::string class_identity() const { return "RrrBitVector"; }
	virtual std::uint32_t compressor() const { return bvcomp_rrr; }
	virtual void serialized_in(std::ifstream& in);
	virtual void save();
	virtual size_t serialized_out(std::ofstream& out);

	virtual void discard_bits();
	virtual void new_bits(std::uint64_t numBits);
	virtual void copy_from(const sdslbitvector* srcBits);
	virtual void copy_from(const rrrbitvector* srcRrrBits);
	virtual void compress();

	virtual int operator[](std::uint64_t pos) const;
	virtual void write_bit(std::uint64_t pos, int val=1);

	virtual std::uint64_t rank1(std::uint64_t pos);
	virtual std::uint64_t select0(std::uint64_t pos);
	virtual void discard_rank_select();

	rrrbitvector* rrrBits;   // exclusive of BitVector.bits; at most one of
	                         // .. bits and rrrBits is non-null at a given time
	rrrrank1*   rrrRanker1;  // exclusive of BitVector.ranker1
	rrrselect0* rrrSelector0;// exclusive of BitVector.selector0
	};


class RoarBitVector: public BitVector
	{
public:
	RoarBitVector(const std::string& filename, const size_t offset=0, size_t numBytes=0);
	RoarBitVector(const BitVector* srcBv);
	RoarBitVector(std::uint64_t numBits=0);
	virtual ~RoarBitVector();

	virtual std::string class_identity() const { return "RoarBitVector"; }
	virtual std::uint32_t compressor() const { return bvcomp_roar; }
	virtual void serialized_in(std::ifstream& in);
	virtual void save();
	virtual size_t serialized_out(std::ofstream& out);

	virtual void discard_bits();
	virtual void new_bits(std::uint64_t numBits);
	virtual void copy_from(const sdslbitvector* srcBits);
	virtual void copy_from(const roaring_bitmap_t* srcRoarBits);
	virtual void compress();

	virtual int operator[](std::uint64_t pos) const;
	virtual void write_bit(std::uint64_t pos, int val=1);

	virtual std::uint64_t rank1(std::uint64_t pos);
	virtual std::uint64_t select0(std::uint64_t pos);

	roaring_bitmap_t* roarBits; // exclusive of BitVector.bits; at most one of
	                            // .. bits and roarBits is non-null at a given
	                            // .. time
	};


class RawBitVector: public BitVector
	{
public:
	RawBitVector(const std::string& filename, const size_t offset=0, const std::uint64_t numBits=0);
	// RawBitVector(BitVector* srcBv);  we intentionally omit this constructor
	RawBitVector(std::uint64_t numBits=0);
	virtual ~RawBitVector();

	virtual std::string class_identity() const { return "RawBitVector"; }
	virtual void serialized_in(std::ifstream& in);
	};


class ZerosBitVector: public BitVector
	{
public:
	ZerosBitVector(const std::string& filename, const size_t offset=0, size_t numBytes=0);
	ZerosBitVector(std::uint64_t numBits=0);
	virtual ~ZerosBitVector();

	virtual std::string class_identity() const { return "ZerosBitVector"; }
	virtual std::uint32_t compressor() const { return bvcomp_zeros; }
	virtual void serialized_in(std::ifstream& in);
	virtual void save();
	virtual size_t serialized_out(std::ofstream& out, const std::string& filename, const size_t offset=0);
	virtual size_t serialized_out(std::ofstream& out);

	virtual void discard_bits();
	virtual void new_bits(std::uint64_t numBits);
	virtual void copy_from(const sdslbitvector* srcBits);
	virtual void copy_from(const rrrbitvector* srcRrrBits);
	virtual void copy_from(const roaring_bitmap_t* srcRoarBits);

	virtual void fill(int bitVal);
	virtual void complement();
	virtual void union_with(const sdslbitvector* srcBits);
	virtual void intersect_with(const sdslbitvector* srcBits);
	virtual void mask_with(const sdslbitvector* srcBits);

	virtual int operator[](std::uint64_t pos) const { return 0; }
	virtual void write_bit(std::uint64_t pos, int val=1);

	virtual std::uint64_t rank1(std::uint64_t pos);
	virtual std::uint64_t select0(std::uint64_t pos);
	};


class OnesBitVector: public ZerosBitVector
	{
public:
	OnesBitVector(const std::string& filename, const size_t offset=0, size_t numBytes=0);
	OnesBitVector(std::uint64_t numBits=0);
	virtual ~OnesBitVector();

	virtual std::string class_identity() const { return "OnesBitVector"; }
	virtual std::uint32_t compressor() const { return bvcomp_ones; }

	virtual int operator[](std::uint64_t pos) const { return 1; }
	};
#endif // bit_vector_H
