// Copyright (c) 2018 The Fittexxcoin Core developers
// Copyright (c) 2021-2022 The Fittexxcoin developers
// Copyright (c) 2022 The Fittexxcoin Node developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/node.h>

#include <addrdb.h>
#include <amount.h>
#include <banman.h>
#include <chain.h>
#include <chainparams.h>
#include <config.h>
#include <init.h>
#include <interfaces/chain.h>
#include <interfaces/handler.h>
#include <interfaces/wallet.h>
#include <net.h>
#include <net_processing.h>
#include <netaddress.h>
#include <netbase.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <primitives/block.h>
#include <rpc/server.h>
#include <scheduler.h>
#include <shutdown.h>
#include <sync.h>
#include <txmempool.h>
#include <ui_interface.h>
#include <util/system.h>
#include <validation.h>
#include <warnings.h>

#if defined(HAVE_CONFIG_H)
#include <config/fittexxcoin-config.h>
#endif

#include <univalue.h>

#include <atomic>

class HTTPRPCRequestProcessor;
class CWallet;
fs::path GetWalletDir();
std::vector<fs::path> ListWalletDir();
std::vector<std::shared_ptr<CWallet>> GetWallets();

namespace interfaces {

class Wallet;

namespace {

    class NodeImpl : public Node {
    public:
        NodeImpl() { m_context.chain = MakeChain(); }
        bool parseParameters(int argc, const char *const argv[],
                             std::string &error) override {
            return gArgs.ParseParameters(argc, argv, error);
        }
        bool readConfigFiles(std::string &error) override {
            return gArgs.ReadConfigFiles(error);
        }
        bool softSetArg(const std::string &arg,
                        const std::string &value) override {
            return gArgs.SoftSetArg(arg, value);
        }
        bool softSetBoolArg(const std::string &arg, bool value) override {
            return gArgs.SoftSetBoolArg(arg, value);
        }
        void selectParams(const std::string &network) override {
            SelectParams(network);
        }
        uint64_t getAssumedBlockchainSize() override {
            return Params().AssumedBlockchainSize();
        }
        uint64_t getAssumedChainStateSize() override {
            return Params().AssumedChainStateSize();
        }
        std::string getNetwork() override { return Params().NetworkIDString(); }
        void initLogging() override { InitLogging(); }
        void initParameterInteraction() override { InitParameterInteraction(); }
        std::string getWarnings(const std::string &type) override {
            return GetWarnings(type);
        }
        bool baseInitialize(Config &config) override {
            return AppInitBasicSetup() && AppInitParameterInteraction(config) &&
                   AppInitSanityChecks() && AppInitLockDataDirectory();
        }
        bool
        appInitMain(Config &config, RPCServer &rpcServer,
                    HTTPRPCRequestProcessor &httpRPCRequestProcessor) override {
            return AppInitMain(config, rpcServer, httpRPCRequestProcessor,
                               m_context);
        }
        void appShutdown() override {
            Interrupt();
            Shutdown(m_context);
        }
        void startShutdown() override { StartShutdown(); }
        bool shutdownRequested() override { return ShutdownRequested(); }
        void mapPort(bool use_upnp) override {
            if (use_upnp) {
                StartMapPort();
            } else {
                InterruptMapPort();
                StopMapPort();
            }
        }
        void setupServerArgs() override { return SetupServerArgs(); }
        bool getProxy(Network net, proxyType &proxy_info) override {
            return GetProxy(net, proxy_info);
        }
        size_t getNodeCount(CConnman::NumConnections flags) override {
            return g_connman ? g_connman->GetNodeCount(flags) : 0;
        }
        bool getNodesStats(NodesStats &stats) override {
            stats.clear();

            if (g_connman) {
                std::vector<CNodeStats> stats_temp;
                g_connman->GetNodeStats(stats_temp);

                stats.reserve(stats_temp.size());
                for (auto &node_stats_temp : stats_temp) {
                    stats.emplace_back(std::move(node_stats_temp), false,
                                       CNodeStateStats());
                }

                // Try to retrieve the CNodeStateStats for each node.
                TRY_LOCK(::cs_main, lockMain);
                if (lockMain) {
                    for (auto &node_stats : stats) {
                        std::get<1>(node_stats) =
                            GetNodeStateStats(std::get<0>(node_stats).nodeid,
                                              std::get<2>(node_stats));
                    }
                }
                return true;
            }
            return false;
        }
        bool getBanned(BanTables &banmap) override {
            if (g_banman) {
                g_banman->GetBanned(banmap);
                return true;
            }
            return false;
        }
        bool ban(const CNetAddr &net_addr, int64_t ban_time_offset) override {
            if (g_banman) {
                g_banman->Ban(net_addr, ban_time_offset);
                return true;
            }
            return false;
        }
        bool unban(const CSubNet &ip) override {
            if (g_banman) {
                g_banman->Unban(ip);
                return true;
            }
            return false;
        }
        bool disconnect(const CNetAddr &net_addr) override {
            if (g_connman) {
                return g_connman->DisconnectNode(net_addr);
            }
            return false;
        }
        bool disconnect(NodeId id) override {
            if (g_connman) {
                return g_connman->DisconnectNode(id);
            }
            return false;
        }
        int64_t getTotalBytesRecv() override {
            return g_connman ? g_connman->GetTotalBytesRecv() : 0;
        }
        int64_t getTotalBytesSent() override {
            return g_connman ? g_connman->GetTotalBytesSent() : 0;
        }
        size_t getMempoolSize() override { return g_mempool.size(); }
        size_t getMempoolTotalTxSize() override { return g_mempool.GetTotalTxSize(); }
        size_t getMempoolDynamicUsage() override {
            return g_mempool.DynamicMemoryUsage();
        }
        bool getHeaderTip(int &height, int64_t &block_time) override {
            LOCK(::cs_main);
            if (::pindexBestHeader) {
                height = ::pindexBestHeader->nHeight;
                block_time = ::pindexBestHeader->GetBlockTime();
                return true;
            }
            return false;
        }
        int getNumBlocks() override {
            LOCK(::cs_main);
            return ::ChainActive().Height();
        }
        int64_t getLastBlockTime() override {
            LOCK(::cs_main);
            if (CBlockIndex *tip = ::ChainActive().Tip()) {
                return tip->GetBlockTime();
            }
            // Genesis block's time of current network
            return Params().GenesisBlock().GetBlockTime();
        }
        BlockHash getLastBlockHash() override {
            LOCK(::cs_main);
            if (CBlockIndex *tip = ::ChainActive().Tip()) {
                return tip->GetBlockHash();
            }
            // Genesis block's hash of current network
            return Params().GenesisBlock().GetHash();
        }
        double getVerificationProgress() override {
            const CBlockIndex *tip;
            {
                LOCK(::cs_main);
                tip = ::ChainActive().Tip();
            }
            return GuessVerificationProgress(Params().TxData(), tip);
        }
        bool isInitialBlockDownload() override {
            return IsInitialBlockDownload();
        }
        bool getReindex() override { return ::fReindex; }
        bool getImporting() override { return ::fImporting; }
        void setNetworkActive(bool active) override {
            if (g_connman) {
                g_connman->SetNetworkActive(active);
            }
        }
        bool getNetworkActive() override {
            return g_connman && g_connman->GetNetworkActive();
        }
        Amount getMaxTxFee() override { return ::maxTxFee; }
        CFeeRate estimateSmartFee() override { return g_mempool.estimateFee(); }
        CFeeRate getDustRelayFee() override { return ::dustRelayFee; }
        UniValue executeRpc(Config &config, const std::string &command,
                            const UniValue &params,
                            const std::string &uri) override {
            JSONRPCRequest req;
            req.params = params;
            req.strMethod = command;
            req.URI = uri;
            return ::tableRPC.execute(config, req);
        }
        std::vector<std::string> listRpcCommands() override {
            return ::tableRPC.listCommands();
        }
        void rpcSetTimerInterfaceIfUnset(RPCTimerInterface *iface) override {
            RPCSetTimerInterfaceIfUnset(iface);
        }
        void rpcUnsetTimerInterface(RPCTimerInterface *iface) override {
            RPCUnsetTimerInterface(iface);
        }
        bool getUnspentOutput(const COutPoint &output, Coin &coin) override {
            LOCK(::cs_main);
            return ::pcoinsTip->GetCoin(output, coin);
        }
        std::string getWalletDir() override { return GetWalletDir().string(); }
        std::vector<std::string> listWalletDir() override {
            std::vector<std::string> paths;
            for (auto &path : ListWalletDir()) {
                paths.push_back(path.string());
            }
            return paths;
        }
        std::vector<std::unique_ptr<Wallet>> getWallets() override {
            std::vector<std::unique_ptr<Wallet>> wallets;
            for (const std::shared_ptr<CWallet> &wallet : GetWallets()) {
                wallets.emplace_back(MakeWallet(wallet));
            }
            return wallets;
        }
        std::unique_ptr<Handler> handleInitMessage(InitMessageFn fn) override {
            return MakeHandler(::uiInterface.InitMessage_connect(fn));
        }
        std::unique_ptr<Handler> handleMessageBox(MessageBoxFn fn) override {
            return MakeHandler(::uiInterface.ThreadSafeMessageBox_connect(fn));
        }
        std::unique_ptr<Handler> handleQuestion(QuestionFn fn) override {
            return MakeHandler(::uiInterface.ThreadSafeQuestion_connect(fn));
        }
        std::unique_ptr<Handler>
        handleShowProgress(ShowProgressFn fn) override {
            return MakeHandler(::uiInterface.ShowProgress_connect(fn));
        }
        std::unique_ptr<Handler> handleLoadWallet(LoadWalletFn fn) override {
            return MakeHandler(::uiInterface.LoadWallet_connect(
                [fn](std::shared_ptr<CWallet> wallet) {
                    fn(MakeWallet(wallet));
                }));
        }
        std::unique_ptr<Handler> handleNotifyNumConnectionsChanged(
            NotifyNumConnectionsChangedFn fn) override {
            return MakeHandler(
                ::uiInterface.NotifyNumConnectionsChanged_connect(fn));
        }
        std::unique_ptr<Handler> handleNotifyNetworkActiveChanged(
            NotifyNetworkActiveChangedFn fn) override {
            return MakeHandler(
                ::uiInterface.NotifyNetworkActiveChanged_connect(fn));
        }
        std::unique_ptr<Handler>
        handleNotifyAlertChanged(NotifyAlertChangedFn fn) override {
            return MakeHandler(::uiInterface.NotifyAlertChanged_connect(fn));
        }
        std::unique_ptr<Handler>
        handleBannedListChanged(BannedListChangedFn fn) override {
            return MakeHandler(::uiInterface.BannedListChanged_connect(fn));
        }
        std::unique_ptr<Handler>
        handleNotifyBlockTip(NotifyBlockTipFn fn) override {
            return MakeHandler(::uiInterface.NotifyBlockTip_connect(
                [fn](bool initial_download, const CBlockIndex *block) {
                    fn(initial_download, block->nHeight, block->GetBlockTime(), block->GetBlockHash(),
                       GuessVerificationProgress(Params().TxData(), block));
                }));
        }
        std::unique_ptr<Handler>
        handleNotifyHeaderTip(NotifyHeaderTipFn fn) override {
            return MakeHandler(::uiInterface.NotifyHeaderTip_connect(
                [fn](bool initial_download, const CBlockIndex *block) {
                    fn(initial_download, block->nHeight, block->GetBlockTime(), block->GetBlockHash(),
                       GuessVerificationProgress(Params().TxData(), block));
                }));
        }
        std::unique_ptr<Handler>
        handleNotifyTransactionDoubleSpent(NotifyTransactionDoubleSpentFn fn) override {
            return MakeHandler(::uiInterface.NotifyTransactionDoubleSpent_connect(fn));
        }
        NodeContext m_context;
    };
} // namespace

std::unique_ptr<Node> MakeNode() {
    return std::make_unique<NodeImpl>();
}

} // namespace interfaces
