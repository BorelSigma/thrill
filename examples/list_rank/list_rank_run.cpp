
#include <examples/graphgen2/SeedGen.hpp>

#include <fstream>
#include <examples/list_rank/mapReduce.cpp>
#include <thrill/api/context.hpp>
#include <thrill/api/generate.hpp>
#include <thrill/api/collapse.hpp>
#include <thrill/api/dia.hpp>
#include <thrill/api/size.hpp>
#include <thrill/api/print.hpp>
#include <thrill/api/write_lines.hpp>


#include <sstream>

using Edge = GraphGen2::Edge;
	int main(int argc, char* argv[])
	{

		Edge edges [9] = {
			Edge(0,1),
			Edge(1,2),
			Edge(2,3),
			Edge(3,4),
			Edge(4,5),
			Edge(5,6),
			Edge(6,7),
			Edge(7,8),
			Edge(8,9)
		};
		
		std::string output = "test.txt";
		auto start_func =
		[&](thrill::Context& ctx) {
			ListRank2::runParallel(ctx, edges, output);
		};

		return thrill::Run(start_func);
	}