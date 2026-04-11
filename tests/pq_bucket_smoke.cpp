#include <type_traits>

#include "utils/pq_bucket.h"

static_assert(!std::is_copy_constructible_v<pq_bucket<2>>);
static_assert(!std::is_copy_assignable_v<pq_bucket<2>>);
static_assert(std::is_move_constructible_v<pq_bucket<2>>);
static_assert(std::is_move_assignable_v<pq_bucket<2>>);

int main() {
    pq_bucket<2> queue(1, 0, 1000);

    CostVec<2> cost_a{};
    cost_a[0] = 5;
    cost_a[1] = 9;
    Label<2> label_a(0, cost_a);

    queue.push(&label_a);
    if (queue.empty() || queue.top() != &label_a) {
        return 1;
    }
    queue.pop();
    if (!queue.empty()) {
        return 2;
    }

    queue.clear();

    CostVec<2> cost_b{};
    cost_b[0] = 3;
    cost_b[1] = 7;
    Label<2> label_b(1, cost_b);

    queue.push(&label_b);
    if (queue.empty() || queue.top() != &label_b) {
        return 3;
    }
    queue.pop();

    return queue.empty() ? 0 : 4;
}
