#pragma once

#include <stdint.h>
#include <utility>
#include <cmath>
#include <cassert>

namespace GraphGen2
{

using Node = uint64_t;
using EdgeId = uint64_t;
using Edge = std::pair<Node, Node>;
using Degree = Node;

class PathGraph
{
public:
	PathGraph(Node nodes):n_(nodes),m_(nodes-1){};
	Edge operator()(const EdgeId &eid) const 
	{
		assert(eid < m_);
		return Edge(eid,eid+1);
	};
	Node number_of_nodes() const{return n_;};
	EdgeId number_of_edges() const{return m_;};
	//std::string getName() const{return name;};

private:
	const Node n_;		// number of nodes
	const EdgeId m_;		// numer of edges
};


class CliqueGraph
{
public:
	CliqueGraph(Node nodes): n_(nodes),m_((nodes*(nodes-1))/2){};

	Edge operator()(const EdgeId &eid) const 
	{	
		assert(eid < m_);
		EdgeId v = 0.5 + sqrt(2*eid + 0.25);
		EdgeId u = eid - (v*(v-1))/2;
		return Edge(u,v);
	}

	Node number_of_nodes() const{return n_;};
	EdgeId number_of_edges() const{return m_;};
	//std::string getName() const{return name;};
private:
	const Node n_;		// number of nodes
	const EdgeId m_;		// number of edges
};

}
