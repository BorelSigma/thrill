#include <iostream>

#include "SeedGen.hpp"
#include "Hash.hpp"
#include "BabarasiAlbert.hpp"
#include "DegreeDistribution.hpp"
//#include "Timer.hpp"


#include <thrill/common/cmdline_parser.hpp>
#include <thrill/common/logger.hpp>
#include <thrill/common/string.hpp>

using namespace thrill;

int main(int argc, char* argv[])
{
	common::CmdlineParser clp;

	uint32_t graph = 0;

    	clp.AddUInt('g', "graph", graph,
                "0 : Path, 1 : Clique, 2 : Babarasi<Path>, 3 : Babarasi<Clique>");

	GraphGen2::Node s = 0;
	GraphGen2::Node n = 0;
	GraphGen2::Node d = 0;

	clp.AddBytes('s', "seed_nodes",s,"number of nodes in seed graph");
	clp.AddBytes('n', "maximum_nodes",n,"number of nodes in babarasi albert graph");
	clp.AddBytes('d', "degree",d,"degree of each new node in babarasi albert graph");

    	std::string output;
    	clp.AddString('o', "output", output,
                  "filename for png output ");


    	if (!clp.Process(argc, argv)) {
        	return -1;
    	}

    	clp.PrintResult();

	std::string title;


	switch(graph)
	{
		case 0:
		{
			std::cout<<"PathGraph called!"<<std::endl;
			GraphGen2::PathGraph path(s);
			GraphGen2::DegreeDistribution<GraphGen2::PathGraph> DD(path);
			title = "PathGraph";
			break;
		}		
		case 1:
		{
			std::cout<<"CliqueGraph called!"<<std::endl;
			GraphGen2::CliqueGraph clique(s);
			GraphGen2::DegreeDistribution<GraphGen2::CliqueGraph> DD(clique);
			title = "CliqueGraph";
			break;
		}
		case 2:
		{
			std::cout<<"BAGraph with PathGraph called!"<<std::endl;
			GraphGen2::PathGraph path(s);
			GraphGen2::BAHash h;
			GraphGen2::BAGraph<GraphGen2::PathGraph,GraphGen2::BAHash> BA(n,d,path,h);
			GraphGen2::DegreeDistribution<GraphGen2::BAGraph<GraphGen2::PathGraph,GraphGen2::BAHash>> DD(BA);
			title = "BabarasiAlbert Seed=Path";
			break;
		}
		case 3:
		{
			std::cout<<"BAGraph with CliqueGraph called!"<<std::endl;
			GraphGen2::CliqueGraph clique(s);
			GraphGen2::BAHash h;
			GraphGen2::BAGraph<GraphGen2::CliqueGraph,GraphGen2::BAHash> BA2(n,d,clique,h);
			GraphGen2::DegreeDistribution<GraphGen2::BAGraph<GraphGen2::CliqueGraph,GraphGen2::BAHash>> DD(BA2);
			title = "BabarasiAlbert Seed=CliqueGraph";
			break;
		}
		default:
		{
			std::cout<<"Unknown Parameter for graph!"<<std::endl;
			break;
		}
	}

	std::string gnuplot_call = "gnuplot -e \"set terminal png size 640,480; set output'"+output+"'; set title'"+title+"';set logscale xy;set xlabel'Degree';set ylabel'Vertex No';plot 'degrees.txt';\" ";
	system(gnuplot_call.c_str());
	return 0;
}
