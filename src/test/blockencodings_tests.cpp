// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockencodings.h>

#include <chainparams.h>
#include <config.h>
#include <consensus/merkle.h>
#include <pow.h>
#include <random.h>
#include <streams.h>
#include <txmempool.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

static std::vector<std::pair<TxHash, CTransactionRef>> extra_txn;

struct RegtestingSetup : public TestingSetup {
    RegtestingSetup() : TestingSetup(CBaseChainParams::REGTEST) {}
};

BOOST_FIXTURE_TEST_SUITE(blockencodings_tests, RegtestingSetup)

static COutPoint InsecureRandOutPoint() {
    return COutPoint(TxId(InsecureRand256()), 0);
}

static CBlock BuildBlockTestCase() {
    CBlock block;
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].scriptSig.resize(10);
    tx.vout.resize(1);
    tx.vout[0].nValue = 42 * SATOSHI;

    block.vtx.resize(3);
    block.vtx[0] = MakeTransactionRef(tx);
    block.nVersion = 42;
    block.hashPrevBlock = BlockHash(InsecureRand256());
    block.nBits = 0x207fffff;

    tx.vin[0].prevout = InsecureRandOutPoint();
    block.vtx[1] = MakeTransactionRef(tx);

    tx.vin.resize(10);
    for (size_t i = 0; i < tx.vin.size(); i++) {
        tx.vin[i].prevout = InsecureRandOutPoint();
    }
    block.vtx[2] = MakeTransactionRef(tx);

    bool mutated;
    block.hashMerkleRoot = BlockMerkleRoot(block, &mutated);
    assert(!mutated);

    GlobalConfig config;
    const Consensus::Params &params = config.GetChainParams().GetConsensus();
    while (!CheckProofOfWork(block.GetHash(), block.nBits, params)) {
        ++block.nNonce;
    }

    return block;
}

// BOOST_CHECK_EXCEPTION predicates to check the exception message
class HasReason {
public:
    HasReason(const std::string &reason) : m_reason(reason) {}
    bool operator()(const std::exception &e) const {
        return std::string(e.what()).find(m_reason) != std::string::npos;
    };

private:
    const std::string m_reason;
};

// Number of shared use_counts we expect for a tx we haven't touched
// (block + mempool + our copy from the GetSharedTx call)
constexpr long SHARED_TX_OFFSET{3};

BOOST_AUTO_TEST_CASE(SimpleRoundTripTest) {
    CTxMemPool pool;
    TestMemPoolEntryHelper entry;
    CBlock block(BuildBlockTestCase());

    LOCK2(cs_main, pool.cs);
    pool.addUnchecked(entry.FromTx(block.vtx[2]));
    BOOST_CHECK_EQUAL(
        pool.mapTx.find(block.vtx[2]->GetId())->GetSharedTx().use_count(),
        SHARED_TX_OFFSET + 0);

    // Do a simple ShortTxIDs RT
    {
        CBlockHeaderAndShortTxIDs shortIDs(block);

        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << shortIDs;

        CBlockHeaderAndShortTxIDs shortIDs2;
        stream >> shortIDs2;

        PartiallyDownloadedBlock partialBlock(GetConfig(), &pool);
        BOOST_CHECK(partialBlock.InitData(shortIDs2, extra_txn) ==
                    READ_STATUS_OK);
        BOOST_CHECK(partialBlock.IsTxAvailable(0));
        BOOST_CHECK(!partialBlock.IsTxAvailable(1));
        BOOST_CHECK(partialBlock.IsTxAvailable(2));

        BOOST_CHECK_EQUAL(
            pool.mapTx.find(block.vtx[2]->GetId())->GetSharedTx().use_count(),
            SHARED_TX_OFFSET + 1);

        size_t poolSize = pool.size();
        pool.removeRecursive(*block.vtx[2]);
        BOOST_CHECK_EQUAL(pool.size(), poolSize - 1);

        CBlock block2;
        {
            // No transactions.
            PartiallyDownloadedBlock tmp = partialBlock;
            BOOST_CHECK(partialBlock.FillBlock(block2, {}) ==
                        READ_STATUS_INVALID);
            partialBlock = tmp;
        }

        // Wrong transaction
        {
            // Current implementation doesn't check txn here, but don't require
            // that.
            PartiallyDownloadedBlock tmp = partialBlock;
            partialBlock.FillBlock(block2, {block.vtx[2]});
            partialBlock = tmp;
        }
        bool mutated;
        BOOST_CHECK(block.hashMerkleRoot != BlockMerkleRoot(block2, &mutated));

        CBlock block3;
        BOOST_CHECK(partialBlock.FillBlock(block3, {block.vtx[1]}) ==
                    READ_STATUS_OK);
        BOOST_CHECK_EQUAL(block.GetHash().ToString(),
                          block3.GetHash().ToString());
        BOOST_CHECK_EQUAL(block.hashMerkleRoot.ToString(),
                          BlockMerkleRoot(block3, &mutated).ToString());
        BOOST_CHECK(!mutated);
    }
}

