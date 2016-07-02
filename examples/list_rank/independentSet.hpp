#pragma once
#include <examples/graphgen2/SeedGen.hpp>
#include <sstream>
#include <thrill/api/context.hpp>
#include <thrill/api/gather.hpp>
#include <thrill/api/generate.hpp>
#include <thrill/api/collapse.hpp>
#include <thrill/api/dia.hpp>
#include <thrill/api/size.hpp>
#include <thrill/api/min.hpp>
#include <thrill/api/print.hpp>
#include <thrill/api/write_lines.hpp>
#include <thrill/api/reduce_by_key.hpp>
#include <thrill/api/concat.hpp>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include <ctime>


#define MAX_RAND_VALUE 60000

namespace ListRank2 
{
	using namespace thrill;

	using Edge = GraphGen2::Edge;
	using Node = GraphGen2::Node;

	struct NodeValues
	{
		Node dest;
		bool value0;
		bool value1;
		uint32_t edgeLength;
		bool isHelper;
	};
	struct NodeValuePair 
	{
		Node n;
		NodeValues v;

		friend std::ostream &operator<<(std::ostream &os, const NodeValuePair &node) 
		{
			return os << "(" << node.n << ", (" << node.v.dest << ", " << node.v.value0 << ", " << node.v.value1 << ", "<<node.v.edgeLength<<", "<<node.v.isHelper<<"))";
		}
	} THRILL_ATTRIBUTE_PACKED;

	DIA<NodeValuePair> findIndependentSet(const DIA<NodeValuePair> nodeValue)
	{
		
		DIA<NodeValuePair> nodeValueWithHelper = nodeValue.template FlatMap<NodeValuePair>(
			[](const NodeValuePair &node, auto emit) 
			{
				uint32_t random_number_0 = rand()%MAX_RAND_VALUE;
				bool value0 = (random_number_0 >= MAX_RAND_VALUE/2);	
				if(node.n == 0) value0 = 0;
				bool value1 = !value0;

				emit(NodeValuePair{node.n, NodeValues{node.v.dest,value0,value1, node.v.edgeLength, false}});
				emit(NodeValuePair{node.v.dest, NodeValues{node.n,value1,value0, node.v.edgeLength, true}});
        		}).Collapse();

		
		DIA<NodeValuePair> independentSetRaw = nodeValueWithHelper.ReduceByKey(
			[](const NodeValuePair& in) -> Node 
			{
            			return in.n;
        		},
        		[](const NodeValuePair& a, const NodeValuePair& b) -> NodeValuePair 
			{
				if(b.v.isHelper)
				{
            				return NodeValuePair{a.n, NodeValues{a.v.dest, a.v.value0 & b.v.value0, false, a.v.edgeLength, false}};
				}
				else
				{
            				return NodeValuePair{b.n, NodeValues{b.v.dest, b.v.value0 & a.v.value0, false,  b.v.edgeLength,false}};
				}
        		}).Collapse();

		DIA<NodeValuePair> independentSet = independentSetRaw.Filter(
			[](const NodeValuePair &node)
			{
				return ((!node.v.isHelper)&((node.v.value0)&(node.n != 0)));
			});

		return independentSet;
	}

	auto changePointer(auto v, auto is)
	{
		auto isTmp = is.Map([](const NodeValuePair &in)
		{
			return NodeValuePair{in.v.dest, NodeValues{in.n, false, false, in.v.edgeLength, true}};
		});
		auto vTmp = v.Map([](const NodeValuePair &in)
				{
					return NodeValuePair{
						in.n, 
						NodeValues{
							in.v.dest,
							false, 
							false, 
							in.v.edgeLength,
							false}};
				});
		auto concatTmp = vTmp.Concat(isTmp);
		auto result = concatTmp.ReduceByKey(
		[](const NodeValuePair &in){return in.v.dest;},
		[](const NodeValuePair &a, const NodeValuePair &b){
			if(a.v.isHelper){
				return NodeValuePair{
					b.n,
					NodeValues{
						a.n,
						false,
						false,
						a.v.edgeLength+ b.v.edgeLength,
						false} 				
				};
			}else{
				return NodeValuePair{
					a.n,
					NodeValues{
						b.n,
						false,
						false,
						a.v.edgeLength+ b.v.edgeLength,
						false} 				
				};
			}
		});

		return result;	
	}

