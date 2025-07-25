// Copyright (c) 2015-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <indirectmap.h>
#include <prevector.h>
#include <util/heapoptional.h>

#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace memusage {

/**
 * Compute the memory used for dynamically allocated but owned data structures.
 * For generic data types, this is *not* recursive.
 * DynamicUsage(vector<vector<int>>) will compute the memory used for the
 * vector<int>'s, but not for the ints inside. This is for efficiency reasons,
 * as these functions are intended to be fast. If application data structures
 * require more accurate inner accounting, they should iterate themselves, or
 * use more efficient caching + updating on modification.
 */
inline constexpr size_t MallocUsage(size_t alloc) {
    // Measured on libc6 2.19 on Linux.
    if (alloc == 0) {
        return 0;
    }
    static_assert(sizeof(void *) == 8 || sizeof(void *) == 4);
    if constexpr (sizeof(void *) == 8) {
        return ((alloc + 31) >> 4) << 4;
    } else { // sizeof(void *) == 4
        return ((alloc + 15) >> 3) << 3;
    }
}

// STL data structures

template <typename X> struct stl_tree_node {
private:
    int color;
    void *parent;
    void *left;
    void *right;
    X x;
};

struct stl_shared_counter {
    /**
     * Various platforms use different sized counters here.
     * Conservatively assume that they won't be larger than size_t.
     */
    void *class_type;
    size_t use_count;
    size_t weak_count;
};

template <typename X>
inline size_t DynamicUsage(const std::vector<X> &v) {
    return MallocUsage(v.capacity() * sizeof(X));
}

template <unsigned int N, typename X, typename S, typename D>
inline size_t DynamicUsage(const prevector<N, X, S, D> &v) {
    return MallocUsage(v.allocated_memory());
}

template <typename X, typename Y>
inline size_t DynamicUsage(const std::set<X, Y> &s) {
    return MallocUsage(sizeof(stl_tree_node<X>)) * s.size();
}

template <typename X, typename Y>
inline constexpr size_t IncrementalDynamicUsage(const std::set<X, Y> &) {
    return MallocUsage(sizeof(stl_tree_node<X>));
}

template <typename X, typename Y>
inline constexpr size_t IncrementalDynamicUsage(const std::set<X, Y> *) {
    return MallocUsage(sizeof(stl_tree_node<X>));
}

template <typename X, typename Y, typename Z>
inline size_t DynamicUsage(const std::map<X, Y, Z> &m) {
    return MallocUsage(sizeof(stl_tree_node<std::pair<const X, Y>>)) * m.size();
}

template <typename X, typename Y, typename Z>
inline constexpr size_t IncrementalDynamicUsage(const std::map<X, Y, Z> &) {
    return MallocUsage(sizeof(stl_tree_node<std::pair<const X, Y>>));
}

// indirectmap has underlying map with pointer as key

template <typename X, typename Y>
inline size_t DynamicUsage(const indirectmap<X, Y> &m) {
    return MallocUsage(sizeof(stl_tree_node<std::pair<const X *, Y>>)) * m.size();
}

template <typename X, typename Y>
inline constexpr size_t IncrementalDynamicUsage(const indirectmap<X, Y> &) {
    return MallocUsage(sizeof(stl_tree_node<std::pair<const X *, Y>>));
}

template <typename X>
inline size_t DynamicUsage(const std::unique_ptr<X> &p) {
    return p ? MallocUsage(sizeof(X)) : 0;
}

template <typename X>
inline size_t DynamicUsage(const std::shared_ptr<X> &p) {
    // A shared_ptr can either use a single continuous memory block for both the
    // counter and the storage (when using std::make_shared), or separate. We
    // can't observe the difference, however, so assume the worst.
    return p ? MallocUsage(sizeof(X)) + MallocUsage(sizeof(stl_shared_counter))
             : 0;
}

// Boost data structures

template <typename X> struct unordered_node : private X {
private:
    void *ptr;
};

template <typename X, typename Hasher, typename Eq, typename A>
inline constexpr size_t IncrementalDynamicUsage(const std::unordered_set<X, Hasher, Eq, A> &) {
    return MallocUsage(sizeof(unordered_node<X>));
}

template <typename X, typename Hasher, typename Eq, typename A>
inline size_t DynamicUsage(const std::unordered_set<X, Hasher, Eq, A> &s) {
    return IncrementalDynamicUsage(s) * s.size() +
           MallocUsage(sizeof(void *) * s.bucket_count());
}

template <typename X, typename Y, typename Hasher, typename Eq, typename A>
inline constexpr size_t IncrementalDynamicUsage(const std::unordered_map<X, Y, Hasher, Eq, A> &) {
    return MallocUsage(sizeof(unordered_node<std::pair<const X, Y>>));
}

template <typename X, typename Y, typename Hasher, typename Eq, typename A>
inline size_t DynamicUsage(const std::unordered_map<X, Y, Hasher, Eq, A> &m) {
    return IncrementalDynamicUsage(m) * m.size() +
           MallocUsage(sizeof(void *) * m.bucket_count());
}

// Some of our utility wrappers

template <typename T>
inline size_t DynamicUsage(const HeapOptional<T> &p) { return p ? MallocUsage(sizeof(T)) : 0; }

} // namespace memusage
