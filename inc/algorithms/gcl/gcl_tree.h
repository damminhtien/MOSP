#ifndef ALGORITHMS_GCL_TREE
#define ALGORITHMS_GCL_TREE

#include"../../common_inc.h"
#include"../../utils/cost.h"

template<int N> struct AVL_Node;
template<int N> class AVL_Tree;

template<int N>
class Gcl_tree {
public:
    using Snapshot = std::vector<CostVec<N>>;

    Gcl_tree(size_t num_node) {
        for (int i = 0; i < num_node + 1; i++) {
            gcl.push_back(AVL_Tree<N>());
        }
    };

    std::string get_name(){ return "avl_tree"; }
    
    inline bool frontier_check(size_t node, const CostVec<N> & cost) {
        if (node >= gcl.size()) { return false; }
        return this->node_check(gcl[node].root, cost);
    }

    inline bool frontier_update(size_t node, const CostVec<N> & cost) {
        if (node >= gcl.size()) { 
            perror(("Invalid Gcl_tree index " + std::to_string(node)).c_str());
            exit(EXIT_FAILURE);
            return false; 
        }

        if (this->node_check(gcl[node].root, cost)) { return false; }

        std::vector<CostVec<N>> filtered_costs;
        this->node_filter(gcl[node].root, cost, filtered_costs);
        for (auto & filtered_cost : filtered_costs) {
            gcl[node].erase(filtered_cost);
        }
        gcl[node].insert(cost);
        return true;
    }

    inline Snapshot snapshot(size_t node) const {
        Snapshot copy;
        if (node >= gcl.size()) { return copy; }

        this->node_snapshot(gcl[node].root, copy);
        return copy;
    }

private:
    std::vector<AVL_Tree<N>> gcl;

    bool node_check(AVL_Node<N>* n_ptr, const CostVec<N>& cost) {
        if (n_ptr == nullptr) { return false; } 
        if (weakly_dominate<N>(n_ptr->key, cost)) { return true; }

        if (lex_smaller<N>(cost, n_ptr->key)) {
            return this->node_check(n_ptr->left, cost);
        } else {
            if (this->node_check(n_ptr->left, cost)) {
                return true;
            } else {
                return this->node_check(n_ptr->right, cost);
            }
        }
    }

    void node_filter(AVL_Node<N>* n_ptr, const CostVec<N>& cost, std::vector<CostVec<N>> & filtered_costs) {
        if (n_ptr == nullptr) { return; }
        if (lex_smaller<N>(n_ptr->key, cost)) {
            this->node_filter(n_ptr->right, cost, filtered_costs);
        } else {
            if (weakly_dominate<N>(cost, n_ptr->key)) {
                filtered_costs.push_back(n_ptr->key);
            }
            this->node_filter(n_ptr->left, cost, filtered_costs);
            this->node_filter(n_ptr->right, cost, filtered_costs);
        }
    }

    void node_snapshot(AVL_Node<N>* n_ptr, Snapshot& copy) const {
        if (n_ptr == nullptr) { return; }
        node_snapshot(n_ptr->left, copy);
        copy.push_back(n_ptr->key);
        node_snapshot(n_ptr->right, copy);
    }
};

template<int N>
struct AVL_Node {
    CostVec<N> key;
    AVL_Node<N>* left;
    AVL_Node<N>* right;
    int height;

    AVL_Node(const CostVec<N>& k) 
    : key(k), left(nullptr), right(nullptr), height(0) {}

    void update_height() {
        int lh, rh;

        if (this->left == nullptr) { lh = -1; }
        else { lh = this->left->height; }

        if (this->right == nullptr) { rh = -1; }
        else { rh = this->right->height; }

        this->height = std::max(lh, rh) + 1;
    }
};

template<int N>
class AVL_Tree {
public:
    int tree_size;
    AVL_Node<N>* root;

    AVL_Tree() : tree_size(0), root(nullptr) {}
    ~AVL_Tree() { this->clear(); }
    int size() { return this->tree_size; }
    bool empty() { return this->tree_size == 0; }

    void clear() { this->clear(this->root); }
    void clear(AVL_Node<N>* n_ptr) {
        if (n_ptr == nullptr) { return; }
        this->clear(n_ptr->left);
        this->clear(n_ptr->right);
        this->tree_size--;
        delete n_ptr;
    }

    int get_height(AVL_Node<N>* n_ptr) {
        if (n_ptr == nullptr) { return -1; }
        return n_ptr->height;
    }

    AVL_Node<N>* insert(CostVec<N> key) {
        this->root = this->insert(key, this->root);
        return this->root;
    }

