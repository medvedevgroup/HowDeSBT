#ifndef file_manager_H
#define file_manager_H

#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>

#include "bloom_filter.h"
#include "bloom_tree.h"

//----------
//
// classes in this module--
//
//----------

struct BloomFilterInfo
	{
	std::string		name;
	std::uint32_t	compressor;	// compressor identifier (one of bvcomp_xxx)
	size_t			offset;		// offset (into file) of bloom filter's data
	size_t			numBytes;	// number of bytes of data; zero means unknown
	};


class FileManager
	{
public:
	FileManager(BloomTree* root);
	virtual ~FileManager();

	virtual bool already_preloaded         (const std::string& filename);
	virtual std::ifstream* preload_content (const std::string& filename, bool leaveFileOpen=false);
	virtual void load_content              (const std::string& filename);

public:
	bool reportLoad = false;

public:
	BloomFilter* modelBf;
	std::unordered_map<std::string,BloomTree*> nameToNode; // hash table
									// .. mapping a node name to the associated
									// .. bloom tree node
	std::unordered_map<std::string,std::vector<std::string>*> filenameToNames;
									// hash table mapping a filename to the
									// .. list of names of nodes to be loaded
									// .. from that file
	};

#endif // file_manager_H
