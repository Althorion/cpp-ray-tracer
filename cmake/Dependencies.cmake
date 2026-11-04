# Third-party dependencies, fetched at pinned commits (README.md § 3).

include(FetchContent)

# pcg-cpp is header-only and has no CMakeLists.txt, so FetchContent only downloads it and the target is defined here.
FetchContent_Declare(pcg
	GIT_REPOSITORY https://github.com/imneme/pcg-cpp.git
	GIT_TAG 428802d1a5634f96bcd0705fab379ff0113bcf13
)
FetchContent_MakeAvailable(pcg)

add_library(pcg INTERFACE)
add_library(pcg::pcg ALIAS pcg)
# SYSTEM: pcg's headers must not trigger the project's warnings
target_include_directories(pcg SYSTEM INTERFACE "${pcg_SOURCE_DIR}/include")

if(RT_BUILD_TESTS)
	set(RC_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
	set(RC_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)

	# SYSTEM: rapidcheck's headers must not trigger the project's warnings
	FetchContent_Declare(rapidcheck
		GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
		GIT_TAG 2c3c4365aca21ef4e612768fdc95b1ce8b39a651
		SYSTEM
	)
	FetchContent_MakeAvailable(rapidcheck)
endif()

# Local Variables:
# ispell-local-dictionary: "en-GB"
# End:
