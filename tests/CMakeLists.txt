# Every tests/test_<area>.cpp is a separate executable registered with CTest (README.md § 8.1).

# Default rapidcheck parameters; prepended so that any RC_PARAMS set in the environment overrides them.
set(RT_RC_PARAMS "max_success=200" CACHE STRING "Default rapidcheck parameters for CTest")

function(rt_add_test name)
	add_executable(${name} ${name}.cpp)
	target_link_libraries(${name} PRIVATE rt_core rapidcheck)
	rt_configure_target(${name})
	add_test(NAME ${name} COMMAND ${name})
	set_tests_properties(${name} PROPERTIES
		ENVIRONMENT_MODIFICATION "RC_PARAMS=string_prepend:${RT_RC_PARAMS} "
	)
endfunction()

rt_add_test(test_vector)
rt_add_test(test_interval)
rt_add_test(test_ray)
rt_add_test(test_colour)
rt_add_test(test_sampler)

# Local Variables:
# ispell-local-dictionary: "en-GB"
# End:
