// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Fittexxcoin Core developers
// Copyright (c) 2019-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Server/client environment: argument handling, config file parsing,
 * thread wrappers, startup time
 */
#pragma once

#if defined(HAVE_CONFIG_H)
#include <config/fittexxcoin-config.h>
#endif

#include <attributes.h>
#include <compat.h>
#include <compat/assumptions.h>
#include <fs.h>
#include <logging.h>
#include <sync.h>
#include <util/threadnames.h>
#include <tinyformat.h>
#include <util/time.h>

#include <any>
#include <cstdint>
#include <exception>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Application startup time (used for uptime calculation)
int64_t GetStartupTime();

extern const char *const FITTEXXCOIN_CONF_FILENAME;

/** Translate a message to the native language of the user. */
const extern std::function<std::string(const char *)> G_TRANSLATION_FUN;

/**
 * Translation function.
 * If no translation function is set, simply return the input.
 */
inline std::string _(const char *psz) {
    return G_TRANSLATION_FUN ? (G_TRANSLATION_FUN)(psz) : psz;
}

void SetupEnvironment();
bool SetupNetworking();

template <typename... Args> bool error(const char *fmt, const Args &... args) {
    LogPrintf("ERROR: %s\n", tfm::format(fmt, args...));
    return false;
}

void PrintExceptionContinue(const std::exception *pex, const char *pszThread);
bool FileCommit(FILE *file);
bool TruncateFile(FILE *file, unsigned int length);
int RaiseFileDescriptorLimit(int nMinFD);
void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length);
bool RenameOver(fs::path src, fs::path dest);
bool LockDirectory(const fs::path &directory, const std::string &lockfile_name,
                   bool probe_only = false);
void UnlockDirectory(const fs::path &directory,
                     const std::string &lockfile_name);
bool DirIsWritable(const fs::path &directory);
bool CheckDiskSpace(const fs::path &dir, uint64_t additional_bytes = 0);

/**
 * Release all directory locks. This is used for unit testing only, at runtime
 * the global destructor will take care of the locks.
 */
void ReleaseDirectoryLocks();

bool TryCreateDirectories(const fs::path &p);
fs::path GetDefaultDataDir();
// The blocks directory is always net specific.
const fs::path &GetBlocksDir();
const fs::path &GetDataDir(bool fNetSpecific = true);
const fs::path &GetIndexDir();
/** Tests only */
void ClearDatadirCache();
fs::path GetConfigFile(const std::string &confPath);
#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif
void runCommand(const std::string &strCommand);

[[nodiscard]] bool ParseKeyValue(std::string &key, std::string &val);

/**
 * Most paths passed as configuration arguments are treated as relative to
 * the datadir if they are not absolute.
 *
 * @param path The path to be conditionally prefixed with datadir.
 * @param net_specific Forwarded to GetDataDir().
 * @return The normalized path.
 */
fs::path AbsPathForConfigVal(const fs::path &path, bool net_specific = true);

