// Copyright (c) 2012-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dbwrapper.h>

#include <random.h>
#include <uint256.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <memory>

// Test if a string consists entirely of null characters
static bool is_null_key(const std::vector<uint8_t> &key) {
    bool isnull = true;

    for (unsigned int i = 0; i < key.size(); i++) {
        isnull &= (key[i] == '\x00');
    }

    return isnull;
}

BOOST_FIXTURE_TEST_SUITE(dbwrapper_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(dbwrapper) {
    // Perform tests both obfuscated and non-obfuscated.
    for (const bool obfuscate : {false, true}) {
        fs::path ph = SetDataDir(
            std::string("dbwrapper").append(obfuscate ? "_true" : "_false"));
        CDBWrapper dbw(ph, (1 << 20), true, false, obfuscate);
        char key = 'k';
        uint256 in = InsecureRand256();
        uint256 res;

        // Ensure that we're doing real obfuscation when obfuscate=true
        BOOST_CHECK(obfuscate !=
                    is_null_key(dbwrapper_private::GetObfuscateKey(dbw)));

        BOOST_CHECK(dbw.Write(key, in));
        BOOST_CHECK(dbw.Read(key, res));
        BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
    }
}

// Test batch operations
BOOST_AUTO_TEST_CASE(dbwrapper_batch) {
    // Perform tests both obfuscated and non-obfuscated.
    for (const bool obfuscate : {false, true}) {
        fs::path ph = SetDataDir(std::string("dbwrapper_batch")
                                     .append(obfuscate ? "_true" : "_false"));
        CDBWrapper dbw(ph, (1 << 20), true, false, obfuscate);

        char key = 'i';
        uint256 in = InsecureRand256();
        char key2 = 'j';
        uint256 in2 = InsecureRand256();
        char key3 = 'k';
        uint256 in3 = InsecureRand256();

        uint256 res;
        CDBBatch batch(dbw);

        batch.Write(key, in);
        batch.Write(key2, in2);
        batch.Write(key3, in3);

        // Remove key3 before it's even been written
        batch.Erase(key3);

        BOOST_CHECK(dbw.WriteBatch(batch));

        BOOST_CHECK(dbw.Read(key, res));
        BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
        BOOST_CHECK(dbw.Read(key2, res));
        BOOST_CHECK_EQUAL(res.ToString(), in2.ToString());

        // key3 should've never been written
        BOOST_CHECK(dbw.Read(key3, res) == false);
    }
}

BOOST_AUTO_TEST_CASE(dbwrapper_iterator) {
    // Perform tests both obfuscated and non-obfuscated.
    for (const bool obfuscate : {false, true}) {
        fs::path ph = SetDataDir(std::string("dbwrapper_iterator")
                                     .append(obfuscate ? "_true" : "_false"));
        CDBWrapper dbw(ph, (1 << 20), true, false, obfuscate);

        // The two keys are intentionally chosen for ordering
        char key = 'j';
        uint256 in = InsecureRand256();
        BOOST_CHECK(dbw.Write(key, in));
        char key2 = 'k';
        uint256 in2 = InsecureRand256();
        BOOST_CHECK(dbw.Write(key2, in2));

        std::unique_ptr<CDBIterator> it(
            const_cast<CDBWrapper &>(dbw).NewIterator());

        // Be sure to seek past the obfuscation key (if it exists)
        it->Seek(key);

        char key_res;
        uint256 val_res;

        it->GetKey(key_res);
        it->GetValue(val_res);
        BOOST_CHECK_EQUAL(key_res, key);
        BOOST_CHECK_EQUAL(val_res.ToString(), in.ToString());

        it->Next();

        it->GetKey(key_res);
        it->GetValue(val_res);
        BOOST_CHECK_EQUAL(key_res, key2);
        BOOST_CHECK_EQUAL(val_res.ToString(), in2.ToString());

        it->Next();
        BOOST_CHECK_EQUAL(it->Valid(), false);
    }
}

BOOST_AUTO_TEST_CASE(dbwrapper_iterator_snapshot) {
    // Perform tests to ensure snapshotted iterators only see the old view of the db
    const fs::path ph = SetDataDir(std::string("dbwrapper_iterator_snapshot"));
    CDBWrapper dbw(ph, (1 << 20), true, false, false);

    const std::vector<std::pair<char, uint256>> original_view{{
        { 'a', InsecureRand256() },
        { 'c', InsecureRand256() },
        { 'e', InsecureRand256() },
    }};

    const std::vector<std::pair<char, uint256>> updated_view{{
        original_view[0],
        { 'b', InsecureRand256() },
    }};

    for (const auto & [key, value] : original_view) {
        dbw.Write(key, value);
    }

    std::unique_ptr<CDBIterator> it_snapshot(dbw.NewIterator(true)); // snapshotted to current view, won't see updates

    auto GetAll = [](CDBIterator *it) {
        std::vector<std::pair<char, uint256>> found;
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            char k;
            uint256 v;
            it->GetKey(k);
            it->GetValue(v);
            found.emplace_back(k, v);
        }
        return found;
    };

    BOOST_CHECK(GetAll(it_snapshot.get()) == original_view);

    // Update db: delete 2 keys, add 1 new key, snapshotted iterator should not see this update
    dbw.Erase('c');
    dbw.Erase('e');
    dbw.Write(updated_view[1].first, updated_view[1].second);

    std::unique_ptr<CDBIterator> it_updated(dbw.NewIterator()); // non-snapshotted (latest view)

    // ensure the two iterators have the views of the DB we expect
    BOOST_CHECK(GetAll(it_updated.get()) == updated_view);
    BOOST_CHECK(GetAll(it_snapshot.get()) != updated_view);
    BOOST_CHECK(GetAll(it_snapshot.get()) == original_view);
}

