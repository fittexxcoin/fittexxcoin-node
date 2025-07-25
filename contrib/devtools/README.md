# Contents

This directory contains tools for developers working on this repository.

## `copyright_header.py`

Provides utilities for managing copyright headers of `The Fittexxcoin developers`
in repository source files. It has three subcommands:

```
./copyright_header.py report <base_directory> [verbose]
./copyright_header.py update <base_directory>
./copyright_header.py insert <file>
```

Running these subcommands without arguments displays a usage string.

### `copyright_header.py report <base_directory> [verbose]`

Produces a report of all copyright header notices found inside the source files
of a repository. Useful to quickly visualize the state of the headers.
Specifying `verbose` will list the full filenames of files of each category.

### `copyright_header.py update <base_directory> [verbose]`

Updates all the copyright headers of `The Fittexxcoin developers` which were changed
in a year more recent than is listed. For example:

```
// Copyright (c) <firstYear>-<lastYear> The Fittexxcoin developers
```

will be updated to:

```
// Copyright (c) <firstYear>-<lastModifiedYear> The Fittexxcoin developers
```

where `<lastModifiedYear>` is obtained from the `git log` history.

This subcommand also handles copyright headers that have only a single year. In
those cases:

```
// Copyright (c) <year> The Fittexxcoin developers
```

will be updated to:

```
// Copyright (c) <year>-<lastModifiedYear> The Fittexxcoin developers
```

where the update is appropriate.

### `copyright_header.py insert <file>`

Inserts a copyright header for `The Fittexxcoin developers` at the top of the file
in either Python or C++ style as determined by the file extension. If the file
is a Python file and it has  `#!` starting the first line, the header is
inserted in the line below it.

The copyright dates will be set to be `<year_introduced>-<current_year>` where
`<year_introduced>` is according to the `git log` history. If
`<year_introduced>` is equal to `<current_year>`, it will be set as a single
year rather than two hyphenated years.

If the file already has a copyright for `The Fittexxcoin developers`, the script
will exit.

## `optimize-pngs.py`

A script to optimize png files in the fittexxcoin
repository (requires pngcrush).

## security-check.py and test-security-check.py

Perform basic ELF security checks on a series of executables.

## `symbol-check.py`

A script to check that the (Linux) executables produced by gitian only contain
allowed gcc, glibc and libstdc++ version symbols. This makes sure they are
still compatible with the minimum supported Linux distribution versions.

Example usage after a gitian build:

```
find ../gitian-builder/build -type f -executable | xargs python3 contrib/devtools/symbol-check.py
```

If only supported symbols are used the return value will be 0 and the output will be empty.

If there are 'unsupported' symbols, the return value will be 1 a list like this will be printed:

```
.../64/test_fittexxcoin: symbol memcpy from unsupported version GLIBC_2.14
.../64/test_fittexxcoin: symbol __fdelt_chk from unsupported version GLIBC_2.15
.../64/test_fittexxcoin: symbol std::out_of_range::~out_of_range() from unsupported version GLIBCXX_3.4.15
.../64/test_fittexxcoin: symbol _ZNSt8__detail15_List_nod from unsupported version GLIBCXX_3.4.15
```

## `circular-dependencies.py`

Run this script from the root of the source tree (`src/`) to find circular dependencies in the source code.
This looks only at which files include other files, treating the `.cpp` and `.h` file as one unit.

Example usage:

```
cd <project root>/src
../contrib/devtools/circular-dependencies.py {*,*/*,*/*/*}.{h,cpp}
```
