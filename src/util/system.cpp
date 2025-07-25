// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Fittexxcoin Core developers
// Copyright (c) 2020-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/fittexxcoin-config.h>
#endif

#include <util/system.h>

#include <chainparamsbase.h>
#include <fs.h>
#include <random.h>
#include <serialize.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/time.h>

#include <cstdarg>
#include <memory>
#include <thread>
#include <typeinfo>

#if (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
#include <pthread.h>
#include <pthread_np.h>
#endif

#ifndef WIN32
// for posix_fallocate
#ifdef __linux__

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#define _POSIX_C_SOURCE 200112L

#endif // __linux__

#include <algorithm>
#include <fcntl.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/stat.h>

#else

#ifdef _MSC_VER
#pragma warning(disable : 4786)
#pragma warning(disable : 4804)
#pragma warning(disable : 4805)
#pragma warning(disable : 4717)
#endif

#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501

#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <codecvt>

#include <io.h> /* for _commit */
#include <shellapi.h>
#include <shlobj.h>
#endif

#ifdef HAVE_MALLOPT_ARENA_MAX
#include <malloc.h>
#endif

// Application startup time (used for uptime calculation)
const int64_t nStartupTime = GetTime();

const char *const FITTEXXCOIN_CONF_FILENAME = "fittexxcoin.conf";

ArgsManager gArgs;

/**
 * A map that contains all the currently held directory locks. After successful
 * locking, these will be held here until the global destructor cleans them up
 * and thus automatically unlocks them, or ReleaseDirectoryLocks is called.
 */
static std::map<std::string, std::unique_ptr<fsbridge::FileLock>> dir_locks;
/** Mutex to protect dir_locks. */
static std::mutex cs_dir_locks;

bool LockDirectory(const fs::path &directory, const std::string &lockfile_name,
                   bool probe_only) {
    std::lock_guard<std::mutex> ulock(cs_dir_locks);
    fs::path pathLockFile = directory / lockfile_name;

    // If a lock for this directory already exists in the map, don't try to
    // re-lock it
    if (dir_locks.count(pathLockFile.string())) {
        return true;
    }

    // Create empty lock file if it doesn't exist.
    FILE *file = fsbridge::fopen(pathLockFile, "a");
    if (file) {
        fclose(file);
    }
    auto lock = std::make_unique<fsbridge::FileLock>(pathLockFile);
    if (!lock->TryLock()) {
        return error("Error while attempting to lock directory %s: %s",
                     directory.string(), lock->GetReason());
    }
    if (!probe_only) {
        // Lock successful and we're not just probing, put it into the map
        dir_locks.emplace(pathLockFile.string(), std::move(lock));
    }
    return true;
}

void UnlockDirectory(const fs::path &directory,
                     const std::string &lockfile_name) {
    std::lock_guard<std::mutex> lock(cs_dir_locks);
    dir_locks.erase((directory / lockfile_name).string());
}

void ReleaseDirectoryLocks() {
    std::lock_guard<std::mutex> ulock(cs_dir_locks);
    dir_locks.clear();
}

bool DirIsWritable(const fs::path &directory) {
    fs::path tmpFile = directory / fs::unique_path();

    FILE *file = fsbridge::fopen(tmpFile, "a");
    if (!file) {
        return false;
    }

    fclose(file);
    remove(tmpFile);

    return true;
}

bool CheckDiskSpace(const fs::path &dir, uint64_t additional_bytes) {
    // 50 MiB
    constexpr uint64_t min_disk_space = 52428800;

    uint64_t free_bytes_available = fs::space(dir).available;
    return free_bytes_available >= min_disk_space + additional_bytes;
}

/**
 * Interpret a string argument as a boolean.
 *
 * The definition of atoi() requires that non-numeric string values like "foo",
 * return 0. This means that if a user unintentionally supplies a non-integer
 * argument here, the return value is always false. This means that -foo=false
 * does what the user probably expects, but -foo=true is well defined but does
 * not do what they probably expected.
 *
 * The return value of atoi() is undefined when given input not representable as
 * an int. On most systems this means string value between "-2147483648" and
 * "2147483647" are well defined (this method will return true). Setting
 * -txindex=2147483648 on most systems, however, is probably undefined.
 *
 * For a more extensive discussion of this topic (and a wide range of opinions
 * on the Right Way to change this code), see PR12713.
 */
static bool InterpretBool(const std::string &strValue) {
    if (strValue.empty()) {
        return true;
    }
    return (atoi(strValue) != 0);
}

/** Internal helper functions for ArgsManager */
class ArgsManagerHelper {
public:
    typedef std::map<std::string, std::vector<std::string>> MapArgs;

    /** Determine whether to use config settings in the default section,
     *  See also comments around ArgsManager::ArgsManager() below. */
    static inline bool UseDefaultSection(const ArgsManager &am,
                                         const std::string &arg)
        EXCLUSIVE_LOCKS_REQUIRED(am.cs_args) {
        return (am.m_network == CBaseChainParams::MAIN ||
                am.m_network_only_args.count(arg) == 0);
    }

    /** Convert regular argument into the network-specific setting */
    static inline std::string NetworkArg(const ArgsManager &am,
                                         const std::string &arg) {
        assert(arg.length() > 1 && arg[0] == '-');
        return "-" + am.m_network + "." + arg.substr(1);
    }

    /** Find arguments in a map and add them to a vector */
    static inline void AddArgs(std::vector<std::string> &res,
                               const MapArgs &map_args,
                               const std::string &arg) {
        auto it = map_args.find(arg);
        if (it != map_args.end()) {
            res.insert(res.end(), it->second.begin(), it->second.end());
        }
    }

