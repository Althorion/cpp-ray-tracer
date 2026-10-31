#ifndef RT_TESTS_GENERATORS_HPP
#define RT_TESTS_GENERATORS_HPP

#include "rt/maths/interval.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"

#include <rapidcheck.h>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <utility>

namespace rt::test {

/// Largest coordinate magnitude produced by the generators; generously covers any sensible scene.
template <std::floating_point T>
inline constexpr T coordinate_bound = T{1e4};

/// Finite number in `(-coordinate_bound, coordinate_bound)`, shrinking towards zero.
template <std::floating_point T>
[[nodiscard]] rc::Gen<T> finite_scalar() {
	return rc::gen::map(rc::gen::arbitrary<T>(),
	                    [](T value) { return std::isfinite(value) ? std::fmod(value, coordinate_bound<T>) : T{0}; });
}

/// Vector with finite components in `(-coordinate_bound, coordinate_bound)`.
template <std::floating_point T, std::size_t N>
[[nodiscard]] rc::Gen<Vector<T, N>> finite_vector() {
	return []<std::size_t... Indices>(std::index_sequence<Indices...>) {
		return rc::gen::construct<Vector<T, N>>(((void)Indices, finite_scalar<T>())...);
	}(std::make_index_sequence<N>{});
}

/// Finite vector other than the zero vector; it may be very short.
template <std::floating_point T, std::size_t N>
[[nodiscard]] rc::Gen<Vector<T, N>> nonzero_vector() {
	return rc::gen::suchThat(finite_vector<T, N>(),
	                         [](const Vector<T, N>& vector) { return vector != Vector<T, N>::zero(); });
}

template <std::floating_point T, std::size_t N>
[[nodiscard]] rc::Gen<Vector<T, N>> unit_vector() {
	return rc::gen::map(nonzero_vector<T, N>(), [](const Vector<T, N>& vector) { return vector.normalised(); });
}

/// Index of a component of an `N`-dimensional vector.
template <std::size_t N>
[[nodiscard]] rc::Gen<std::size_t> component_index() {
	return rc::gen::inRange<std::size_t>(0, N);
}

/// Exponent `e` for which 2^e times a `finite_vector` stays finite, reaching down into the subnormal range.
template <std::floating_point T>
[[nodiscard]] rc::Gen<int> scaling_exponent() {
	constexpr int headroom = 14; // coordinate_bound = 1e4 < 2^14
	return rc::gen::inRange(std::numeric_limits<T>::min_exponent - std::numeric_limits<T>::digits,
	                        std::numeric_limits<T>::max_exponent - headroom + 1);
}

/// Positive or negative infinity, or NaN: the values a `Vector` must reject.
template <std::floating_point T>
[[nodiscard]] rc::Gen<T> non_finite_scalar() {
	return rc::gen::element(std::numeric_limits<T>::infinity(), -std::numeric_limits<T>::infinity(),
	                        std::numeric_limits<T>::quiet_NaN());
}

/// Finite number or, now and then, an infinity; the values an interval bound or a ray parameter can take.
template <std::floating_point T>
[[nodiscard]] rc::Gen<T> extended_scalar() {
	constexpr T infinity = std::numeric_limits<T>::infinity();
	return rc::gen::weightedOneOf<T>({
			{8, finite_scalar<T>()},
			{1, rc::gen::just(-infinity)},
			{1, rc::gen::just(infinity)},
	});
}

/// Interval with arbitrary bounds; empty whenever the lower bound exceeds the upper one.
template <std::floating_point T>
[[nodiscard]] rc::Gen<Interval<T>> any_interval() {
	return rc::gen::construct<Interval<T>>(extended_scalar<T>(), extended_scalar<T>());
}

/// Interval with `min <= max`, possibly degenerate (`min == max`) or unbounded.
template <std::floating_point T>
[[nodiscard]] rc::Gen<Interval<T>> nonempty_interval() {
	return rc::gen::map(rc::gen::pair(extended_scalar<T>(), extended_scalar<T>()), [](const std::pair<T, T>& bounds) {
		const auto [lower, upper] = std::minmax(bounds.first, bounds.second);
		return Interval<T>{lower, upper};
	});
}

/// Ray with a finite origin and a direction that is not (numerically) zero.
[[nodiscard]] inline rc::Gen<Ray> any_ray() {
	return rc::gen::construct<Ray>(finite_vector<double, 3>(), nonzero_vector<double, 3>());
}

} // namespace rt::test

namespace rc {

template <>
struct Arbitrary<rt::Ray> {
	static Gen<rt::Ray> arbitrary() { return rt::test::any_ray(); }
};

template <std::floating_point T>
struct Arbitrary<rt::Interval<T>> {
	static Gen<rt::Interval<T>> arbitrary() { return rt::test::any_interval<T>(); }
};

template <std::floating_point T, std::size_t N>
struct Arbitrary<rt::Vector<T, N>> {
	static Gen<rt::Vector<T, N>> arbitrary() { return rt::test::finite_vector<T, N>(); }
};

} // namespace rc

#endif // RT_TESTS_GENERATORS_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
