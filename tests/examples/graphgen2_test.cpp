#include <examples/graphgen2/SeedGen.hpp>
#include <examples/graphgen2/BabarasiAlbert.hpp>

#include <thrill/api/all_gather.hpp>
#include <thrill/api/equal_to_dia.hpp>

#include <gtest/gtest.h>

using namespace thrill;

/**
 * \test Testing the path graph
 */
TEST(PathGraph, PathGraphTest) {

    auto start_func =
            []() {
                GraphGen2::PathGraph p(100);

                for (uint32_t i = 0; i < 99; i++)
                    ASSERT_EQ(p(i), GraphGen2::Edge(i, i + 1));
            };

    start_func();
}

/**
 * \test Testing the clique graph
* First test if c_3 is a 3 clique
* then test if c_4 is exactly c_3 + edges (i,3) with i=0,1,2
* so that c_4 is also a clique
* By this it is proven that c_n is a clique of size nodes=n if
* c_n-1 is a clique of size nodes=n-1 ...
* because it is already known that c_3 is a clique all c_n are 
* also cliques
 */
TEST(CliqueGraph, CliqueGraphTest) {

    auto start_func =
            []() {
		// 
                GraphGen2::CliqueGraph c_3(3);
		ASSERT_EQ(GraphGen2::Edge(0,1),c_3(0));
		ASSERT_EQ(GraphGen2::Edge(0,2),c_3(1));
		ASSERT_EQ(GraphGen2::Edge(1,2),c_3(2));

		GraphGen2::CliqueGraph c_4(4);
		ASSERT_EQ(c_4(0),c_3(0));
		ASSERT_EQ(c_4(1),c_3(1));
		ASSERT_EQ(c_4(2),c_3(2));
		ASSERT_EQ(GraphGen2::Edge(0,3),c_4(3));
		ASSERT_EQ(GraphGen2::Edge(1,3),c_4(4));
		ASSERT_EQ(GraphGen2::Edge(2,3),c_4(5));
		ASSERT_EQ(c_4.number_of_edges(), 6);
				
		
            };

    start_func();
}

/**
 * \class BATestHash
 * \brief Helper hash function used for simplifying BA graph generator tests
 */
class BATestHash {
public:
    BATestHash() { };

    uint64_t operator()(const uint64_t &value) const {
        return value - 1;
    }
};

/**
 * \class BATestHash2
 * \brief Helper hash function used for simplifying BA graph generator tests
 */
class BATestHash2 {
public:
    BATestHash2() { };

    uint64_t operator()(const uint64_t &value) const {
        return value / 2;
    }
};


/**
 * \test testing the Barabasi-Albert graph generator by using a path graph as seed, a degree of 1 for each new node and a simple test hash function
*  New version also tests BA(15) for testing multi-hashing and hashing to seed nodes
 */
TEST(BAGraph, BAGraphTest) {

    auto start_func =
            []() {
                BATestHash h;
                GraphGen2::PathGraph p(10);
                GraphGen2::BAGraph<GraphGen2::PathGraph, BATestHash> BA(100, 1, p, h);

                uint32_t i = 0;
                for (; i < p.number_of_edges(); i++)
                    ASSERT_EQ(BA(i), GraphGen2::Edge(i, i + 1));
                for (; i < BA.number_of_edges(); i++)
                    ASSERT_EQ(BA(i), GraphGen2::Edge(i + 1, i + 1));

		BATestHash2 h2;
                GraphGen2::PathGraph p2(5);
                GraphGen2::BAGraph<GraphGen2::PathGraph, BATestHash2> BA2(100, 1, p2, h2);

                i = 0;
                for (; i < p2.number_of_edges(); i++)
                    ASSERT_EQ(BA2(i), GraphGen2::Edge(i, i + 1));

		ASSERT_EQ(BA2(4),GraphGen2::Edge(5,2));
		ASSERT_EQ(BA2(15),GraphGen2::Edge(16,4));
            };

    start_func();
}

/**
 * \test Testing whether the Barabasi-Albert Hash function hashes to a lower value for the first 1e8 natural numbers.
 */
TEST(BAHash, BAHashTest) {

    auto start_func =
            []() {
                GraphGen2::BAHash h;

                for (uint32_t i = 10; i < 1e8; i++) {
                    ASSERT_LT(h(i), i);
                }
            };

    start_func();
}
