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
            [](Context &ctx) {
                GraphGen2::PathGraph p(100);

                for (uint32_t i = 0; i < 99; i++)
                    ASSERT_EQ(p(i), GraphGen2::Edge(i, i + 1));
            };

    api::RunLocalTests(start_func);
}

/**
 * \test Testing the clique graph
 */
TEST(CliqueGraph, CliqueGraphTest) {

    auto start_func =
            [](Context &ctx) {
                GraphGen2::CliqueGraph c(100);
                std::vector<GraphGen2::Degree> degrees(100, 0);

                for (uint32_t i = 0; i < c.number_of_edges(); i++) {
                    degrees[c(i).first]++;
                    degrees[c(i).second]++;
                }

                for (uint32_t i = 0; i < 100; i++)
                    ASSERT_EQ(degrees[i], 99);
            };

    api::RunLocalTests(start_func);
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
 * \test testing the Barabasi-Albert graph generator by using a path graph as seed, a degree of 1 for each new node and a simple test hash function
 */
TEST(BAGraph, BAGraphTest) {

    auto start_func =
            [](Context &ctx) {
                BATestHash h;
                GraphGen2::PathGraph p(10);
                GraphGen2::BAGraph<GraphGen2::PathGraph, BATestHash> BA(100, 1, p, h);

                uint32_t i = 0;
                for (; i < p.number_of_edges(); i++)
                    ASSERT_EQ(BA(i), GraphGen2::Edge(i, i + 1));
                for (; i < BA.number_of_edges(); i++)
                    ASSERT_EQ(BA(i), GraphGen2::Edge(i + 1, i + 1));
            };

    api::RunLocalTests(start_func);
}

/**
 * \test Testing whether the Barabasi-Albert Hash function hashes to a lower value for the first 1e8 natural numbers.
 */
TEST(BAHash, BAHashTest) {

    auto start_func =
            [](Context &ctx) {
                GraphGen2::BAHash h;

                for (uint32_t i = 10; i < 1e8; i++) {
                    ASSERT_LT(h(i), i);
                }
            };

    api::RunLocalTests(start_func);
}
