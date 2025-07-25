// Copyright (c) 2014-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

static const int64_t DEFAULT_MAX_TIME_ADJUSTMENT = 70 * 60;

class CNetAddr;

/**
 * Median filter over a stream of values.
 * Returns the median of the last N numbers
 */
template <typename T> class CMedianFilter {
private:
    std::vector<T> vValues;
    std::vector<T> vSorted;
    unsigned int nSize;

public:
    CMedianFilter(unsigned int _size, T initial_value) : nSize(_size) {
        vValues.reserve(_size);
        vValues.push_back(initial_value);
        vSorted = vValues;
    }

    void input(T value) {
        if (vValues.size() == nSize) {
            vValues.erase(vValues.begin());
        }
        vValues.push_back(value);

        vSorted.resize(vValues.size());
        std::copy(vValues.begin(), vValues.end(), vSorted.begin());
        std::sort(vSorted.begin(), vSorted.end());
    }

    T median() const {
        int vSortedSize = vSorted.size();
        assert(vSortedSize > 0);
        if (vSortedSize & 1) {
            // Odd number of elements
            return vSorted[vSortedSize / 2];
        } else {
            // Even number of elements
            auto left = vSorted[vSortedSize / 2 - 1];
            auto right = vSorted[vSortedSize / 2];
            return left / 2 + right / 2 + (left & right & 1);
        }
    }

    int size() const { return int(vValues.size()); }

    /// Returns a reference to the internal "sorted" vector.
    /// It is an error to iterate over this reference's data while also
    /// mixing-in calls to input(). If you need to do that, take a copy.
    const std::vector<T> & sorted() const { return vSorted; }
};

/** Functions to keep track of adjusted P2P time */
int64_t GetTimeOffset();
int64_t GetAdjustedTime();
void AddTimeData(const CNetAddr &ip, int64_t nTime);
