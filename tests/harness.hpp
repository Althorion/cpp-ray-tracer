#ifndef RT_TESTS_HARNESS_HPP
#define RT_TESTS_HARNESS_HPP

#include "rt/maths/vector.hpp"

#include <rapidcheck.h>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <functional>
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

/// Tolerance for results whose rounding error is proportional to `scale`, the magnitude of the inputs.
template <std::floating_point T>
[[nodiscard]] T tolerance(T scale) {
	return default_eps<T> * std::max(T{1}, scale);
}

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

/// Draws per statistical check; enough for `mean_is_close` to be sharp.
inline constexpr int draws = 4096;

/// Whether the mean of `count` values produced by `draw` lies within six standard errors of `expected`, for values
/// of the given `variance`. A correct implementation fails with probability about 2·10⁻⁹.
[[nodiscard]] inline bool mean_is_close(const std::function<double()>& draw, double expected, double variance,
                                        int count = draws) {
	double sum = 0.0;
	for (int i = 0; i < count; ++i) {
		sum += draw();
	}
	const double standard_error = std::sqrt(variance / count);
	return std::abs((sum / count) - expected) <= 6.0 * standard_error;
}

} // namespace rt::test

#endif // RT_TESTS_HARNESS_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