    /**
     * Return true/false if an argument is set in a map, and also
     * return the first (or last) of the possibly multiple values it has
     */
    static inline std::pair<bool, std::string>
    GetArgHelper(const MapArgs &map_args, const std::string &arg,
                 bool getLast = false) {
        auto it = map_args.find(arg);

        if (it == map_args.end() || it->second.empty()) {
            return std::make_pair(false, std::string());
        }

        if (getLast) {
            return std::make_pair(true, it->second.back());
        } else {
            return std::make_pair(true, it->second.front());
        }
    }

    /**
     * Get the string value of an argument, returning a pair of a boolean
     * indicating the argument was found, and the value for the argument
     * if it was found (or the empty string if not found).
     */
    static inline std::pair<bool, std::string> GetArg(const ArgsManager &am,
                                                      const std::string &arg) {
        LOCK(am.cs_args);
        std::pair<bool, std::string> found_result(false, std::string());

        // We pass "true" to GetArgHelper in order to return the last
        // argument value seen from the command line (so "fittexxcoind -foo=bar
        // -foo=baz" gives GetArg(am,"foo")=={true,"baz"}
        found_result = GetArgHelper(am.m_override_args, arg, true);
        if (found_result.first) {
            return found_result;
        }

        // But in contrast we return the first argument seen in a config file,
        // so "foo=bar \n foo=baz" in the config file gives
        // GetArg(am,"foo")={true,"bar"}
        if (!am.m_network.empty()) {
            found_result = GetArgHelper(am.m_config_args, NetworkArg(am, arg));
            if (found_result.first) {
                return found_result;
            }
        }

        if (UseDefaultSection(am, arg)) {
            found_result = GetArgHelper(am.m_config_args, arg);
            if (found_result.first) {
                return found_result;
            }
        }

        return found_result;
    }

    /* Special test for -testnet and -regtest args, because we don't want to be
     * confused by craziness like "[regtest] testnet=1"
     */
    static inline bool GetNetBoolArg(const ArgsManager &am,
                                     const std::string &net_arg)
        EXCLUSIVE_LOCKS_REQUIRED(am.cs_args) {
        std::pair<bool, std::string> found_result(false, std::string());
        found_result = GetArgHelper(am.m_override_args, net_arg, true);
        if (!found_result.first) {
            found_result = GetArgHelper(am.m_config_args, net_arg, true);
            if (!found_result.first) {
                // not set
                return false;
            }
        }
        // is set, so evaluate
        return InterpretBool(found_result.second);
    }
};

/**
 * Interpret -nofoo as if the user supplied -foo=0.
 *
 * This method also tracks when the -no form was supplied, and if so, checks
 * whether there was a double-negative (-nofoo=0 -> -foo=1).
 *
 * If there was not a double negative, it removes the "no" from the key
 * and clears the args vector to indicate a negated option.
 *
 * If there was a double negative, it removes "no" from the key, sets the
 * value to "1" and pushes the key and the updated value to the args vector.
 *
 * If there was no "no", it leaves key and value untouched and pushes them
 * to the args vector.
 *
 * Where an option was negated can be later checked using the IsArgNegated()
 * method. One use case for this is to have a way to disable options that are
 * not normally boolean (e.g. using -nodebuglogfile to request that debug log
 * output is not sent to any file at all).
 */

[[nodiscard]] static bool
InterpretOption(std::string key, std::string val, unsigned int flags,
                std::map<std::string, std::vector<std::string>> &args,
                std::string &error) {
    assert(key[0] == '-');

    size_t option_index = key.find('.');
    if (option_index == std::string::npos) {
        option_index = 1;
    } else {
        ++option_index;
    }
    if (key.substr(option_index, 2) == "no") {
        key.erase(option_index, 2);
        if (flags & ArgsManager::ALLOW_BOOL) {
            if (InterpretBool(val)) {
                args[key].clear();
                return true;
            }
            // Double negatives like -nofoo=0 are supported (but discouraged)
            LogPrintf(
                "Warning: parsed potentially confusing double-negative %s=%s\n",
                key, val);
            val = "1";
        } else {
            error = strprintf(
                "Negating of %s is meaningless and therefore forbidden",
                key.c_str());
            return false;
        }
    }
    args[key].push_back(val);
    return true;
}

ArgsManager::ArgsManager() {
    // nothing to do
}

std::set<std::string> ArgsManager::GetUnsuitableSectionOnlyArgs() const {
    std::set<std::string> unsuitables;

    LOCK(cs_args);

    // if there's no section selected, don't worry
    if (m_network.empty()) {
        return {};
    }

    // if it's okay to use the default section for this network, don't worry
    if (m_network == CBaseChainParams::MAIN) {
        return {};
    }

    for (const auto &arg : m_network_only_args) {
        std::pair<bool, std::string> found_result;

        // if this option is overridden it's fine
        found_result = ArgsManagerHelper::GetArgHelper(m_override_args, arg);
        if (found_result.first) {
            continue;
        }

        // if there's a network-specific value for this option, it's fine
        found_result = ArgsManagerHelper::GetArgHelper(
            m_config_args, ArgsManagerHelper::NetworkArg(*this, arg));
        if (found_result.first) {
            continue;
        }

        // if there isn't a default value for this option, it's fine
        found_result = ArgsManagerHelper::GetArgHelper(m_config_args, arg);
        if (!found_result.first) {
            continue;
        }

        // otherwise, issue a warning
        unsuitables.insert(arg);
    }
    return unsuitables;
}

