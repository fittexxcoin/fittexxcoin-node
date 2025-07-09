# Compiling/running unit tests

Unit tests are not part of the default build but can be built on demand.

All the unit tests can be built and run with a single command: `ninja check`.

## fittexxcoind unit tests

The `fittexxcoind` unit tests can be built with `ninja test_fittexxcoin`.
They can also be built and run in a single command with `ninja check-fittexxcoin`.

To run the `fittexxcoind` tests manually, launch `src/test/test_fittexxcoin`.

To add more `fittexxcoind` tests, add `BOOST_AUTO_TEST_CASE` functions to the
existing .cpp files in the `src/test/` directory or add new .cpp files that
implement new `BOOST_AUTO_TEST_SUITE` sections.

## fittexxcoin-qt unit tests

The `fittexxcoin-qt` tests can be built with `ninja test_fittexxcoin-qt` or
built and run in a single command with `ninja check-fittexxcoin-qt`.

To run the `fittexxcoin-qt` tests manually, launch `src/qt/test/test_fittexxcoin-qt`.

To add more `fittexxcoin-qt` tests, add them to the `src/qt/test/` directory and
the `src/qt/test/test_main.cpp` file.

## fittexxcoin-seeder unit tests

The `fittexxcoin-seeder` unit tests can be built with `ninja test_fittexxcoin-seeder` or
built and run in a single command with `ninja check-fittexxcoin-seeder`.

To run the `fittexxcoin-seeder` tests manually, launch
`src/seeder/test/test_fittexxcoin-seeder`.

To add more `fittexxcoin-seeder` tests, add `BOOST_AUTO_TEST_CASE` functions to the
existing .cpp files in the `src/seeder/test/` directory or add new .cpp files
that implement new `BOOST_AUTO_TEST_SUITE` sections.

## Running individual tests

`test_fittexxcoin` has some built-in command-line arguments; for
example, to run just the `getarg_tests` verbosely:

```
test_fittexxcoin --log_level=all --run_test=getarg_tests
```

... or to run just the doubledash test:

```
test_fittexxcoin --run_test=getarg_tests/doubledash
```

Run `test_fittexxcoin --help` for the full list.

## Note on adding test cases

The build system is setup to compile an executable called `test_fittexxcoin`
that runs all of the unit tests.  The main source file is called
setup_common.cpp. To add a new unit test file to our test suite you need
to add the file to `src/test/CMakeLists.txt`. The pattern is to create
one test file for each class or source file for which you want to create
unit tests.  The file naming convention is `<source_filename>_tests.cpp`
and such files should wrap their tests in a test suite
called `<source_filename>_tests`. For an example of this pattern,
examine `uint256_tests.cpp`.

For further reading, I found the following website to be helpful in
explaining how the boost unit test framework works:
[https://legalizeadulthood.wordpress.com/2009/07/04/c-unit-tests-with-boost-test-part-1/](https://legalizeadulthood.wordpress.com/2009/07/04/c-unit-tests-with-boost-test-part-1/)

## Debugging unit tests

Simple example of debugging unit tests with GDB on Linux:

```
cd /build/src/test
gdb test_fittexxcoin
break interpreter.cpp:295  # No path is necessary, just the file name and line number
run
# GDB hits the breakpoint
p/x opcode  # print the value of the variable (in this case, opcode) in hex
c           # continue
```

Simple example of debugging unit tests with LLDB (OSX or Linux):

```
cd /build/src/test
lldb -- test_fittexxcoin
break set --file interpreter.cpp --line 295
run
```
