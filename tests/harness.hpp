#ifndef RT_TESTS_HARNESS_HPP
#define RT_TESTS_HARNESS_HPP

#include "rt/math/vector.hpp"

#include <rapidcheck.h>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>

namespace rt::test {

/// Runs properties with rapidcheck and turns their combined outcome into the exit code of `main`.
class Suite {
public:
	template <typename Testable>
	void check(const std::string& name, Testable&& property) {
		if (!rc::check(name, std::forward<Testable>(property))) {
			++failures_;
		}
	}

	[[nodiscard]] int exit_code() const noexcept { return failures_ == 0 ? EXIT_SUCCESS : EXIT_FAILURE; }

private:
	int failures_ = 0;
};

template <std::floating_point T>
inline constexpr T default_eps = T{1e-9};

template <>
inline constexpr float default_eps<float> = 1e-5F;

/// Equality within `eps`, absolute for values up to 1 in magnitude and relative above that.
template <std::floating_point T>
[[nodiscard]] bool approx_equal(T a, T b, T eps = default_eps<T>) {
	return std::abs(a - b) <= eps * std::max({T{1}, std::abs(a), std::abs(b)});
}

/// Component-wise `approx_equal`.
template <std::floating_point T, std::size_t N>
[[nodiscard]] bool approx_equal(const Vector<T, N>& a, const Vector<T, N>& b, T eps = default_eps<T>) {
	for (std::size_t i = 0; i < N; ++i) {
		if (!approx_equal(a[i], b[i], eps)) {
			return false;
		}
	}
	return true;
}

} // namespace rt::test

#endif // RT_TESTS_HARNESS_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