class TestHeaderAndShortIDs {
    // Utility to encode custom CBlockHeaderAndShortTxIDs
public:
    CBlockHeader header;
    uint64_t nonce;
    std::vector<uint64_t> shorttxids;
    std::vector<PrefilledTransaction> prefilledtxn;

    explicit TestHeaderAndShortIDs(const CBlockHeaderAndShortTxIDs &orig) {
        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << orig;
        stream >> *this;
    }
    explicit TestHeaderAndShortIDs(const CBlock &block)
        : TestHeaderAndShortIDs(CBlockHeaderAndShortTxIDs(block)) {}

    uint64_t GetShortID(const TxHash &txhash) const {
        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << *this;
        CBlockHeaderAndShortTxIDs base;
        stream >> base;
        return base.GetShortID(txhash);
    }

    SERIALIZE_METHODS(TestHeaderAndShortIDs, obj) {
        READWRITE(obj.header, obj.nonce,
                  Using<VectorFormatter<CustomUintFormatter<CBlockHeaderAndShortTxIDs::SHORTTXIDS_LENGTH>>>(obj.shorttxids),
                  obj.prefilledtxn);
    }
};

BOOST_AUTO_TEST_CASE(NonCoinbasePreforwardRTTest) {
    CTxMemPool pool;
    TestMemPoolEntryHelper entry;
    CBlock block(BuildBlockTestCase());

    LOCK2(cs_main, pool.cs);
    pool.addUnchecked(entry.FromTx(block.vtx[2]));
    BOOST_CHECK_EQUAL(
        pool.mapTx.find(block.vtx[2]->GetId())->GetSharedTx().use_count(),
        SHARED_TX_OFFSET + 0);

    TxId txid;

    // Test with pre-forwarding tx 1, but not coinbase
    {
        TestHeaderAndShortIDs shortIDs(block);
        shortIDs.prefilledtxn.resize(1);
        shortIDs.prefilledtxn[0] = {1, block.vtx[1]};
        shortIDs.shorttxids.resize(2);
        shortIDs.shorttxids[0] = shortIDs.GetShortID(block.vtx[0]->GetHash());
        shortIDs.shorttxids[1] = shortIDs.GetShortID(block.vtx[2]->GetHash());

        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << shortIDs;

        CBlockHeaderAndShortTxIDs shortIDs2;
        stream >> shortIDs2;

        PartiallyDownloadedBlock partialBlock(GetConfig(), &pool);
        BOOST_CHECK(partialBlock.InitData(shortIDs2, extra_txn) ==
                    READ_STATUS_OK);
        BOOST_CHECK(!partialBlock.IsTxAvailable(0));
        BOOST_CHECK(partialBlock.IsTxAvailable(1));
        BOOST_CHECK(partialBlock.IsTxAvailable(2));

        // +1 because of partialBlock
        BOOST_CHECK_EQUAL(
            pool.mapTx.find(block.vtx[2]->GetId())->GetSharedTx().use_count(),
            SHARED_TX_OFFSET + 1);

        CBlock block2;
        {
            // No transactions.
            PartiallyDownloadedBlock tmp = partialBlock;
            BOOST_CHECK(partialBlock.FillBlock(block2, {}) ==
                        READ_STATUS_INVALID);
            partialBlock = tmp;
        }

        // Wrong transaction
        {
            // Current implementation doesn't check txn here, but don't require
            // that.
            PartiallyDownloadedBlock tmp = partialBlock;
            partialBlock.FillBlock(block2, {block.vtx[1]});
            partialBlock = tmp;
        }

        // +2 because of partialBlock and block2
        BOOST_CHECK_EQUAL(
            pool.mapTx.find(block.vtx[2]->GetId())->GetSharedTx().use_count(),
            SHARED_TX_OFFSET + 2);
        bool mutated;
        BOOST_CHECK(block.hashMerkleRoot != BlockMerkleRoot(block2, &mutated));

        CBlock block3;
        PartiallyDownloadedBlock partialBlockCopy = partialBlock;
        BOOST_CHECK(partialBlock.FillBlock(block3, {block.vtx[0]}) ==
                    READ_STATUS_OK);
        BOOST_CHECK_EQUAL(block.GetHash().ToString(),
                          block3.GetHash().ToString());
        BOOST_CHECK_EQUAL(block.hashMerkleRoot.ToString(),
                          BlockMerkleRoot(block3, &mutated).ToString());
        BOOST_CHECK(!mutated);

        // +3 because of partialBlock and block2 and block3
        BOOST_CHECK_EQUAL(
            pool.mapTx.find(block.vtx[2]->GetId())->GetSharedTx().use_count(),
            SHARED_TX_OFFSET + 3);

        txid = block.vtx[2]->GetId();
        block.vtx.clear();
        block2.vtx.clear();
        block3.vtx.clear();

        // + 1 because of partialBlock; -1 because of block.
        BOOST_CHECK_EQUAL(pool.mapTx.find(txid)->GetSharedTx().use_count(),
                          SHARED_TX_OFFSET + 1 - 1);
    }

    // -1 because of block
    BOOST_CHECK_EQUAL(pool.mapTx.find(txid)->GetSharedTx().use_count(),
                      SHARED_TX_OFFSET - 1);
}