// Test that we do not obfuscation if there is existing data.
BOOST_AUTO_TEST_CASE(existing_data_no_obfuscate) {
    // We're going to share this fs::path between two wrappers
    fs::path ph = SetDataDir("existing_data_no_obfuscate");
    create_directories(ph);

    // Set up a non-obfuscated wrapper to write some initial data.
    std::unique_ptr<CDBWrapper> dbw =
        std::make_unique<CDBWrapper>(ph, (1 << 10), false, false, false);
    char key = 'k';
    uint256 in = InsecureRand256();
    uint256 res;

    BOOST_CHECK(dbw->Write(key, in));
    BOOST_CHECK(dbw->Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());

    // Call the destructor to free leveldb LOCK
    dbw.reset();

    // Now, set up another wrapper that wants to obfuscate the same directory
    CDBWrapper odbw(ph, (1 << 10), false, false, true);

    // Check that the key/val we wrote with unobfuscated wrapper exists and
    // is readable.
    uint256 res2;
    BOOST_CHECK(odbw.Read(key, res2));
    BOOST_CHECK_EQUAL(res2.ToString(), in.ToString());

    // There should be existing data
    BOOST_CHECK(!odbw.IsEmpty());
    // The key should be an empty string
    BOOST_CHECK(is_null_key(dbwrapper_private::GetObfuscateKey(odbw)));

    uint256 in2 = InsecureRand256();
    uint256 res3;

    // Check that we can write successfully
    BOOST_CHECK(odbw.Write(key, in2));
    BOOST_CHECK(odbw.Read(key, res3));
    BOOST_CHECK_EQUAL(res3.ToString(), in2.ToString());
}

// Ensure that we start obfuscating during a reindex.
BOOST_AUTO_TEST_CASE(existing_data_reindex) {
    // We're going to share this fs::path between two wrappers
    fs::path ph = SetDataDir("existing_data_reindex");
    create_directories(ph);

    // Set up a non-obfuscated wrapper to write some initial data.
    std::unique_ptr<CDBWrapper> dbw =
        std::make_unique<CDBWrapper>(ph, (1 << 10), false, false, false);
    char key = 'k';
    uint256 in = InsecureRand256();
    uint256 res;

    BOOST_CHECK(dbw->Write(key, in));
    BOOST_CHECK(dbw->Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());

    // Call the destructor to free leveldb LOCK
    dbw.reset();

    // Simulate a -reindex by wiping the existing data store
    CDBWrapper odbw(ph, (1 << 10), false, true, true);

    // Check that the key/val we wrote with unobfuscated wrapper doesn't exist
    uint256 res2;
    BOOST_CHECK(!odbw.Read(key, res2));
    BOOST_CHECK(!is_null_key(dbwrapper_private::GetObfuscateKey(odbw)));

    uint256 in2 = InsecureRand256();
    uint256 res3;

    // Check that we can write successfully
    BOOST_CHECK(odbw.Write(key, in2));
    BOOST_CHECK(odbw.Read(key, res3));
    BOOST_CHECK_EQUAL(res3.ToString(), in2.ToString());
}