const std::list<SectionInfo> ArgsManager::GetUnrecognizedSections() const {
    // Section names to be recognized in the config file.
    static const std::set<std::string> available_sections{
        CBaseChainParams::REGTEST,
        CBaseChainParams::TESTNET, CBaseChainParams::TESTNET4, CBaseChainParams::SCALENET, CBaseChainParams::CHIPNET,
        CBaseChainParams::MAIN};

    LOCK(cs_args);
    std::list<SectionInfo> unrecognized = m_config_sections;
    unrecognized.remove_if([](const SectionInfo &appeared) {
        return available_sections.find(appeared.m_name) !=
               available_sections.end();
    });
    return unrecognized;
}

void ArgsManager::SelectConfigNetwork(const std::string &network) {
    LOCK(cs_args);
    m_network = network;
}

bool ParseKeyValue(std::string &key, std::string &val) {
    size_t is_index = key.find('=');
    if (is_index != std::string::npos) {
        val = key.substr(is_index + 1);
        key.erase(is_index);
    }
#ifdef WIN32
    key = ToLower(key);
    if (key[0] == '/') {
        key[0] = '-';
    }
#endif

    if (key[0] != '-') {
        return false;
    }

    // Transform --foo to -foo
    if (key.length() > 1 && key[1] == '-') {
        key.erase(0, 1);
    }
    return true;
}

bool ArgsManager::ParseParameters(int argc, const char *const argv[],
                                  std::string &error) {
    LOCK(cs_args);
    m_override_args.clear();

    for (int i = 1; i < argc; i++) {
        std::string key(argv[i]);
        if (key == "-") break; // fittexxcoin-tx using stdin
        std::string val;
        if (!ParseKeyValue(key, val)) {
            break;
        }

        const unsigned int flags = FlagsOfKnownArg(key);
        if (flags) {
            if (!InterpretOption(key, val, flags, m_override_args, error)) {
                return false;
            }
        } else {
            error = strprintf("Invalid parameter %s", key.c_str());
            return false;
        }
    }

    // we do not allow -includeconf from command line, so we clear it here
    auto it = m_override_args.find("-includeconf");
    if (it != m_override_args.end()) {
        if (it->second.size() > 0) {
            for (const auto &ic : it->second) {
                error += "-includeconf cannot be used from commandline; "
                         "-includeconf=" +
                         ic + "\n";
            }
            return false;
        }
    }
    return true;
}

unsigned int ArgsManager::FlagsOfKnownArg(const std::string &key) const {
    assert(key[0] == '-');

    size_t option_index = key.find('.');
    if (option_index == std::string::npos) {
        option_index = 1;
    } else {
        ++option_index;
    }
    if (key.substr(option_index, 2) == "no") {
        option_index += 2;
    }

    const std::string base_arg_name = '-' + key.substr(option_index);

    LOCK(cs_args);
    for (const auto &arg_map : m_available_args) {
        const auto search = arg_map.second.find(base_arg_name);
        if (search != arg_map.second.end()) {
            return search->second.m_flags;
        }
    }
    return ArgsManager::NONE;
}

std::vector<std::string> ArgsManager::GetArgs(const std::string &strArg) const {
    std::vector<std::string> result = {};
    // special case
    if (IsArgNegated(strArg)) {
        return result;
    }

    LOCK(cs_args);

    ArgsManagerHelper::AddArgs(result, m_override_args, strArg);
    if (!m_network.empty()) {
        ArgsManagerHelper::AddArgs(
            result, m_config_args,
            ArgsManagerHelper::NetworkArg(*this, strArg));
    }

    if (ArgsManagerHelper::UseDefaultSection(*this, strArg)) {
        ArgsManagerHelper::AddArgs(result, m_config_args, strArg);
    }

    return result;
}

bool ArgsManager::IsArgSet(const std::string &strArg) const {
    // special case
    if (IsArgNegated(strArg)) {
        return true;
    }
    return ArgsManagerHelper::GetArg(*this, strArg).first;
}

bool ArgsManager::IsArgNegated(const std::string &strArg) const {
    LOCK(cs_args);

    const auto &ov = m_override_args.find(strArg);
    if (ov != m_override_args.end()) {
        return ov->second.empty();
    }

    if (!m_network.empty()) {
        const auto &cfs =
            m_config_args.find(ArgsManagerHelper::NetworkArg(*this, strArg));
        if (cfs != m_config_args.end()) {
            return cfs->second.empty();
        }
    }

    const auto &cf = m_config_args.find(strArg);
    if (cf != m_config_args.end()) {
        return cf->second.empty();
    }

    return false;
}

std::string ArgsManager::GetArg(const std::string &strArg,
                                const std::string &strDefault) const {
    if (IsArgNegated(strArg)) {
        return "0";
    }
    std::pair<bool, std::string> found_res =
        ArgsManagerHelper::GetArg(*this, strArg);
    if (found_res.first) {
        return found_res.second;
    }
    return strDefault;
}

int64_t ArgsManager::GetArg(const std::string &strArg, int64_t nDefault) const {
    if (IsArgNegated(strArg)) {
        return 0;
    }
    std::pair<bool, std::string> found_res =
        ArgsManagerHelper::GetArg(*this, strArg);
    if (found_res.first) {
        return atoi64(found_res.second);
    }
    return nDefault;
}

bool ArgsManager::GetBoolArg(const std::string &strArg, bool fDefault) const {
    if (IsArgNegated(strArg)) {
        return false;
    }
    std::pair<bool, std::string> found_res =
        ArgsManagerHelper::GetArg(*this, strArg);
    if (found_res.first) {
        return InterpretBool(found_res.second);
    }
    return fDefault;
}