BOOST_AUTO_TEST_CASE(SufficientPreforwardRTTest) {
    CTxMemPool pool;
    TestMemPoolEntryHelper entry;
    CBlock block(BuildBlockTestCase());

    LOCK2(cs_main, pool.cs);
    pool.addUnchecked(entry.FromTx(block.vtx[1]));
    BOOST_CHECK_EQUAL(
        pool.mapTx.find(block.vtx[1]->GetId())->GetSharedTx().use_count(),
        SHARED_TX_OFFSET + 0);

    TxId txid;

    // Test with pre-forwarding coinbase + tx 2 with tx 1 in mempool
    {
        TestHeaderAndShortIDs shortIDs(block);
        shortIDs.prefilledtxn.resize(2);
        shortIDs.prefilledtxn[0] = {0, block.vtx[0]};
        // id == 1 as it is 1 after index 1
        shortIDs.prefilledtxn[1] = {1, block.vtx[2]};
        shortIDs.shorttxids.resize(1);
        shortIDs.shorttxids[0] = shortIDs.GetShortID(block.vtx[1]->GetHash());

        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << shortIDs;

        CBlockHeaderAndShortTxIDs shortIDs2;
        stream >> shortIDs2;

        PartiallyDownloadedBlock partialBlock(GetConfig(), &pool);
        BOOST_CHECK(partialBlock.InitData(shortIDs2, extra_txn) ==
                    READ_STATUS_OK);
        BOOST_CHECK(partialBlock.IsTxAvailable(0));
        BOOST_CHECK(partialBlock.IsTxAvailable(1));
        BOOST_CHECK(partialBlock.IsTxAvailable(2));

        BOOST_CHECK_EQUAL(
            pool.mapTx.find(block.vtx[1]->GetId())->GetSharedTx().use_count(),
            SHARED_TX_OFFSET + 1);

        CBlock block2;
        PartiallyDownloadedBlock partialBlockCopy = partialBlock;
        BOOST_CHECK(partialBlock.FillBlock(block2, {}) == READ_STATUS_OK);
        BOOST_CHECK_EQUAL(block.GetHash().ToString(),
                          block2.GetHash().ToString());
        bool mutated;
        BOOST_CHECK_EQUAL(block.hashMerkleRoot.ToString(),
                          BlockMerkleRoot(block2, &mutated).ToString());
        BOOST_CHECK(!mutated);

        txid = block.vtx[1]->GetId();
        block.vtx.clear();
        block2.vtx.clear();

        // + 1 because of partialBlock; -1 because of block.
        BOOST_CHECK_EQUAL(pool.mapTx.find(txid)->GetSharedTx().use_count(),
                          SHARED_TX_OFFSET + 1 - 1);
    }

    // -1 because of block
    BOOST_CHECK_EQUAL(pool.mapTx.find(txid)->GetSharedTx().use_count(),
                      SHARED_TX_OFFSET - 1);
}

