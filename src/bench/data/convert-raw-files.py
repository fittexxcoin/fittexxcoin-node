#!/usr/bin/env python3
# Copyright (c) 2019 The Fittexxcoin Core developers
# Copyright (c) 2019-2021 The Fittexxcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import base64
import bz2
import os
import sys
from pathlib import Path, PurePath


def main(src_dir, dst_dir):
    os.makedirs(dst_dir, exist_ok=True)
    names_raw = sorted(Path(src_dir).glob("*.raw.bz2"))
    print("Found " + str(len(names_raw)) + " .raw file(s) in " + str(Path(src_dir).resolve(strict=True)))
    names = []

    for name_raw in names_raw:
        name = PurePath(PurePath(name_raw).stem).stem
        name_cpp = Path(dst_dir) / (name + ".cpp")

        with open(name_raw, "rb") as file_raw, open(name_cpp, "w") as file_cpp:
            print("Converting " + str(name_raw) + " to " + str(name_cpp) + " ...")
            contents = bz2.decompress(file_raw.read())
            file_cpp.write(
                """// DO NOT EDIT THIS FILE - it is machine-generated, use convert-raw-files.py to regenerate
#include <util/strencodings.h> // for DecodeBase64

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace benchmark {
namespace data {

const std::vector<uint8_t> &Get_""" + name + """() {
    static const char *const raw = \"""" + base64.b64encode(contents).decode('ascii') + """\";
    static std::vector<uint8_t> ret = DecodeBase64(raw); // this executes at most once first time through
    assert(ret.size() == """ + str(len(contents)) + """);
    return ret;
}

} // namespace data
} // namespace benchmark
""")
        names.append(name)

    name_h = Path(src_dir) / ".." / "data.h"
    with open(name_h, "w", encoding="utf8") as file_h:
        print("Writing " + str(len(names)) + " declaration(s) to " + str(name_h) + " ...")

        file_h.write(
            """// DO NOT EDIT THIS FILE - it is machine-generated, use data/convert-raw-files.py to regenerate
#pragma once

#include <cstdint>
#include <vector>

namespace benchmark {
namespace data {

""" + '\n'.join(["extern const std::vector<uint8_t> &Get_" + name + "();" for name in names]) + """

} // namespace data
} // namespace benchmark

""")
        print("Done")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(f"Usage: {sys.argv[0]} srd_dir dst_dir")
    main(sys.argv[1], sys.argv[2])
