#pragma once
#include <thrill/api/context.hpp>
#include <thrill/api/dia.hpp>
#include <thrill/api/size.hpp>
#include <thrill/api/reduce_by_key.hpp>
namespace GraphGen2 {
    using namespace thrill;
    struct NodeCount {
        Node node;
        Degree deg;

        friend std::ostream& operator << (std::ostream& os, const NodeCount& a) {
            return os << '(' << a.node << '|' << a.deg << ')';
        }
    } THRILL_ATTRIBUTE_PACKED;

    template <typename InputStack>
    auto MapEdges (
        DIA<Edge ,InputStack>& input_edges
        ){
    	std::cout << "MapEdges called" << std::endl;
    	std::cout << "input DIA size: " << input_edges.Size() << std::endl;

        auto nodes_dia = input_edges.template FlatMap<NodeCount>([](const Edge& edge, auto emit){
            emit(NodeCount{edge.first, 1});
            emit(NodeCount{edge.second, 1});
        });

        std::cout << "DIA size after flatMap: " << nodes_dia.Size() << std::endl;

        return nodes_dia.ReduceByKey(
        [](const NodeCount& in) -> Node {
            return in.node;
        },
        [](const NodeCount& a, const NodeCount& b) -> NodeCount {
            /* associative reduction operator: add counters */
            return NodeCount{a.node, a.deg + b.deg};
        });
    }


}