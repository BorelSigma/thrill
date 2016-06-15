
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
namespace ListRank2 {
	using namespace thrill;

	using Edge = GraphGen2::Edge;
	using Node = GraphGen2::Node;
	using Rank = uint64_t;



	/**
	 * 
	 * rank is computet as follows: 
	 */
	struct ListElm {
		Node elm_index; /// index of this element
		Node prev_elm; /// index of element which is needed for rank computation. If value is 0 -> rank is computed. 
		Rank elm_rank;	/// current computed rank. Might depend on other element's rank (if prev_elm == 0)
		bool isHelper; /// indicates whether this element is only helper element in reduce step

		friend std::ostream &operator<<(std::ostream &os, const ListElm &a) {
			return os << '(' << a.elm_index << '|' << a.prev_elm << '|' << a.elm_rank << '|' << a.isHelper << ')';
		}
	} THRILL_ATTRIBUTE_PACKED;

	bool rankIsComputed(ListElm &e){
		return e.prev_elm == 0;
	}

    template<typename InputStack>
	auto computeStep(DIA <ListElm, InputStack> &input_list){
		return input_list.template FlatMap<ListElm>([](const ListElm &elm, auto emit){
			emit(elm);
			if(elm.elm_index != 0){
				emit(ListElm{elm.prev_elm, elm.elm_index, elm.elm_rank, true});
			}
		})
		.ReduceByKey(
            [](const ListElm &in) -> Node {
                return in.elm_index;
            },
            [](const ListElm &a, const ListElm &b) -> ListElm {
            	ListElm helper = a.isHelper ? a : b;
            	ListElm original = a.isHelper ? b : a;
            	if(rankIsComputed(helper)){
            		//elm has already its rank computed
            		return ListElm{helper.prev_elm, 0, helper.elm_rank, false};
            	}else if(rankIsComputed(original)){
            		//previous elm need for rank has already its rank computed
				 	return ListElm{helper.prev_elm, 0, original.elm_rank + helper.elm_rank, false};
            	}else{
            		//neither this elm nor previous elm needed for rank have their ranks computed
            		return ListElm{helper.prev_elm, original.prev_elm, helper.elm_rank +1, false};
            	}
            }).Collapse();
	}


    template<typename InputStack>
	void computeStep2(DIA <ListElm, InputStack> &input_list){
		auto dia = input_list.template FlatMap<ListElm>([](const ListElm &elm, auto emit){
			emit(elm);
			if(elm.elm_index != 0){
				emit(ListElm{elm.prev_elm, elm.elm_index, elm.elm_rank, true});
			}
		})
		.template GroupByKey<ListElm>(
            [](const ListElm &in){return in.elm_index;},
            [all = std::vector < ListElm > ()](auto &iter, const ListElm&) mutable{
            	if(iter.HasNext()){
            		ListElm elm = iter.Next();
            		all.push_back(elm);
            	}
            	return all;
            }).Cache();
	}


    template<typename InputStack>
	void prepareDia(DIA <Edge, InputStack> &input_edges){

		auto list_dia = input_edges.template FlatMap<ListElm>([](const Edge &edge, auto emit) {
			if(edge.first == 0){
				/// 0 is head of path!
				emit(ListElm{edge.first,0,0,false});
			}
			emit(ListElm{edge.second, edge.first, 1, false});
			});
		std::cout << "size " << list_dia.Size() ;
		
		computeStep2(list_dia);
		//dia2.Print("elm");
		//auto dia3 = computeStep(dia2);
		//dia3.Print("ListElm");
		//auto dia3 = computeStep(dia2);
	}

		static void runParallel(thrill::Context& ctx, Edge edges[], std::string output) 
		{
			ctx.enable_consume();

			auto edges_dia = Generate(ctx, [edges](const size_t& index){
				return edges[index];
			}, 9);

			prepareDia(edges_dia);
		
		}
}
