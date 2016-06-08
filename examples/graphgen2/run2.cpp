
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


using Edge = GraphGen2::Edge;

static void runParallel(
        thrill::Context& ctx,
        GraphGen2::BAGraph<GraphGen2::CliqueGraph,GraphGen2::BAHash> graph) {
    ctx.enable_consume();
    auto edges_dia = Generate(ctx, [graph](const size_t& index){
        return graph(index);
    }, 100).Collapse();
    GraphGen2::MapEdges(edges_dia).Print("node | degree");

}

int main(int argc, char* argv[])
{
	GraphGen2::BAHash h;
	auto s = 5;
	GraphGen2::CliqueGraph clique(s);
	GraphGen2::BAGraph<GraphGen2::CliqueGraph,GraphGen2::BAHash> BA2(100,3,clique,h);

	auto start_func =
        [&](thrill::Context& ctx) {
            runParallel(ctx, BA2);
        };

	thrill::Run(start_func);

    return 0;    
}


