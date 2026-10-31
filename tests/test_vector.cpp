#include "rt/errors.hpp"
#include "rt/math/vector.hpp"

#include <rapidcheck/Assertions.h>

#include <concepts>
#include <cstddef>
#include <format>
#include <sstream>
#include <string>
#include <type_traits>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

// Compile-time checks of construction, deduction and the dimension constraints.
static_assert(std::same_as<decltype(Vector{1.0, 2.0, 3.0}), Vec3>);
static_assert(std::same_as<decltype(Vector{1.0F, 2.0F}), Vector<float, 2>>);
static_assert(Vec3{} == Vec3::zero());
static_assert(Vec3{1, 2, 3} + Vec3{4, 5, 6} == Vec3{5, 7, 9});
static_assert(cross(Vec3::unit_x(), Vec3::unit_y()) == Vec3::unit_z(), "the coordinate system is right-handed");
static_assert(!std::is_convertible_v<double, Vector<double, 1>>, "a one-dimensional vector is not a scalar");
static_assert(std::is_trivially_copyable_v<Vec3>);

template <typename V>
concept has_cross = requires(V v) { cross(v, v); };
static_assert(has_cross<Vec3>);
static_assert(!has_cross<Vector<double, 2>>);

template <std::floating_point T, std::size_t N>
void check_arithmetic(Suite& suite, const std::string& label) {
	using V = Vector<T, N>;

	suite.check(label + ": addition is commutative", [](const V& a, const V& b) { RC_ASSERT(a + b == b + a); });

	suite.check(label + ": addition is associative", [](const V& a, const V& b, const V& c) {
		const T scale = a.length() + b.length() + c.length();
		RC_ASSERT(approx_equal((a + b) + c, a + (b + c), tolerance(scale)));
	});

	suite.check(label + ": v - v is zero", [](const V& v) { RC_ASSERT(v - v == V::zero()); });

	suite.check(label + ": v + (-v) is zero", [](const V& v) { RC_ASSERT(v + -v == V::zero()); });

	suite.check(label + ": scalar multiplication distributes over addition", [](const V& a, const V& b) {
		const T s = *finite_scalar<T>();
		const T scale = (a.length() + b.length()) * std::abs(s);
		RC_ASSERT(approx_equal((a + b) * s, a * s + b * s, tolerance(scale)));
	});

	suite.check(label + ": scalar multiplication is commutative", [](const V& v) {
		const T s = *finite_scalar<T>();
		RC_ASSERT(s * v == v * s);
	});

	suite.check(label + ": compound assignment matches the binary operator", [](const V& a, const V& b) {
		V sum{a};
		sum += b;
		V difference{a};
		difference -= b;
		V product{a};
		product *= b;
		RC_ASSERT(sum == a + b);
		RC_ASSERT(difference == a - b);
		RC_ASSERT(product == a * b);
	});

	suite.check(label + ": dot is symmetric", [](const V& a, const V& b) { RC_ASSERT(dot(a, b) == dot(b, a)); });

	suite.check(label + ": length squared is the square of length", [](const V& v) {
		RC_ASSERT(v.length() >= T{0});
		RC_ASSERT(approx_equal(v.length() * v.length(), v.length_squared(), tolerance(v.length_squared())));
	});

	suite.check(label + ": normalised vector has unit length", [] {
		const V v = *nonzero_vector<T, N>();
		RC_ASSERT(approx_equal(v.normalised().length(), T{1}));
	});

	suite.check(label + ": normalised vector keeps the direction", [] {
		const V v = *nonzero_vector<T, N>();
		RC_ASSERT(approx_equal(v.normalised() * v.length(), v, tolerance(v.length())));
	});

	suite.check(label + ": normalising the zero vector throws MathError",
	            [] { RC_ASSERT_THROWS_AS(static_cast<void>(V::zero().normalised()), MathError); });
}

template <std::floating_point T>
void check_cross(Suite& suite, const std::string& label) {
	using V = Vector<T, 3>;

	suite.check(label + ": cross product is perpendicular to both factors", [](const V& a, const V& b) {
		const V c = cross(a, b);
		const T scale = a.length() * b.length();
		RC_ASSERT(approx_equal(dot(c, a), T{0}, tolerance(scale * a.length())));
		RC_ASSERT(approx_equal(dot(c, b), T{0}, tolerance(scale * b.length())));
	});

	suite.check(label + ": cross product is anticommutative", [](const V& a, const V& b) {
		RC_ASSERT(approx_equal(cross(a, b), -cross(b, a), tolerance(a.length() * b.length())));
	});

	suite.check(label + ": cross product of parallel vectors is zero", [](const V& a) {
		const T s = *finite_scalar<T>();
		RC_ASSERT(approx_equal(cross(a, a * s), V::zero(), tolerance(a.length_squared() * std::abs(s))));
	});
}

void check_examples(Suite& suite) {
	suite.check("components are stored in order", [] {
		Vec3 v{1, 2, 3};
		RC_ASSERT(v[0] == 1.0);
		RC_ASSERT(v[1] == 2.0);
		RC_ASSERT(v[2] == 3.0);
		v[1] = 5.0;
		RC_ASSERT(v == Vec3(1, 5, 3));
	});

	suite.check("unit factories point along the axes", [] {
		RC_ASSERT(Vec3::unit_x() == Vec3(1, 0, 0));
		RC_ASSERT(Vec3::unit_y() == Vec3(0, 1, 0));
		RC_ASSERT(Vec3::unit_z() == Vec3(0, 0, 1));
		RC_ASSERT((Vector<double, 2>::unit_y() == Vector<double, 2>(0, 1)));
	});

	suite.check("component-wise and scalar operators", [] {
		RC_ASSERT(Vec3(1, 2, 3) * Vec3(4, 5, 6) == Vec3(4, 10, 18));
		RC_ASSERT(Vec3(4, 10, 18) / Vec3(4, 5, 6) == Vec3(1, 2, 3));
		RC_ASSERT(Vec3(1, 2, 3) * 2.0 == Vec3(2, 4, 6));
		RC_ASSERT(Vec3(2, 4, 6) / 2.0 == Vec3(1, 2, 3));
		RC_ASSERT(dot(Vec3(1, 2, 3), Vec3(4, 5, 6)) == 32.0);
		RC_ASSERT(Vec3(3, 4, 0).length() == 5.0);
	});

	suite.check("formatter applies the specification to every component", [] {
		RC_ASSERT(std::format("{}", Vec3(1, 2.5, -3)) == "(1, 2.5, -3)");
		RC_ASSERT(std::format("{:.2f}", Vec3(1, 2.5, -3)) == "(1.00, 2.50, -3.00)");
		RC_ASSERT((std::format("{:>4}", Vector<float, 2>(1, 2)) == "(   1,    2)"));
	});

	suite.check("stream output matches the default format", [] {
		std::ostringstream stream;
		stream << Vec3(1, 2.5, -3);
		RC_ASSERT(stream.str() == "(1, 2.5, -3)");
	});
}

} // namespace
} // namespace rt::test

int main() {
	rt::test::Suite suite;
	rt::test::check_examples(suite);
	rt::test::check_arithmetic<double, 3>(suite, "Vec3");
	rt::test::check_arithmetic<double, 2>(suite, "Vector<double, 2>");
	rt::test::check_arithmetic<float, 3>(suite, "Vector<float, 3>");
	rt::test::check_cross<double>(suite, "Vec3");
	rt::test::check_cross<float>(suite, "Vector<float, 3>");
	return suite.exit_code();
}

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
