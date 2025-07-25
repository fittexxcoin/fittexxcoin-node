# Developer Notes

## Coding Style

Various coding styles have been used during the history of the codebase,
and the result is not very consistent. However, we're now trying to converge to
a single style, so please use it in new code. Old code will be converted
gradually and you are encouraged to use the provided
[clang-format-diff script](../contrib/devtools/README.md#clang-format-diffpy)
to clean up the patch automatically before submitting a pull request.

- Basic rules specified in `src/.clang-format`.
    - Braces on new lines for namespaces, classes, functions, methods.
    - Braces on the same line for everything else.
    - 4 space indentation (no tabs) for every block except namespaces.
    - No indentation for `public`/`protected`/`private` or for `namespace`.
    - No extra spaces inside parenthesis; don't do ( this )
    - No space after function names; one space after `if`, `for` and `while`.
    - Always add braces for block statements (e.g. `if`, `for`, `while`).
    - `++i` is preferred over `i++`.
    - `static_assert` is preferred over `assert` where possible.
      Generally; compile-time checking is preferred over run-time checking.
    - Use CamelCase for functions/methods, and lowerCamelCase for variables.
        - GLOBAL_CONSTANTS should use UPPER_SNAKE_CASE.
        - namespaces should use lower_snake_case.
    - Function names should generally start with an English command-form verb
      (e.g. `ValidateTransaction`, `AddTransactionToMempool`, `ConnectBlock`)
    - Variable names should generally be nouns or past/future tense verbs.
      (e.g. `canDoThing`, `signatureOperations`, `didThing`)
    - Avoid using globals, remove existing globals whenever possible.
    - Class member variable names should be prepended with `m_`
    - DO choose easily readable identifier names.
    - DO favor readability over brevity.
    - DO NOT use Hungarian notation.
    - DO NOT use abbreviations or contractions within identifiers.
        - WRONG: mempool
        - RIGHT: MemoryPool
        - WRONG: ChangeDir
        - RIGHT: ChangeDirectory
    - DO NOT use obscure acronyms, DO uppercase any acronyms.
    - FINALLY, do not migrate existing code unless refactoring. It makes
      forwarding-porting from Fittexxcoin Core and Fittexxcoin ABC more difficult.

The naming convention roughly mirrors [Microsoft Naming Conventions](https://docs.microsoft.com/en-us/dotnet/standard/design-guidelines/general-naming-conventions)

C++ Coding Standards should strive to follow the [LLVM Coding Standards](https://llvm.org/docs/CodingStandards.html)

Code style example:

```c++
// namespaces should be lower_snake_case
namespace foo_bar_bob {

/**
 * Class is used for doing classy things.  All classes should
 * have a doxygen comment describing their PURPOSE.  That is to say,
 * why they exist.  Functional details can be determined from the code.
 * @see PerformTask()
 */
class Class {
private:
    //! memberVariable's name should be lowerCamelCase, and be a noun.
    int m_memberVariable;

public:
    /**
    * The documentation before a function or class method should follow Doxygen
    * spec. The name of the function should start with an English verb which
    * indicates the intended purpose of this code.
    *
    * The function name should be should be CamelCase.
    *
    * @param[in] s    A description
    * @param[in] n    Another argument description
    * @pre Precondition for function...
    */
    bool PerformTask(const std::string& s, int n) {
        // Use lowerChamelCase for local variables.
        bool didMore = false;

        // Comment summarizing the intended purpose of this section of code
        for (int i = 0; i < n; ++i) {
            if (!DidSomethingFail()) {
              return false;
            }
            ...
            if (IsSomethingElse()) {
                DoMore();
                didMore = true;
            } else {
                DoLess();
            }
        }

        return didMore;
    }
}
} // namespace foo
```

## Doxygen comments

To facilitate the generation of documentation, use doxygen-compatible comment
blocks for functions, methods and fields.

For example, to describe a function use:

```c++
/**
 * ... text ...
 * @param[in] arg1    A description
 * @param[in] arg2    Another argument description
 * @pre Precondition for function...
 */
bool function(int arg1, const char *arg2)
```

A complete list of `@xxx` commands can be found at
[https://www.doxygen.nl/manual/commands.html](https://www.doxygen.nl/manual/commands.html).
As Doxygen recognizes the comments by the delimiters (`/**` and `*/` in this case),
you don't *need* to provide any commands for a comment to be valid; just a description
text is fine.

To describe a class use the same construct above the class definition:

```c++
/**
 * Alerts are for notifying old versions if they become too obsolete and
 * need to upgrade. The message is displayed in the status bar.
 * @see GetWarnings()
 */
class CAlert
{
```

To describe a member or variable use:

```c++
int var; //!< Detailed description after the member
```

or

```cpp
//! Description before the member
int var;
```

Also OK:

```c++
///
/// ... text ...
///
bool function2(int arg1, const char *arg2)
```

Not OK (used plenty in the current source, but not picked up):

```c++
//
// ... text ...
//
```

A full list of comment syntaxes picked up by doxygen can be found at
[https://www.doxygen.nl/manual/docblocks.html](https://www.doxygen.nl/manual/docblocks.html),
but if possible use one of the above styles.

To build doxygen locally to test changes to the Doxyfile or visualize your
comments before landing changes:

```
# In the build directory, call:
doxygen doc/Doxyfile
# output goes to doc/doxygen/html/
```

## Development tips and tricks

### Compiling for debugging

Run `cmake`  with `-DCMAKE_BUILD_TYPE=Debug` to add additional compiler flags
that produce better debugging builds.

### Compiling for gprof profiling

```
  cmake -GNinja .. -DENABLE_HARDENING=OFF -DENABLE_PROFIILING=gprof
```

### debug.log

If the code is behaving strangely, take a look in the debug.log file in the data
directory; error and debugging messages are written there.

The `-debug=...` command-line option controls debugging; running with just
`-debug` or `-debug=1` will turn on all categories (and give you a very large
debug.log file).

The Qt code routes `qDebug()` output to debug.log under category "qt": run with
`-debug=qt` to see it.

### Writing tests

For details on unit tests, see `unit-tests.md`

For details on functional tests, see `functional-tests.md`

### Writing script integration tests

Script integration tests are built using `src/test/script_tests.cpp`:

1. Uncomment the line with `#define UPDATE_JSON_TESTS`
2. Add a new TestBuilder to the `script_build` test to cover your test case.
3. `ninja check-fittexxcoin-script_tests`
4. Copy your newly generated test JSON from `<build-dir>/src/script_tests.json.gen`
   to `src/test/data/script_tests.json`.

Please commit your TestBuilder along with your generated test JSON and cleanup
the uncommented #define before code review.

### Testnet and Regtest modes

Run with the `-testnet` option to run with "play fittexxcoins" on the test network,
if you are testing multi-machine code that needs to operate across the internet.

If you are testing something that can run on one machine, run with the `-regtest`
option. In regression test mode, blocks can be created on-demand; see [test/functional/](../test/functional)
for tests that run in `-regtest` mode.

### DEBUG_LOCKORDER

Fittexxcoin Node is a multi-threaded application, and deadlocks or other
multi-threading bugs can be very difficult to track down.
The `-DCMAKE_BUILD_TYPE=Debug` cmake option adds `-DDEBUG_LOCKORDER` to the
compiler flags. This inserts run-time checks to keep track of which locks are
held, and adds warnings to the debug.log file if inconsistencies are detected.

### Valgrind suppressions file

Valgrind is a programming tool for memory debugging, memory leak detection, and
profiling. The repo contains a Valgrind suppressions file
([`valgrind.supp`](../contrib/valgrind.supp))
which includes known Valgrind warnings in our dependencies that cannot be fixed
in-tree. Example use:

```shell
$ valgrind --suppressions=contrib/valgrind.supp src/test/test_fittexxcoin
$ valgrind --suppressions=contrib/valgrind.supp --leak-check=full \
      --show-leak-kinds=all src/test/test_fittexxcoin --log_level=test_suite
$ valgrind -v --leak-check=full src/fittexxcoind -printtoconsole
```

### Compiling for test coverage

LCOV can be used to generate a test coverage report based upon some test targets
execution. Some packages are required to generate the coverage report:
`c++filt`, `gcov`, `genhtml`, `lcov` and `python3`.

To install these dependencies on Debian 10:

```shell
sudo apt install binutils-common g++ lcov python3
```

To enable LCOV report generation during test runs:

```shell
cmake -GNinja .. -DENABLE_COVERAGE=ON
ninja coverage-check-all
```

A coverage report will now be accessible at `./check-all.coverage/index.html`.

To include branch coverage, you can add the `-DENABLE_BRANCH_COVERAGE=ON` option
to the `cmake` command line.

### Sanitizers

Fittexxcoin Node can be compiled with various "sanitizers" enabled, which add
instrumentation for issues regarding things like memory safety, thread race
conditions, or undefined behavior. This is controlled with the
`-DENABLE_SANITIZERS` cmake flag, which should be a semicolon separated list of
sanitizers to enable. The sanitizer list should correspond to supported
`-fsanitize=` options in your compiler. These sanitizers have runtime overhead,
so they are most useful when testing changes or producing debugging builds.

Some examples:

```bash
# Enable both the address sanitizer and the undefined behavior sanitizer
cmake -GNinja .. -DENABLE_SANITIZERS="address;undefined"

# Enable the thread sanitizer
cmake -GNinja .. -DENABLE_SANITIZERS=thread
```

If you are compiling with GCC you will typically need to install corresponding
"san" libraries to actually compile with these flags, e.g. libasan for the
address sanitizer, libtsan for the thread sanitizer, and libubsan for the
undefined sanitizer. If you are missing required libraries, the cmake script
will fail with an error when testing the sanitizer flags.

Note that the sanitizers will give a better output if they are run with a Debug
build configuration.

There are a number of known problems for which suppressions files are provided
under `test/sanitizer_suppressions`. These files are intended to be used with
the `suppressions` option from the sanitizers. If you are using the `check-*`
targets to run the tests, the suppression options are automatically set.
Otherwise they need to be set manually using environment variables; refer to
your compiler manual for the correct syntax.

The address sanitizer is known to fail in
[sha256_sse4::Transform](/src/crypto/sha256_sse4.cpp) which makes it unusable
unless you also use `-DCRYPTO_USE_ASM=OFF` when running cmake.
We would like to fix sanitizer issues, so please send pull requests if you can
fix any errors found by the address sanitizer (or any other sanitizer).

Not all sanitizer options can be enabled at the same time, e.g. trying to build
with `-DENABLE_SANITIZERS=="address;thread" will fail in the cmake script as
these sanitizers are mutually incompatible. Refer to your compiler manual to
learn more about these options and which sanitizers are supported by your
compiler.

Examples:

Build and run the test suite with the address sanitizer enabled:

```bash
mkdir build_asan
cd build_asan

cmake -GNinja .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_SANITIZERS=address \
  -DCRYPTO_USE_ASM=OFF

ninja check check-functional
```

Build and run the test suite with the thread sanitizer enabled (it can take a
very long time to complete):

```bash
mkdir build_tsan
cd build_tsan

cmake -GNinja .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_SANITIZERS=thread

ninja check check-functional
```

Build and run the test suite with the undefined sanitizer enabled:

```bash
mkdir build_ubsan
cd build_ubsan

cmake -GNinja .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_SANITIZERS=undefined

ninja check check-functional
```

Additional resources:

- [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)
- [LeakSanitizer](https://clang.llvm.org/docs/LeakSanitizer.html)
- [MemorySanitizer](https://clang.llvm.org/docs/MemorySanitizer.html)
- [ThreadSanitizer](https://clang.llvm.org/docs/ThreadSanitizer.html)
- [UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [GCC Instrumentation Options](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html)
- [Google Sanitizers Wiki](https://github.com/google/sanitizers/wiki)
- [Issue #12691: Enable -fsanitize flags in Travis](https://github.com/fittexxcoin/fittexxcoin/issues/12691)

## Locking/mutex usage notes

The code is multi-threaded, and uses mutexes and the
`LOCK` and `TRY_LOCK` macros to protect data structures.

Deadlocks due to inconsistent lock ordering (thread 1 locks `cs_main` and then
`cs_wallet`, while thread 2 locks them in the opposite order: result, deadlock
as each waits for the other to release its lock) are a problem. Compile with
`-DDEBUG_LOCKORDER` (or use `-DCMAKE_BUILD_TYPE=Debug`) to get lock order
inconsistencies reported in the debug.log file.

Re-architecting the core code so there are better-defined interfaces
between the various components is a goal, with any necessary locking
done by the components (e.g. see the self-contained `CBasicKeyStore` class
and its `cs_KeyStore` lock for example).

## Threads

- ThreadScriptCheck : Verifies block scripts.
- ThreadImport : Loads blocks from blk*.dat files or bootstrap.dat.
- StartNode : Starts other threads.
- ThreadDNSAddressSeed : Loads addresses of peers from the DNS.
- ThreadMapPort : Universal plug-and-play startup/shutdown
- ThreadSocketHandler : Sends/Receives data from peers on port 7890.
- ThreadOpenAddedConnections : Opens network connections to added nodes.
- ThreadOpenConnections : Initiates new connections to peers.
- ThreadMessageHandler : Higher-level message handling (sending and receiving).
- DumpAddresses : Dumps IP addresses of nodes to peers.dat.
- ThreadRPCServer : Remote procedure call handler, listens on port 7889 for
  connections and services them.
- Shutdown : Does an orderly shutdown of everything.

## Ignoring IDE/editor files

In closed-source environments in which everyone uses the same IDE it is common
to add temporary files it produces to the project-wide `.gitignore` file.

However, in open source software such as Fittexxcoin Node, where everyone uses
their own editors/IDE/tools, it is less common. Only you know what files your
editor produces and this may change from version to version. The canonical way
to do this is thus to create your local gitignore. Add this to `~/.gitconfig`:

```
[core]
        excludesfile = /home/.../.gitignore_global
```

(alternatively, type the command `git config --global core.excludesfile ~/.gitignore_global`
on a terminal)

Then put your favorite tool's temporary filenames in that file, e.g.

```
# NetBeans
nbproject/
```

Another option is to create a per-repository excludes file `.git/info/exclude`.
These are not committed but apply only to one repository.

If a set of tools is used by the build system or scripts the repository (for
example, lcov) it is perfectly acceptable to add its files to `.gitignore`
and commit them.

## Development guidelines

A few non-style-related recommendations for developers, as well as points to
pay attention to for reviewers of Fittexxcoin Node code.

### Wallet

- Make sure that no crashes happen with run-time option `-disablewallet`.
    - *Rationale*: In RPC code that conditionally uses the wallet (such as
      `validateaddress`) it is easy to forget that global pointer `pwalletMain`
      can be NULL. See `test/functional/disablewallet.py` for functional tests
      exercising the API with `-disablewallet`
- Include `db_cxx.h` (BerkeleyDB header) only when `ENABLE_WALLET` is set
    - *Rationale*: Otherwise compilation of the disable-wallet build will fail in
      environments without BerkeleyDB

### General C++

- Assertions should not have side-effects
    - *Rationale*: Even though the source code is set to refuse to compile
      with assertions disabled, having side-effects in assertions is unexpected and
      makes the code harder to understand
- If you use the `.h`, you must link the `.cpp`
    - *Rationale*: Include files define the interface for the code in implementation
      files. Including one but not linking the other is confusing. Please avoid that.
      Moving functions from the `.h` to the `.cpp` should not result in build errors
- Use the RAII (Resource Acquisition Is Initialization) paradigm where possible.
  For example by using `unique_ptr` for allocations in a function.
    - *Rationale*: This avoids memory and resource leaks, and ensures exception safety
- Use `std::make_unique()` to construct objects owned by `unique_ptr`s
    - *Rationale*: `std::make_unique` is concise and ensures exception safety
      in complex expressions.

### C++ data structures

- Never use the `std::map []` syntax when reading from a map, but instead use `.find()`
    - *Rationale*: `[]` does an insert (of the default element) if the item doesn't
      exist in the map yet. This has resulted in memory leaks in the past, as
      well as race conditions (expecting read-read behavior). Using `[]` is fine
      for *writing* to a map.
- Do not compare an iterator from one data structure with an iterator of
  another data structure (even if of the same type)
    - *Rationale*: Behavior is undefined. In C++ parlance this means "may reformat
      the universe", in practice this has resulted in at least one hard-to-debug
      crash bug.
- Watch out for out-of-bounds vector access. `&vch[vch.size()]` is illegal,
  including `&vch[0]` for an empty vector. Use `vch.data()` and `vch.data() +
  vch.size()` instead.
- Vector bounds checking is only enabled in debug mode. Do not rely on it
- Make sure that constructors initialize all fields. If this is skipped for a
  good reason (i.e., optimization on the critical path), add an explicit
  comment about this
    - *Rationale*: Ensure determinism by avoiding accidental use of uninitialized
      values. Also, static analyzers balk about this.
- By default, declare single-argument constructors `explicit`.
    - *Rationale*: This is a precaution to avoid unintended conversions that might
      arise when single-argument constructors are used as implicit conversion
      functions.
- Use explicitly signed or unsigned `char`s, or even better `uint8_t` and
  `int8_t`. Do not use bare `char` unless it is to pass to a third-party API.
  This type can be signed or unsigned depending on the architecture, which can
  lead to interoperability problems or dangerous conditions such as
  out-of-bounds array accesses
- Prefer explicit constructions over implicit ones that rely on 'magical' C++ behavior
    - *Rationale*: Easier to understand what is happening, thus easier to spot
      mistakes, even for those that are not language lawyers
- Use `Span` as function argument when it can operate on any range-like container.

    ```cpp
    void Foo(Span<const int> data);
    std::vector<int> vec{1,2,3};
    Foo(vec);
    ```

    - *Rationale*: Compared to `Foo(const vector<int>&)` this avoids the need for a (potentially expensive)
      conversion to vector if the caller happens to have the input stored in another type of container.
      However, be aware of the pitfalls documented in [span.h](../src/span.h).

- Initialize all non-static class members where they are defined

    ```cpp
    class A
    {
        uint32_t m_count{0};
    }
    ```

    - *Rationale*: Initializing the members in the declaration makes it easy to
      spot uninitialized ones, and avoids accidentally reading uninitialized memory

### Strings and formatting

- Use `std::string`, avoid C string manipulation functions
    - *Rationale*: C++ string handling is marginally safer, less scope for
      buffer overflows and surprises with `\0` characters. Also some C string manipulations
      tend to act differently depending on platform, or even the user locale
- Use `ParseInt32`, `ParseInt64`, `ParseUInt32`, `ParseUInt64`, `ParseDouble`
  from `utilstrencodings.h` for number parsing
    - *Rationale*: These functions do overflow checking, and avoid pesky locale
      issues

### Variable names

The shadowing warning (`-Wshadow`) is enabled by default. It prevents issues rising
from using a different variable with the same name.

E.g. in member initializers, prepend `_` to the argument name shadowing the
member name:

```c++
class AddressBookPage
{
    Mode m_mode;
}

AddressBookPage::AddressBookPage(Mode _mode) :
      m_mode(_mode)
...
```

When using nested cycles, do not name the inner cycle variable the same as in
upper cycle etc.

Please name variables so that their names do not shadow variables defined in the
source code.

### Threads and synchronization

- Build and run tests with `-DDEBUG_LOCKORDER` to verify that no potential
  deadlocks are introduced. As of 0.12, this is defined by default when
  configuring with `-DCMAKE_BUILD_TYPE=Debug`

- When using `LOCK`/`TRY_LOCK` be aware that the lock exists in the context of
  the current scope, so surround the statement and the code that needs the lock
  with braces

    OK:

    ```c++
    {
        TRY_LOCK(cs_vNodes, lockNodes);
        ...
    }
    ```

    Wrong:

    ```c++
    TRY_LOCK(cs_vNodes, lockNodes);
    {
        ...
    }
    ```

### Scripts

#### Shebang

- Use `#!/usr/bin/env bash` instead of obsolete `#!/bin/bash`.
    - [*Rationale*](https://github.com/dylanaraps/pure-bash-bible#shebang):
      `#!/bin/bash` assumes it is always installed to /bin/ which can cause issues;
      `#!/usr/bin/env bash` searches the user's PATH to find the bash binary.

  OK:

  ```bash
  #!/usr/bin/env bash
  ```

  Wrong:

  ```bash
  #!/bin/bash
  ```

### Source code organization

- Implementation code should go into the `.cpp` file and not the `.h`, unless
  necessary due to template usage or when performance due to inlining is critical
    - *Rationale*: Shorter and simpler header files are easier to read, and
      reduce compile time

- Use only the lowercase alphanumerics (`a-z0-9`), underscore (`_`) and hyphen
  (`-`) in source code filenames.
    - *Rationale*: `grep`:ing and auto-completing filenames is easier when using
      a consistent naming pattern. Potential problems when building on case-insensitive
      filesystems are avoided when using only lowercase characters in source code
      filenames.
- Don't import anything into the global namespace (`using namespace ...`). Use
  fully specified types such as `std::string`.
    - *Rationale*: Avoids symbol conflicts
- Terminate namespaces with a comment (`// namespace mynamespace`). The comment
  should be placed on the same line as the brace closing the namespace, e.g.

  ```c++
  namespace mynamespace {
      ...
  } // namespace mynamespace

  namespace {
      ...
  } // namespace
  ```

    - *Rationale*: Avoids confusion about the namespace context

### Header Inclusions

- Header inclusions should use angle brackets (`#include <>`).
  The include path should be relative to the `src` folder.
  e.g.: `#include <qt/test/guiutiltests.h>`

- Native C++ headers should be preferred over C compatibility headers.
  e.g.: use `<cstdint>` instead of `<stdint.h>`

- In order to make the code consistent, header files should be included in the
  following order, with each section separated by a newline:
    1. In a .cpp file, the associated .h is in first position. In a test source,
       this is the header file under test.
    2. The project headers.
    3. The test headers.
    4. The 3rd party libraries headers. Different libraries should be in different
       sections.
    5. The system libraries.

All headers should be lexically ordered inside their block.

- Use include guards to avoid the problem of double inclusion. The header file
  `foo/bar.h` should use the include guard identifier `FITTEXXCOIN_FOO_BAR_H`, e.g.

    ```c++
    #ifndef FITTEXXCOIN_FOO_BAR_H
    #define FITTEXXCOIN_FOO_BAR_H
    ...
    #endif // FITTEXXCOIN_FOO_BAR_H
    ```

### GUI

- Do not display or manipulate dialogs in model code (classes `*Model`)
    - *Rationale*: Model classes pass through events and data from the core, they
      should not interact with the user. That's where View classes come in. The
      converse also holds: try to not directly access core data structures from Views.
- Avoid adding slow or blocking code in the GUI thread. In particular do not
  add new `interface::Node` and `interface::Wallet` method calls, even if they
  may be fast now, in case they are changed to lock or communicate across
  processes in the future.
- Prefer to offload work from the GUI thread to worker threads (see
  `RPCExecutor` in console code as an example) or take other steps (see
  [https://doc.qt.io/archives/qq/qq27-responsive-guis.html](https://doc.qt.io/archives/qq/qq27-responsive-guis.html))
  to keep the GUI
  responsive.
    - *Rationale*: Blocking the GUI thread can increase latency, and lead to
      hangs and deadlocks.

### Unit Tests

- Test suite naming convention: The Boost test suite in file
  `src/test/foo_tests.cpp` should be named `foo_tests`. Test suite names must
  be unique.

### Subtrees

Several parts of the repository are subtrees of software maintained elsewhere.

Some of these are maintained by active developers of Fittexxcoin Core, in which case
changes should probably go directly upstream without being PRed directly against
the project.  They will be merged back in the next subtree merge.

Others are external projects without a tight relationship with our project.
Changes to these should also be sent upstream but bugfixes may also be prudent to
PR against Fittexxcoin Core so that they can be integrated quickly. Cosmetic changes
should be purely taken upstream.

There is a tool in `test/lint/git-subtree-check.sh` to check a subtree directory
for consistency with its upstream repository.

Current subtrees include:

- src/leveldb
    - Upstream at [https://github.com/google/leveldb](https://github.com/google/leveldb);
      Maintained by Google, but open important PRs to Core to avoid delay.
    - **Note**: Follow the instructions in [Upgrading LevelDB](#upgrading-leveldb)
      when merging upstream changes to the leveldb subtree.
- src/libsecp256k1
    - Upstream at [https://github.com/fittexxcoin-core/secp256k1/](https://github.com/fittexxcoin-core/secp256k1/);
      actively maintained by Core contributors.
- src/crypto/ctaes
    - Upstream at [https://github.com/fittexxcoin-core/ctaes](https://github.com/fittexxcoin-core/ctaes);
      actively maintained by Core contributors.
- src/univalue
    - fxxN no longer has a single upstream for `src/univalue`, but maintains its
      own univalue code based on fixes from several repositories, namely [https://github.com/jgarzik/univalue](https://github.com/jgarzik/univalue),
      Fittexxcoin Core's fork of univalue and the Fittexxcoin ABC repository.

### Upgrading LevelDB

Extra care must be taken when upgrading LevelDB. This section explains issues
you must be aware of.

#### File Descriptor Counts

In most configurations we use the default LevelDB value for `max_open_files`,
which is 1000 at the time of this writing. If LevelDB actually uses this many
file descriptors it will cause problems with Fittexxcoin's `select()` loop, because
it may cause new sockets to be created where the fd value is >= 1024. For this
reason, on 64-bit Unix systems we rely on an internal LevelDB optimization that
uses `mmap()` + `close()` to open table files without actually retaining
references to the table file descriptors. If you are upgrading LevelDB, you must
sanity check the changes to make sure that this assumption remains valid.

In addition to reviewing the upstream changes in `env_posix.cc`, you can use `lsof`
to check this. For example, on Linux this command will show open `.ldb` file counts:

```bash
$ lsof -p $(pidof fittexxcoind) |\
    awk 'BEGIN { fd=0; mem=0; } /ldb$/ { if ($4 == "mem") mem++; else fd++ } END { printf "mem = %s, fd = %s\n", mem, fd}'
mem = 119, fd = 0
```

The `mem` value shows how many files are mmap'ed, and the `fd` value shows you
many file descriptors these files are using. You should check that `fd` is a
small number (usually 0 on 64-bit hosts).

See the notes in the `SetMaxOpenFiles()` function in `dbwrapper.cc` for more
details.

#### Consensus Compatibility

It is possible for LevelDB changes to inadvertently change consensus
compatibility between nodes. This happened in Fittexxcoin 0.8 (when LevelDB was
first introduced). When upgrading LevelDB you should review the upstream changes
to check for issues affecting consensus compatibility.

For example, if LevelDB had a bug that accidentally prevented a key from being
returned in an edge case, and that bug was fixed upstream, the bug "fix" would
be an incompatible consensus change. In this situation the correct behavior
would be to revert the upstream fix before applying the updates to Fittexxcoin
Cash Node's copy of LevelDB. In general you should be wary of any upstream
changes affecting what data is returned from LevelDB queries.

## Git and GitLab tips

- See CONTRIBUTING.md for instructions on setting up your repo correctly.

- Your git remote origin should be set to: `git@gitlab.com:username/fittexxcoin-node.git`
  where `username` is your account name on GitLab. See CONTRIBUTING.md for details.

- For resolving merge/rebase conflicts, it can be useful to enable diff3 style using
  `git config merge.conflictstyle diff3`. Instead of

  ```
  <<<
  yours
  ===
  theirs
  >>>
  ```

  you will see

  ```
  <<<
  yours
  |||
  original
  ===
  theirs
  >>>
  ```

  This may make it much clearer what caused the conflict. In this style, you can
  often just look at what changed between *original* and *theirs*, and mechanically
  apply that to *yours* (or the other way around).

- When reviewing patches which change indentation in C++ files, use `git diff -w`
  and `git show -w`. This makes the diff algorithm ignore whitespace changes. This
  feature is also available on gitlab.com, by adding `?w=1` at the end of any URL
  which shows a diff.

- When reviewing patches that change symbol names in many places, use
  `git diff --word-diff`. This will instead of showing the patch as deleted/added
  *lines*, show deleted/added *words*.

- When reviewing patches that move code around, try using
  `git diff --patience commit~:old/file.cpp commit:new/file/name.cpp`, and ignoring
  everything except the moved body of code which should show up as neither `+`
  or `-` lines. In case it was not a pure move, this may even work when combined
  with the `-w` or `--word-diff` options described above.

- When looking at other's pull requests, it may make sense to add the following
  section to your `.git/config` file:

  ```
  [remote "upstream-merges"]
          url = git@gitlab.com:fittexxcoin-node/fittexxcoin-node.git
          fetch = +refs/merge-requests/*/head:refs/remotes/upstream-merges/merge-requests/*
  ```

  This will add an `upstream-merges` remote to your git repository, which can be
  fetched using `git fetch --all` or `git fetch upstream-merges`. Afterwards, you
  can use `upstream-merges/merge-requests/NUMBER` in arguments to `git show`,
  `git checkout` and anywhere a commit id would be acceptable to see the changes
  from merge request NUMBER.

## RPC interface guidelines

A few guidelines for introducing and reviewing new RPC interfaces:

- Method naming: use consecutive lower-case names such as `getrawtransaction`
  and `submitblock`
    - *Rationale*: Consistency with existing interface.
- Argument naming: use snake case `fee_delta` (and not, e.g. camel case `feeDelta`)
    - *Rationale*: Consistency with existing interface.
- Use the JSON parser for parsing, don't manually parse integers or strings from
  arguments unless absolutely necessary.
    - *Rationale*: Introduces hand-rolled string manipulation code at both the
      caller and callee sites, which is error prone, and it is easy to get things
      such as escaping wrong.
      JSON already supports nested data structures, no need to re-invent the wheel.
    - *Exception*: AmountFromValue can parse amounts as string. This was introduced
      because many JSON parsers and formatters hard-code handling decimal numbers
      as floating point values, resulting in potential loss of precision. This is
      unacceptable for monetary values. **Always** use `AmountFromValue` and
      `ValueFromAmount` when inputting or outputting monetary values. The only
      exceptions to this are `prioritisetransaction` and `getblocktemplate`
      because their interface is specified as-is in BIP22.
- Missing arguments and 'null' should be treated the same: as default values. If
  there is no default value, both cases should fail in the same way. The easiest
  way to follow this guideline is detect unspecified arguments with `params[x].isNull()`
  instead of `params.size() <= x`. The former returns true if the argument is
  either null or missing, while the latter returns true if is missing, and false
  if it is null.
    - *Rationale*: Avoids surprises when switching to name-based arguments. Missing
      name-based arguments are passed as 'null'.
- Try not to overload methods on argument type. E.g. don't make `getblock(true)`
  and `getblock("hash")` do different things.
    - *Rationale*: This is impossible to use with `fittexxcoin-cli`, and can be
      surprising to users.
    - *Exception*: Some RPC calls can take both an `int` and `bool`, most notably
      when a bool was switched to a multi-value, or due to other historical reasons.
      **Always** have false map to 0 and true to 1 in this case.
- Don't forget to fill in the argument names correctly in the RPC command table.
    - *Rationale*: If not, the call can not be used with name-based arguments.
- Set okSafeMode in the RPC command table to a sensible value: safe mode is when
  the blockchain is regarded to be in a confused state, and the client deems it
  unsafe to do anything irreversible such as send. Anything that just queries
  should be permitted.
    - *Rationale*: Troubleshooting a node in safe mode is difficult if half the
      RPCs don't work.
- Add every non-string RPC argument `(method, idx, name)` to the table `vRPCConvertParams`
  in `rpc/client.cpp`.
    - *Rationale*: `fittexxcoin-cli` and the GUI debug console use this table to
      determine how to convert a plaintext command line to JSON. If the types
      don't match, the method can be unusable from there.
- A RPC method must either be a wallet method or a non-wallet method. Do not
  introduce new methods such as `signrawtransaction` that differ in behavior
  based on presence of a wallet.
    - *Rationale*: as well as complicating the implementation and interfering
      with the introduction of multi-wallet, wallet and non-wallet code should be
      separated to avoid introducing circular dependencies between code units.
- Try to make the RPC response a JSON object.
    - *Rationale*: If a RPC response is not a JSON object then it is harder to
      avoid API breakage if new data in the response is needed.
- Wallet RPCs call BlockUntilSyncedToCurrentChain to maintain consistency with
  `getblockchaininfo`'s state immediately prior to the call's execution. Wallet
  RPCs whose behavior does *not* depend on the current chainstate may omit this
  call.
    - *Rationale*: In previous versions of Fittexxcoin Core, the wallet was always
      in-sync with the chainstate (by virtue of them all being updated in the
      same cs_main lock). In order to maintain the behavior that wallet RPCs
      return results as of at least the highest best-known block an RPC
      client may be aware of prior to entering a wallet RPC call, we must block
      until the wallet is caught up to the chainstate as of the RPC call's entry.
      This also makes the API much easier for RPC clients to reason about.
- Be aware of RPC method aliases and generally avoid registering the same
  callback function pointer for different RPCs.
    - *Rationale*: RPC methods registered with the same function pointer will be
      considered aliases and only the first method name will show up in the
      `help` rpc command list.
    - *Exception*: Using RPC method aliases may be appropriate in cases where a
      new RPC is replacing a deprecated RPC, to avoid both RPCs confusingly
      showing up in the command list.
