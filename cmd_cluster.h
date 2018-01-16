#ifndef cmd_cluster_H
#define cmd_cluster_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "bit_vector.h"
#include "commands.h"

class BinaryTree
	{
public:
	BinaryTree(const std::uint32_t _nodeNum, std::uint64_t* _bits,
	           BinaryTree* child0=nullptr, BinaryTree* child1=nullptr)
	  :	nodeNum(_nodeNum),
		bits(_bits)
		{
		children[0] = child0;
		children[1] = child1;
		height = 1;
		if (child0 != nullptr) height = 1 + child0->height;
		if (child1 != nullptr) height = std::max(height,1+child1->height);
		}

	virtual ~BinaryTree()
		{
		if (children[0] != nullptr) delete children[0];
		if (children[1] != nullptr) delete children[1];
		if (trackMemory)
			std::cerr << "@-" << this << " discarding BinaryTree node" << std::endl;
		}

	std::uint32_t nodeNum;
	std::uint32_t height;
	std::uint64_t* bits;
	BinaryTree* children[2];
	bool trackMemory;
	};


class ClusterCommand: public Command
	{
public:
	static const std::uint64_t defaultEndPosition = 100*1000;

public:
	ClusterCommand(const std::string& name): Command(name),treeRoot(nullptr) {}
	virtual ~ClusterCommand();
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);
	virtual void find_leaf_vectors (void);
	virtual void cluster_greedily (void);
	virtual void print_topology (std::ostream& out, BinaryTree* root, int level);
	virtual void dump_bits (std::ostream& out, const std::uint64_t* bits);

	std::string listFilename;
	std::string treeFilename;
	std::string nodeTemplate;
	std::uint64_t startPosition;	// origin-zero, half-open
	std::uint64_t endPosition;
	bool inhibitBuild;
	bool trackMemory;

	std::vector<BitVector*> leafVectors;
	BinaryTree* treeRoot;
	};

#endif // cmd_cluster_H
