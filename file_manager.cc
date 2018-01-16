// file_manager.cc-- manage associations between bloom filters and files,
//                   including which filers are resident in memory

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>
#include <unordered_map>

#include "utilities.h"
#include "bloom_filter.h"
#include "bloom_tree.h"
#include "file_manager.h"

using std::string;
using std::vector;
using std::pair;
using std::cerr;
using std::endl;
#define u32 std::uint32_t

//----------
//
// FileManager--
//
//----------

// $$$ add trackMemory to hash constructor/destructor

FileManager::FileManager
   (BloomTree* root)
	  :	modelBf(nullptr)
	{
	// scan the tree, setting up hashes from (a) node name to a node's filter
	// filename and from (b) filter filename to names of nodes within; we also
	// verify that the node names are distinct

	std::unordered_map<string,string> nameToFile;

	vector<BloomTree*> order;
	root->post_order(order);
	for (const auto& node : order)
		{
		if (nameToFile.count(node->name) > 0)
			fatal ("error: tree contains more than one node named \""
			     + node->name 
			     + " (in \"" + node->bfFilename + "\""
			     + " and \"" + nameToFile[node->name] + "\")");

		nameToFile[node->name] = node->bfFilename;
		nameToNode[node->name] = node;

		if (filenameToNames.count(node->bfFilename) == 0)
			{
			// !!! $$$ this looks like a memory leak
			filenameToNames[node->bfFilename] = new vector<string>;
			}
		filenameToNames[node->bfFilename]->emplace_back(node->name);
		}

	// preload the content headers for every node, file-by-file; this has
	// two side effects -- (1) the bloom filter properties are checked for
	// consistency, and (2) we are installed as every bloom filter's manager

	// øøø change this, don't do the consistency check (it's a separate command)

	for (auto iter : filenameToNames) 
		{
		string filename = iter.first;
		preload_content (filename);
		}
	}

FileManager::~FileManager()
	{
	for (auto iter : filenameToNames) 
		{
		vector<string>* names = iter.second;
		delete names;
		}
	}

bool FileManager::already_preloaded
   (const string&	filename)
	{
	bool someNotPreloaded   = false;
	bool someArePreloaded   = false;
	string nonPreloadedName = "";
	string preloadedName    = "";

	vector<string>* nodeNames = filenameToNames[filename];
	for (const auto& nodeName : *nodeNames)
		{
		BloomTree* node = nameToNode[nodeName];
		if ((node->bf != nullptr) and (node->bf->ready))
			{
			if (someNotPreloaded)
				fatal ("internal error: attempt to preload content from \"" + filename + "\""
				     + "; \"" + nodeName + "\" was already preloaded"
				     + " but \"" + nonPreloadedName + "\" wasn't");
			someArePreloaded = true;
			preloadedName = nodeName;
			}
		else
			{
			if (someArePreloaded)
				fatal ("internal error: attempt to preload content from \"" + filename + "\""
				     + "; \"" + preloadedName + "\" was already preloaded"
				     + " but \"" + nodeName + "\" wasn't");
			someNotPreloaded = true;
			nonPreloadedName = nodeName;
			}
		}

	return someArePreloaded;
	}

std::ifstream* FileManager::preload_content
   (const string&	filename,
	bool			leaveFileOpen)
	{
	if (filenameToNames.count(filename) == 0)
		fatal ("internal error: attempt to preload content from"
		       " unknown file \"" + filename + "\"");

	if (already_preloaded(filename)) return nullptr;

	// $$$ add trackMemory for in
	std::ifstream* in = new std::ifstream(filename, std::ios::binary | std::ios::in);

	//std::ifstream in (filename, std::ios::binary | std::ios::in);
	if (not *in)
		fatal ("error: FileManager::preload_content()"
			   " failed to open \"" + filename + "\"");
	vector<pair<string,BloomFilter*>> content
		= BloomFilter::identify_content(*in,filename);

	vector<string>* nodeNames = filenameToNames[filename];
	for (const auto& templatePair : content)
		{
		string       bfName     = templatePair.first;
		BloomFilter* templateBf = templatePair.second;
		if (not contains (*nodeNames, bfName))
			fatal ("error: \"" + filename + "\""
			     + " contains the bloom filter \"" + bfName + "\""
			     + ", in conflict with the tree's topology");

		BloomTree* node = nameToNode[bfName];

		// copy the template into the node's filter

		if (node->bf == nullptr)
			{
			node->bf = BloomFilter::bloom_filter(templateBf);
			node->bf->manager = this;
			}
		else
			node->bf->copy_properties(templateBf);

		node->bf->steal_bits(templateBf);
		delete templateBf;

		// make sure all bloom filters in the tree are consistent

		if (modelBf == nullptr)
			modelBf = node->bf;
		else
			node->bf->is_consistent_with (modelBf, /*beFatal*/ true);
		}

	// if we're supposed to leave the file open, return the file to the caller
	// (the caller is expected to close and delete it); otherwise, clean up the
	// file and return null

	if (leaveFileOpen)
		return in;

	// $$$ add trackMemory for in
	in->close();
	delete in;
	return nullptr;
	}

void FileManager::load_content
   (const string&	filename)
	{
	// ……… when we implement a heap, and dirty bits, we'll need to empty the heap here

	if (filenameToNames.count(filename) == 0)
		fatal ("internal error: attempt to load content from"
		       " unknown file \"" + filename + "\"");

//øøø
//if already_preloaded, open the file and seek to the first component
//otherwise, preload_content(leaveFileOpen)
//
//bf->load() needs to be able to receive a stream
//
//in turn, bitvector->load() probably needs to be able to receive a stream and
//avoid doing a seek
//
//øøø
//though this loads everything in order, we really need to make sure that the
//components are in the same order as they are in the file, and that the sizes
//add up so that no seeks are necessary
//
//but where should that check be performed?
//if we do that check in identify_content ...

	if (not already_preloaded(filename))
		preload_content (filename);

	vector<string>* nodeNames = filenameToNames[filename];
	for (const auto& nodeName : *nodeNames)
		{
		BloomTree* node = nameToNode[nodeName];
		node->bf->reportLoad = reportLoad;
		node->bf->load(/*bypassManager*/ true);
		}

	}

