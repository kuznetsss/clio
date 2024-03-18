cmake_minimum_required(VERSION 3.16.3)

set(CMAKE_PROJECT_INCLUDE_BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ClioVersion.cmake)

project(clio VERSION ${CLIO_VERSION} HOMEPAGE_URL "https://github.com/XRPLF/clio"
        DESCRIPTION "An XRP Ledger API Server"
)

option(benchmark "Build benchmarks" FALSE)
option(docs "Generate doxygen docs" FALSE)
option(packaging "Create distribution packages" FALSE)
option(lint "Run clang-tidy checks during compilation" FALSE)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

add_library(clio_options INTERFACE)
target_include_directories(clio_options INTERFACE ${CMAKE_SOURCE_DIR}/src)

# Include required modules
include(CheckCompiler)
include(BuildTypes)
include(Ccache)
include(CheckCXXCompilerFlag)
include(ClangTidy)

# Set coverage build options
if (CMAKE_BUILD_TYPE STREQUAL "CodeCoverage")
  include(CodeCoverage)
  append_coverage_compiler_flags_to_target(clio_options INTERFACE)
endif ()

# Clio tweaks and checks
include(Settings)
include(SourceLocation)

# Clio deps
include(deps/libxrpl)
include(deps/Boost)
include(deps/OpenSSL)
include(deps/Threads)
include(deps/libfmt)
include(deps/cassandra)
include(deps/libbacktrace)

add_subdirectory(src)

add_subdirectory(unittests)

if (benchmark)
  add_subdirectory(benchmarks)
endif ()

# Generate `docs` target for doxygen documentation if enabled Note: use `make docs` to generate the documentation
if (docs)
  add_subdirectory(docs)
endif ()

include(install/install)
if (packaging)
  add_definitions(-DPKG=1)
  target_compile_definitions(clio_options INTERFACE PKG=1)
  include(cmake/packaging.cmake) # This file exists only in build runner
endif ()
