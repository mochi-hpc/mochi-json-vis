#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/* hard lesson learned:  that 'cluster_' prefix in the name of the subgraph is
 * significant */
void graph_header(std::stringstream & in, json & j)
{
    in << "digraph pools {" << std::endl
        << "   subgraph cluster_margo {" << std::endl
        << "   label=\"Margo\";" << std::endl;
}

/* for the given "pool", graph all the xstreams associated with it.  An xstream
 * can take work from one or more pools */
void graph_xstreams(std::stringstream &in, const std::string &pool, json
        &xstreams)
{
    for (auto xstream_it = xstreams.begin(); xstream_it != xstreams.end(); xstream_it++) {
        for (auto pool_it = xstream_it->at("scheduler").at("pools").begin();
                pool_it != xstream_it->at("scheduler").at("pools").end();
                pool_it++) {
            if (pool_it->is_string() && *pool_it == pool) {
                in << "        subgraph cluster_xstream_" << xstream_it - xstreams.begin() << " {" << std::endl;
                in << "            label = xs" << xstream_it - xstreams.begin() << std::endl;
                in << "            " << xstream_it->at("name") << std::endl;
                in << "        }" << std::endl;
            }
        }
    }
}
/* not particularly efficient: O(n^2) with the number of xstreams.  but if
 * someone has a thousand xstreams we aren't going to visualize it all that
 * well! */
void graph_progress_pool(std::stringstream &in, json &j)
{
    if (j.contains(json::json_pointer("/margo/progress_pool")) ) {
        in << "    subgraph  cluster_progress_pool {" << std::endl;
        in << "    label = \"Progress Pool:\";"
           << "    " << j.at("margo").at("progress_pool") << std::endl;

        graph_xstreams(in, j.at("margo").at("progress_pool"), j.at("margo").at("argobots").at("xstreams") );

        in << "    }" << std::endl;
    }

    if (j.contains(json::json_pointer("/margo/rpc_pool")) ) {
        in << "    subgraph  cluster_rpc_pool {" << std::endl;
        in << "    label = \"RPC Pool:\" " << j.at("margo").at("rpc_pool") << std::endl;
        in << "    " << j.at("margo").at("rpc_pool") << std::endl;

        graph_xstreams(in, j.at("margo").at("rpc_pool"), j.at("margo").at("argobots").at("xstreams") );

        in << "    }" << std::endl;
    }
}

void graph_footer(std::stringstream &in, json &j)
{
    in << "}" << std::endl; // margo subgraph
    in << "}" << std::endl; // overall digraph
}
int main(int argc, char **argv)
{
    std::ifstream input(argv[1]);
    json j;
    input >> j;
    std::stringstream graph_stream;

    if (!j.contains("margo")) {
        std::cout << "no margo entity found" << std::endl;
        return -1;
    }
#if 0
    std::cout <<
        "margo progress_pool: " << j[json::json_pointer("/margo/progress_pool")] <<
        " margo rpc_pool: " << j[json::json_pointer("/margo/rpc_pool")];
#endif
    /* Goal: produce a graphivis "digraph"
     * - pools are one subgraph
     * - execution streams are nodes in the associated pool subgraph
     * - providers point to associated pools */
    graph_header(graph_stream, j);
    graph_progress_pool(graph_stream, j);

    graph_footer(graph_stream, j);

    std::cout << graph_stream.str();
}
