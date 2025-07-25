// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Fittexxcoin Core developers
// Copyright (c) 2019-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Compile-time verification of assumptions we make.

#pragma once

#include <climits>
#include <cstdint>
#include <limits>
#include <type_traits>

// Assumption: We assume that the macro NDEBUG is not defined.
// Example(s): We use assert(...) extensively with the assumption of it never
//             being a noop at runtime.
#if defined(NDEBUG)
#error "Fittexxcoin cannot be compiled without assertions."
#endif

// Assumption: We assume a C++17 (ISO/IEC 14882:2017) compiler (minimum
//             requirement).
// Example(s): We may use use C++17 only constructs such as if constexpr,
//             structured binding, std::is_same_v, etc.
// Note:       MSVC does not report the expected __cplusplus value due to
//             legacy reasons.
#if !defined(_MSC_VER)
// N4713 §19.8/p1  [cpp.predefined]/p1::
// "The name __cplusplus is defined to the value 201703L when compiling a C++
//  translation unit."
static_assert(__cplusplus >= 201703L, "C++17 standard assumed");
#endif

// Assumption: We assume the floating-point types to fulfill the requirements of
//             IEC 559 (IEEE 754) standard.
// Example(s): Floating-point division by zero in ConnectBlock,
// CreateTransaction
//             and EstimateMedianVal.
static_assert(std::numeric_limits<float>::is_iec559, "IEEE 754 float assumed");
static_assert(std::numeric_limits<double>::is_iec559,
              "IEEE 754 double assumed");

// Assumption: We assume floating-point widths.
// Example(s): Type punning in serialization code
// (ser_{float,double}_to_uint{32,64}).
static_assert(sizeof(float) == 4, "32-bit float assumed");
static_assert(sizeof(double) == 8, "64-bit double assumed");

// Assumption: We assume integer widths.
// Example(s): GetSizeOfCompactSize and WriteCompactSize in the serialization
//             code.
static_assert(sizeof(short) == 2, "16-bit short assumed");
static_assert(sizeof(int) == 4, "32-bit int assumed");
static_assert(sizeof(unsigned) == 4, "32-bit unsigned assumed");

// Assumption: We assume 8-bit bytes, because 32-bit int and 16-bit short are
// assumed.
static_assert(CHAR_BIT == 8, "8-bit bytes assumed");

// Assumption: We assume uint8_t is an alias of unsigned char.
// char, unsigned char, and std::byte (C++17) are the only "byte types"
// according to the C++ Standard. "byte type" means a type that can be used to
// observe an object's value representation. We use uint8_t everywhere to see
// bytes, so we have to ensure that uint8_t is an alias to a "byte type".
// http://eel.is/c++draft/basic.types
// http://eel.is/c++draft/basic.memobj#def:byte
// http://eel.is/c++draft/expr.sizeof#1
// http://eel.is/c++draft/cstdint#syn
static_assert(std::is_same<uint8_t, unsigned char>::value,
              "uint8_t is an alias of unsigned char");

// Assumption: We assume Two's complement representation for negative numbers.
static_assert(static_cast<int>(0xffffffff) == -1, "Two's complement representation for negative numbers assumed");

// Some important things we are NOT assuming (non-exhaustive list):
// * We are NOT assuming a specific value for sizeof(std::size_t).
// * We are NOT assuming a specific value for std::endian::native.
// * We are NOT assuming a specific value for std::locale("").name().
// * We are NOT assuming a specific value for
// std::numeric_limits<char>::is_signed.