    AVL_Node<N>* insert(CostVec<N> key, AVL_Node<N>* n_ptr) {
        if (n_ptr == nullptr) {
            this->tree_size++;
            n_ptr = new AVL_Node<N>(key);
        } else if (lex_smaller<N>(key, n_ptr->key)) {
            n_ptr->left = this->insert(key, n_ptr->left);
        } else if (lex_smaller<N>(n_ptr->key, key)) {
            n_ptr->right = this->insert(key, n_ptr->right);
        }
        n_ptr->update_height();

        if (this->get_balance_factor(n_ptr) > 1) {
            if (lex_smaller<N>(key, n_ptr->left->key)) {
                return this->right_rotate(n_ptr);
            } else if (lex_smaller<N>(n_ptr->left->key, key)) {
                n_ptr->left = this->left_rotate(n_ptr->left);
                return this->right_rotate(n_ptr);
            }
        } else if (this->get_balance_factor(n_ptr) < -1) {
            if (lex_smaller<N>(n_ptr->right->key, key)) {
                return this->left_rotate(n_ptr);
            } else if (lex_smaller<N>(key, n_ptr->right->key)) {
                n_ptr->right = this->right_rotate(n_ptr->right);
                return this->left_rotate(n_ptr);
            }
        }
        return n_ptr;
    }

    AVL_Node<N>* erase(CostVec<N> key) {
        this->root = this->erase(key, this->root);
        return this->root;
    }

    AVL_Node<N> * erase(CostVec<N> key, AVL_Node<N>* n_ptr) {
        if (n_ptr == nullptr) {
            return nullptr; 
        } else if (lex_smaller<N>(key, n_ptr->key)) {
            n_ptr->left = this->erase(key, n_ptr->left);
        } else if (lex_smaller<N>(n_ptr->key, key)) {
            n_ptr->right = this->erase(key, n_ptr->right);
        } else {
            if (n_ptr->left == nullptr || n_ptr->right == nullptr) {
                AVL_Node<N> * temp;
                if (n_ptr->left != nullptr) { temp = n_ptr->left; } 
                else { temp = n_ptr->right; }

                if (temp == nullptr) {
                    temp = n_ptr;
                    n_ptr = nullptr;
                } else { *n_ptr = *temp; }

                this->tree_size--;
                delete temp;
            } else {
                AVL_Node<N>* temp = this->find_min(n_ptr->right);
                n_ptr->key = temp->key;
                n_ptr->right = this->erase(temp->key, n_ptr->right);
            }
        }
        if (n_ptr == nullptr) { return nullptr; }
        n_ptr->update_height();

        if (this->get_balance_factor(n_ptr) > 1) {
            if (this->get_balance_factor(n_ptr->left) >= 0) {
                return this->right_rotate(n_ptr);
            } else {
                n_ptr->left = this->left_rotate(n_ptr->left);
                return this->right_rotate(n_ptr);
            }
        } else if (this->get_balance_factor(n_ptr) < -1) {
            if (this->get_balance_factor(n_ptr->right) <= 0) {
                return this->left_rotate(n_ptr);
            } else {
                n_ptr->right = this->right_rotate(n_ptr->right);
                return this->left_rotate(n_ptr);
            }
        }
        return n_ptr;
    }

private:
    int get_balance_factor(AVL_Node<N>* n_ptr) {
        if (n_ptr == nullptr) { return 0; }
        return this->get_height(n_ptr->left) - this->get_height(n_ptr->right);
    }

    AVL_Node<N> * left_rotate(AVL_Node<N>* & n_ptr) { //reference of an AVL_Node's pointer
        AVL_Node<N> * u = n_ptr->right;
        n_ptr->right = u->left;
        u->left = n_ptr;
        n_ptr->update_height();
        u->update_height();
        return u;
    }

    AVL_Node<N>* right_rotate(AVL_Node<N>* & n_ptr) { //reference of an AVL_Node's pointer
        AVL_Node<N>* u = n_ptr->left;
        n_ptr->left = u->right;
        u->right = n_ptr;
        n_ptr->update_height();
        u->update_height();
        return u;
    }

    AVL_Node<N>* find_min(AVL_Node<N>* n_ptr) {
        if (n_ptr == nullptr) {
            return nullptr; 
        } else if (n_ptr->left == nullptr) {
            return n_ptr;
        } else {
            return this->find_min(n_ptr->left);
        }
    }

    AVL_Node<N>* find_max(AVL_Node<N>* n_ptr) {
        if (n_ptr == nullptr) {
            return nullptr; 
        } else if (n_ptr->right == nullptr) {
            return n_ptr;
        } else {
            return this->find_max(n_ptr->right);
        }
    }

};

template class Gcl_tree<1>;
template class Gcl_tree<2>;
template class Gcl_tree<3>;
template class Gcl_tree<4>;
template class Gcl_tree<5>;

#endif
