#include <examples/graphgen2/SeedGen.hpp>

#include <fstream>
//#include <examples/list_rank/mapReduce.cpp>
#include "independentSet.hpp"
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
	std::vector<Edge> edges;
	
	for(unsigned int i=0;i<200;i++)
		edges.push_back(Edge(i,i+1));	
		
	std::string output = "test.txt";
	auto start_func = [&](thrill::Context& ctx) { ListRank2::runParallel(ctx, edges, output);};

	return thrill::Run(start_func);
}
