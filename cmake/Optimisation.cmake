# Link-time optimisation, CPU tuning and the debug optimisation level for the project's own targets (README.md § 3).
# Third-party targets are built without them.

include(CheckCXXCompilerFlag)
include(CheckIPOSupported)

option(RT_ENABLE_LTO "Use link-time optimisation in release builds and GCC's ODR checks in debug builds" ON)
option(RT_NATIVE_ARCH "Tune release builds for the CPU of the building machine; the binaries may not run elsewhere" ON)

if(RT_ENABLE_LTO)
	check_ipo_supported(RESULT RT_LTO_SUPPORTED OUTPUT rt_lto_output LANGUAGES CXX)
	if(NOT RT_LTO_SUPPORTED)
		message(WARNING "Link-time optimisation is not supported by this toolchain and is disabled: ${rt_lto_output}")
	endif()
endif()

if(RT_NATIVE_ARCH)
	check_cxx_compiler_flag(-march=native RT_HAS_MARCH_NATIVE)
	if(NOT RT_HAS_MARCH_NATIVE)
		message(WARNING "The compiler does not accept -march=native; building for its default CPU")
	endif()
endif()

# Applies link-time optimisation, CPU tuning and the debug optimisation level to `target`.
function(rt_configure_optimisation target)
	if(RT_ENABLE_LTO AND RT_LTO_SUPPORTED)
		set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
		# GCC diagnoses ODR violations and mismatched declarations across translation units only while linking with LTO,
		# so debug builds use it too. Clang has no such diagnostics.
		if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_DEBUG ON)
			target_link_options(${target} PRIVATE
				"$<$<CONFIG:Debug>:-Wodr;-Wlto-type-mismatch>"
				"$<$<AND:$<CONFIG:Debug>,$<BOOL:${RT_WARNINGS_AS_ERRORS}>>:-Werror=odr;-Werror=lto-type-mismatch>"
			)
		endif()
	endif()
	if(RT_NATIVE_ARCH AND RT_HAS_MARCH_NATIVE)
		target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:-march=native>)
	endif()
	# GCC's -Og makes the tests about six times faster. Clang's is effectively -O1 and worse to debug with, so its debug
	# builds stay at -O0, which also keeps one unoptimised configuration.
	target_compile_options(${target} PRIVATE $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU>>:-Og>)
endfunction()

# Local Variables:
# ispell-local-dictionary: "en-GB"
# End:
