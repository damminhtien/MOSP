#ifndef UTILS_PQ_BUCKET
#define UTILS_PQ_BUCKET

#include <cassert>

#include"../common_inc.h"
#include"cost.h"
#include"label.h"
#include"linkedlist.h"

template <int N>
class pq_bucket {
public:
    pq_bucket(const pq_bucket&) = delete;
    pq_bucket& operator=(const pq_bucket&) = delete;
    pq_bucket(pq_bucket&&) noexcept = default;
    pq_bucket& operator=(pq_bucket&&) noexcept = default;

    pq_bucket(size_t size=1, cost_t fmin=0, cost_t fmax=1)
    : initsize_(0), max_cursor_(0), queuesize_(0), cursor_(0), f_min(fmin), bucket_width_(1) {
        static_cast<void>(size);
        initsize_ = (fmax - fmin) / bucket_width_ + 1;
        elts_.resize(initsize_);
        max_cursor_ = initsize_ - 1;
    }

    ~pq_bucket() = default;

    inline void range_update(cost_t fmin, cost_t fmax) {
        f_min = fmin;
        resize(fmax - fmin + 1);
    }

    void clear() {
        queuesize_ = 0;
        cursor_ = 0;
        elts_ = std::vector<linkedlist<Label<N>*>>(initsize_);
        max_cursor_ = initsize_ - 1;
    }

    void push (Label<N>* lb) {
        size_t index = (lb->f[0] - f_min);
        if (index > max_cursor_) {
            size_t new_size = std::max<size_t>(max_cursor_ * 2, 1);
            while(index > new_size){new_size = new_size * 2;}
            resize(new_size);
        }
        elts_[index].push_front(lb);
        
        queuesize_++;
        return;
    }

    Label<N>* top() {
        if (queuesize_) {
            Label<N>* ans = this->peek();
            return ans;
        }
        return nullptr;
    }

    void pop() {
        if (queuesize_) {
            elts_[cursor_].pop_front();
            queuesize_--;
        }
    }

    inline Label<N>* peek() {
        while(!elts_[cursor_].size()) { cursor_++; }
        return elts_[cursor_].front();
    }

    inline unsigned int size() const { return queuesize_; }
    inline bool empty() const { return queuesize_ == 0; }

    void resize(size_t newsize){
        if (newsize == 0) {
            newsize = 1;
        }

        if(newsize <= elts_.size()){
            return;
        }

        elts_.resize(newsize);
        max_cursor_ = newsize-1;
        return;
    }

    inline size_t get_cmp() { return cursor_; }
    void pull_down(Label<N>* lb) { return; }
    size_t mem() {
        return elts_.capacity() * sizeof(linkedlist<Label<N>*>) + sizeof(*this);
    }

private:
    size_t initsize_;
    size_t max_cursor_;
    unsigned int queuesize_;
    size_t cursor_;
    cost_t f_min;
    size_t bucket_width_;
    std::vector<linkedlist<Label<N>*>> elts_;
};

template class pq_bucket<2>;
template class pq_bucket<3>;
template class pq_bucket<4>;
template class pq_bucket<5>;

#endif
