#include "problem/graph.h"

AdjacencyMatrix::AdjacencyMatrix(size_t num_node, size_t num_obj, std::vector<Edge> &edges, bool reverse)
: matrix(num_node, std::vector<Edge>()), num_obj(num_obj), num_node(num_node) {
    num_edge = 0;
    for (auto it = edges.begin(); it != edges.end(); it++) {
        if (it->head >= num_node || it->tail >= num_node) { continue; }
    
        if (!reverse) {
            this->add(*it);
        } else {
            this->add(it->reverse());
        }
        num_edge++;
    }
}

void AdjacencyMatrix::add(Edge edge) {
    (this->matrix[edge.head]).push_back(edge);
}

std::vector<Edge>& AdjacencyMatrix::operator[](size_t node) {
    return this->matrix.at(node);
}
const std::vector<Edge>& AdjacencyMatrix::operator[](size_t node) const {
    return this->matrix.at(node);
}
