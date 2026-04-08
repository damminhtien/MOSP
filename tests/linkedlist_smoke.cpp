#include <type_traits>

#include "utils/linkedlist.h"

struct LinkedListNode {
    LinkedListNode* next_ = nullptr;

    LinkedListNode* get_next() { return next_; }
    void set_next(LinkedListNode* next) { next_ = next; }
};

static_assert(!std::is_copy_constructible_v<linkedlist<LinkedListNode*>>);
static_assert(!std::is_copy_assignable_v<linkedlist<LinkedListNode*>>);
static_assert(std::is_move_constructible_v<linkedlist<LinkedListNode*>>);
static_assert(std::is_move_assignable_v<linkedlist<LinkedListNode*>>);

int main() {
    LinkedListNode a;
    LinkedListNode b;
    LinkedListNode c;

    linkedlist<LinkedListNode*> list;
    if (!list.empty() || list.size() != 0) {
        return 1;
    }

    list.push_front(&a);
    if (list.front() != &a || list.back() != &a || list.prev_back() != nullptr || list.size() != 1) {
        return 2;
    }

    list.push_front(&b);
    if (list.front() != &b || list.back() != &a || list.prev_back() != &b || list.size() != 2) {
        return 3;
    }

    list.push_back(&c);
    if (list.front() != &b || list.back() != &c || list.prev_back() != &a || list.size() != 3) {
        return 4;
    }

    list.pop_front();
    if (list.front() != &a || list.back() != &c || list.prev_back() != &a || list.size() != 2) {
        return 5;
    }

    list.pop_back();
    if (list.front() != &a || list.back() != &a || list.prev_back() != nullptr || list.size() != 1) {
        return 6;
    }

    list.clear();
    if (!list.empty() || list.front() != nullptr || list.back() != nullptr || list.prev_back() != nullptr) {
        return 7;
    }

    return 0;
}
