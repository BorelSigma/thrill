#pragma once
#include <sstream>
#include <thrill/api/dia.hpp>
#include <thrill/api/write_lines.hpp>
#include <string>
#include <examples/graphgen2/SeedGen.hpp>

namespace ListRank2 
{
	using namespace thrill;

	using Edge = GraphGen2::Edge;
	using Node = GraphGen2::Node;

	struct NodeValues
	{
		Node dest;
		bool value0;
		bool value1;
		uint32_t edgeLength;
		bool isHelper;
	};
	struct NodeValuePair 
	{
		Node n;
		NodeValues v;

		friend std::ostream &operator<<(std::ostream &os, const NodeValuePair &node) 
		{
			return os << "(" << node.n << ", (" << node.v.dest << ", " << node.v.value0 << ", " << node.v.value1 << ", "<<node.v.edgeLength<<", "<<node.v.isHelper<<"))";
		}
	} THRILL_ATTRIBUTE_PACKED;

	void printDia(auto &dia, std::string path){
		dia.Map(
			[](const NodeValuePair& e)	-> std::string
			{
				std::stringstream ss;
				ss<<e.n;
				std::stringstream ss2;
				ss2<<e.v.dest;
				std::stringstream ss3;
				ss3<<e.v.value0;
				std::stringstream ss4;
				ss4<<e.v.value1;
				std::stringstream ss5;
				ss5<<e.v.edgeLength;
				std::stringstream ss6;
				ss6<<e.v.isHelper;
				std::string output = "(" + ss.str() + ", "+ss2.str()+", "+ss3.str()+", "+ss4.str()+", "+ss5.str()+", "+ss6.str()+")";
				return output;
		}).WriteLines(path);
	}

}