bool ArgsManager::SoftSetArg(const std::string &strArg,
                             const std::string &strValue) {
    LOCK(cs_args);
    if (IsArgSet(strArg)) {
        return false;
    }
    ForceSetArg(strArg, strValue);
    return true;
}

bool ArgsManager::SoftSetBoolArg(const std::string &strArg, bool fValue) {
    if (fValue) {
        return SoftSetArg(strArg, std::string("1"));
    } else {
        return SoftSetArg(strArg, std::string("0"));
    }
}

void ArgsManager::ForceSetArg(const std::string &strArg,
                              const std::string &strValue) {
    LOCK(cs_args);
    m_override_args[strArg] = {strValue};
}

/**
 * This function is only used for testing purpose so
 * so we should not worry about element uniqueness and
 * integrity of mapMultiArgs data structure
 */
void ArgsManager::ForceSetMultiArg(const std::string &strArg,
                                   const std::string &strValue) {
    LOCK(cs_args);
    m_override_args[strArg].push_back(strValue);
}

void ArgsManager::AddArg(const std::string &name, const std::string &help,
                         unsigned int flags, const OptionsCategory &cat) {
    // Parse the name.
    // Anything behind a comma or equals sign is a usage instruction,
    // that needs to be stripped from the argument name.
    auto comma_index = name.find(", ");
    auto eq_index = std::min(comma_index, name.find('='));
    auto arg_name =
        eq_index == std::string::npos ? name : name.substr(0, eq_index);
    {
        LOCK(cs_args);
        std::map<std::string, Arg> &arg_map = m_available_args[cat];
        auto ret = arg_map.emplace(
            arg_name,
            Arg{eq_index == std::string::npos ? "" : name.substr(eq_index),
                help, flags});
        // Make sure an insertion actually happened.
        assert(ret.second);

        if (flags & ArgsManager::NETWORK_ONLY) {
            m_network_only_args.emplace(arg_name);
        }
    }
    if (comma_index != std::string::npos) {
        // name is a comma-separated list.
        // Any parameter behind the comma is an alias of the current parameter.
        // Use recursion to add aliases as hidden arguments.
        AddArg(name.substr(comma_index + 2), "", flags,
               OptionsCategory::HIDDEN);
    }
}

void ArgsManager::AddHiddenArgs(const std::vector<std::string> &names) {
    for (const std::string &name : names) {
        AddArg(name, "", ArgsManager::ALLOW_ANY, OptionsCategory::HIDDEN);
    }
}

void ArgsManager::ClearArg(const std::string &strArg) {
    LOCK(cs_args);
    m_override_args.erase(strArg);
    m_config_args.erase(strArg);
}

std::string ArgsManager::GetHelpMessage() const {
    const bool show_debug = gArgs.IsArgSet("-??") || gArgs.IsArgSet("-hh") || gArgs.IsArgSet("-help-debug");

    std::string usage = "";
    LOCK(cs_args);
    for (const auto &arg_map : m_available_args) {
        switch (arg_map.first) {
            case OptionsCategory::OPTIONS:
                usage += HelpMessageGroup("Options:");
                break;
            case OptionsCategory::CONNECTION:
                usage += HelpMessageGroup("Connection options:");
                break;
            case OptionsCategory::ZMQ:
                usage += HelpMessageGroup("ZeroMQ notification options:");
                break;
            case OptionsCategory::DEBUG_TEST:
                usage += HelpMessageGroup("Debugging/Testing options:");
                break;
            case OptionsCategory::NODE_RELAY:
                usage += HelpMessageGroup("Node relay options:");
                break;
            case OptionsCategory::BLOCK_CREATION:
                usage += HelpMessageGroup("Block creation options:");
                break;
            case OptionsCategory::RPC:
                usage += HelpMessageGroup("RPC server options:");
                break;
            case OptionsCategory::WALLET:
                usage += HelpMessageGroup("Wallet options:");
                break;
            case OptionsCategory::WALLET_DEBUG_TEST:
                if (show_debug) {
                    usage +=
                        HelpMessageGroup("Wallet debugging/testing options:");
                }
                break;
            case OptionsCategory::CHAINPARAMS:
                usage += HelpMessageGroup("Chain selection options:");
                break;
            case OptionsCategory::GUI:
                usage += HelpMessageGroup("UI Options:");
                break;
            case OptionsCategory::COMMANDS:
                usage += HelpMessageGroup("Commands:");
                break;
            case OptionsCategory::REGISTER_COMMANDS:
                usage += HelpMessageGroup("Register Commands:");
                break;
            default:
                break;
        }

        // When we get to the hidden options, stop
        if (arg_map.first == OptionsCategory::HIDDEN) {
            break;
        }

        for (const auto &arg : arg_map.second) {
            if (show_debug || !(arg.second.m_flags & ArgsManager::DEBUG_ONLY)) {
                std::string name;
                if (arg.second.m_help_param.empty()) {
                    name = arg.first;
                } else {
                    name = arg.first + arg.second.m_help_param;
                }
                usage += HelpMessageOpt(name, arg.second.m_help_text);
            }
        }
    }
    return usage;
}

bool HelpRequested(const ArgsManager &args) {
    return args.IsArgSet("-?") || args.IsArgSet("-h") || args.IsArgSet("-help") ||
           args.IsArgSet("-??") || args.IsArgSet("-hh") || args.IsArgSet("-help-debug");
}

