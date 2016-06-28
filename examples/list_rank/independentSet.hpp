#pragma once
#include <examples/graphgen2/SeedGen.hpp>
#include <sstream>
#include <thrill/api/context.hpp>
#include <thrill/api/generate.hpp>
#include <thrill/api/collapse.hpp>
#include <thrill/api/dia.hpp>
#include <thrill/api/size.hpp>
#include <thrill/api/print.hpp>
#include <thrill/api/write_lines.hpp>
#include <thrill/api/reduce_by_key.hpp>
#include <thrill/api/concat.hpp>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include <ctime>

namespace ListRank2 
{
	using namespace thrill;

	using Edge = GraphGen2::Edge;
	using Node = GraphGen2::Node;
	using Rank = uint64_t;

	struct NodeValues
	{
		Node dest;
		bool value0;
		bool value1;
		bool isHelper;
	};
	struct NodeValuePair 
	{
		Node n;
		NodeValues v;

		friend std::ostream &operator<<(std::ostream &os, const NodeValuePair &node) 
		{
			return os << "(" << node.n << ", (" << node.v.dest << ", " << node.v.value0 << ", " << node.v.value1 << "), "<<node.v.isHelper<<"))";
		}
	} THRILL_ATTRIBUTE_PACKED;

	auto findIndependentSet(auto nodeValue)
	{
		
		auto nodeValueWithHelper = nodeValue.template FlatMap<NodeValuePair>(
			[](const NodeValuePair &node, auto emit) 
			{
            			emit(NodeValuePair{node.n, node.v});
            			emit(NodeValuePair{node.v.dest, NodeValues{node.n,node.v.value1, node.v.value0, true}});
        		}).Collapse();

		
		auto independentSetRaw = nodeValueWithHelper.ReduceByKey(
			[](const NodeValuePair& in) -> Node 
			{
            			return in.n;
        		},
        		[](const NodeValuePair& a, const NodeValuePair& b) -> NodeValuePair 
			{
				//return NodeValuePair{0,NodeValues{0,false,false,false}};
				if(b.v.isHelper)
				{
            				return NodeValuePair{a.n, NodeValues{a.v.dest, a.v.value0 & b.v.value0, false, false}};
				}
				else
				{
            				return NodeValuePair{b.n, NodeValues{b.v.dest, b.v.value0 & a.v.value0, false, false}};
				}
        		}).Collapse();

		auto independentSet = independentSetRaw.Filter(
			[](const NodeValuePair &node)
			{
				return ((!node.v.isHelper)&((node.v.value0)&(node.n != 0)));
			});

		return independentSet;
	}

	auto getComplement(auto nodeList, auto independentSet)
	{
		auto concatDia = nodeList.Concat(independentSet);

		auto withoutDoubles = concatDia.ReduceByKey(
			[](const NodeValuePair& in) -> Node {return in.n; },
			[](const NodeValuePair& a, const NodeValuePair& b) -> NodeValuePair
			{
				if(a.n == b.n)
					return NodeValuePair{0, NodeValues{0,0,0,0}};
			});

		auto complement = withoutDoubles.Filter([](const NodeValuePair& in)
		{
			return !( (in.n == 0) && (in.v.dest == 0) );
		});

		auto output_independentSet = complement.Map(
			[](const NodeValuePair& e)	-> std::string
			{
				std::stringstream ss;
				ss<<e.n;
				std::stringstream ss2;
				ss2<<e.v.dest;
				std::stringstream ss3;
				ss3<<e.v.value0;
				std::stringstream ss4;
				ss4<<e.v.value1;
				std::stringstream ss5;
				ss5<<e.v.isHelper;
				std::string output = "(" + ss.str() + ", "+ss2.str()+", "+ss3.str()+", "+ss4.str()+", "+ss5.str()+")";
				return output;
			});	

		output_independentSet.WriteLines("debug_logs/IndependentSet.txt");
	}

	static void runParallel(thrill::Context& ctx, std::vector<Edge> edges, std::string output) 
	{
		ctx.enable_consume();

		auto nodeValue = Generate(ctx, 
			[edges](const size_t& index)
			{
				NodeValuePair evp;
				evp.n = edges[index].first;
				evp.v.dest = edges[index].second;
				
				unsigned int random_number_0 = rand()%100000;
				unsigned int random_number_1 = rand()%100000;

				evp.v.value0 = (random_number_0 >= random_number_1);	
				evp.v.value1 = (random_number_0 < random_number_1);
				
				evp.v.isHelper = false;
			
				return evp;
			}, edges.size()).Collapse();

		auto IS = findIndependentSet(nodeValue);
		auto V  = getComplement(nodeValue,IS);
	}		
}