	DIA<NodeValuePair> copy(DIA<NodeValuePair> &nodes)
	{
		return nodes.Map([](const NodeValuePair &node){return node;});
	}

	/*uint32_t sizeOf(DIA<NodeValuePair> &nodes)
	{
		return copy(nodes).Size();
	}*/

	

	//template<typename InputStack, typename InputStack2>
	DIA<NodeValuePair> getComplement(DIA <NodeValuePair>& nodeList, DIA <NodeValuePair>& independentSet)
	{
		DIA<NodeValuePair> concatDia = nodeList.Concat(independentSet);

		DIA<NodeValuePair> withoutDoubles = concatDia.ReduceByKey(
			[](const NodeValuePair& in) -> Node {return in.n; },
			[](const NodeValuePair& a, const NodeValuePair& b) -> NodeValuePair
			{
				if(a.n == b.n)
					return NodeValuePair{99999, NodeValues{99999,false,false,0,true}};
			});

		DIA<NodeValuePair> complement = withoutDoubles.Filter([](const NodeValuePair& in)
		{
			//return !( (in.n == 0) & (in.v.dest == 0) );
			return !(in.v.dest == 99999);		
		});

		return complement;
	}

	auto fuse(auto V, auto IS)
	{
		
		auto concatTmp = V.Concat(IS);

		auto reduced = concatTmp.ReduceByKey(
			[](const NodeValuePair& node) -> Node {return node.v.dest;}, 
			[](const NodeValuePair& a, const NodeValuePair& b)-> NodeValuePair
			{
				if(a.n == 0){
					return NodeValuePair{0, NodeValues{b.n,false, false,a.v.edgeLength-b.v.edgeLength,false}};
				} 
			});
	}

	//template<typename InputStack>
	static void runParallel(thrill::Context& ctx, std::vector<Edge> edges, std::string output) 
	{
		ctx.enable_consume();

		auto nodeValue = Generate(ctx, 
			[edges](const size_t& index)
			{
				NodeValuePair evp;
				evp.n = edges[index].first;
				evp.v.dest = edges[index].second;
				/*
				unsigned int random_number_0 = rand()%MAX_RAND_VALUE;
				evp.v.value0 = (random_number_0 >= MAX_RAND_VALUE/2);	
				evp.v.value1 = !evp.v.value0;
*/
				evp.v.value0 = false;
				evp.v.value1 = false;
				evp.v.edgeLength = 1;
				
				evp.v.isHelper = false;
			
				return evp;
			}, edges.size()).Collapse();

		std::vector<DIA<NodeValuePair>> independentSets;
		std::vector<DIA<NodeValuePair>> nodes;

		DIA<NodeValuePair> IS = findIndependentSet(nodeValue);
		DIA<NodeValuePair> V  = getComplement(nodeValue,IS);
		DIA<NodeValuePair> newV = changePointer(V,IS).Keep(1);

		//auto newV = changePointer(V,IS);
		

		//newV.KeepForever();
		//auto sizeOutputCopyOfIS = copy(IS); 
		uint32_t sizeV = newV.Size();

		
		independentSets.push_back(IS);
		nodes.push_back(newV);

		uint32_t i = 0;
		
		while(sizeV > 5)
		{
			if(ctx.my_rank() == 0)
				std::cout<<"TESTESTESTESTEST Size: "<<sizeV<<std::endl;
				
			DIA<NodeValuePair> IS2 = findIndependentSet(nodes[i]);
			DIA<NodeValuePair> V2  = getComplement(nodes[i],IS2);
			DIA<NodeValuePair> newV2 = changePointer(V2,IS2).Keep(1);

			sizeV = newV2.Size();
			//sizeV = sizeOf(newV2);
			//sizeV-=10;

			independentSets.push_back(IS2);
			nodes.push_back(newV2);
			i++;
		}

		auto output_independentSet = nodes[i].Map(
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
				ss5<<e.v.edgeLength;
				std::stringstream ss6;
				ss6<<e.v.isHelper;
				std::string output = "(" + ss.str() + ", "+ss2.str()+", "+ss3.str()+", "+ss4.str()+", "+ss5.str()+", "+ss6.str()+")";
				return output;
			});	

		output_independentSet.WriteLines("debug_logs/IndependentSet.txt");
	}
}
