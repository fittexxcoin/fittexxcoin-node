// Copyright (c) 2015-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

/**
 * An RAII-style reverse lock. Unlocks on construction and locks on destruction.
 */
template <typename Lock> class reverse_lock {
public:
    explicit reverse_lock(Lock &_lock) : lock(_lock) {
        _lock.unlock();
        _lock.swap(templock);
    }

    ~reverse_lock() {
        templock.lock();
        templock.swap(lock);
    }

private:
    reverse_lock(reverse_lock const &);
    reverse_lock &operator=(reverse_lock const &);

    Lock &lock;
    Lock templock;
};