BOOST_AUTO_TEST_CASE(EmptyBlockRoundTripTest) {
    CTxMemPool pool;
    CMutableTransaction coinbase;
    coinbase.vin.resize(1);
    coinbase.vin[0].scriptSig.resize(10);
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = 42 * SATOSHI;

    CBlock block;
    block.vtx.resize(1);
    block.vtx[0] = MakeTransactionRef(std::move(coinbase));
    block.nVersion = 42;
    block.hashPrevBlock = BlockHash(InsecureRand256());
    block.nBits = 0x207fffff;

    bool mutated;
    block.hashMerkleRoot = BlockMerkleRoot(block, &mutated);
    assert(!mutated);

    GlobalConfig config;
    const Consensus::Params &params = config.GetChainParams().GetConsensus();
    while (!CheckProofOfWork(block.GetHash(), block.nBits, params)) {
        ++block.nNonce;
    }

    // Test simple header round-trip with only coinbase
    {
        CBlockHeaderAndShortTxIDs shortIDs(block);

        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << shortIDs;

        CBlockHeaderAndShortTxIDs shortIDs2;
        stream >> shortIDs2;

        PartiallyDownloadedBlock partialBlock(GetConfig(), &pool);
        BOOST_CHECK(partialBlock.InitData(shortIDs2, extra_txn) ==
                    READ_STATUS_OK);
        BOOST_CHECK(partialBlock.IsTxAvailable(0));

        CBlock block2;
        std::vector<CTransactionRef> vtx_missing;
        BOOST_CHECK(partialBlock.FillBlock(block2, vtx_missing) ==
                    READ_STATUS_OK);
        BOOST_CHECK_EQUAL(block.GetHash().ToString(),
                          block2.GetHash().ToString());
        BOOST_CHECK_EQUAL(block.hashMerkleRoot.ToString(),
                          BlockMerkleRoot(block2, &mutated).ToString());
        BOOST_CHECK(!mutated);
    }
}

BOOST_AUTO_TEST_CASE(TransactionsRequestSerializationTest) {
    BlockTransactionsRequest req1;
    req1.blockhash = BlockHash(InsecureRand256());
    req1.indices.resize(4);
    req1.indices[0] = 0;
    req1.indices[1] = 1;
    req1.indices[2] = 3;
    req1.indices[3] = 4;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << req1;

    BlockTransactionsRequest req2;
    stream >> req2;

    BOOST_CHECK_EQUAL(req1.blockhash.ToString(), req2.blockhash.ToString());
    BOOST_CHECK_EQUAL(req1.indices.size(), req2.indices.size());
    BOOST_CHECK_EQUAL(req1.indices[0], req2.indices[0]);
    BOOST_CHECK_EQUAL(req1.indices[1], req2.indices[1]);
    BOOST_CHECK_EQUAL(req1.indices[2], req2.indices[2]);
    BOOST_CHECK_EQUAL(req1.indices[3], req2.indices[3]);
}

