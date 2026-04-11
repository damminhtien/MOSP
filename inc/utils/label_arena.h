#ifndef UTILS_LABEL_ARENA
#define UTILS_LABEL_ARENA

#include "../common_inc.h"
#include "label.h"

template<int N, size_t BLOCK_SIZE = 8192>
class LabelArena {
public:
    LabelArena() = default;

    Label<N>* create(size_t node, const CostVec<N>& f) {
        Label<N>* slot = next_slot();
        *slot = Label<N>(node, f);
        slot->parent = nullptr;
        slot->should_check_sol = false;
        slot->set_next(nullptr);
        ++allocated_;
        return slot;
    }

    Label<N>* create(size_t node, CostVec<N> g, CostVec<N>& h) {
        Label<N>* slot = next_slot();
        *slot = Label<N>(node, g, h);
        slot->parent = nullptr;
        slot->should_check_sol = false;
        slot->set_next(nullptr);
        ++allocated_;
        return slot;
    }

    void clear() {
        blocks_.clear();
        next_index_ = BLOCK_SIZE;
        allocated_ = 0;
    }

    size_t allocated() const {
        return allocated_;
    }

private:
    Label<N>* next_slot() {
        if (blocks_.empty() || next_index_ == BLOCK_SIZE) {
            blocks_.push_back(std::unique_ptr<Label<N>[]>(new Label<N>[BLOCK_SIZE]));
            next_index_ = 0;
        }

        return &blocks_.back()[next_index_++];
    }

    std::vector<std::unique_ptr<Label<N>[]>> blocks_;
    size_t next_index_ = BLOCK_SIZE;
    size_t allocated_ = 0;
};

#endif
