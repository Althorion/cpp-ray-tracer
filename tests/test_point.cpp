#include "rt/errors.hpp"
#include "rt/maths/point.hpp"
#include "rt/maths/vector.hpp"

#include <rapidcheck/Assertions.h>

#include <concepts>
#include <cstddef>
#include <format>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

// Compile-time checks: points and vectors do not mix, and points have only the affine operations.
static_assert(std::same_as<decltype(Point{1.0, 2.0, 3.0}), Point3>);
static_assert(std::same_as<decltype(Point{1.0F, 2.0F}), Point<float, 2>>);
static_assert(Point3{} == Point3::origin());
static_assert(Point3{1, 2, 3} + Vec3{4, 5, 6} == Point3{5, 7, 9});
static_assert(Point3{5, 7, 9} - Point3{1, 2, 3} == Vec3{4, 5, 6});
static_assert(!std::is_convertible_v<double, Point<double, 1>>, "a one-dimensional point is not a scalar");
static_assert(!std::is_convertible_v<Vec3, Point3>, "a vector is not a point");
static_assert(!std::is_convertible_v<Point3, Vec3>, "a point is not a vector");
static_assert(!std::is_constructible_v<Point3, Vec3>, "a point is made from a vector only as an offset of a point");
static_assert(std::is_trivially_copyable_v<Point3>);
static_assert(!std::is_assignable_v<decltype(std::declval<Point3&>()[0]), double>,
              "coordinates change only through set");

template <typename P>
concept addable = requires(P p) { p + p; };
template <typename P>
concept scalable = requires(P p) { p * 2.0; };
template <typename P>
concept negatable = requires(P p) { -p; };
template <typename P>
concept has_vector_operations = requires(P p) {
	p.length();
	p.normalised();
	dot(p, p);
	cross(p, p);
};
template <typename A, typename B>
concept subtractable = requires(A a, B b) { a - b; };

static_assert(!addable<Point3>, "points cannot be added together");
static_assert(!scalable<Point3>, "points cannot be scaled");
static_assert(!negatable<Point3>);
static_assert(!has_vector_operations<Point3>);
static_assert(!subtractable<Vec3, Point3>, "a vector minus a point means nothing");
static_assert(addable<Vec3> && scalable<Vec3> && negatable<Vec3> && has_vector_operations<Vec3>);

/// Point whose coordinate `index` is `value` and all others are 1.
template <std::floating_point T, std::size_t N>
[[nodiscard]] Point<T, N> with_coordinate(std::size_t index, T value) {
	return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
		return Point<T, N>{(Indices == index ? value : T{1})...};
	}(std::make_index_sequence<N>{});
}

template <std::floating_point T, std::size_t N>
void check_affine(Suite& suite, const std::string& label) {
	using P = Point<T, N>;
	using V = Vector<T, N>;

	suite.check(label + ": p - p is zero", [](const P& p) { RC_ASSERT(p - p == V::zero()); });

	suite.check(label + ": p + 0 is p", [](const P& p) { RC_ASSERT(p + V::zero() == p); });

	suite.check(label + ": moving by a vector and back returns to the point", [](const P& p, const V& v) {
		RC_ASSERT(approx_equal((p + v) - v, p, tolerance(distance_from_origin(p) + v.length())));
	});

	suite.check(label + ": the difference of points takes one to the other", [](const P& p, const P& q) {
		RC_ASSERT(approx_equal(p + (q - p), q, tolerance(distance_from_origin(p) + distance_from_origin(q))));
	});

	suite.check(label + ": the difference of moved points is the difference of the moves", [](const P& p, const V& a,
	                                                                                          const V& b) {
		RC_ASSERT(approx_equal((p + a) - (p + b), a - b, tolerance(distance_from_origin(p) + a.length() + b.length())));
	});

	suite.check(label + ": compound assignment matches the binary operator", [](const P& p, const V& v) {
		P moved{p};
		moved += v;
		P back{p};
		back -= v;
		RC_ASSERT(moved == p + v);
		RC_ASSERT(back == p - v);
	});

	suite.check(label + ": the origin is the zero position", [](const P& p) {
		for (std::size_t i = 0; i < N; ++i) {
			RC_ASSERT((p - P::origin())[i] == p[i]);
		}
	});
}

template <std::floating_point T, std::size_t N>
void check_finiteness(Suite& suite, const std::string& label) {
	using P = Point<T, N>;
	using V = Vector<T, N>;

	suite.check(label + ": a non-finite coordinate throws MathsError", [] {
		const auto index = *component_index<N>();
		const T value = *non_finite_scalar<T>();
		RC_ASSERT_THROWS_AS(static_cast<void>(with_coordinate<T, N>(index, value)), MathsError);
	});

	suite.check(label + ": set changes exactly one coordinate", [](const P& p) {
		const auto index = *component_index<N>();
		const T value = *finite_scalar<T>();
		P changed{p};
		changed.set(index, value);
		for (std::size_t i = 0; i < N; ++i) {
			RC_ASSERT(changed[i] == (i == index ? value : p[i]));
		}
	});

	suite.check(label + ": setting a non-finite coordinate throws MathsError and keeps the point", [](const P& p) {
		P changed{p};
		RC_ASSERT_THROWS_AS(changed.set(*component_index<N>(), *non_finite_scalar<T>()), MathsError);
		RC_ASSERT(changed == p);
	});

	suite.check(label + ": overflow throws MathsError and keeps the point", [] {
		const T largest = std::numeric_limits<T>::max();
		P far{};
		far.set(0, largest);
		V huge{};
		huge.set(0, largest);
		P changed{far};
		RC_ASSERT_THROWS_AS(changed += huge, MathsError);
		RC_ASSERT_THROWS_AS(changed -= -huge, MathsError);
		RC_ASSERT(changed == far);
		P opposite{};
		opposite.set(0, -largest);
		RC_ASSERT_THROWS_AS(static_cast<void>(far - opposite), MathsError);
	});
}

void check_examples(Suite& suite) {
	suite.check("coordinates are stored in order", [] {
		Point3 p{1, 2, 3};
		RC_ASSERT(p[0] == 1.0);
		RC_ASSERT(p[1] == 2.0);
		RC_ASSERT(p[2] == 3.0);
		p.set(1, 5.0);
		RC_ASSERT(p == Point3(1, 5, 3));
	});

	suite.check("formatting matches that of a vector", [] {
		RC_ASSERT(std::format("{}", Point3(1, 2.5, -3)) == "(1, 2.5, -3)");
		RC_ASSERT(std::format("{:.2f}", Point3(1, 2.5, -3)) == "(1.00, 2.50, -3.00)");
		RC_ASSERT((std::format("{:>4}", Point<float, 2>(1, 2)) == "(   1,    2)"));
	});

	suite.check("stream output matches the default format", [] {
		std::ostringstream stream;
		stream << Point3(1, 2.5, -3);
		RC_ASSERT(stream.str() == "(1, 2.5, -3)");
	});
}

} // namespace
} // namespace rt::test

int main() {
	rt::test::Suite suite;
	rt::test::check_examples(suite);
	rt::test::check_affine<double, 3>(suite, "Point3");
	rt::test::check_affine<double, 2>(suite, "Point<double, 2>");
	rt::test::check_affine<float, 3>(suite, "Point<float, 3>");
	rt::test::check_finiteness<double, 3>(suite, "Point3");
	rt::test::check_finiteness<float, 3>(suite, "Point<float, 3>");
	return suite.exit_code();
}

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
