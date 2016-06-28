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

	struct Values
	{
		bool value0;
		bool value1;
		bool isHelper;
	};
	struct EdgeValuePair 
	{
		Edge e;
		Values v;

		friend std::ostream &operator<<(std::ostream &os, const EdgeValuePair &edge) 
		{
			return os << "( (" << edge.e.first << ", " << edge.e.second << "), (" << edge.v.value0 << ", " << edge.v.value1 << "), "<<edge.v.isHelper<<")";
		}
	} THRILL_ATTRIBUTE_PACKED;

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


	static void runParallel(thrill::Context& ctx, Edge edges[], std::string output) 
	{
		ctx.enable_consume();

		auto edgeValue = Generate(ctx, 
			[edges](const size_t& index)
			{
				EdgeValuePair evp;
				evp.e = edges[index];
				
				unsigned int random_number_0 = rand()%100000;
				unsigned int random_number_1 = rand()%100000;

				evp.v.value0 = (random_number_0 >= random_number_1);	
				evp.v.value1 = (random_number_0 < random_number_1);
				
				evp.v.isHelper = false;
			
				return evp;
			}, 9).Collapse();

		/*auto output_edgeValue = edgeValue.Map(
			[](const EdgeValuePair& evp)
			{
				std::stringstream ss;
				ss<<evp.e.first;
				std::stringstream ss2;
				ss2<<evp.e.second;
				std::stringstream ss3;
				ss3<<evp.v.value0;
				std::stringstream ss4;
				ss4<<evp.v.value1;
				std::stringstream ss5;
				ss5<<evp.v.isHelper;
				std::string output = "((" + ss.str() + ", "+ss2.str()+"), ("+ss3.str()+", "+ss4.str()+"), "+ss5.str()+")";
				return output;
			});	
		
		output_edgeValue.WriteLines("debug_logs/0-PlainEdgeValuePairs.txt");
*/
		auto edgeValueWithHelper = edgeValue.template FlatMap<EdgeValuePair>(
			[](const EdgeValuePair &edge, auto emit) 
			{
            			emit(EdgeValuePair{edge.e, edge.v});
            			emit(EdgeValuePair{Edge(edge.e.second,edge.e.first), Values{edge.v.value1, edge.v.value0, true}});
        		}).Collapse();

/*		auto output_edgeValueHelper = edgeValueWithHelper.Map(
			[](const EdgeValuePair& evp)
			{
				std::stringstream ss;
				ss<<evp.e.first;
				std::stringstream ss2;
				ss2<<evp.e.second;
				std::stringstream ss3;
				ss3<<evp.v.value0;
				std::stringstream ss4;
				ss4<<evp.v.value1;
				std::stringstream ss5;
				ss5<<evp.v.isHelper;
				std::string output = "((" + ss.str() + ", "+ss2.str()+"), ("+ss3.str()+", "+ss4.str()+"), "+ss5.str()+")";
				return output;
			});	
		
		output_edgeValueHelper.WriteLines("debug_logs/1-EdgeValueHelper.txt");
*/
		auto nodeValueWithHelper = edgeValueWithHelper.template FlatMap<NodeValuePair>(
			[](const EdgeValuePair &edge, auto emit) 
			{
            			emit(NodeValuePair{edge.e.first, NodeValues{edge.e.second,edge.v.value0,edge.v.value1,edge.v.isHelper}});
            			//emit(NodeValuePair{edge.e.second, NodeValues{edge.e.first, edge.v.value1, edge.v.value0, true}});
        		}).Collapse();
/*
		auto output_nodeValueHelper = nodeValueWithHelper.Map(
			[](const NodeValuePair& evp)
			{
				std::stringstream ss;
				ss<<evp.n;
				std::stringstream ss2;
				ss2<<evp.v.dest;
				std::stringstream ss3;
				ss3<<(int)evp.v.value0;
				std::stringstream ss4;
				ss4<<(int)evp.v.value1;
				std::stringstream ss5;
				ss5<<(int)evp.v.isHelper;
				std::string output = "(" + ss.str() + ", ("+ss2.str()+", "+ss3.str()+", "+ss4.str()+", "+ss5.str()+"))";
				return output;
			});	
		
		output_nodeValueHelper.WriteLines("debug_logs/2-NodeValueHelper.txt");
*/
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
            				return NodeValuePair{a.n, NodeValues{a.v.dest, a.v.value0 & b.v.value1, false, false}};
				}
				else
				{
            				return NodeValuePair{b.n, NodeValues{b.v.dest, a.v.value0 & b.v.value1, false, false}};
				}
        		}).Collapse();

		//std::cout<<"Size IdS Raw: "<<independentSetRaw.Size()<<std::endl;
/*
		auto output_independentSetRaw = independentSetRaw.Map(
			[](const NodeValuePair& evp)
			{
				uint32_t value0 = evp.v.value0;
				uint32_t value1 = evp.v.value1;
				uint32_t isHelper = evp.v.isHelper;
				std::stringstream ss;
				ss<<evp.n;
				std::stringstream ss2;
				ss2<<evp.v.dest;
				std::stringstream ss3;
				ss3<<value0;
				std::stringstream ss4;
				ss4<<value1;
				std::stringstream ss5;
				ss5<<isHelper;
				std::string output = "(" + ss.str() + ", ("+ss2.str()+", "+ss3.str()+", "+ss4.str()+", "+ss5.str()+"))";
				return output;
			}).Collapse();	
		
		output_independentSetRaw.WriteLines("debug_logs/3-RawIndependentSet.txt");
*/
		auto independentSet = independentSetRaw.Filter(
			[](const NodeValuePair &node)
			{
				return ((!node.v.isHelper)&(node.v.value0));
			});

		/*auto independentSetEdges = independentSet.Map(
			[](const NodeValuePair& evp) -> Edge
			{
				return Edge(evp.n,evp.v.dest);
			}).Collapse();
		*/
		auto output_independentSet = independentSet.Map(
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

		output_independentSet.WriteLines("debug_logs/4-IndependentSet.txt");
	}		
}
