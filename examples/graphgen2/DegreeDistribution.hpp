#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <fstream>

namespace GraphGen2 {

    /**
     * \class DegreeDistribution
     * \brief Used for calculating the degree distribution of a given graph.
     *
     * Used for calculation the degree distribution of a given graph. The result is stored in a file degrees.txt and can
     * be used for further processing, such as plotting the resulting histogram.
     */
    template<class Graph>
    class DegreeDistribution {
    public:
        DegreeDistribution(const Graph &g  /*!< Graph used for calculating its degree distribution . */) {
            const Node n = g.number_of_nodes();
            std::vector<Node> nodes(n);
            EdgeId m = g.number_of_edges();

            std::cout << "#Nodes=" << n << " #Edges=" << m << std::endl;

            for (EdgeId i = 0; i < m; i++) {
                const Edge buffer = g(i);

                if (buffer.first >= n)
                    std::cerr << " Buffer.First >= n! i = " << i << "  buffer.first = " << buffer.first << "  n = " <<
                    n << std::endl;
                if (buffer.second >= n)
                    std::cerr << " Buffer.Second >= n! i = " << i << "  buffer.second = " << buffer.second <<
                    "  n = " << n << std::endl;
                nodes[buffer.first]++;
                nodes[buffer.second]++;

                if (m >= 1000000) {
                    if (i % (m / 100) == 0)
                        std::cout << "Process: " << i * (100.0 / m) << " %" << std::endl;
                }
            }

            /// Inital guess for max_degress
            Degree max_degree = nodes[0];

            if (n > 1000) {
                for (Node i = 1; i < 1000; i++) {
                    if (nodes[i] > max_degree) max_degree = nodes[i];
                }
            }

            std::vector<Degree> distr(max_degree + 1);
            std::cout << "Initial guess: " << max_degree << std::endl;
            uint64_t amount_of_push_backs = 0;

            std::cout << "Calculate distribution" << std::endl;
            for (Node i = 0; i < n; i++) {
                while (nodes[i] >= max_degree) {
                    distr.push_back(0);
                    amount_of_push_backs++;
                    max_degree++;
                }
                distr[nodes[i]]++;
            }

            std::cout << "MaxDegree: " << max_degree << "  Amount of push_backs:" << amount_of_push_backs << std::endl;
            std::string fileName = "degrees.txt";
            std::fstream f;
            f.open(fileName, std::ios::out);

            for (Degree i = 0; i < distr.size(); i++) {
                if (distr[i] > 0)
                    f << i << " " << distr[i] << std::endl;
            }
            f.close();
        }
    };

}
