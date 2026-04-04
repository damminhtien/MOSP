#ifndef UTILS_DATA_IO
#define UTILS_DATA_IO

#include"../common_inc.h"
#include"../problem/graph.h"

void read_graph_files(std::vector<std::string> file_paths, size_t &num_node, size_t &num_obj, std::vector<Edge> &edges);
std::vector<std::tuple<std::string, size_t, size_t>> read_scenario_file(std::string file_path);

#endif
