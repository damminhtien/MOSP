#ifndef ALGORITHMS_GCL_FAST_ARRAY
#define ALGORITHMS_GCL_FAST_ARRAY

#include "../../common_inc.h"
#include "../../utils/cost.h"

template<int M>
struct PackedFrontEntry {
    CostVec<M> cost{};
};

template<int M, uint32_t INITIAL_CAPACITY = 4>
class FastNodeFrontier {
public:
    using Snapshot = std::vector<CostVec<M>>;
    static_assert(INITIAL_CAPACITY > 0, "INITIAL_CAPACITY must be positive");
    static constexpr uint32_t INLINE_CAPACITY = INITIAL_CAPACITY;

    FastNodeFrontier()
    : data_(inline_data_.data()),
      size_(0),
      capacity_(INLINE_CAPACITY) {}

    FastNodeFrontier(const FastNodeFrontier&) = delete;
    FastNodeFrontier& operator=(const FastNodeFrontier&) = delete;

    FastNodeFrontier(FastNodeFrontier&& other) noexcept {
        move_from(std::move(other));
    }

    FastNodeFrontier& operator=(FastNodeFrontier&& other) noexcept {
        if (this != &other) {
            move_from(std::move(other));
        }
        return *this;
    }

    bool empty() const {
        return size_ == 0;
    }

    uint32_t size() const {
        return size_;
    }

    uint32_t capacity() const {
        return capacity_;
    }

    bool dominated(const CostVec<M>& x) const {
        for (uint32_t idx = 0; idx < size_; idx++) {
            if (weakly_dominate<M>(data_[idx].cost, x)) {
                return true;
            }
        }
        return false;
    }

    bool try_insert_or_prune(const CostVec<M>& x) {
        for (uint32_t idx = 0; idx < size_; idx++) {
            if (weakly_dominate<M>(data_[idx].cost, x)) {
                return false;
            }
        }

        uint32_t write_idx = 0;
        for (uint32_t read_idx = 0; read_idx < size_; read_idx++) {
            if (weakly_dominate<M>(x, data_[read_idx].cost)) {
                continue;
            }
            if (write_idx != read_idx) {
                data_[write_idx] = data_[read_idx];
            }
            write_idx++;
        }
        size_ = write_idx;

        ensure_capacity(size_ + 1);
        data_[size_++].cost = x;
        return true;
    }

    Snapshot snapshot() const {
        Snapshot copy;
        copy.reserve(size_);
        for (uint32_t idx = 0; idx < size_; idx++) {
            copy.push_back(data_[idx].cost);
        }
        return copy;
    }

    void clear() {
        size_ = 0;
    }

private:
    void move_from(FastNodeFrontier&& other) noexcept {
        size_ = other.size_;
        capacity_ = other.capacity_;
        if (other.heap_data_) {
            heap_data_ = std::move(other.heap_data_);
            data_ = heap_data_.get();
        } else {
            heap_data_.reset();
            data_ = inline_data_.data();
            for (uint32_t idx = 0; idx < size_; idx++) {
                inline_data_[idx] = other.inline_data_[idx];
            }
        }

        other.heap_data_.reset();
        other.data_ = other.inline_data_.data();
        other.size_ = 0;
        other.capacity_ = INLINE_CAPACITY;
    }

    void ensure_capacity(uint32_t desired_capacity) {
        if (desired_capacity <= capacity_) {
            return;
        }

        uint32_t next_capacity = capacity_;
        while (next_capacity < desired_capacity) {
            next_capacity *= 2;
        }

        std::unique_ptr<PackedFrontEntry<M>[]> next_data(new PackedFrontEntry<M>[next_capacity]);
        for (uint32_t idx = 0; idx < size_; idx++) {
            next_data[idx] = data_[idx];
        }

        heap_data_ = std::move(next_data);
        data_ = heap_data_.get();
        capacity_ = next_capacity;
    }

    std::array<PackedFrontEntry<M>, INLINE_CAPACITY> inline_data_{};
    std::unique_ptr<PackedFrontEntry<M>[]> heap_data_;
    PackedFrontEntry<M>* data_ = nullptr;
    uint32_t size_ = 0;
    uint32_t capacity_ = 0;
};

template<int M>
class FastGclArray {
public:
    using Snapshot = typename FastNodeFrontier<M>::Snapshot;

    explicit FastGclArray(size_t num_node)
    : frontiers_(num_node + 1) {}

    std::string get_name() const {
        return "fast_array";
    }

    bool dominated(size_t node, const CostVec<M>& x) const {
        if (node >= frontiers_.size()) {
            return false;
        }
        return frontiers_[node].dominated(x);
    }

    bool try_insert_or_prune(size_t node, const CostVec<M>& x) {
        if (node >= frontiers_.size()) {
            perror(("Invalid FastGclArray index " + std::to_string(node)).c_str());
            exit(EXIT_FAILURE);
            return false;
        }
        return frontiers_[node].try_insert_or_prune(x);
    }

    Snapshot snapshot(size_t node) const {
        if (node >= frontiers_.size()) {
            return {};
        }
        return frontiers_[node].snapshot();
    }

    void clear() {
        for (auto& frontier : frontiers_) {
            frontier.clear();
        }
    }

    std::vector<size_t> live_frontier_sizes() const {
        std::vector<size_t> sizes;
        sizes.reserve(frontiers_.size());
        for (const auto& frontier : frontiers_) {
            if (!frontier.empty()) {
                sizes.push_back(frontier.size());
            }
        }
        return sizes;
    }

private:
    std::vector<FastNodeFrontier<M>> frontiers_;
};

#endif
