#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main(int argc, char **argv)
{
    std::ifstream input(argv[1]);
    json j;
    input >> j;
}
