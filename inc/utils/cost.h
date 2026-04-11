#ifndef UTILS_COST
#define UTILS_COST

#include"../common_inc.h"

typedef uint32_t cost_t;
template <unsigned int N> using CostVec = std::array<cost_t, N>;

const cost_t MAX_COST = std::numeric_limits<cost_t>::max();
const cost_t MIN_COST = std::numeric_limits<cost_t>::lowest();

template <unsigned int N>
inline CostVec<N-1> truncate(const CostVec<N> & vec){
    CostVec<N-1> tr_vec;
    std::copy(std::begin(vec) + 1, std::end(vec), std::begin(tr_vec));
    return tr_vec;
}

template <unsigned int N>
inline bool weakly_dominate(const CostVec<N> & vec1, const CostVec<N> & vec2){
    for (size_t i = 0; i < N; i++){
        if (vec2[i] < vec1[i]) { return false; }
    }
    return true;
}

template <unsigned int N>
bool weakly_dominate_dr(const CostVec<N> & vec1, const CostVec<N> & vec2){
    for (size_t i = 1; i < N; i++){
        if (vec2[i] < vec1[i]) { return false; }
    }
    return true;
}

template <unsigned int N>
bool lex_smaller(const CostVec<N> & vec1, const CostVec<N> & vec2){
    for (size_t i = 0; i < N; i++) {
        if (vec1[i] < vec2[i]) { return true; }
        else if (vec1[i] > vec2[i]) { return false; }
    }
    return false;
}

#endif
