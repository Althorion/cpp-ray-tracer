cmake_minimum_required(VERSION 3.30)

project(raytracer
	VERSION 0.1.0
	DESCRIPTION "Monte Carlo path tracer"
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# The project does not use C++ modules (README.md § 1.2); scanning would only add GCC-specific flags.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(RT_BUILD_TESTS "Build the property-based tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Warnings)
include(Dependencies)

# All program logic lives in this library; executables only link against it.
add_library(rt_core STATIC
	src/errors.cpp
	src/worker_pool.cpp
)
target_include_directories(rt_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
# PUBLIC: the inline members of `Sampler` hold a `pcg32` (README.md § 7.2)
target_link_libraries(rt_core PUBLIC pcg::pcg)
# `WorkerPool` runs `std::jthread`s (README.md § 6.6)
find_package(Threads REQUIRED)
target_link_libraries(rt_core PRIVATE Threads::Threads)
rt_configure_target(rt_core)

if(RT_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

# Formatting: `format` rewrites the sources in place, `format-check` fails on any difference.
find_program(RT_CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if(RT_CLANG_FORMAT_EXECUTABLE)
	file(GLOB_RECURSE rt_format_sources CONFIGURE_DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
	)
	add_custom_target(format
		COMMAND "${RT_CLANG_FORMAT_EXECUTABLE}" -i ${rt_format_sources}
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		COMMENT "Formatting sources with clang-format"
		VERBATIM
	)
	add_custom_target(format-check
		COMMAND "${RT_CLANG_FORMAT_EXECUTABLE}" --dry-run --Werror ${rt_format_sources}
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		COMMENT "Checking formatting with clang-format"
		VERBATIM
	)
endif()

# Local Variables:
# ispell-local-dictionary: "en-GB"
# End:
