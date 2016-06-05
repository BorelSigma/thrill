#pragma once

#include "SeedGen.hpp"
#include "Hash.hpp"

#include <iostream>

namespace GraphGen2
{
template<class SeedGraph, class Hash=BAHash>
class BAGraph
{
public:
	BAGraph(const Node & number_of_nodes,
		const Degree & edges_per_node,
		const SeedGraph & seed_graph,
		const Hash & hash) : n_(number_of_nodes),n0_(seed_graph.number_of_nodes()),m0_(seed_graph.number_of_edges()),d_(edges_per_node),seed_(seed_graph),h_(hash)
	{
	}
	Edge operator()(const EdgeId &eid) const 
	{
		assert(eid < number_of_edges());
		
		if(eid < m0_)
			return seed_(eid);
		
		Node r = h_(2*eid+1);
		
		while(((r & 1) == 1) && (r>=(m0_*2)))
			r = h_(r);
			
		// Falls nicht auf den Seed gehasht wurde
		if(r>=(m0_*2))	
		{
			Edge buffer = Edge(n0_+(eid-m0_)/d_ , n0_+(r-(m0_*2))/(2*d_));
			return buffer;
		}		
		else
		{	
			Edge buffer = seed_(r/2);

			if( (r&1) == 0)
				return Edge(n0_+(eid-m0_)/d_ , buffer.first);
			else
				return Edge(n0_+(eid-m0_)/d_ , buffer.second);
		}
	}

	Node number_of_nodes() const{return n_;};
	EdgeId number_of_edges() const{return m0_+(n_-n0_)*d_;};
	
private:
	const Node n_;
	const Node n0_;
	const EdgeId m0_;

	const Degree d_;
	const SeedGraph &seed_;
	const Hash &h_;
};
}
