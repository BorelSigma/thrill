#pragma once
#include <examples/graphgen2/SeedGen.hpp>
#include <sstream>
#include <thrill/api/context.hpp>
#include <thrill/api/gather.hpp>
#include <thrill/api/all_gather.hpp>
#include <thrill/api/generate.hpp>
#include <thrill/api/collapse.hpp>
#include <thrill/api/dia.hpp>
#include <thrill/api/equal_to_dia.hpp>
#include <thrill/api/group_by_key.hpp>
#include <thrill/api/group_to_index.hpp>
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
#include "utils.hpp"


#define MAX_RAND_VALUE 10000


namespace ListRank2 
{
	using namespace thrill;

	using Edge = GraphGen2::Edge;
	using Node = GraphGen2::Node;
	using NodeValuePair = ListRank2::NodeValuePair;
	using NodeValues = ListRank2::NodeValues;

	DIA<NodeValuePair> prepareDia(const DIA<NodeValuePair> nodeValue){
		return nodeValue.template FlatMap<NodeValuePair>(
			[](const NodeValuePair &node, auto emit) 
			{
				uint32_t random_number_0 = rand()%MAX_RAND_VALUE;
				bool value0 = (random_number_0 >= MAX_RAND_VALUE/2);	
				if(node.n == 0) value0 = 0;
				bool value1 = !value0;

				emit(NodeValuePair{node.n, NodeValues{node.v.dest,value0,value1, node.v.edgeLength, false}});
				emit(NodeValuePair{node.v.dest, NodeValues{node.n,value1,value0, node.v.edgeLength, true}});
			});
	}



	auto calculateIndependentSet(const DIA<NodeValuePair> preparedDia){
		auto tmp1 =  preparedDia.ReduceByKey(
			[](const NodeValuePair& in) -> Node 
			{
				return in.n;
			},
			[](const NodeValuePair& a, const NodeValuePair& b) -> NodeValuePair 
			{

				if(a.v.isHelper)
				{
					return NodeValuePair{b.n, NodeValues{b.v.dest, a.v.value0 & b.v.value0, false, b.v.edgeLength, false}};
				}
				else
				{
					return NodeValuePair{a.n, NodeValues{a.v.dest, a.v.value0 & b.v.value0, false, a.v.edgeLength, false}};
				}

			}).Filter([](const NodeValuePair& p){return !p.v.isHelper;});

		auto tmp2 = tmp1.FlatMap<NodeValuePair>(
			[](const NodeValuePair &node, auto emit) 
			{

				emit(node);
				if(node.v.value0){
					emit(NodeValuePair{node.v.dest, NodeValues{node.n,false,false, node.v.edgeLength, true}});
				}
			});


		return tmp2.template GroupByKey<NodeValuePair>(
			[](const NodeValuePair &nvp){return nvp.v.dest;},
			[](auto &iter, const auto something){
				std::vector < NodeValuePair > edges;
				while(iter.HasNext()){
					edges.push_back(iter.Next());
				}
				if(edges.size()>1){
					NodeValuePair a = edges[0];
					NodeValuePair b = edges[1];
					if(a.v.isHelper){
						return NodeValuePair{b.n,NodeValues{a.n, false,true,a.v.edgeLength+ b.v.edgeLength,false}};
					}else{
						return NodeValuePair{a.n,NodeValues{b.n,false,true,a.v.edgeLength+ b.v.edgeLength,false}};
					}
				}else{
					return edges.back();
				}
				
			});
	}

	auto fuse(DIA<NodeValuePair>& V, DIA<NodeValuePair>& IS)
	{
		auto concatTmp = V.Concat(IS);
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
			});
		auto tmp2 = tmp1
		.Filter([](const NodeValuePair &in){ return in.v.isHelper;});
		auto tmp3 = tmp2
		.Map([](const NodeValuePair &in){return NodeValuePair{in.n, NodeValues{in.v.dest, false, false, in.v.edgeLength, false}};});
		auto tmp4 = tmp3
		.Concat(V);
		return tmp4;
	}

	auto fuse(thrill::Context& ctx, std::vector<NodeValuePair> &vecV, std::vector<NodeValuePair> &IS)
	{
		IS.insert( IS.end(), vecV.begin(), vecV.end() );

		auto concatTmp = Generate(ctx,[IS](const size_t& index)
		{
			NodeValuePair evp;
			evp.n = IS[index].n;
			evp.v.dest = IS[index].v.dest;
			evp.v.value0 = false;
			evp.v.value1 = false;
			evp.v.edgeLength = IS[index].v.edgeLength;
			evp.v.isHelper = false;
			return evp;
		}, IS.size());

		auto V = EqualToDIA(ctx, vecV);

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
			});
		auto tmp2 = tmp1
		.Filter([](const NodeValuePair &in){ return in.v.isHelper;});
		auto tmp3 = tmp2
		.Map([](const NodeValuePair &in){return NodeValuePair{in.n, NodeValues{in.v.dest, false, false, in.v.edgeLength, false}};});
		return tmp3;
	}

	static void runParallel(thrill::Context& ctx, std::vector<Edge> edges, std::string output) 
	{
		ctx.enable_consume();
		std::vector<std::vector<NodeValuePair>> independentSets;
		std::vector<NodeValuePair> nodes;


		for(auto e : edges)
			nodes.push_back(NodeValuePair{e.first,NodeValues{e.second,false,false,1,false}});

		uint32_t sizeV = nodes.size();
		int32_t i = 0;

		while(sizeV > 1)
		{
			if(ctx.my_rank() == 0)
			{
				std::cout<<"REMAINING SIZE: "<<sizeV<<std::endl;
			}

			auto tmp0 = Generate(ctx,[nodes](const size_t& index){return nodes[index];}, sizeV);
			auto tmp1 = prepareDia(tmp0);
			DIA<NodeValuePair> tmp2 = calculateIndependentSet(tmp1).Keep(1);

			DIA<NodeValuePair> newV  = tmp2.Filter([](const NodeValuePair& p){return !p.v.value0;});
			DIA<NodeValuePair> IS  = tmp2.Filter([](const NodeValuePair& p){return p.v.value0;});
			nodes.clear();
			independentSets.push_back(IS.AllGather());
			nodes = newV.AllGather();
			sizeV = nodes.size();
			i++;
		}
		i--;

		if(ctx.my_rank() == 0) std::cout<<" phase 1 finished!"<<std::endl;


		while(i>=0)
		{
			if(ctx.my_rank() == 0) std::cout << " generate i-th DIA from independent set vector and stack fuse function:" <<  i << std::endl;

			auto tmpFuse = fuse(ctx, nodes, independentSets[i]);
			auto tmp4 = tmpFuse.AllGather();
			nodes.insert(nodes.end(), tmp4.begin(), tmp4.end());
			i--;
		}	

		if(ctx.my_rank() == 0) std::cout << "phase 2 finished! " << std::endl;

		auto result2 = Generate(ctx, 
			[nodes](const size_t& index) -> NodeValuePair
			{
				NodeValuePair evp;
				evp.n = nodes[index].n;
				evp.v.dest = nodes[index].v.dest;
				evp.v.value0 = false;
				evp.v.value1 = false;
				evp.v.edgeLength = nodes[index].v.edgeLength;
				evp.v.isHelper = false;
				return evp;
			}, nodes.size());

		printDia(result2, "debug_logs/v.txt");
	}
}