BOOST_AUTO_TEST_CASE(TransactionsRequestDeserializationMaxTest) {
    // Check that the highest legal index is decoded correctly
    BlockTransactionsRequest req0;
    req0.blockhash = BlockHash(InsecureRand256());
    req0.indices.resize(1);

    using indiceType = decltype(req0.indices)::value_type;
    static_assert(MAX_SIZE < std::numeric_limits<indiceType>::max(),
                  "The max payload size cannot fit into the indice type");

    req0.indices[0] = MAX_SIZE;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << req0;

    BlockTransactionsRequest req1;
    stream >> req1;
    BOOST_CHECK_EQUAL(req0.indices.size(), req1.indices.size());
    BOOST_CHECK_EQUAL(req0.indices[0], req1.indices[0]);

    req0.indices[0] += 1;
    stream << req0;
    BlockTransactionsRequest req2;
    BOOST_CHECK_EXCEPTION(stream >> req2, std::ios_base::failure,
                          HasReason("ReadCompactSize(): size too large"));
}

BOOST_AUTO_TEST_CASE(TransactionsRequestDeserializationOverflowTest) {
    // Any set of index deltas that starts with N values that sum to
    // (0x100000000 - N) causes the edge-case overflow that was originally not
    // checked for. Such a request cannot be created by serializing a real
    // BlockTransactionsRequest due to the overflow, so here we'll serialize
    // from raw deltas. This can only occur if MAX_SIZE is greater than the
    // maximum value for that the indice type can handle.
    BlockTransactionsRequest req0;
    req0.blockhash = BlockHash(InsecureRand256());
    req0.indices.resize(3);

    using indiceType = decltype(req0.indices)::value_type;
    static_assert(std::is_same<indiceType, uint32_t>::value,
                  "This test expects the indice type to be an uint32_t");

    req0.indices[0] = 0x7000;
    req0.indices[1] = 0x100000000 - 0x7000 - 2;
    req0.indices[2] = 0;
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << req0.blockhash;
    WriteCompactSize(stream, req0.indices.size());
    WriteCompactSize(stream, req0.indices[0]);
    WriteCompactSize(stream, req0.indices[1]);
    WriteCompactSize(stream, req0.indices[2]);

    BlockTransactionsRequest req1;
    // If MAX_SIZE is the limiting factor, the deserialization should throw.
    // Otherwise make sure that the overflow edge-case is under control.
    BOOST_CHECK_EXCEPTION(stream >> req1, std::ios_base::failure,
                          HasReason((MAX_SIZE < req0.indices[1])
                                        ? "ReadCompactSize(): size too large"
                                        : "indices overflowed 32 bits"));
}

BOOST_AUTO_TEST_CASE(CompactBlocksSerializationLoadTest) {
    // Ensure we can serialize and deserialize compact blocks with at least
    // LARGE_NUMBER_OF_TXS transactions.
    auto constexpr LARGE_NUMBER_OF_TXS{10000000};

    CBlock block(BuildBlockTestCase());
    block.vtx.resize(LARGE_NUMBER_OF_TXS);
    std::fill(std::begin(block.vtx), std::end(block.vtx), block.vtx[0]);
    CBlockHeaderAndShortTxIDs shortIDs(block);

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << shortIDs;
    CBlockHeaderAndShortTxIDs shortIDs2;
    stream >> shortIDs2;

    BOOST_CHECK_EQUAL(shortIDs.BlockTxCount(), LARGE_NUMBER_OF_TXS);
    BOOST_CHECK_EQUAL(shortIDs2.BlockTxCount(), LARGE_NUMBER_OF_TXS);
}