inline bool IsSwitchChar(char c) {
#ifdef WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

enum class OptionsCategory {
    OPTIONS,
    CONNECTION,
    WALLET,
    WALLET_DEBUG_TEST,
    ZMQ,
    DEBUG_TEST,
    CHAINPARAMS,
    NODE_RELAY,
    BLOCK_CREATION,
    RPC,
    GUI,
    COMMANDS,
    REGISTER_COMMANDS,

    // Always the last option to avoid printing these in the help
    HIDDEN,
};

struct SectionInfo {
    std::string m_name;
    std::string m_file;
    int m_line;
};

class ArgsManager {
public:
    enum Flags {
        NONE = 0x00,
        // Boolean options can accept negation syntax -noOPTION or -noOPTION=1
        ALLOW_BOOL = 0x01,
        ALLOW_INT = 0x02,
        ALLOW_STRING = 0x04,
        ALLOW_ANY = ALLOW_BOOL | ALLOW_INT | ALLOW_STRING,
        DEBUG_ONLY = 0x100,
        /* Some options would cause cross-contamination if values for
         * mainnet were used while running on regtest/testnet (or vice-versa).
         * Setting them as NETWORK_ONLY ensures that sharing a config file
         * between mainnet and regtest/testnet won't cause problems due to these
         * parameters by accident. */
        NETWORK_ONLY = 0x200,
    };

protected:
    friend class ArgsManagerHelper;

    struct Arg {
        std::string m_help_param;
        std::string m_help_text;
        unsigned int m_flags;
    };

    mutable RecursiveMutex cs_args;
    std::map<std::string, std::vector<std::string>>
        m_override_args GUARDED_BY(cs_args);
    std::map<std::string, std::vector<std::string>>
        m_config_args GUARDED_BY(cs_args);
    std::string m_network GUARDED_BY(cs_args);
    std::set<std::string> m_network_only_args GUARDED_BY(cs_args);
    std::map<OptionsCategory, std::map<std::string, Arg>>
        m_available_args GUARDED_BY(cs_args);
    std::list<SectionInfo> m_config_sections GUARDED_BY(cs_args);

    [[nodiscard]] bool ReadConfigStream(std::istream &stream,
                                        const std::string &filepath,
                                        std::string &error,
                                        bool ignore_invalid_keys = false);

public:
    ArgsManager();

    /**
     * Select the network in use
     */
    void SelectConfigNetwork(const std::string &network);

    [[nodiscard]] bool ParseParameters(int argc, const char *const argv[],
                                       std::string &error);
    bool ReadConfigFiles(std::string &error, bool ignore_invalid_keys = false);

    /**
     * Log warnings for options in m_section_only_args when they are specified
     * in the default section but not overridden on the command line or in a
     * network-specific section in the config file.
     */
    std::set<std::string> GetUnsuitableSectionOnlyArgs() const;

    /**
     * Log warnings for unrecognized section names in the config file.
     */
    const std::list<SectionInfo> GetUnrecognizedSections() const;

    /**
     * Return a vector of strings of the given argument
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @return command-line arguments
     */
    std::vector<std::string> GetArgs(const std::string &strArg) const;

    /**
     * Return true if the given argument has been manually set.
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @return true if the argument has been set
     */
    bool IsArgSet(const std::string &strArg) const;

    /**
     * Return true if the argument was originally passed as a negated option,
     * i.e. -nofoo.
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @return true if the argument was passed negated
     */
    bool IsArgNegated(const std::string &strArg) const;

    /**
     * Return string argument or default value.
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @param default (e.g. "1")
     * @return command-line argument or default value
     */
    std::string GetArg(const std::string &strArg,
                       const std::string &strDefault) const;

    /**
     * Return integer argument or default value.
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @param default (e.g. 1)
     * @return command-line argument (0 if invalid number) or default value
     */
    int64_t GetArg(const std::string &strArg, int64_t nDefault) const;

    /**
     * Return boolean argument or default value.
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @param default (true or false)
     * @return command-line argument or default value
     */
    bool GetBoolArg(const std::string &strArg, bool fDefault) const;

    /**
     * Set an argument if it doesn't already have a value.
     *
     * @param strArg Argument to set (e.g. "-foo")
     * @param strValue Value (e.g. "1")
     * @return true if argument gets set, false if it already had a value
     */
    bool SoftSetArg(const std::string &strArg, const std::string &strValue);

    /**
     * Set a boolean argument if it doesn't already have a value.
     *
     * @param strArg Argument to set (e.g. "-foo")
     * @param fValue Value (e.g. false)
     * @return true if argument gets set, false if it already had a value
     */
    bool SoftSetBoolArg(const std::string &strArg, bool fValue);

    // Forces an arg setting. Called by SoftSetArg() if the arg hasn't already
    // been set. Also called directly in testing.
    void ForceSetArg(const std::string &strArg, const std::string &strValue);

    // Forces a multi arg setting, used only in testing
    void ForceSetMultiArg(const std::string &strArg,
                          const std::string &strValue);

    /**
     * Looks for -regtest, -testnet and returns the appropriate BIP70 chain
     * name.
     * @return CBaseChainParams::MAIN by default; raises runtime error if an
     * invalid combination is given.
     */
    std::string GetChainName() const;

    /**
     * Add argument
     */
    void AddArg(const std::string &name, const std::string &help,
                unsigned int flags, const OptionsCategory &cat);

    // Remove an arg setting, used only in testing
    void ClearArg(const std::string &strArg);

    /**
     * Add many hidden arguments
     */
    void AddHiddenArgs(const std::vector<std::string> &args);

    /**
     * Clear available arguments
     */
    void ClearArgs() {
        LOCK(cs_args);
        m_available_args.clear();
        m_network_only_args.clear();
    }

    /**
     * Get the help string
     */
    std::string GetHelpMessage() const;

    /**
     * Return Flags for known arg.
     * Return ArgsManager::NONE for unknown arg.
     */
    unsigned int FlagsOfKnownArg(const std::string &key) const;
};

extern ArgsManager gArgs;

/**
 * @return true if help has been requested via a command-line arg
 */
bool HelpRequested(const ArgsManager &args);

/** Add help options to the args manager */
void SetupHelpOptions(ArgsManager& args);

/**
 * Format a string to be used as group of options in help messages.
 *
 * @param message Group name (e.g. "RPC server options:")
 * @return the formatted string
 */
std::string HelpMessageGroup(const std::string &message);

/**
 * Format a string to be used as option description in help messages.
 *
 * @param option Option message (e.g. "-rpcuser=<user>")
 * @param message Option description (e.g. "Username for JSON-RPC connections")
 * @return the formatted string
 */
std::string HelpMessageOpt(const std::string &option,
                           const std::string &message);

/**
 * Return the number of cores available on the current system.
 * @note This does count virtual cores, such as those provided by
 * HyperThreading.
 */
int GetNumCores();

/**
 * .. and a wrapper that just calls func once
 */
template <typename Callable> void TraceThread(const char* name,  Callable func)
{
    util::ThreadRename(name);
    try
    {
        LogPrintf("%s thread start\n", name);
        func();
        LogPrintf("%s thread exit\n", name);
    } catch (const std::exception &e) {
        PrintExceptionContinue(&e, name);
        throw;
    } catch (...) {
        PrintExceptionContinue(nullptr, name);
        throw;
    }
}

std::string CopyrightHolders(const std::string &strPrefix);

/**
 * On platforms that support it, tell the kernel the calling thread is
 * CPU-intensive and non-interactive. See SCHED_BATCH in sched(7) for details.
 *
 */
void ScheduleBatchPriority();

namespace util {

//! Simplification of std insertion
template <typename Tdst, typename Tsrc>
inline void insert(Tdst &dst, const Tsrc &src) {
    dst.insert(dst.begin(), src.begin(), src.end());
}
template <typename TsetT, typename Tsrc>
inline void insert(std::set<TsetT> &dst, const Tsrc &src) {
    dst.insert(src.begin(), src.end());
}

/**
 * Helper function to access the contained object of a std::any instance.
 * Returns a pointer to the object if passed instance has a value and the type
 * matches, nullptr otherwise.
 */
template<typename T>
T* AnyPtr(const std::any& any) noexcept
{
    T* const* ptr = std::any_cast<T*>(&any);
    return ptr ? *ptr : nullptr;
}

#ifdef WIN32
class WinCmdLineArgs {
public:
    WinCmdLineArgs();
    ~WinCmdLineArgs();
    std::pair<int, char **> get();

private:
    int argc;
    char **argv;
    std::vector<std::string> args;
};
#endif

} // namespace util
