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

    	int graph = 0;
    	clp.AddInt('g', "graph", graph,
                "0 : Path, 1 : Clique, 2 : Babarasi<Path>, 3 : Babarasi<Clique>");

	int s = 0;
	int ni = 0;
	int di = 0;
	clp.AddInt('s', "seed_nodes",s,"number of nodes in seed graph");
	clp.AddInt('n', "maximum_nodes",ni,"number of nodes in babarasi albert graph");
	clp.AddInt('d', "degree",di,"degree of each new node in babarasi albert graph");

	//n0 = s;
	//n = ni;
	//d = di;

	//std::cout<<"N0: "<<n0<<" n: "<<n<<" d: "<<d<<std::endl;

    	std::string output;
    	clp.AddString('o', "output", output,
                  "filename for png output ");


    	if (!clp.Process(argc, argv)) {
        	return -1;
    	}

    	clp.PrintResult();


	std::cout<<"Before anything!"<<std::endl;

	std::string title;

	//FNVHash h2;
	//CRC32Hash h3;

	std::cout<<"Before switch!"<<std::endl;

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
			GraphGen2::BAGraph<GraphGen2::PathGraph,GraphGen2::BAHash> BA(ni,di,path,h);
			GraphGen2::DegreeDistribution<GraphGen2::BAGraph<GraphGen2::PathGraph,GraphGen2::BAHash>> DD(BA);
			title = "BabarasiAlbert Seed=Path";
			break;
		}
		case 3:
		{
			std::cout<<"BAGraph with CliqueGraph called!"<<std::endl;
			GraphGen2::CliqueGraph clique(s);
			GraphGen2::BAHash h;
			GraphGen2::BAGraph<GraphGen2::CliqueGraph,GraphGen2::BAHash> BA2(ni,di,clique,h);
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

	std::string gnuplot_call = "gnuplot -p -e \"set terminal png size 640,480; set output'"+output+"'; set title'"+title+"';set logscale xy;set xlabel'Vertex No';set ylabel'Degree';plot 'degrees.txt';\" ";
	system(gnuplot_call.c_str());
	return 0;
}
