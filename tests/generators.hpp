#ifndef RT_TESTS_GENERATORS_HPP
#define RT_TESTS_GENERATORS_HPP

#include "rt/math/interval.hpp"
#include "rt/math/vector.hpp"

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

/// Finite vector long enough to be normalised accurately (its squared length is a normal number).
template <std::floating_point T, std::size_t N>
[[nodiscard]] rc::Gen<Vector<T, N>> nonzero_vector() {
	return rc::gen::suchThat(finite_vector<T, N>(), [](const Vector<T, N>& vector) {
		return vector.length_squared() >= std::numeric_limits<T>::min();
	});
}

template <std::floating_point T, std::size_t N>
[[nodiscard]] rc::Gen<Vector<T, N>> unit_vector() {
	return rc::gen::map(nonzero_vector<T, N>(), [](const Vector<T, N>& vector) { return vector.normalised(); });
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

} // namespace rt::test

namespace rc {

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
