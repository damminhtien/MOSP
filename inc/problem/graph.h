#ifndef PROBLEM_GRAPH
#define PROBLEM_GRAPH

#include"../common_inc.h"
#include"../utils/cost.h"

struct Edge {
    static constexpr size_t MAX_NUM_OBJ = 5;

    size_t head, tail;
    std::array<cost_t, MAX_NUM_OBJ> cost{};

    Edge() = default;

    Edge(size_t head, size_t tail)
    : head(head), tail(tail) {
        cost.fill(0);
    }

    Edge(size_t head, size_t tail, const std::array<cost_t, MAX_NUM_OBJ>& cost)
    : head(head), tail(tail), cost(cost) {}

    Edge reverse() const { return Edge(tail, head, cost); }
};

class AdjacencyMatrix {
public:
    std::vector<std::vector<Edge>> matrix;
    
    AdjacencyMatrix() {}
    AdjacencyMatrix(size_t num_node, size_t num_obj, std::vector<Edge> &edges, bool reverse=false);
    
    std::vector<Edge>& operator[](size_t node);
    const std::vector<Edge> &operator[](size_t node) const;

    void add(Edge edge);

    int get_num_obj() const { return num_obj; }
    int get_num_node() const { return num_node; }
    int get_num_edge() const { return num_edge; }
    
private:
    int num_obj, num_node, num_edge;
};

#endif
