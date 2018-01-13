#ifndef bloom_tree_H
#define bloom_tree_H

#include <string>
#include <vector>
#include <iostream>

#include "bloom_filter.h"
#include "query.h"

//----------
//
// classes in this module--
//
//----------

class BloomTree
	{
public:
	BloomTree(const std::string& name="", const std::string& bfFilename="");
	BloomTree(BloomTree* tree);
	virtual ~BloomTree();

	virtual void preload();
	virtual void load();
	virtual void save();
	virtual void unloadable();

	virtual void relay_debug_settings();

	virtual void add_child(BloomTree* offspring);
	virtual void disown_children();
	virtual size_t num_children() { return children.size(); }
	virtual bool is_dummy() const { return isDummy; }
	virtual bool is_leaf() const { return isLeaf; }
	virtual BloomTree* child(size_t childNum);
	virtual BloomFilter* real_filter();

	virtual void pre_order (std::vector<BloomTree*>& order);
	virtual void post_order (std::vector<BloomTree*>& order);
	virtual void leaves (std::vector<BloomTree*>& order);

	virtual void print_topology(std::ostream& out, int level=0) const;
	virtual void construct_union_nodes ();
	virtual void construct_allsome_nodes ();
	virtual void construct_determined_nodes ();
	virtual void construct_determined_brief_nodes ();
	virtual void construct_intersection_nodes ();

	virtual int lookup (const std::uint64_t pos) const;
	virtual void batch_query (std::vector<Query*> queries, double queryThreshold,
	                          bool isLeafOnly=false, bool distinctKmers=false);
private:
	virtual void batch_query (std::uint64_t activeQueries, std::vector<Query*> queries);
	virtual void query_matches_leaves (Query* q);

public:
	virtual void batch_count_kmer_hits (std::vector<Query*> queries, double queryThreshold,
	                                    bool isLeafOnly=false, bool distinctKmers=false);
private:
	virtual void batch_count_kmer_hits (std::vector<Query*> queries);

public:
	bool isDummy;						// a dummy has no filter; the root might
										// .. be a dummy, to allow for forests
	std::string name;
	std::string bfFilename;
	BloomFilter* bf;
	bool isLeaf;
	BloomTree* parent;
	std::vector<BloomTree*> children;	// this will either be empty or have size
										// .. at least 2 (never size 1)

public:
	bool reportLoad = false;
	bool reportSave = false;
	static bool trackMemory;
	static bool reportUnload;
	static int  dbgTraversalCounter;

public:
	bool dbgTraversal           = false;
	bool dbgSortKmerPositions   = false;
	bool dbgKmerPositions       = false;
	bool dbgKmerPositionsByHash = false;
	bool dbgLookups             = false;
	bool dbgInhibitChildUpdate  = false;
	bool dbgAdjustPosList       = false;
	bool dbgRankSelectLookup    = false;

public:
	static BloomTree* read_topology(const std::string& filename, bool onlyLeaves=false);
	};

#endif // bloom_tree_H