void SetupHelpOptions(ArgsManager& args)
{
    args.AddArg("-?, -h, -help", "Print this help message and exit", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
}

static const int screenWidth = 79;
static const int optIndent = 2;
static const int msgIndent = 7;

std::string HelpMessageGroup(const std::string &message) {
    return message + "\n\n";
}

std::string HelpMessageOpt(const std::string &option,
                           const std::string &message) {
    return std::string(optIndent, ' ') + option + '\n' +
           FormatParagraph(message, screenWidth, msgIndent) + "\n\n";
}

static std::string FormatException(const std::exception *pex,
                                   const char *pszThread) {
#ifdef WIN32
    char pszModule[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, pszModule, sizeof(pszModule));
#else
    const char *pszModule = "fittexxcoin";
#endif
    if (pex) {
        return strprintf("EXCEPTION: %s       \n%s       \n%s in %s       \n",
                         typeid(*pex).name(), pex->what(), pszModule,
                         pszThread);
    } else {
        return strprintf("UNKNOWN EXCEPTION       \n%s in %s       \n",
                         pszModule, pszThread);
    }
}

void PrintExceptionContinue(const std::exception *pex, const char *pszThread) {
    std::string message = FormatException(pex, pszThread);
    LogPrintf("\n\n************************\n%s\n", message);
    fprintf(stderr, "\n\n************************\n%s\n", message.c_str());
}

fs::path GetDefaultDataDir() {
// Windows < Vista: C:\Documents and Settings\Username\Application Data\Fittexxcoin
// Windows >= Vista: C:\Users\Username\AppData\Roaming\Fittexxcoin
// Mac: ~/Library/Application Support/Fittexxcoin
// Unix: ~/.fittexxcoin
#ifdef WIN32
    // Windows
    return GetSpecialFolderPath(CSIDL_APPDATA) / "Fittexxcoin";
#else
    fs::path pathRet;
    char *pszHome = getenv("HOME");
    if (pszHome == nullptr || strlen(pszHome) == 0) {
        pathRet = fs::path("/");
    } else {
        pathRet = fs::path(pszHome);
    }
#ifdef MAC_OSX
    // Mac
    return pathRet / "Library/Application Support/Fittexxcoin";
#else
    // Unix
    return pathRet / ".fittexxcoin";
#endif
#endif
}

static fs::path g_blocks_path_cache_net_specific;
static fs::path pathCached;
static fs::path pathCachedNetSpecific;
static fs::path pathWithIndex;
static RecursiveMutex csPathCached;

const fs::path &GetBlocksDir() {
    LOCK(csPathCached);
    fs::path &path = g_blocks_path_cache_net_specific;

    // Cache the path to avoid calling fs::create_directories on every call of
    // this function
    if (!path.empty()) {
        return path;
    }

    if (gArgs.IsArgSet("-blocksdir")) {
        path = fs::system_complete(gArgs.GetArg("-blocksdir", ""));
        if (!fs::is_directory(path)) {
            path = "";
            return path;
        }
    } else {
        path = GetDataDir(false);
    }

    path /= BaseParams().DataDir();
    path /= "blocks";
    fs::create_directories(path);
    return path;
}

const fs::path &GetDataDir(bool fNetSpecific) {
    LOCK(csPathCached);
    fs::path &path = fNetSpecific ? pathCachedNetSpecific : pathCached;

    // Cache the path to avoid calling fs::create_directories on every call of
    // this function
    if (!path.empty()) {
        return path;
    }

    if (gArgs.IsArgSet("-datadir")) {
        path = fs::system_complete(gArgs.GetArg("-datadir", ""));
        if (!fs::is_directory(path)) {
            path = "";
            return path;
        }
    } else {
        path = GetDefaultDataDir();
    }

    if (fNetSpecific) {
        path /= BaseParams().DataDir();
    }

    if (fs::create_directories(path)) {
        // This is the first run, create wallets subdirectory too
        //
        // TODO: this is an ugly way to create the wallets/ directory and
        // really shouldn't be done here. Once this is fixed, please
        // also remove the corresponding line in fittexxcoind.cpp AppInit.
        // See more info at:
        // https://reviews.fittexxcoinabc.org/D3312
        fs::create_directories(path / "wallets");
    }

    return path;
}

const fs::path &GetIndexDir() {
    LOCK(csPathCached);
    fs::path &path = pathWithIndex;

    // Cache the path to avoid calling fs::create_directories on every call of
    // this function
    if (!path.empty()) {
        return path;
    }

    if (gArgs.IsArgSet("-indexdir")) {
        path = fs::system_complete(gArgs.GetArg("-indexdir", ""));
        path /= BaseParams().DataDir();
    } else {
        path = gArgs.IsArgSet("-blocksdir") ? GetDataDir() / "blocks" : GetBlocksDir();
    }
    path /= "index";

    if (!fs::is_directory(path)) {
        fs::create_directories(path);
    }
    return path;
}

void ClearDatadirCache() {
    LOCK(csPathCached);

    pathCached = fs::path();
    pathCachedNetSpecific = fs::path();
    g_blocks_path_cache_net_specific = fs::path();
    pathWithIndex = fs::path();
}

fs::path GetConfigFile(const std::string &confPath) {
    return AbsPathForConfigVal(fs::path(confPath), false);
}

static bool
GetConfigOptions(std::istream &stream, const std::string &filepath,
                 std::string &error,
                 std::vector<std::pair<std::string, std::string>> &options,
                 std::list<SectionInfo> &sections) {
    std::string str, prefix;
    std::string::size_type pos;
    int linenr = 1;
    while (std::getline(stream, str)) {
        bool used_hash = false;
        if ((pos = str.find('#')) != std::string::npos) {
            str = str.substr(0, pos);
            used_hash = true;
        }
        const static std::string pattern = " \t\r\n";
        str = TrimString(str, pattern);
        if (!str.empty()) {
            if (*str.begin() == '[' && *str.rbegin() == ']') {
                const std::string section = str.substr(1, str.size() - 2);
                sections.emplace_back(SectionInfo{section, filepath, linenr});
                prefix = section + '.';
            } else if (*str.begin() == '-') {
                error = strprintf(
                    "parse error on line %i: %s, options in configuration file "
                    "must be specified without leading -",
                    linenr, str);
                return false;
            } else if ((pos = str.find('=')) != std::string::npos) {
                std::string name =
                    prefix + TrimString(str.substr(0, pos), pattern);
                std::string value = TrimString(str.substr(pos + 1), pattern);
                if (used_hash &&
                    name.find("rpcpassword") != std::string::npos) {
                    error = strprintf(
                        "parse error on line %i, using # in rpcpassword can be "
                        "ambiguous and should be avoided",
                        linenr);
                    return false;
                }
                options.emplace_back(name, value);
                if ((pos = name.rfind('.')) != std::string::npos &&
                    prefix.length() <= pos) {
                    sections.emplace_back(
                        SectionInfo{name.substr(0, pos), filepath, linenr});
                }
            } else {
                error = strprintf("parse error on line %i: %s", linenr, str);
                if (str.size() >= 2 && str.substr(0, 2) == "no") {
                    error += strprintf(", if you intended to specify a negated "
                                       "option, use %s=1 instead",
                                       str);
                }
                return false;
            }
        }
        ++linenr;
    }
    return true;
}

bool ArgsManager::ReadConfigStream(std::istream &stream,
                                   const std::string &filepath,
                                   std::string &error,
                                   bool ignore_invalid_keys) {
    LOCK(cs_args);
    std::vector<std::pair<std::string, std::string>> options;
    if (!GetConfigOptions(stream, filepath, error, options,
                          m_config_sections)) {
        return false;
    }
    for (const std::pair<std::string, std::string> &option : options) {
        const std::string strKey = std::string("-") + option.first;
        const unsigned int flags = FlagsOfKnownArg(strKey);
        if (flags) {
            if (!InterpretOption(strKey, option.second, flags, m_config_args,
                                 error)) {
                return false;
            }
        } else {
            if (ignore_invalid_keys) {
                LogPrintf("Ignoring unknown configuration value %s\n",
                          option.first);
            } else {
                error = strprintf("Invalid configuration value %s",
                                  option.first.c_str());
                return false;
            }
        }
    }
    return true;
}

bool ArgsManager::ReadConfigFiles(std::string &error,
                                  bool ignore_invalid_keys) {
    {
        LOCK(cs_args);
        m_config_args.clear();
        m_config_sections.clear();
    }

    // Error out if the conf file is specified but it doesn't exist
    const fs::path config_file_path = GetConfigFile(gArgs.GetArg("-conf", FITTEXXCOIN_CONF_FILENAME));
    if (!fs::exists(config_file_path) && IsArgSet("-conf")) {
        error = strprintf(_("The specified config file %s does not exist"),
                          config_file_path.string());
        return false;
    }

    const std::string confPath = config_file_path.string();
    fs::ifstream stream(GetConfigFile(confPath));

    // ok to not have a config file
    if (stream.good()) {
        if (!ReadConfigStream(stream, confPath, error, ignore_invalid_keys)) {
            return false;
        }
        // `-includeconf` cannot be included in the command line arguments
        // except as `-noincludeconf` (which indicates that no conf file should
        // be used).
        bool use_conf_file{true};
        {
            LOCK(cs_args);
            auto it = m_override_args.find("-includeconf");
            if (it != m_override_args.end()) {
                // ParseParameters() fails if a non-negated -includeconf is
                // passed on the command-line
                assert(it->second.empty());
                use_conf_file = false;
            }
        }
        if (use_conf_file) {
            std::string chain_id = GetChainName();
            std::vector<std::string> includeconf(GetArgs("-includeconf"));
            {
                // We haven't set m_network yet (that happens in
                // SelectParams()), so manually check for network.includeconf
                // args.
                std::vector<std::string> includeconf_net(
                    GetArgs(std::string("-") + chain_id + ".includeconf"));
                includeconf.insert(includeconf.end(), includeconf_net.begin(),
                                   includeconf_net.end());
            }

            // Remove -includeconf from configuration, so we can warn about
            // recursion later
            {
                LOCK(cs_args);
                m_config_args.erase("-includeconf");
                m_config_args.erase(std::string("-") + chain_id +
                                    ".includeconf");
            }

            for (const std::string &to_include : includeconf) {
                fs::ifstream include_config(GetConfigFile(to_include));
                if (include_config.good()) {
                    if (!ReadConfigStream(include_config, to_include, error,
                                          ignore_invalid_keys)) {
                        return false;
                    }
                    LogPrintf("Included configuration file %s\n",
                              to_include.c_str());
                } else {
                    error =
                        "Failed to include configuration file " + to_include;
                    return false;
                }
            }

            // Warn about recursive -includeconf
            includeconf = GetArgs("-includeconf");
            {
                std::vector<std::string> includeconf_net(
                    GetArgs(std::string("-") + chain_id + ".includeconf"));
                includeconf.insert(includeconf.end(), includeconf_net.begin(),
                                   includeconf_net.end());
                std::string chain_id_final = GetChainName();
                if (chain_id_final != chain_id) {
                    // Also warn about recursive includeconf for the chain that
                    // was specified in one of the includeconfs
                    includeconf_net = GetArgs(std::string("-") +
                                              chain_id_final + ".includeconf");
                    includeconf.insert(includeconf.end(),
                                       includeconf_net.begin(),
                                       includeconf_net.end());
                }
            }
            for (const std::string &to_include : includeconf) {
                fprintf(stderr,
                        "warning: -includeconf cannot be used from included "
                        "files; ignoring -includeconf=%s\n",
                        to_include.c_str());
            }
        }
    }

    // If datadir is changed in .conf file:
    ClearDatadirCache();
    if (!fs::is_directory(GetDataDir(false))) {
        error = strprintf("specified data directory \"%s\" does not exist.",
                          gArgs.GetArg("-datadir", "").c_str());
        return false;
    }
    return true;
}

std::string ArgsManager::GetChainName() const {
    LOCK(cs_args);
    bool fRegTest = ArgsManagerHelper::GetNetBoolArg(*this, "-regtest");
    bool fTestNet = ArgsManagerHelper::GetNetBoolArg(*this, "-testnet");
    bool fTestNet4 = ArgsManagerHelper::GetNetBoolArg(*this, "-testnet4");
    bool fScaleNet = ArgsManagerHelper::GetNetBoolArg(*this, "-scalenet");
    bool fChipNet = ArgsManagerHelper::GetNetBoolArg(*this, "-chipnet");

    if (fTestNet + fTestNet4 + fScaleNet + fRegTest + fChipNet > 1) {
        throw std::runtime_error(
            "Invalid combination of -regtest, -testnet, -testnet4, -scalenet, and -chipnet.");
    }
    if (fRegTest) {
        return CBaseChainParams::REGTEST;
    }
    if (fTestNet) {
        return CBaseChainParams::TESTNET;
    }
    if (fTestNet4) {
        return CBaseChainParams::TESTNET4;
    }
    if (fScaleNet) {
        return CBaseChainParams::SCALENET;
    }
    if (fChipNet) {
        return CBaseChainParams::CHIPNET;
    }
    return CBaseChainParams::MAIN;
}

bool RenameOver(fs::path src, fs::path dest) {
#ifdef WIN32
    return MoveFileExA(src.string().c_str(), dest.string().c_str(),
                       MOVEFILE_REPLACE_EXISTING) != 0;
#else
    int rc = std::rename(src.string().c_str(), dest.string().c_str());
    return (rc == 0);
#endif /* WIN32 */
}

/**
 * Ignores exceptions thrown by Boost's create_directories if the requested
 * directory exists. Specifically handles case where path p exists, but it
 * wasn't possible for the user to write to the parent directory.
 */
bool TryCreateDirectories(const fs::path &p) {
    try {
        return fs::create_directories(p);
    } catch (const fs::filesystem_error &) {
        if (!fs::exists(p) || !fs::is_directory(p)) {
            throw;
        }
    }

    // create_directory didn't create the directory, it had to have existed
    // already.
    return false;
}

bool FileCommit(FILE *file) {
    // harmless if redundantly called
    if (fflush(file) != 0) {
        LogPrintf("%s: fflush failed: %d\n", __func__, errno);
        return false;
    }
#ifdef WIN32
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file));
    if (FlushFileBuffers(hFile) == 0) {
        LogPrintf("%s: FlushFileBuffers failed: %d\n", __func__,
                  GetLastError());
        return false;
    }
