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
#include <thrill/api/sort.hpp>
#include <thrill/api/cache.hpp>
#include <thrill/api/write_lines.hpp>
#include <thrill/api/reduce_by_key.hpp>
#include <thrill/api/concat.hpp>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include <ctime>


#define MAX_RAND_VALUE 10000


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

	void printDia(auto &dia, std::string path){
		dia.Map(
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
			}).WriteLines(path);
	}


	DIA<NodeValuePair> findIndependentSet(const DIA<NodeValuePair> nodeValue)
	{
		
		return nodeValue.template FlatMap<NodeValuePair>(
			[](const NodeValuePair &node, auto emit) 
			{
				uint32_t random_number_0 = rand()%MAX_RAND_VALUE;
				bool value0 = (random_number_0 >= MAX_RAND_VALUE/2);	
				if(node.n == 0) value0 = 0;
				bool value1 = !value0;

				emit(NodeValuePair{node.n, NodeValues{node.v.dest,value0,value1, node.v.edgeLength, false}});
				emit(NodeValuePair{node.v.dest, NodeValues{node.n,value1,value0, node.v.edgeLength, true}});
			}).ReduceByKey(
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
			}).Filter(
			[](const NodeValuePair &node)
			{
				return ((!node.v.isHelper)&((node.v.value0)&(node.n != 0)));
			});
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

		return concatTmp.ReduceByKey(
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
	}

	DIA<NodeValuePair> copy(DIA<NodeValuePair> &nodes)
	{
		return nodes.Map([](const NodeValuePair &node){return node;});
	}
	
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
			return !(in.v.dest == 99999);		
		});

		return complement;
	}

	auto fuse(DIA<NodeValuePair>& V, DIA<NodeValuePair>& IS)
	{
		std::cout << "fuse called" << std::endl;
		auto concatTmp = V.Concat(IS).Cache().Keep(1);
		auto tmp1 = concatTmp
		.ReduceByKey(
			[](const NodeValuePair& node) -> Node {return node.v.dest;}, 
			[](const NodeValuePair& a, const NodeValuePair& b)-> NodeValuePair
			{
				if(a.n == 0){
					return NodeValuePair{0, NodeValues{b.n,false, false,a.v.edgeLength-b.v.edgeLength,true}}; //set isHelper to true for filtering doubles in next step
				}else{
					return NodeValuePair{0, NodeValues{a.n,false, false,b.v.edgeLength-a.v.edgeLength,true}}; //set isHelper to true for filtering doubles in next step
				}
			}).Collapse().Keep(1);
		auto tmp2 = tmp1
		.Filter([](const NodeValuePair &in){ return in.v.isHelper;}).Collapse().Keep(1);
		auto tmp3 = tmp2
		.Map([](const NodeValuePair &in){return NodeValuePair{in.n, NodeValues{in.v.dest, false, false, in.v.edgeLength, false}};}).Collapse().Keep(1);
		auto tmp4 = tmp3
		.Concat(V).Keep(1);
		return tmp4.Collapse().Keep(1);
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
				evp.v.value0 = false;
				evp.v.value1 = false;
				evp.v.edgeLength = 1;
				evp.v.isHelper = false;

				return evp;
			}, edges.size());

		std::vector<DIA<NodeValuePair>> independentSets;
		std::vector<DIA<NodeValuePair>> nodes;

		DIA<NodeValuePair> IS = findIndependentSet(nodeValue);
		DIA<NodeValuePair> vComplement  = getComplement(nodeValue,IS);
		DIA<NodeValuePair> newV = changePointer(vComplement,IS).Keep(1);

		uint32_t sizeV = newV.Size();

		
		independentSets.push_back(IS.Keep(1));
		nodes.push_back(newV);
		
		uint32_t i = 0;
		while(sizeV > 1)
		{
			if(ctx.my_rank() == 0)
				std::cout<<"TESTESTESTESTEST Size: "<<sizeV<<std::endl;

			DIA<NodeValuePair> IS2 = findIndependentSet(nodes[i]).Cache().Keep(1).Keep(1).Keep(1).Keep(1);
			DIA<NodeValuePair> V2  = getComplement(nodes[i],IS2);
			DIA<NodeValuePair> newV2 = changePointer(V2,IS2).Keep(1);

			sizeV = newV2.Size();
			independentSets.push_back(IS2.Keep(1).Keep(1).Keep(1).Keep(1));
			nodes.push_back(newV2);
			i++;
		}

		std::vector<DIA<NodeValuePair>> result;
		result.push_back(fuse(nodes.back().Keep(1).Keep(1).Keep(1), independentSets[i]).Cache().Keep(1).Keep(1).Keep(1).Keep(1).Keep(1));
		i--;
		while(i>0){
			if(ctx.my_rank() == 0)
				std::cout << "i: " <<  i << std::endl;
			result.push_back(fuse(result.back().Keep(1).Keep(1).Keep(1), independentSets[i].Keep(1).Keep(1).Keep(1)).Keep(1).Keep(1).Keep(1).Keep(1).Keep(1).Collapse());
			i--;
		}
		result.back().Keep(1).Sort([](const NodeValuePair &a, const NodeValuePair &b) -> bool{return a.v.dest >= b.v.dest;}).Print("DIA");
	}
}
