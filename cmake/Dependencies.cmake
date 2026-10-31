# Third-party dependencies, fetched at pinned commits (README.md § 3).

include(FetchContent)

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
