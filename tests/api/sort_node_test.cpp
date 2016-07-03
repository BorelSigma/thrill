/*******************************************************************************
 * tests/api/sort_node_test.cpp
 *
 * Part of Project Thrill - http://project-thrill.org
 *
 * Copyright (C) 2015 Alexander Noe <aleexnoe@gmail.com>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <thrill/api/all_gather.hpp>
#include <thrill/api/generate.hpp>
#include <thrill/api/read_binary.hpp>
#include <thrill/api/sort.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace thrill; // NOLINT

TEST(Sort, SortKnownIntegers) {

    static constexpr size_t test_size = 6000000u;

    auto start_func =
        [](Context& ctx) {

            auto integers = Generate(
                ctx,
                [](const size_t& index) -> size_t {
                    return test_size - index - 1;
                },
                test_size);

            auto sorted = integers.Sort();

            std::vector<size_t> out_vec = sorted.AllGather();

            ASSERT_EQ(test_size, out_vec.size());
            for (size_t i = 0; i < out_vec.size(); i++) {
                ASSERT_EQ(i, out_vec[i]);
            }
        };

    // set fixed amount of RAM for testing
    api::MemoryConfig mem_config;
    mem_config.setup(128 * 1024 * 1024llu);

    api::RunLocalMock(mem_config, 2, 1, start_func);
}

TEST(Sort, SortRandomIntegers) {

    auto start_func =
        [](Context& ctx) {

            std::default_random_engine generator(std::random_device { } ());
            std::uniform_int_distribution<int> distribution(0, 10000);

            auto integers = Generate(
                ctx,
                [&distribution, &generator](const size_t&) -> int {
                    return distribution(generator);
                },
                1000000);

            auto sorted = integers.Sort();

            std::vector<int> out_vec = sorted.AllGather();

            for (size_t i = 0; i < out_vec.size() - 1; i++) {
                ASSERT_FALSE(out_vec[i + 1] < out_vec[i]);
            }

            ASSERT_EQ(1000000u, out_vec.size());
        };

    api::RunLocalTests(start_func);
}

TEST(Sort, SortRandomIntegersCustomCompareFunction) {

    auto start_func =
        [](Context& ctx) {

            std::default_random_engine generator(std::random_device { } ());
            std::uniform_int_distribution<int> distribution(1, 10000);

            auto integers = Generate(
                ctx,
                [&distribution, &generator](const size_t&) -> int {
                    return distribution(generator);
                },
                10000);

            auto compare_fn = [](int in1, int in2) {
                                  return in1 > in2;
                              };

            auto sorted = integers.Sort(compare_fn);

            std::vector<int> out_vec = sorted.AllGather();

            for (size_t i = 0; i < out_vec.size() - 1; i++) {
                ASSERT_FALSE(out_vec[i + 1] > out_vec[i]);
            }

            ASSERT_EQ(10000u, out_vec.size());
        };

    api::RunLocalTests(start_func);
}

struct IntIntStruct {
    int a, b;

    friend std::ostream& operator << (std::ostream& os, const IntIntStruct& c) {
        return os << '(' << c.a << ',' << c.b << ')';
    }
};

TEST(Sort, SortRandomIntIntStructs) {

    auto start_func =
        [](Context& ctx) {

            std::default_random_engine generator(std::random_device { } ());
            std::uniform_int_distribution<int> distribution(1, 10);

            auto integers = Generate(
                ctx,
                [&distribution, &generator](const size_t&) -> IntIntStruct {
                    return IntIntStruct {
                        distribution(generator), distribution(generator)
                    };
                },
                10000);

            auto cmp_fn = [](const IntIntStruct& in1, const IntIntStruct& in2) {
                              if (in1.a != in2.a) {
                                  return in1.a < in2.a;
                              }
                              else {
                                  return in1.b < in2.b;
                              }
                          };

            auto sorted = integers.Sort(cmp_fn);

            std::vector<IntIntStruct> out_vec = sorted.AllGather();

            for (size_t i = 0; i < out_vec.size() - 1; i++) {
                ASSERT_FALSE(out_vec[i + 1].a < out_vec[i].a);
            }

            ASSERT_EQ(10000u, out_vec.size());
        };

    api::RunLocalTests(start_func);
}

TEST(Sort, SortZeros) {

    auto start_func =
        [](Context& ctx) {

            auto integers = Generate(
                ctx,
                [](const size_t&) -> size_t {
                    return 1;
                },
                10000);

            auto sorted = integers.Sort();

            std::vector<size_t> out_vec = sorted.AllGather();

            ASSERT_EQ(10000u, out_vec.size());

            for (size_t i = 0; i < out_vec.size(); i++) {
                ASSERT_EQ(1u, out_vec[i]);
            }
        };

    api::RunLocalTests(start_func);
}

TEST(Sort, SortZeroToThree) {

    auto start_func =
        [](Context& ctx) {

            auto integers = Generate(
                ctx,
                [](const size_t& index) -> size_t {
                    return index % 4;
                },
                10000);

            auto sorted = integers.Sort();

            std::vector<size_t> out_vec = sorted.AllGather();

            ASSERT_EQ(10000u, out_vec.size());

            for (size_t i = 0; i < out_vec.size(); i++) {
                ASSERT_EQ(i * 4 / out_vec.size(), out_vec[i]);
            }
        };

    api::RunLocalTests(start_func);
}

TEST(Sort, SortWithEmptyWorkers) {

    auto start_func =
        [](Context& ctx) {

            std::string in = "inputs/compressed-0-0.gzip";
            auto integers = api::ReadBinary<size_t>(ctx, in);

            auto sorted = integers.Sort();

            std::vector<size_t> out_vec = sorted.AllGather();

            size_t prev = 0;
            for (size_t i = 0; i < out_vec.size() - 1; i++) {
                ASSERT_TRUE(out_vec[i] >= prev);
                prev = out_vec[i];
            }

            ASSERT_EQ(10000u, out_vec.size());
        };

    api::RunLocalTests(start_func);
}

TEST(Sort, SortOneInteger) {

    auto start_func =
        [](Context& ctx) {

            LOG0 << "Sorting with " << ctx.num_workers() << " workers";

            auto integers = Generate(ctx, 1);

            auto sorted = integers.Sort();

            std::vector<size_t> out_vec;
            sorted.AllGather(&out_vec);

            for (size_t i = 0; i < out_vec.size() - 1; i++) {
                ASSERT_EQ(i, out_vec[i]);
            }

            ASSERT_EQ(1u, out_vec.size());
        };

    api::RunLocalTests(start_func);
}

TEST(Sort, SortZeroIntegers) {

    auto start_func =
        [](Context& ctx) {

            auto integers = Generate(ctx, 0);

            auto sorted = integers.Sort();

            std::vector<size_t> out_vec;
            sorted.AllGather(&out_vec);

            ASSERT_EQ(0u, out_vec.size());
        };

    api::RunLocalTests(start_func);
}

/******************************************************************************/
