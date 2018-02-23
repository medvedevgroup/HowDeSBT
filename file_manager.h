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

	virtual bool already_preloaded (const std::string& filename);
	virtual void preload_content   (const std::string& filename);
	virtual void load_content      (const std::string& filename);

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

public:
	static bool reportOpenClose;
	static std::ifstream* open_file  (const std::string& filename,
	                                  std::ios_base::openmode mode=std::ios::in);
	static void           close_file (std::ifstream* in=nullptr, bool really=false);

public:
	static std::string    openedFilename;
	static std::ifstream* openedFile;
	};

#endif // file_manager_H