#else
#if defined(__linux__) || defined(__NetBSD__)
    // Ignore EINVAL for filesystems that don't support sync
    if (fdatasync(fileno(file)) != 0 && errno != EINVAL) {
        LogPrintf("%s: fdatasync failed: %d\n", __func__, errno);
        return false;
    }
#elif defined(MAC_OSX) && defined(F_FULLFSYNC)
    // Manpage says "value other than -1" is returned on success
    if (fcntl(fileno(file), F_FULLFSYNC, 0) == -1) {
        LogPrintf("%s: fcntl F_FULLFSYNC failed: %d\n", __func__, errno);
        return false;
    }
#else
    if (fsync(fileno(file)) != 0 && errno != EINVAL) {
        LogPrintf("%s: fsync failed: %d\n", __func__, errno);
        return false;
    }
#endif
#endif
    return true;
}

bool TruncateFile(FILE *file, unsigned int length) {
#if defined(WIN32)
    return _chsize(_fileno(file), length) == 0;
#else
    return ftruncate(fileno(file), length) == 0;
#endif
}

/**
 * This function tries to raise the file descriptor limit to the requested
 * number. It returns the actual file descriptor limit (which may be more or
 * less than nMinFD)
 */
int RaiseFileDescriptorLimit(int nMinFD) {
#if defined(WIN32)
    return 2048;
#else
    struct rlimit limitFD;
    if (getrlimit(RLIMIT_NOFILE, &limitFD) != -1) {
        if (limitFD.rlim_cur < (rlim_t)nMinFD) {
            limitFD.rlim_cur = nMinFD;
            if (limitFD.rlim_cur > limitFD.rlim_max) {
                limitFD.rlim_cur = limitFD.rlim_max;
            }
            setrlimit(RLIMIT_NOFILE, &limitFD);
            getrlimit(RLIMIT_NOFILE, &limitFD);
        }
        return limitFD.rlim_cur;
    }
    // getrlimit failed, assume it's fine.
    return nMinFD;
#endif
}

