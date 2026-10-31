# Compiler warnings, clang-tidy and sanitiser settings for the project's own targets (README.md § 3, § 10).
# Third-party targets are built without them.

option(RT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors (used in CI)" OFF)
option(RT_ENABLE_CLANG_TIDY "Run clang-tidy while compiling the project's own targets" ON)

if(RT_ENABLE_CLANG_TIDY)
	find_program(RT_CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
	if(NOT RT_CLANG_TIDY_EXECUTABLE)
		message(WARNING "clang-tidy not found; static analysis is disabled")
	endif()
endif()

# Applies the project's warning flags and, if enabled, clang-tidy to `target`.
function(rt_configure_target target)
	target_compile_options(${target} PRIVATE
		-Wall
		-Wextra
		-Wpedantic
		-Wconversion
		-Wsign-conversion
		-Wshadow
		-Wnon-virtual-dtor
		-Wold-style-cast
		-Woverloaded-virtual
		$<$<BOOL:${RT_WARNINGS_AS_ERRORS}>:-Werror>
	)
	# Bounds checks in the standard library (and thus in `Vector::operator[]`) for debug builds
	target_compile_definitions(${target} PRIVATE $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)
	if(RT_ENABLE_CLANG_TIDY AND RT_CLANG_TIDY_EXECUTABLE)
		set(tidy_command "${RT_CLANG_TIDY_EXECUTABLE}")
		if(RT_WARNINGS_AS_ERRORS)
			list(APPEND tidy_command "--warnings-as-errors=*")
		endif()
		set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${tidy_command}")
	endif()
endfunction()

# Local Variables:
# ispell-local-dictionary: "en-GB"
# End:
