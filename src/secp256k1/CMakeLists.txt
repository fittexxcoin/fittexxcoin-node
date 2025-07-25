# Copyright (c) 2017 The Fittexxcoin developers

cmake_minimum_required(VERSION 3.13)
project(secp256k1 LANGUAGES C VERSION 0.1.0)

# Add path for custom modules when building as a standalone project
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# Default to RelWithDebInfo configuration
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
		"Select the configuration for the build" FORCE)
	set(__NO_USER_CMAKE_BUILD_TYPE ON CACHE BOOL "True if the user didn't set a build type on the command line")
endif()

option(SECP256K1_ENABLE_COVERAGE "Enable coverage" OFF)
option(SECP256K1_ENABLE_BRANCH_COVERAGE "Enable branch coverage" OFF)

include(AddCompilerFlags)

if(SECP256K1_ENABLE_COVERAGE)
	include(Coverage)

	enable_coverage(${SECP256K1_ENABLE_BRANCH_COVERAGE})

	exclude_from_coverage("${CMAKE_CURRENT_SOURCE_DIR}/src/bench")

	# If no build type is manually defined, override the optimization level.
	# Otherwise, alert the user than the coverage result might be useless.
	if(__NO_USER_CMAKE_BUILD_TYPE)
		set_c_optimization_level(0)
	else()
		message(WARNING "It is advised to not enforce CMAKE_BUILD_TYPE to get the best coverage results")
	endif()

	set(COVERAGE 1)
endif()



# libsecp256k1 use a different set of flags.
add_c_compiler_flags(
	-pedantic
	-Wall
	-Wextra
	-Wcast-align
	-Wshadow
	-Wno-unused-function
	-Wno-overlength-strings
	-std=c89
	-Wnested-externs
	-Wstrict-prototypes
	-Wno-long-long
)

# Default visibility is hidden on all targets.
set(CMAKE_C_VISIBILITY_PRESET hidden)

include_directories(
	.
	src
	# For the config
	${CMAKE_CURRENT_BINARY_DIR}/src
)

# The library
add_library(secp256k1 src/secp256k1.c)
target_include_directories(secp256k1 PUBLIC include)

set(SECP256K1_PUBLIC_HEADERS
	include/secp256k1.h
	include/secp256k1_preallocated.h
)

option(SECP256K1_ENABLE_BIGNUM "Use the GMP bignum implementation" OFF)
if(SECP256K1_ENABLE_BIGNUM)
	# We need to link in GMP
	find_package(GMP REQUIRED)
	target_link_libraries(secp256k1 GMP::gmp)
	set(USE_NUM_GMP 1)
	set(USE_FIELD_INV_NUM 1)
	set(USE_SCALAR_INV_NUM 1)
else()
	set(USE_NUM_NONE 1)
	set(USE_FIELD_INV_BUILTIN 1)
	set(USE_SCALAR_INV_BUILTIN 1)
endif()