/**
 * This function tries to make a particular range of a file allocated
 * (corresponding to disk space) it is advisory, and the range specified in the
 * arguments will never contain live data.
 */
void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length) {
#if defined(WIN32)
    // Windows-specific version.
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file));
    LARGE_INTEGER nFileSize;
    int64_t nEndPos = (int64_t)offset + length;
    nFileSize.u.LowPart = nEndPos & 0xFFFFFFFF;
    nFileSize.u.HighPart = nEndPos >> 32;
    SetFilePointerEx(hFile, nFileSize, 0, FILE_BEGIN);
    SetEndOfFile(hFile);
#elif defined(MAC_OSX)
    // OSX specific version.
    fstore_t fst;
    fst.fst_flags = F_ALLOCATECONTIG;
    fst.fst_posmode = F_PEOFPOSMODE;
    fst.fst_offset = 0;
    fst.fst_length = (off_t)offset + length;
    fst.fst_bytesalloc = 0;
    if (fcntl(fileno(file), F_PREALLOCATE, &fst) == -1) {
        fst.fst_flags = F_ALLOCATEALL;
        fcntl(fileno(file), F_PREALLOCATE, &fst);
    }
    ftruncate(fileno(file), fst.fst_length);
#elif defined(__linux__)
    // Version using posix_fallocate.
    off_t nEndPos = (off_t)offset + length;
    posix_fallocate(fileno(file), 0, nEndPos);
