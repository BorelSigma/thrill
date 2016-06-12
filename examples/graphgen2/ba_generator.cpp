
#include "SeedGen.hpp"
#include "Hash.hpp"
#include "BabarasiAlbert.hpp"
#include "DegreeDistribution.hpp"
#include "MapReduce.hpp"

#include <vector>
#include <fstream>

#include <thrill/api/context.hpp>
#include <thrill/api/generate.hpp>
#include <thrill/api/collapse.hpp>
#include <thrill/api/dia.hpp>
#include <thrill/api/size.hpp>
#include <thrill/api/print.hpp>
#include <thrill/api/write_lines.hpp>

#include <thrill/common/cmdline_parser.hpp>
#include <thrill/common/logger.hpp>
#include <thrill/common/string.hpp>


using Edge = GraphGen2::Edge;
using Node = GraphGen2::Node;
using Degree = GraphGen2::Degree;
using DegreeCount = GraphGen2::DegreeCount;

/**
* \brief function runParallel is executed in main and uses class 
* GraphGen2::ParallelDegreeDistribution to calculate the degree distribution
* by using a dia.
* In the end the results are collected by using Gather and printed into a file
**/
template<class Graph>
static void runParallel(thrill::Context& ctx, Graph &graph, std::string output) 
{
    	ctx.enable_consume();
    
	auto edges_dia = Generate(ctx, [graph](const size_t& index){
        	return graph(index);
    	}, graph.number_of_edges()).Collapse();

    	auto degrees = GraphGen2::ParallelDegreeDistribution(edges_dia);
    
    std::cout<<"Vor der Ausgabe"<<std::endl;
    std::vector<DegreeCount> output_vector = degrees.Gather(0);
    
    std::fstream file;
    file.open(output,std::ios::out);
 
    for(DegreeCount dc : output_vector)
	file<<dc.deg<<" "<<dc.count<<std::endl;
}

int main(int argc, char* argv[])
{
	thrill::common::CmdlineParser clp;

	uint32_t graph = 0;

    	clp.AddUInt('g', "graph", graph,
                "0 : Path, 1 : Clique, 2 : Babarasi<Path>, 3 : Babarasi<Clique>");

	GraphGen2::Node s = 0;
	GraphGen2::Node n = 0;
	GraphGen2::Degree d = 0;

	clp.AddBytes('s', "seed_nodes",s,"number of nodes in seed graph");
	clp.AddBytes('n', "maximum_nodes",n,"number of nodes in babarasi albert graph");
	clp.AddBytes('d', "degree",d,"degree of each new node in babarasi albert graph");

    	std::string output;
    	clp.AddString('o', "output", output,
                  "filename for png output ");


    	if (!clp.Process(argc, argv)) 
	{
        	return -1;
    	}

    	clp.PrintResult();

	switch(graph)
	{
		case 0:
		{
			GraphGen2::PathGraph path(s);
	
			auto start_func =
        			[&](thrill::Context& ctx) {
            			runParallel(ctx, path, output);
        		};

			return thrill::Run(start_func);
			break;
		}
		case 1:
		{
			GraphGen2::CliqueGraph clique(s);
			auto start_func =
        			[&](thrill::Context& ctx) {
            			runParallel(ctx, clique, output);
        		};

			return thrill::Run(start_func);
			break;
		}
		case 2:
		{
			GraphGen2::BAHash h;
			GraphGen2::PathGraph path(s);
			GraphGen2::BAGraph<GraphGen2::PathGraph,GraphGen2::BAHash> BA(n,d,path,h);
			auto start_func =
        			[&](thrill::Context& ctx) {
            			runParallel(ctx, BA, output);
        		};

			return thrill::Run(start_func);
			break;
		}
		case 3:
		{
			GraphGen2::BAHash h;
			GraphGen2::CliqueGraph clique(s);
			GraphGen2::BAGraph<GraphGen2::CliqueGraph,GraphGen2::BAHash> BA(n,d,clique,h);
			auto start_func =
        			[&](thrill::Context& ctx) {
            			runParallel(ctx, BA, output);
        		};

			return thrill::Run(start_func);
			break;
		}
		default:
		{
			std::cout<<"Unknown Graph input"<<std::endl;
			return 1;
		}
	}

    return 0;    
}


