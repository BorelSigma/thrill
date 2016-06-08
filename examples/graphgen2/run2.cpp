
#include "SeedGen.hpp"
#include "Hash.hpp"
#include "BabarasiAlbert.hpp"
#include "DegreeDistribution.hpp"
#include "MapReduce.hpp"
#include <thrill/api/context.hpp>
#include <thrill/api/generate.hpp>
#include <thrill/api/collapse.hpp>
#include <thrill/api/dia.hpp>
#include <thrill/api/size.hpp>
#include <thrill/api/print.hpp>

#include <thrill/common/cmdline_parser.hpp>
#include <thrill/common/logger.hpp>
#include <thrill/common/string.hpp>


using Edge = GraphGen2::Edge;
using Node = GraphGen2::Node;
using Degree = GraphGen2::Degree;

static void runParallel(
        thrill::Context& ctx, uint32_t graph, Node seed_nodes, Node n, Degree d, std::string output) {
    ctx.enable_consume();

	
	GraphGen2::BAHash h;
	GraphGen2::CliqueGraph clique(seed_nodes);
	GraphGen2::BAGraph<GraphGen2::CliqueGraph,GraphGen2::BAHash> BA(n,d,clique,h);

    auto edges_dia = Generate(ctx, [BA](const size_t& index){
        return BA(index);
    }, BA.number_of_edges()).Collapse();
    auto degrees = GraphGen2::MapEdges(edges_dia);
    //auto distribution = GraphGen2::ParallelDegreeDistribution(degrees);
    degrees.Print(" ");
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


	auto start_func =
        [&](thrill::Context& ctx) {
            runParallel(ctx, graph, s, n, d, output);
        };

	thrill::Run(start_func);

    return 0;    
}