#else
    // Fallback version
    // TODO: just write one byte per block
    static const char buf[65536] = {};
    if (fseek(file, offset, SEEK_SET)) {
        return;
    }
    while (length > 0) {
        unsigned int now = 65536;
        if (length < now) {
            now = length;
        }
        // Allowed to fail; this function is advisory anyway.
        fwrite(buf, 1, now, file);
        length -= now;
    }
#endif
}

#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate) {
    WCHAR pszPath[MAX_PATH] = L"";

    if (SHGetSpecialFolderPathW(nullptr, pszPath, nFolder, fCreate)) {
        return fs::path(pszPath);
    }

    LogPrintf(
        "SHGetSpecialFolderPathW() failed, could not obtain requested path.\n");
    return fs::path("");
}
#endif

void runCommand(const std::string &strCommand) {
    if (strCommand.empty()) {
        return;
    }
#ifndef WIN32
    int nErr = ::system(strCommand.c_str());
#else
    int nErr = ::_wsystem(
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t>()
            .from_bytes(strCommand)
            .c_str());
#endif
    if (nErr) {
        LogPrintf("runCommand error: system(%s) returned %d\n", strCommand,
                  nErr);
    }
}

void SetupEnvironment()
{
#ifdef HAVE_MALLOPT_ARENA_MAX
    // glibc-specific: On 32-bit systems set the number of arenas to 1. By
    // default, since glibc 2.10, the C library will create up to two heap
    // arenas per core. This is known to cause excessive virtual address space
    // usage in our usage. Work around it by setting the maximum number of
    // arenas to 1.
    if (sizeof(void *) == 4) {
        mallopt(M_ARENA_MAX, 1);
    }
#endif
// On most POSIX systems (e.g. Linux, but not BSD) the environment's locale may
// be invalid, in which case the "C" locale is used as fallback.
#if !defined(WIN32) && !defined(MAC_OSX) && !defined(__FreeBSD__) &&           \
    !defined(__OpenBSD__)
    try {
        // Raises a runtime error if current locale is invalid.
        std::locale("");
    } catch (const std::runtime_error &) {
        setenv("LC_ALL", "C", 1);
    }
#elif defined(WIN32)
    // Set the default input/output charset is utf-8
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    // The path locale is lazy initialized and to avoid deinitialization errors
    // in multithreading environments, it is set explicitly by the main thread.
    // A dummy locale is used to extract the internal default locale, used by
    // fs::path, which is then used to explicitly imbue the path.
    std::locale loc = fs::path::imbue(std::locale::classic());
#ifndef WIN32
    fs::path::imbue(loc);
#else
    fs::path::imbue(std::locale(loc, new std::codecvt_utf8_utf16<wchar_t>()));
#endif
}

bool SetupNetworking() {
#ifdef WIN32
    // the below guard is to ensure we can call SetupNetworking() from multiple
    // places in the codebase and that WSAStartup() will only execute precisely
    // once, with the results cached in the static variable.
    static const bool wsaResult = [] {
        // Initialize Windows Sockets.
        WSADATA wsadata;
        int ret = WSAStartup(MAKEWORD(2, 2), &wsadata);
        if (ret != NO_ERROR || LOBYTE(wsadata.wVersion) != 2 ||
            HIBYTE(wsadata.wVersion) != 2) {
            return false;
        }
        return true;
    }();
    return wsaResult;
#else
    return true;
#endif
}

int GetNumCores() {
    return std::thread::hardware_concurrency();
}

std::string CopyrightHolders(const std::string &strPrefix) {
    return strPrefix +
           strprintf(_(COPYRIGHT_HOLDERS), COPYRIGHT_HOLDERS_SUBSTITUTION);
}

// Obtain the application startup time (used for uptime calculation)
int64_t GetStartupTime() {
    return nStartupTime;
}

fs::path AbsPathForConfigVal(const fs::path &path, bool net_specific) {
    return fs::absolute(path, GetDataDir(net_specific));
}

void ScheduleBatchPriority() {
#ifdef SCHED_BATCH
    const static sched_param param{};
    if (pthread_setschedparam(pthread_self(), SCHED_BATCH, &param) != 0) {
        LogPrintf("Failed to pthread_setschedparam: %s\n", strerror(errno));
    }
#endif
}

namespace util {
#ifdef WIN32
WinCmdLineArgs::WinCmdLineArgs() {
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_cvt;
    argv = new char *[argc];
    args.resize(argc);
    for (int i = 0; i < argc; i++) {
        args[i] = utf8_cvt.to_bytes(wargv[i]);
        argv[i] = &*args[i].begin();
    }
    LocalFree(wargv);
}

WinCmdLineArgs::~WinCmdLineArgs() {
    delete[] argv;
}

std::pair<int, char **> WinCmdLineArgs::get() {
    return std::make_pair(argc, argv);
}
#endif
} // namespace util
