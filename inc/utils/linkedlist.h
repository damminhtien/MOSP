#ifndef UTILS_LINKEDLIST
#define UTILS_LINKEDLIST

#include <cassert>
#include <type_traits>

#include "../common_inc.h"

template <class T>
class linkedlist {
    static_assert(std::is_pointer<T>::value, "linkedlist expects a pointer element type");

public:
    linkedlist() = default;

    explicit linkedlist(T element) {
        if (element != nullptr) {
            head_ = element;
            tail_ = element;
            size_ = 1;
        }
    }

    linkedlist(const linkedlist&) = delete;
    linkedlist& operator=(const linkedlist&) = delete;
    linkedlist(linkedlist&&) noexcept = default;
    linkedlist& operator=(linkedlist&&) noexcept = default;
    ~linkedlist() = default;

    void push_front(T element) {
        assert(element != nullptr);

        if (empty()) {
            element->set_next(nullptr);
            head_ = element;
            tail_ = element;
            tail_prev_ = nullptr;
            size_ = 1;
            return;
        }

        element->set_next(head_);
        head_ = element;
        if (size_ == 1) {
            tail_prev_ = element;
        }
        size_++;
    }

    void push_back(T element) {
        assert(element != nullptr);
        element->set_next(nullptr);

        if (empty()) {
            head_ = element;
            tail_ = element;
            tail_prev_ = nullptr;
            size_ = 1;
            return;
        }

        tail_prev_ = tail_;
        tail_->set_next(element);
        tail_ = element;
        size_++;
    }

    T front() const {
        return head_;
    }

    T back() const {
        return tail_;
    }

    T prev_back() const {
        return tail_prev_;
    }

    size_t size() const {
        return size_;
    }

    bool empty() const {
        return size_ == 0;
    }

    void pop_front() {
        assert(!empty());

        if (size_ == 1) {
            clear();
            return;
        }

        head_ = static_cast<T>(head_->get_next());
        size_--;
        if (size_ == 1) {
            tail_ = head_;
            tail_prev_ = nullptr;
        }
    }

    void pop_back() {
        assert(!empty());

        if (size_ == 1) {
            clear();
            return;
        }

        T previous = nullptr;
        T current = head_;
        while (current != nullptr && current != tail_) {
            previous = current;
            current = static_cast<T>(current->get_next());
        }

        assert(current == tail_);

        tail_ = previous;
        if (tail_ != nullptr) {
            tail_->set_next(nullptr);
        }

        if (size_ == 2) {
            tail_prev_ = nullptr;
        } else {
            T before_tail = head_;
            while (before_tail != nullptr && static_cast<T>(before_tail->get_next()) != tail_) {
                before_tail = static_cast<T>(before_tail->get_next());
            }
            tail_prev_ = before_tail;
        }

        size_--;
    }

    void clear() {
        head_ = nullptr;
        tail_ = nullptr;
        tail_prev_ = nullptr;
        size_ = 0;
    }

    size_t mem() const {
        return sizeof(*this);
    }

private:
    T head_ = nullptr;
    T tail_ = nullptr;
    T tail_prev_ = nullptr;
    size_t size_ = 0;
};

#endif