BOOST_AUTO_TEST_CASE(ToStringTest) {
    CBlock block;

    BOOST_CHECK_EQUAL(block.ToString(),
        "CBlock(hash=00000000ad94e3880810e8d74ba77a2c4beb8e84707b89a8baed89ec40431906, ver=0x00000000, "
        "hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, "
        "hashMerkleRoot=ccfeb2a869400272dc0562a210c05fb32de725092f5a2bc55f6999f2bb032792, "
        "nTime=0, nBits=00000000, nNonce=0, vtx=0)\n");

    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].scriptSig.resize(10);
    tx.vout.resize(1);
    tx.vout[0].nValue = 42 * SATOSHI;

    block.vtx.resize(3);
    block.vtx[0] = MakeTransactionRef(tx);
    block.nVersion = 42;
    block.hashPrevBlock = BlockHash(ArithToUint256(1234567890));
    block.nBits = 0x1d00ffff;

    tx.vin[0].prevout = COutPoint(TxId(ArithToUint256(9876543210)), 0);
    block.vtx[1] = MakeTransactionRef(tx);

    tx.vin.resize(10);
    for (size_t i = 0; i < tx.vin.size(); i++) {
        tx.vin[i].prevout = COutPoint(TxId(ArithToUint256(1357986420)), 0);
    }
    block.vtx[2] = MakeTransactionRef(tx);

    BOOST_CHECK_EQUAL(block.ToString(),
        "CBlock(hash=edad8495e648009f06ad566e3246c4cc0530a633c35fd0cf71b450110885b635, ver=0x0000002a, "
        "hashPrevBlock=00000000000000000000000000000000000000000000000000000000499602d2, "
        "hashMerkleRoot=0000000000000000000000000000000000000000000000000000000000000000, nTime=0, nBits=207fffff, nNonce=0, vtx=3)"
        "\n  CTransaction(txid=f598013009, ver=2, vin.size=1, vout.size=1, nLockTime=0)"
        "\n    CTxIn(COutPoint(0000000000, 4294967295), coinbase 00000000000000000000)"
        "\n    CTxOut(nValue=0.00000042, scriptPubKey=)"
        "\n"
        "\n  CTransaction(txid=28c3daba31, ver=2, vin.size=1, vout.size=1, nLockTime=0)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=00000000000000000000)"
        "\n    CTxOut(nValue=0.00000042, scriptPubKey=)"
        "\n"
        "\n  CTransaction(txid=7418cfcbb6, ver=2, vin.size=10, vout.size=1, nLockTime=0)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=00000000000000000000)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxOut(nValue=0.00000042, scriptPubKey=)"
        "\n"
        "\n");

    bool mutated;
    block.hashMerkleRoot = BlockMerkleRoot(block, &mutated);
    assert(!mutated);

    GlobalConfig config;
    const Consensus::Params &params = config.GetChainParams().GetConsensus();
    while (!CheckProofOfWork(block.GetHash(), block.nBits, params)) {
        ++block.nNonce;
    }
    BOOST_CHECK_EQUAL(block.ToString(),
        "CBlock(hash=54bd6ad7054552d536c0bb793164b8410f5f58f10d9340a97a5f31e088917462, ver=0x0000002a, "
        "hashPrevBlock=00000000000000000000000000000000000000000000000000000000499602d2, "
        "hashMerkleRoot=3c29ff8818239743c8999bf24edd3436f3c79f1f8421a0f767ba7bc05e541d53, nTime=0, nBits=207fffff, nNonce=2, vtx=3)"
        "\n  CTransaction(txid=f598013009, ver=2, vin.size=1, vout.size=1, nLockTime=0)"
        "\n    CTxIn(COutPoint(0000000000, 4294967295), coinbase 00000000000000000000)"
        "\n    CTxOut(nValue=0.00000042, scriptPubKey=)"
        "\n"
        "\n  CTransaction(txid=28c3daba31, ver=2, vin.size=1, vout.size=1, nLockTime=0)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=00000000000000000000)"
        "\n    CTxOut(nValue=0.00000042, scriptPubKey=)"
        "\n"
        "\n  CTransaction(txid=7418cfcbb6, ver=2, vin.size=10, vout.size=1, nLockTime=0)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=00000000000000000000)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxIn(COutPoint(0000000000, 0), scriptSig=)"
        "\n    CTxOut(nValue=0.00000042, scriptPubKey=)"
        "\n"
        "\n");
}

BOOST_AUTO_TEST_SUITE_END()