# Guess the target architecture, within the ones with supported ASM.
# First check if the CMAKE_C_COMPILER_TARGET is set (should be when
# cross compiling), then CMAKE_SYSTEM_PROCESSOR as a fallback if meaningful
# (this is not the case for ARM as the content is highly non standard).
if(CMAKE_C_COMPILER_TARGET MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
	set(SECP256K1_ASM_BUILD_TARGET "x86_64")
	set(SECP256K1_DEFAULT_USE_ASM ON)
elseif(CMAKE_C_COMPILER_TARGET MATCHES "arm-linux-gnueabihf")
	set(SECP256K1_ASM_BUILD_TARGET "arm-linux-gnueabihf")
	set(SECP256K1_DEFAULT_USE_ASM ON)
endif()

# Enable ASM by default only if we are building for a compatible target.
# The user can still enable/disable it manually if needed.
option(SECP256K1_USE_ASM "Use assembly" ${SECP256K1_DEFAULT_USE_ASM})

if(SECP256K1_USE_ASM)
	macro(unsupported_asm_error)
		message(FATAL_ERROR
			"Assembly is enabled, but not supported for your target architecture."
			"Re-run cmake with -DSECP256K1_USE_ASM=OFF to disable ASM support."
		)
	endmacro()

	if(SECP256K1_ASM_BUILD_TARGET MATCHES "x86_64")
		# We check if amd64 asm is supported.
		check_c_source_compiles("
			#include <stdint.h>
			int main() {
				uint64_t a = 11, tmp;
				__asm__ __volatile__(\"movq \$0x100000000,%1; mulq %%rsi\" : \"+a\"(a) : \"S\"(tmp) : \"cc\", \"%rdx\");
				return 0;
			}
		" USE_ASM_X86_64)

		if(NOT USE_ASM_X86_64)
			unsupported_asm_error()
		endif()
	elseif(SECP256K1_ASM_BUILD_TARGET MATCHES "arm-linux-gnueabihf")
		enable_language(ASM)
		set(USE_EXTERNAL_ASM 1)
		add_library(secp256k1_common src/asm/field_10x26_arm.s)
		target_link_libraries(secp256k1 secp256k1_common)
	else()
		unsupported_asm_error()
	endif()
endif()

# We make sure __int128 is defined
include(CheckTypeSize)
check_type_size(__int128 SIZEOF___INT128)
if(SIZEOF___INT128 EQUAL 16)
	set(HAVE___INT128 1)
else()
	# If we do not support __int128, we should be falling back
	# on 32bits implementations for field and scalar.
endif()

# Select the finite field implementation to use.
# This can be autodetected or forced by setting USE_FIELD to 32bit or 64bit.
# See the truth table below:
# +----------------------------------------------------------------------------------------------------------------------+
# | USE_FIELD=64bit | USE_FIELD=32bit | HAVE___INT128 | USE_ASM_X86_64 | USE_FIELD_5X52 | USE_FIELD_10x26 | Config error |
# +----------------------------------------------------------------------------------------------------------------------+
# |        0        |        0        |       0       |       0        |       0        |        1        |      0       |
# |        0        |        0        |       0       |       1        |       1        |        0        |      0       |
# |        0        |        0        |       1       |       0        |       1        |        0        |      0       |
# |        0        |        0        |       1       |       1        |       1        |        0        |      0       |
# |        0        |        1        |       0       |       0        |       0        |        1        |      0       |
# |        0        |        1        |       0       |       1        |       0        |        1        |      0       |
# |        0        |        1        |       1       |       0        |       0        |        1        |      0       |
# |        0        |        1        |       1       |       1        |       0        |        1        |      0       |
# |        1        |        0        |       0       |       0        |       0        |        0        |      1       |
# |        1        |        0        |       0       |       1        |       1        |        0        |      0       |
# |        1        |        0        |       1       |       0        |       1        |        0        |      0       |
# |        1        |        0        |       1       |       1        |       1        |        0        |      0       |
# +----------------------------------------------------------------------------------------------------------------------+
set(USE_FIELD "" CACHE STRING "Force the finite field implementation to use (can be 32bit or 64bit)")
if(USE_FIELD STREQUAL "64bit" AND NOT (HAVE___INT128 OR USE_ASM_X86_64))
	message(SEND_ERROR "64 finite field requested but the compiler does not support __int128 or inline assembly")
elseif(NOT USE_FIELD STREQUAL "32bit" AND (HAVE___INT128 OR USE_ASM_X86_64))
	set(USE_FIELD_5X52 1)
else()
	set(USE_FIELD_10X26 1)
endif()

# Select the scalar implementation to use.
# This can be autodetected or forced by setting USE_SCALAR to 32bit or 64bit.
# See the truth table below:
# +--------------------------------------------------------------------------------------------------------+
# | USE_SCALAR=64bit | USE_SCALAR=32bit | HAVE___INT128 | USE_SCALAR_4X64 | USE_SCALAR_8X32 | Config error |
# +--------------------------------------------------------------------------------------------------------+
# |        0         |        0         |       0       |        0        |        1        |      0       |
# |        0         |        0         |       1       |        1        |        0        |      0       |
# |        0         |        1         |       0       |        0        |        1        |      0       |
# |        0         |        1         |       1       |        0        |        1        |      0       |
# |        1         |        0         |       0       |        0        |        0        |      1       |
# |        1         |        0         |       1       |        1        |        0        |      0       |
# +--------------------------------------------------------------------------------------------------------+
set(USE_SCALAR "" CACHE STRING "Force the scalar implementation to use (can be 32bit or 64bit)")
if(USE_SCALAR STREQUAL "64bit" AND NOT HAVE___INT128)
	message(SEND_ERROR "64 scalar requested but the compiler does not support __int128")
elseif(NOT USE_SCALAR STREQUAL "32bit" AND HAVE___INT128)
	set(USE_SCALAR_4X64 1)
else()
	set(USE_SCALAR_8X32 1)
endif()

option(SECP256K1_BUILD_TEST "Build secp256k1's unit tests" ON)

# Executable internal to secp256k1 need to have the HAVE_CONFIG_H define set.
# For convenience, we wrap this into a function.
function(link_secp256k1_internal NAME)
	target_link_libraries(${NAME} secp256k1)
	target_compile_definitions(${NAME} PRIVATE HAVE_CONFIG_H SECP256K1_BUILD)
endfunction(link_secp256k1_internal)

# Phony target to build benchmarks
add_custom_target(bench-secp256k1)

function(add_secp256k1_bench NAME)
	set(EXECUTABLE_NAME "${NAME}-bench")
	add_executable(${EXECUTABLE_NAME} EXCLUDE_FROM_ALL ${ARGN})
	link_secp256k1_internal(${EXECUTABLE_NAME})

	set(BENCH_NAME "bench-secp256k1-${NAME}")
	add_custom_target(${BENCH_NAME}
		COMMENT "Benchmarking libsecp256k1 ${NAME}"
		COMMAND ${EXECUTABLE_NAME}
		USES_TERMINAL
	)
	add_dependencies(bench-secp256k1 ${BENCH_NAME})
endfunction(add_secp256k1_bench)

# ECDH module
option(SECP256K1_ENABLE_MODULE_ECDH "Build libsecp256k1's ECDH module" OFF)
if(SECP256K1_ENABLE_MODULE_ECDH)
	set(ENABLE_MODULE_ECDH 1)
	add_secp256k1_bench(ecdh src/bench_ecdh.c)
	list(APPEND SECP256K1_PUBLIC_HEADERS include/secp256k1_ecdh.h)
endif()

# MultiSet module
option(SECP256K1_ENABLE_MODULE_MULTISET "Build libsecp256k1's MULTISET module" ON)
if(SECP256K1_ENABLE_MODULE_MULTISET)
    set(ENABLE_MODULE_MULTISET 1)
	add_secp256k1_bench(multiset src/bench_multiset.c)
	list(APPEND SECP256K1_PUBLIC_HEADERS include/secp256k1_multiset.h)
endif()

# Recovery module
option(SECP256K1_ENABLE_MODULE_RECOVERY "Build libsecp256k1's recovery module" ON)
if(SECP256K1_ENABLE_MODULE_RECOVERY)
	set(ENABLE_MODULE_RECOVERY 1)
	add_secp256k1_bench(recover src/bench_recover.c)
	list(APPEND SECP256K1_PUBLIC_HEADERS include/secp256k1_recovery.h)
endif()

# Schnorr module
option(SECP256K1_ENABLE_MODULE_SCHNORR "Build libsecp256k1's Schnorr module" ON)
if(SECP256K1_ENABLE_MODULE_SCHNORR)
	set(ENABLE_MODULE_SCHNORR 1)
	list(APPEND SECP256K1_PUBLIC_HEADERS include/secp256k1_schnorr.h)
endif()

# External default callbacks
option(SECP256K1_ENABLE_EXTERNAL_DEFAULT_CALLBACKS "Enable external default callbacks" OFF)
if(SECP256K1_ENABLE_EXTERNAL_DEFAULT_CALLBACKS)
	set(USE_EXTERNAL_DEFAULT_CALLBACKS 1)
endif()

# Endomorphism
option(SECP256K1_ENABLE_ENDOMORPHISM "Enable endomorphism" ON)
if(SECP256K1_ENABLE_ENDOMORPHISM)
	set(USE_ENDOMORPHISM 1)
endif()

# Make the emult window size customizable.
set(SECP256K1_ECMULT_WINDOW_SIZE 15 CACHE STRING "Window size for ecmult precomputation for verification, specified as integer in range [2..24].")
if(${SECP256K1_ECMULT_WINDOW_SIZE} LESS 2 OR ${SECP256K1_ECMULT_WINDOW_SIZE} GREATER 24)
	message(FATAL_ERROR "SECP256K1_ECMULT_WINDOW_SIZE must be an integer in range [2..24]")
endif()

set(SECP256K1_ECMULT_GEN_PRECISION 4 CACHE STRING "Precision bits to tune the precomputed table size for signing.")
set(VALID_PRECISIONS 2 4 8)
if(NOT ${SECP256K1_ECMULT_GEN_PRECISION} IN_LIST VALID_PRECISIONS)
	message(FATAL_ERROR "SECP256K1_ECMULT_GEN_PRECISION not 2, 4, 8")
endif()

# Static precomputation for elliptic curve multiplication
option(SECP256K1_ECMULT_STATIC_PRECOMPUTATION "Precompute libsecp256k1's elliptic curve multiplication tables" ON)
if(SECP256K1_ECMULT_STATIC_PRECOMPUTATION)
	set(USE_ECMULT_STATIC_PRECOMPUTATION 1)

	include(NativeExecutable)
	native_add_cmake_flags(
		"-DSECP256K1_ECMULT_WINDOW_SIZE=${SECP256K1_ECMULT_WINDOW_SIZE}"
		"-DSECP256K1_ECMULT_GEN_PRECISION=${SECP256K1_ECMULT_GEN_PRECISION}"
		"-DSECP256K1_USE_ASM=OFF"
	)
	add_native_executable(gen_context src/gen_context.c)

	add_custom_command(
		OUTPUT src/ecmult_static_context.h
		COMMAND gen_context
	)

	target_sources(secp256k1 PRIVATE src/ecmult_static_context.h)
endif()

include(InstallationHelper)
if(BUILD_SHARED_LIBS)
	install_shared_library(secp256k1 PUBLIC_HEADER ${SECP256K1_PUBLIC_HEADERS})
else()
	set_property(TARGET secp256k1 PROPERTY PUBLIC_HEADER ${SECP256K1_PUBLIC_HEADERS})
	install_target(secp256k1)
endif()

# Generate the config
configure_file(src/libsecp256k1-config.h.cmake.in src/libsecp256k1-config.h ESCAPE_QUOTES)
target_compile_definitions(secp256k1 PRIVATE HAVE_CONFIG_H SECP256K1_BUILD)

# Build the Java binding
option(SECP256K1_ENABLE_JNI "Enable the Java Native Interface binding" OFF)
if(SECP256K1_ENABLE_JNI)
	if(NOT SECP256K1_ENABLE_MODULE_ECDH)
		message(FATAL_ERROR "The secp256k1 JNI support requires ECDH. Try again with -DSECP256K1_ENABLE_MODULE_ECDH=ON.")
	endif()

	find_package(Java REQUIRED)
	find_package(JNI REQUIRED)
	include(UseJava)

	add_library(secp256k1_jni SHARED
		src/java/org_fittexxcoin_NativeSecp256k1.c
		src/java/org_fittexxcoin_Secp256k1Context.c
	)

	install_shared_library(secp256k1_jni)

	target_include_directories(secp256k1_jni PUBLIC ${JNI_INCLUDE_DIRS})
	# As per CMake documentation: the POSITION_INDEPENDENT_CODE property is set
	# when a target is created. It defaults to True for SHARED or MODULE library
	# targets and False otherwise.
	# The secp256ki_jni library being shared, the property is set and it will
	# build with PIC enabled. But the secp256k1 dependency might not have the
	# property set, so it's associated source files won't be built with PIC
	# enabled. That would cause the linker to fail.
	# Forcing the property for the secp256k1 library fixes the issue.
	set_target_properties(secp256k1 PROPERTIES POSITION_INDEPENDENT_CODE ON)
	link_secp256k1_internal(secp256k1_jni)
endif()

# Tests
if(SECP256K1_BUILD_TEST)
	include(TestSuite)
	create_test_suite(secp256k1)

	function(create_secp256k1_test NAME FILES)
		add_test_to_suite(secp256k1 ${NAME} EXCLUDE_FROM_ALL ${FILES})
		link_secp256k1_internal(${NAME})
	endfunction()

	create_secp256k1_test(secp256k1-tests src/tests.c)
	create_secp256k1_test(secp256k1-exhaustive_tests src/tests_exhaustive.c)

	# This should not be enabled at the same time as coverage is.
	# The VERIFY failure branch is not expected to be reached, so it would make
	# coverage appear lower if set.
	if(NOT SECP256K1_ENABLE_COVERAGE)
		target_compile_definitions(secp256k1-tests PRIVATE VERIFY)
		target_compile_definitions(secp256k1-exhaustive_tests PRIVATE VERIFY)
	endif()

	if(SECP256K1_ENABLE_JNI)
		set(SECP256k1_JNI_TEST_JAR "secp256k1-jni-test")

		set(CMAKE_JNI_TARGET TRUE)
		add_jar(secp256k1-jni-test-jar
			SOURCES
				src/java/org/fittexxcoin/NativeSecp256k1.java
				src/java/org/fittexxcoin/NativeSecp256k1Test.java
				src/java/org/fittexxcoin/NativeSecp256k1Util.java
				src/java/org/fittexxcoin/Secp256k1Context.java
			ENTRY_POINT org/fittexxcoin/NativeSecp256k1Test
			OUTPUT_NAME "${SECP256k1_JNI_TEST_JAR}"
		)
		add_dependencies(secp256k1-jni-test-jar secp256k1_jni)

		add_custom_target(check-secp256k1-java
			COMMAND
				"${Java_JAVA_EXECUTABLE}"
				"-Djava.library.path=${CMAKE_CURRENT_BINARY_DIR}"
				"-jar"
				"${SECP256k1_JNI_TEST_JAR}.jar"
			WORKING_DIRECTORY
				"${CMAKE_CURRENT_BINARY_DIR}"
		)
		add_dependencies(check-secp256k1-java secp256k1-jni-test-jar)
		add_dependencies(check-secp256k1 check-secp256k1-java)
	endif()
endif(SECP256K1_BUILD_TEST)

# Benchmarks
add_secp256k1_bench(verify src/bench_verify.c)
add_secp256k1_bench(sign src/bench_sign.c)
add_secp256k1_bench(internal src/bench_internal.c)
add_secp256k1_bench(ecmult src/bench_ecmult.c)