BOOST_AUTO_TEST_CASE(iterator_ordering) {
    fs::path ph = SetDataDir("iterator_ordering");
    CDBWrapper dbw(ph, (1 << 20), true, false, false);
    for (int x = 0x00; x < 256; ++x) {
        uint8_t key = x;
        uint32_t value = x * x;
        if (!(x & 1)) {
            BOOST_CHECK(dbw.Write(key, value));
        }
    }

    // Check that creating an iterator creates a snapshot
    std::unique_ptr<CDBIterator> it(
        const_cast<CDBWrapper &>(dbw).NewIterator());

    for (unsigned int x = 0x00; x < 256; ++x) {
        uint8_t key = x;
        uint32_t value = x * x;
        if (x & 1) {
            BOOST_CHECK(dbw.Write(key, value));
        }
    }

    for (const int seek_start : {0x00, 0x80}) {
        it->Seek((uint8_t)seek_start);
        for (unsigned int x = seek_start; x < 255; ++x) {
            uint8_t key;
            uint32_t value;
            BOOST_CHECK(it->Valid());
            // Avoid spurious errors about invalid iterator's  key and value in
            // case of failure
            if (!it->Valid()) {
                break;
            }
            BOOST_CHECK(it->GetKey(key));
            if (x & 1) {
                BOOST_CHECK_EQUAL(key, x + 1);
                continue;
            }
            BOOST_CHECK(it->GetValue(value));
            BOOST_CHECK_EQUAL(key, x);
            BOOST_CHECK_EQUAL(value, x * x);
            it->Next();
        }
        BOOST_CHECK(!it->Valid());
    }
}

struct StringContentsSerializer {
    // Used to make two serialized objects the same while letting them have
    // different lengths. This is a terrible idea.
    std::string str;
    StringContentsSerializer() {}
    explicit StringContentsSerializer(const std::string &inp) : str(inp) {}

    StringContentsSerializer &operator+=(const std::string &s) {
        str += s;
        return *this;
    }
    StringContentsSerializer &operator+=(const StringContentsSerializer &s) {
        return *this += s.str;
    }

    template <typename Stream> void Serialize(Stream &s) const {
        for (size_t i = 0; i < str.size(); i++) {
            s << str[i];
        }
    }

    template <typename Stream> void Unserialize(Stream &s) {
        str.clear();
        char c = 0;
        while (true) {
            try {
                s >> c;
                str.push_back(c);
            } catch (const std::ios_base::failure &) {
                break;
            }
        }
    }
};

BOOST_AUTO_TEST_CASE(iterator_string_ordering) {
    char buf[10];

    fs::path ph = SetDataDir("iterator_string_ordering");
    CDBWrapper dbw(ph, (1 << 20), true, false, false);
    for (int x = 0x00; x < 10; ++x) {
        for (int y = 0; y < 10; y++) {
            snprintf(buf, sizeof(buf), "%d", x);
            StringContentsSerializer key(buf);
            for (int z = 0; z < y; z++) {
                key += key;
            }
            uint32_t value = x * x;
            BOOST_CHECK(dbw.Write(key, value));
        }
    }

    std::unique_ptr<CDBIterator> it(
        const_cast<CDBWrapper &>(dbw).NewIterator());
    for (const int seek_start : {0, 5}) {
        snprintf(buf, sizeof(buf), "%d", seek_start);
        StringContentsSerializer seek_key(buf);
        it->Seek(seek_key);
        for (unsigned int x = seek_start; x < 10; ++x) {
            for (int y = 0; y < 10; y++) {
                snprintf(buf, sizeof(buf), "%d", x);
                std::string exp_key(buf);
                for (int z = 0; z < y; z++) {
                    exp_key += exp_key;
                }
                StringContentsSerializer key;
                uint32_t value;
                BOOST_CHECK(it->Valid());
                // Avoid spurious errors about invalid iterator's key and value
                // in case of failure
                if (!it->Valid()) {
                    break;
                }
                BOOST_CHECK(it->GetKey(key));
                BOOST_CHECK(it->GetValue(value));
                BOOST_CHECK_EQUAL(key.str, exp_key);
                BOOST_CHECK_EQUAL(value, x * x);
                it->Next();
            }
        }
        BOOST_CHECK(!it->Valid());
    }
}

BOOST_AUTO_TEST_SUITE_END()
