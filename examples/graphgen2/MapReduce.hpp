#pragma once

#include <thrill/api/context.hpp>
#include <thrill/api/dia.hpp>
#include <thrill/api/size.hpp>
#include <thrill/api/reduce_by_key.hpp>

#include <sstream>

namespace GraphGen2 {
    using namespace thrill;

    /**
     * \brief Wrapper for Node Degree tuple (node, deg) with ostream support
     */
    struct NodeCount {
        Node node;
        Degree deg;

        friend std::ostream &operator<<(std::ostream &os, const NodeCount &a) {
            return os << '(' << a.node << '|' << a.deg << ')';
        }
    } THRILL_ATTRIBUTE_PACKED;

    /**
     * \brief Wrapper for Degree Count tuple (degree, count) with ostream support
     */
    struct DegreeCount {
        Degree deg;
        Degree count;

        friend std::ostream &operator<<(std::ostream &os, const DegreeCount &a) {
            return os << '(' << a.deg << '|' << a.count << ')';
        }
    } THRILL_ATTRIBUTE_PACKED;

    template<typename InputStack>
    auto ParallelDegreeDistribution(DIA <Edge, InputStack> &input_edges) {
        /**
         * DIA containing edges are expanded to DIA containing Node Count tuples, (node1, node2) becomes (node1, 1), (node2,1)
         */
        auto nodes_dia = input_edges.template FlatMap<NodeCount>([](const Edge &edge, auto emit) {
            emit(NodeCount{edge.first, 1});
            emit(NodeCount{edge.second, 1});
        });

        /**
         * reduce by key and sum count of same node: (node1,1)(node1,1) becomes (node1, 2)
         */
        auto reduced = nodes_dia.ReduceByKey(
                [](const NodeCount &in) -> Node {
                    return in.node;
                },
                [](const NodeCount &a, const NodeCount &b) -> NodeCount {
                    /* associative reduction operator: add counters */
                    return NodeCount{a.node, a.deg + b.deg};
                });

        /**
         * convert DIA containing Node Counts (node,total_count/degree) to DIA containing Degree Count (deg,1).
         * (node1, 3)(node2,2)(node3,3) -> (3,1)(2,1)(3,1)
         */
        auto nodes_dia2 = reduced.template FlatMap<DegreeCount>([](const NodeCount &degree, auto emit) {
            emit(DegreeCount{degree.deg, 1});
        });

	 /**
         * return reduces DIA containing total count of each degree. (deg1, 1)(deg1,1) -> (deg1, 2)
         */
	return nodes_dia2.ReduceByKey(
        [](const DegreeCount& in) -> Node {
            return in.deg;
        },
        [](const DegreeCount& a, const DegreeCount& b) -> DegreeCount {
            return DegreeCount{a.deg, a.count + b.count};
        }).Map([](const DegreeCount& dc){
		std::stringstream ss;
		ss<<dc.deg;
		std::stringstream ss2;
		ss2<<dc.count;
		std::string output = ss.str() + " "+ss2.str();
		return output;
	});
    }
}
