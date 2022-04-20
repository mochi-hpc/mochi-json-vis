#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>

// this code is definitely not performance sensitive; glad to have more
// information in output
#define JSON_DIAGNOSTICS 1
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/* when rendering the associated xstreams with a pool, skip over some if we
 * have more than this many (otherwise you'll need an exceedingly wide monitor) */
const int MAX_XSTREAMS=5;

class PoolMap {
    public:
        explicit PoolMap(const json &j);
        // do I want to check if the pool already exists?
        void add_pool(const std::string &pool_name) { m_map.push_back({pool_name}); }
        bool find_pool(const std::string &pool_name) {
            for (auto found : m_map) {
                if (pool_name == found[0]) {
                    return true;
                }
            }
            return false;
        }
        auto begin() { return m_map.begin();}
        auto cbegin() { return m_map.cbegin();}
        auto end() { return m_map.end(); }
        auto cend() { return m_map.cend(); }
        void print() {
            for (auto const &pool: m_map) {
                for (auto const &xstream: pool) {
                    std::cout << xstream << " ";
                }
                std::cout << std::endl;
            }
        }

        std::vector<std::string> & operator [](std::size_t n) {return m_map[n]; }
        const std::vector<std::string> & operator [](std::size_t n) const {return m_map[n]; }

    private:
        /* The 'm_map' data structure is not a vector of strings, but a vector
         * of a vector of strings.  I want to be able to both add pools to the
         * map, and add xstreams to each pool.  I also want to be able to find
         * pools by index in some cases */
        std::vector<std::vector<std::string>> m_map;
};

PoolMap::PoolMap(const json &j)
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
    /* furthermore, the json file does not require every single section.  If a
     * section is not in the json config, default values will be used */
    json pools;
    try {
        pools = j.at("margo").at("argobots").at("pools");
    } catch (...) {} // deliberatly ignoring missing items in the json object

    for (auto & pool : pools) {
        m_map.push_back({pool.at("name")});
    }

    /* with all the pools indexed, now we can associate xstreams with them */
    json xstreams;
    try {
        xstreams = j.at("margo").at("argobots").at("xstreams");
    } catch (...) {} // same as above: will fill in missing objects with defaults

    /* from margo json documentation:
     * ```Note that one of the xstream must be
     * named __primary__. If no __primary__ xstream is found by Margo, it
     * will automatically be added, along with a __primary__ pool. ``` */
    int found_primary = 0;
    for (auto xstream_it = xstreams.begin(); xstream_it != xstreams.end(); xstream_it++) {
        std::stringstream xstream_name;
        if (xstream_it->contains("name")) {
            xstream_name << xstream_it->at("name").get<std::string>();
        } else {
            xstream_name << "__xstream_" << xstream_it - xstreams.begin() << "__";
        }

        if (xstream_name.str() == "__primary__") {
            found_primary=1;
        }
        auto my_pools = xstream_it->at("scheduler").at("pools");
        for (auto xpool : my_pools) {
            /* An xstream with a given "name" will have in "xstreams.scheduler" an array
             * of pools given by either names (strings) or indexes */
            if (xpool.is_string() ) {
                /* Pretty dense C++11 code here: each item in 'm_map' is itself
                 * a vector.  See comment at top: first item of these vectors is the name of the
                 * pool.  Requires a custom predicate to handle this odd arrangement */
                auto xstream_pool = std::find_if(m_map.begin(), m_map.end(),
                        [&xpool](const auto &haystack) { return (haystack[0] == xpool.get_ref<const std::string&>()); });
                if (xstream_pool != m_map.end()) {
                    xstream_pool->push_back(xstream_name.str());
                }
            }
            if (xpool.is_number() ) {
                m_map[xpool].push_back(xstream_it->at("name"));
            }
        }
    }
    if (found_primary == 0) {
        m_map.push_back({"__primary__", "__primary__"});
    }
}

/* hard lesson learned:  that 'cluster_' prefix in the name of the subgraph is
 * significant */
void graph_header(std::stringstream & in)
{
    in << "digraph pools {" << std::endl
        << "   compound=true;" << std::endl;
}


void graph_footer(std::stringstream &in)
{
    in << "}" << std::endl; // overall digraph
}

void graph_pools(std::stringstream &in, PoolMap &pools)
{
    in << "   subgraph cluster_margo {" << std::endl
        << "   label=\"Margo\";" << std::endl;
    for (auto list = pools.cbegin(); list != pools.cend(); list++) {
        in << "       subgraph cluster_pool" << list-pools.cbegin() << "{" << std::endl;
        in << "           label = \"" << (*list)[0] <<
                          "\\n" << (*list).size()-1 <<" xstreams\";" << std::endl;
        /* a hidden point for this pool so we can connect providers to it later if need be */
        in << "           " << (*list)[0] << " [shape=point style=invis] " << std::endl;
        /* if the list of associated xstreams is really large, the image is unusable.  */
        if ( (*list).size() > MAX_XSTREAMS ) {
            in << "              " << (*list)[1] << ";" << std::endl;
            in << "              " << (*list)[2] << ";" << std::endl;
            in << "              " << "\"...\";"            << std::endl;
            in << "              " << list->back() << ";" << std::endl;
            in << "       }" << std::endl;
        } else {  /* smaller lists we can just print every item */
            /* skipping [0]: first entry in the 'pool map' is the pool itself */
            for (size_t i=1; i< (*list).size(); i++) {
                in << "              " << (*list)[i] << ";" << std::endl;
            }
            in << "       }" << std::endl;
        }
    }
    in << "    }" << std::endl;
}

void graph_instance(std::stringstream &in, const std::string &name, const json &j, PoolMap &pools)
{
    if(j.contains("pool")) {
        if (j.at("pool").is_string() ) {
            in << "    " << name << " -> " << j.at("pool").get<std::string>() << ";" << std::endl;
        }
        if (j.at("pool").is_number() ) {
            in << "    " << name << " -> " << pools[j.at("pool").get<int>()][0] << ";" << std::endl;
        }
    }
}
void usage(const std::string &program) {
    std::cout << "usage:  " << program << " [json-file] " << std::endl;
}
int main(int argc, char **argv)
{
    if (argc != 2) {
        usage(argv[0]);
        return -1;
    }

    json j;

    try {
        if (strcmp(argv[1], "-") == 0)
            std::cin >> j;
        else {
            std::ifstream input(argv[1]);
            if (input) {
                input >> j;
            } else {
                std::cerr << "Unable to open file: " << strerror(errno) << std::endl;
                return -1;
            }
        }
    } catch (json::exception &e) {
        std::cout << e.what() << std::endl;
        return -1;
    }
    std::stringstream graph_stream;

    PoolMap pools(j);

    /* Goal: produce a graphivis "digraph"
     * - pools are one subgraph
     * - execution streams are nodes in the associated pool subgraph
     * - providers point to associated pools */
    graph_header(graph_stream);
    graph_pools(graph_stream, pools);

    /* these sections might have a single entity (e.g. always bedrock) or
     * contain an array of json objects, so we need to handle both cases */
    std::vector<std::string> sections = {"bedrock", "abt_io", "ssg", "clients", "providers"};
    for (auto const &s : sections) {
        if (j.contains(s)) {
            if (j.at(s).is_array()) {
                for (auto const& object : j.at(s) ) {
                    if (object.contains("name")) {
                        graph_instance(graph_stream, object.at("name").get<std::string>(), object, pools);
                    }
                }
            } else {
                graph_instance(graph_stream, s, j.at(s), pools);
            }
        }
    }

    graph_footer(graph_stream);

    std::cout << graph_stream.str();
}
