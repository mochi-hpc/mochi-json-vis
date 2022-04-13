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
    /* from margo json documentation:
     * ```Note that one of the xstream must be
     * named “__primary__”. If no __primary__ xstream is found by Margo, it
     * will automatically be added, along with a __primary__ pool. ``` */
    int found_primary = 0;
    for (auto xstream_it = xstreams.begin(); xstream_it != xstreams.end(); xstream_it++) {
        if (xstream_it->at("name") == "__primary__")
            found_primary=1;
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
    if (!found_primary)
        m_map.push_back({"__primary__", "__primary__"});

}

/* hard lesson learned:  that 'cluster_' prefix in the name of the subgraph is
 * significant */
void graph_header(std::stringstream & in, json & j)
{
    in << "digraph pools {" << std::endl
        << "   subgraph cluster_margo {" << std::endl
        << "   label=\"Margo\";" << std::endl;
}


void graph_footer(std::stringstream &in, json &j)
{
    in << "}" << std::endl; // margo subgraph
    in << "}" << std::endl; // overall digraph
}

void graph_pools(std::stringstream &in, PoolMap &pools)
{
    for (auto list = pools.cbegin(); list != pools.cend(); list++) {
        in << "       subgraph cluster_pool" << list-pools.cbegin() << "{" << std::endl;
        in << "           label = \"" << (*list)[0] << "\";" << std::endl;
        /* first entry in the 'pool map' is the pool itself */
        for (size_t i=1; i< (*list).size(); i++) {
            in << "              " << (*list)[i] << ";" << std::endl;
        }
        in << "       }" << std::endl;
    }
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

    /* Goal: produce a graphivis "digraph"
     * - pools are one subgraph
     * - execution streams are nodes in the associated pool subgraph
     * - providers point to associated pools */
    graph_header(graph_stream, j);
    graph_pools(graph_stream, pools);

    graph_footer(graph_stream, j);

    std::cout << graph_stream.str();
}
