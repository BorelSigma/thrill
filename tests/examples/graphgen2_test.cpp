#include <examples/graphgen2/SeedGen.hpp>
#include <examples/graphgen2/BabarasiAlbert.hpp>

#include <thrill/api/all_gather.hpp>
#include <thrill/api/equal_to_dia.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

using namespace thrill;
//using namespace GraphGen2;

TEST(PathGraph, PathGraphTest) {

    auto start_func =
        [](Context& ctx) {
	    GraphGen2::PathGraph p(5);
            ASSERT_EQ(GraphGen2::Edge(0,1), p(0));
            ASSERT_EQ(GraphGen2::Edge(1,2), p(1));
        };

    api::RunLocalTests(start_func);
}

TEST(CliqueGraph, CliqueGraphTest) {

    auto start_func =
        [](Context& ctx) {
	    GraphGen2::CliqueGraph c(5);
            ASSERT_EQ(c.number_of_edges(), 10);
            ASSERT_EQ(GraphGen2::Edge(2,3), c(5));
        };

    api::RunLocalTests(start_func);
}

TEST(BAGraph, BAGraphTest) {

    auto start_func =
        [](Context& ctx) {
	    GraphGen2::BAHash h;
	    GraphGen2::CliqueGraph c(5);
	    GraphGen2::BAGraph<GraphGen2::CliqueGraph,GraphGen2::BAHash> BA(10,2,c,h);

            ASSERT_EQ(BA.number_of_edges(), 20);
            ASSERT_EQ(GraphGen2::Edge(2,3), BA(5));
            ASSERT_EQ(5, BA(10).first);
        };

    api::RunLocalTests(start_func);
}

TEST(DegreeDistribution, DDTest) {

    auto start_func =
        [](Context& ctx) {
		// There is nothing here yet
		// Still need a few tests for degree distribution
            ASSERT_EQ(1, 1);
        };

    api::RunLocalTests(start_func);
}
