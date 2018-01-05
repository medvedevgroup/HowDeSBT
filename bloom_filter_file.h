#ifndef bloom_filter_file_H
#define bloom_filter_file_H

#include <cstdint>

//----------
//
// File header for bloom filter files
//
//----------
//
// Implementation Notes
//	[1]	The file consists of a header followed by a stream of data defining
//		one or more bloom filters.
//	[2]	The header contains three parts: (a) the properties of the bloom
//		filters (all filters in the file have the same properties), (b) an
//		array describing of 'components' (the bloom filters), and (c) an
//		array of text for component names of the (see note 3).
//	[3]	Component names are expected to be something different than filenames.
//		These names are used by the file manager to determine which tree nodes
//		can be loaded from the file.  Note that if the file only contains one
//		component, we anticipate that no name will be included.
//	[4]	We require the bit data to be in the same order as the components and
//		back-to-back within the file (i.e. no empty space). This is necessary
//		so that the data can be read from the file without having to do seeks.
//
//----------

// record for each bit vector in the file

struct bfvectorinfo
	{
    std::uint32_t	compressor;	// [x+00] compressor identifier for the bit
    							//        .. vector; the least significant byte
    							//        .. is one of bvcomp_xxx; some
    							//        .. compressor types define additional
    							//        .. information in the other bytes, as
    							//        .. folows:
    							//        ..   bvcomp_rrr: the second byte is
    							//        ..               the RRR chunk size
	std::uint32_t	name;		// [x+04] offset (from start of file) to the
    							//        .. name of of this bitvector; the name
    							//        .. is a zero-terminated string; if
    							//        .. this field is zero the bitvector
    							//        .. has no name; note that the offset,
    							//        .. if non-zero, is expected to be
    							//        .. after bffileheader.info[]
	std::uint64_t	offset;		// [x+08] offset (from start of file) to data
    							//        .. for the bit vector
	std::uint64_t	numBytes;	// [x+10] number of bytes of data occupied by
    							//        .. the bit vector's data
	std::uint64_t	filterInfo;	// [x+18] filter-dependent info for this bit
    							//        .. vector; typically zero
	};

enum
	{
	bvcomp_unknown      = 0,
	bvcomp_uncompressed = 1,
	bvcomp_zeros        = 2,	// all zeros; no bit data stored in the file
	bvcomp_ones         = 3,	// all ones; no bit data stored in the file
	bvcomp_rrr          = 4,
	bvcomp_roar         = 5
	};

// header record

struct bffileprefix // convenience type, matching the beginning of bffileheader;
	{				// .. sizeof(bffileprefix) is required to be a multiple of
					// .. 8 bytes
	std::uint64_t	magic;
	std::uint32_t	headerSize;
	std::uint32_t	version;
	};

struct bffileheader
	{
	std::uint64_t	magic;		// [00] (bffileheaderMagic)
	std::uint32_t	headerSize;	// [08] number of bytes in the header record;
								//      .. this can be more than
								//      .. sizeof(bffileheader), since it
								//      .. includes, e.g., all entries in the
								//      .. info[] array, and bfvectorinfo.name
								//      .. characters (if any)
	std::uint32_t	version;	// [0C] file format version (1)
    std::uint32_t	bfKind;		// [10] (one of bfkind_xxx) identifier for the
    							//      .. type of bloom filter
	std::uint32_t	padding1;	// [14] (expected to be 0)
	std::uint32_t	kmerSize;	// [18]
	std::uint32_t	numHashes;	// [1C]
	std::uint64_t	hashSeed1;	// [20]
	std::uint64_t	hashSeed2;	// [28]
	std::uint64_t	hashModulus;// [30]
	std::uint64_t	numBits;	// [38]
	std::uint32_t	numVectors;	// [40]
	std::uint32_t	padding2;	// [44] (expected to be 0)
	std::uint32_t	padding3;	// [48] (expected to be 0)
	std::uint32_t	padding4;	// [4C] (expected to be 0)
    bfvectorinfo	info[1];	// [50] (array with numVectors entries)
    // after info[], characters for the bfvectorinfo.name fields 
	};

#define bffileheader_size(numVectors) (sizeof(bffileheader) + ((numVectors)-1)*sizeof(bfvectorinfo))

const std::uint64_t bffileheaderMagic   = 0xD532006662544253; // little-endian ascii "SBTbf" plus some extra bits
const std::uint64_t bffileheaderMagicUn = 0xCD96AD692C96649A; // (used for header written to an unfinished file)
const std::uint64_t bffileheaderVersion = 1;

enum
	{
	bfkind_simple           = 1,
	bfkind_allsome          = 2,
	bfkind_determined       = 3,
	bfkind_determined_brief = 4,
	bfkind_intersection     = 0xFFFFFF00
	// nota bene: when new bloom filter types are added here, they should also
	// .. be added in BloomFilter::filter_kind_to_string()
	};
	

#endif // bloom_filter_file_H
