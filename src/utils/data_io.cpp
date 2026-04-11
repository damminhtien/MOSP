#include"utils/data_io.h"

void read_graph_files(std::vector<std::string> file_paths, size_t &num_node, size_t &num_obj, std::vector<Edge> &edges) {
    size_t max_node_id = 0;
    num_obj = file_paths.size();
    if (num_obj > Edge::MAX_NUM_OBJ) {
        throw std::runtime_error("Too many objective files: " + std::to_string(num_obj));
    }

    for (size_t obj_idx = 0; obj_idx < file_paths.size(); obj_idx++) {
        const std::string& file_path = file_paths[obj_idx];
        std::ifstream gr_file;
        gr_file.open(file_path);
        if(!gr_file.is_open()) {
            throw std::runtime_error("Error opening file " + file_path);
        }

        std::string line;
        size_t edge_id = 0;
        while(getline(gr_file, line)) {
            std::istringstream stream(line);
            std::string prefix_str;
            stream >> prefix_str;

            if (prefix_str == "c") { 
                continue;
            } else if (prefix_str == "a") {
                std::string head_str, tail_str, weight_str;
                stream >> head_str >> tail_str >> weight_str;

                size_t head = std::stoi(head_str);
                size_t tail = std::stoi(tail_str);
                cost_t weight = std::stoi(weight_str);

                if (edge_id < edges.size()) { // Read from second graph file until last
                    if (head != edges[edge_id].head || tail != edges[edge_id].tail) {
                        throw std::runtime_error("Graph file inconsistency " + file_path);
                    }
                    edges[edge_id].cost[obj_idx] = weight;
                } else { // Read from first graph file
                    Edge edge(head, tail);
                    edge.cost[obj_idx] = weight;
                    edges.push_back(edge);

                    if (head > max_node_id) { max_node_id = head; }
                    if (tail > max_node_id) { max_node_id = tail; }
                }

                edge_id++;
            }
        }
        gr_file.close();
    }
    num_node = max_node_id + 1;
}

std::vector<std::tuple<std::string, size_t, size_t>> read_scenario_file(std::string file_path) {
    std::vector<std::tuple<std::string, size_t, size_t>> scenarios;

    nlohmann::json scen_datas;
    std::ifstream ifs(file_path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open scenario file " + file_path);
    }
    scen_datas = nlohmann::json::parse(ifs);
    if (!scen_datas.is_array()) {
        throw std::runtime_error("Scenario file must be a JSON array: " + file_path);
    }
    
    for (const auto& scen_data : scen_datas) {
        if (!scen_data.is_object()
            || !scen_data.contains("name")
            || !scen_data.contains("start_data")
            || !scen_data.contains("end_data")) {
            throw std::runtime_error("Scenario entry missing required fields in " + file_path);
        }
        std::string name = scen_data["name"].get<std::string>();
        size_t start_node = scen_data["start_data"].get<size_t>();
        size_t end_node = scen_data["end_data"].get<size_t>();
        scenarios.push_back(std::make_tuple(name, start_node, end_node));
    }

    ifs.close();
    return scenarios;
}
