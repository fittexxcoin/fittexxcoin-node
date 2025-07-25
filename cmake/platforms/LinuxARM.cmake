# Copyright (c) 2019 The Fittexxcoin developers

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Use given TOOLCHAIN_PREFIX if specified
if(CMAKE_TOOLCHAIN_PREFIX)
  set(TOOLCHAIN_PREFIX ${CMAKE_TOOLCHAIN_PREFIX})
else()
  set(TOOLCHAIN_PREFIX ${CMAKE_SYSTEM_PROCESSOR}-linux-gnueabihf)
endif()

# Cross compilers to use for C and C++
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)

set(CMAKE_C_COMPILER_TARGET ${TOOLCHAIN_PREFIX})
set(CMAKE_CXX_COMPILER_TARGET ${TOOLCHAIN_PREFIX})

# Target environment on the build host system
# Set 1st to directory with the cross compiler's C/C++ headers/libs
set(CMAKE_FIND_ROOT_PATH
	"${CMAKE_CURRENT_SOURCE_DIR}/depends/${TOOLCHAIN_PREFIX}"
	"/usr/${TOOLCHAIN_PREFIX}"
)

# We also may have built dependencies for the native platform.
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/depends/${TOOLCHAIN_PREFIX}/native")

# Modify default behavior of FIND_XXX() commands to
# search for headers/libs in the target environment and
# search for programs in the build host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
