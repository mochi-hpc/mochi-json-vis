#include <iostream>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <nlohmann/json.hpp>

using json = nlohmann::json;


class PoolMap {
    public:
        PoolMap(json &j);
        // do I want to check if the pool already exists?
        void add_pool(const std::string &pool_name) { m_map.push_back({pool_name}); }
        bool find_pool(const std::string &pool_name) {
            for (auto found : m_map) {
                if (pool_name == found[0])
                    return true;
            }
            return false;
        }
        auto begin() { return m_map.begin();}
        auto cbegin() { return m_map.cbegin();}
        auto end() { return m_map.end(); }
        auto cend() { return m_map.cend(); }
        void print() {
            for (auto pool: m_map) {
                for (auto xstream: pool) {
                    std::cout << xstream << " ";
                }
                std::cout << std::endl;
            }
        }

    private:
        /* The 'm_map' data structure is not a vector of strings, but a vector
         * of a vector of strings.  I want to be able to both add pools to the
         * map, and add xstreams to each pool.  I also want to be able to find
         * pools by index in some cases */
        std::vector<std::vector<std::string>> m_map;
};

PoolMap::PoolMap(json &j)
{
    /* the json file has a mapping between xstreams and the pools from which
     * they draw work, but it's not in exactly the form we need.  Instead,
     * something like this: a vector of vector of strings, [0] is name, [1...end] is the xstreams
     *
     * 0: {"pool1", "xs1", "xs2", "xs3"...}
     * 1: {"pool2", "xs9, "xs10, "xs11"...}
     * ...
     *
     */
    auto pools = j.at("margo").at("argobots").at("pools");
    for (auto pool_it = pools.begin(); pool_it != pools.end(); pool_it++)
    {
        std::vector<std::string> next { pool_it->at("name") };
        m_map.push_back(next);
    }

    /* with all the pools indexed, now we can associate xstreams with them */
    auto xstreams = j.at("margo").at("argobots").at("xstreams");
    for (auto xstream_it = xstreams.begin(); xstream_it != xstreams.end(); xstream_it++) {
        auto my_pools = xstream_it->at("scheduler").at("pools");
        for(auto xpool_it = my_pools.begin();
                xpool_it != my_pools.end();
                xpool_it++)
        {
            /* An xstream with a given "name" will have in "xstreams.scheduler" an array
             * of pools given by either names (strings) or indexes */
            if (xpool_it->is_string() ) {
                /* Pretty dense C++11 code here: each item in 'm_map' is itself
                 * a vector.  See comment at top: first item of these vectors is the name of the
                 * pool.  Requires a custom predicate to handle this odd arrangement */
                auto xstream_pool = std::find_if(m_map.begin(), m_map.end(),
                        [&xpool_it](const auto &haystack) { return (haystack[0] == xpool_it->get_ref<const std::string&>()); });
                if (xstream_pool != m_map.end())
                    xstream_pool->push_back(xstream_it->at("name"));
            }
            if (xpool_it->is_number() ) {
                m_map[*xpool_it].push_back(xstream_it->at("name"));
            }
        }
    }

}

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

    PoolMap pools(j);

    /*  If no __primary__ xstream is found by Margo, it will automatically be
     *  added, along with a __primary__ pool */
    if (pools.find_pool("__primary__")) {
        pools.add_pool("__primary__");
    }
    pools.print();

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
