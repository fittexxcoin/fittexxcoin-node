// Copyright (c) 2019 The Fittexxcoin Core developers
// Copyright (c) 2019-2020 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <flatfile.h>

#include <clientversion.h>
#include <streams.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(flatfile_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(flatfile_filename) {
    auto data_dir = SetDataDir("flatfile_test");

    FlatFilePos pos(456, 789);

    FlatFileSeq seq1(data_dir, "a", 16 * 1024);
    BOOST_CHECK_EQUAL(seq1.FileName(pos), data_dir / "a00456.dat");

    FlatFileSeq seq2(data_dir / "a", "b", 16 * 1024);
    BOOST_CHECK_EQUAL(seq2.FileName(pos), data_dir / "a" / "b00456.dat");
}

BOOST_AUTO_TEST_CASE(flatfile_open) {
    auto data_dir = SetDataDir("flatfile_test");
    FlatFileSeq seq(data_dir, "a", 16 * 1024);

    std::string line1(
        "A purely peer-to-peer version of electronic cash would allow online "
        "payments to be sent directly from one party to another without going "
        "through a financial institution.");
    std::string line2("Digital signatures provide part of the solution, but "
                      "the main benefits are lost if a trusted third party is "
                      "still required to prevent double-spending.");

    size_t pos1 = 0;
    size_t pos2 = pos1 + GetSerializeSize(line1, CLIENT_VERSION);

    // Write first line to file.
    {
        CAutoFile file(seq.Open(FlatFilePos(0, pos1)), SER_DISK,
                       CLIENT_VERSION);
        file << LIMITED_STRING(line1, 256);
    }

    // Attempt to append to file opened in read-only mode.
    {
        CAutoFile file(seq.Open(FlatFilePos(0, pos2), true), SER_DISK,
                       CLIENT_VERSION);
        BOOST_CHECK_THROW(file << LIMITED_STRING(line2, 256),
                          std::ios_base::failure);
    }

    // Append second line to file.
    {
        CAutoFile file(seq.Open(FlatFilePos(0, pos2)), SER_DISK,
                       CLIENT_VERSION);
        file << LIMITED_STRING(line2, 256);
    }

    // Read text from file in read-only mode.
    {
        std::string text;
        CAutoFile file(seq.Open(FlatFilePos(0, pos1), true), SER_DISK,
                       CLIENT_VERSION);

        file >> LIMITED_STRING(text, 256);
        BOOST_CHECK_EQUAL(text, line1);

        file >> LIMITED_STRING(text, 256);
        BOOST_CHECK_EQUAL(text, line2);
    }

    // Read text from file with position offset.
    {
        std::string text;
        CAutoFile file(seq.Open(FlatFilePos(0, pos2)), SER_DISK,
                       CLIENT_VERSION);

        file >> LIMITED_STRING(text, 256);
        BOOST_CHECK_EQUAL(text, line2);
    }

    // Ensure another file in the sequence has no data.
    {
        std::string text;
        CAutoFile file(seq.Open(FlatFilePos(1, pos2)), SER_DISK,
                       CLIENT_VERSION);
        BOOST_CHECK_THROW(file >> LIMITED_STRING(text, 256),
                          std::ios_base::failure);
    }
}

BOOST_AUTO_TEST_CASE(flatfile_allocate) {
    auto data_dir = SetDataDir("flatfile_test");
    FlatFileSeq seq(data_dir, "a", 100);

    bool out_of_space;

    BOOST_CHECK_EQUAL(seq.Allocate(FlatFilePos(0, 0), 1, out_of_space), 100);
    BOOST_CHECK_EQUAL(fs::file_size(seq.FileName(FlatFilePos(0, 0))), 100);
    BOOST_CHECK(!out_of_space);

    BOOST_CHECK_EQUAL(seq.Allocate(FlatFilePos(0, 99), 1, out_of_space), 0);
    BOOST_CHECK_EQUAL(fs::file_size(seq.FileName(FlatFilePos(0, 99))), 100);
    BOOST_CHECK(!out_of_space);

    BOOST_CHECK_EQUAL(seq.Allocate(FlatFilePos(0, 99), 2, out_of_space), 101);
    BOOST_CHECK_EQUAL(fs::file_size(seq.FileName(FlatFilePos(0, 99))), 200);
    BOOST_CHECK(!out_of_space);
}

BOOST_AUTO_TEST_CASE(flatfile_flush) {
    auto data_dir = SetDataDir("flatfile_test");
    FlatFileSeq seq(data_dir, "a", 100);

    bool out_of_space;
    seq.Allocate(FlatFilePos(0, 0), 1, out_of_space);

    // Flush without finalize should not truncate file.
    seq.Flush(FlatFilePos(0, 1));
    BOOST_CHECK_EQUAL(fs::file_size(seq.FileName(FlatFilePos(0, 1))), 100);

    // Flush with finalize should truncate file.
    seq.Flush(FlatFilePos(0, 1), true);
    BOOST_CHECK_EQUAL(fs::file_size(seq.FileName(FlatFilePos(0, 1))), 1);
}

BOOST_AUTO_TEST_SUITE_END()
