#pragma once

#include <thrill/common/defines.hpp>
#include "SeedGen.hpp"
#include "Hash.hpp"

#include <iostream>

namespace GraphGen2
{
template<class SeedGraph, class Hash=BAHash>
/**
 * \class BAGraph
 * \brief A graph generator following the Barabasi-Albert model
 *  Starting with a seed graph new nodes are generated by linking them to existing nodes. The linking requires a hash function to find nodes to link to.
 */
class BAGraph
{
public:
	BAGraph(const Node & number_of_nodes /*!< Number of nodes to be geneated. */,
		const Degree & edges_per_node /*!< Degree of each new node while generated the graph */,
		const SeedGraph & seed_graph /*!< Seed graph used for generating the Barabasi-Albert graph. */,
		const Hash & hash /*!< Hash function used for finding existing nodes to link to from newly generated node */)
            : n_(number_of_nodes),
              n0_(seed_graph.number_of_nodes()),
              m0_(seed_graph.number_of_edges()),
              d_(edges_per_node),
              seed_(seed_graph),
              h_(hash)
	{
	}
	Edge operator()(const EdgeId &eid) const 
	{
		assert(eid < number_of_edges());
		
		if(THRILL_UNLIKELY(eid < m0_))
			return seed_(eid);
		
		Node r = h_(2*eid+1);
		
		while(((r & 1) == 1) && (r>=(m0_*2)))
			r = h_(r);
			

		if(THRILL_LIKELY(r>=(m0_*2)))	/// If hash does not point to seed graph
		{
			Edge buffer = Edge(n0_+(eid-m0_)/d_ , n0_+(r-(m0_*2))/(2*d_));
			return buffer;
		}
		else /// If hash does point to a node from seed graph
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
