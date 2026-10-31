#include "rt/errors.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"

#include <rapidcheck/Assertions.h>

#include <cmath>
#include <limits>
#include <sstream>
#include <type_traits>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

// A ray is always valid after construction, so there is no default (directionless) ray.
static_assert(!std::is_default_constructible_v<Ray>);
static_assert(std::is_trivially_copyable_v<Ray>);

void check_properties(Suite& suite) {
	suite.check("the point at t lies |t| away from the origin", [](const Ray& ray) {
		const double t = *finite_scalar<double>();
		const double distance = (ray.at(t) - ray.origin()).length();
		RC_ASSERT(approx_equal(distance, std::abs(t), tolerance(ray.origin().length() + std::abs(t))));
	});

	suite.check("the direction has unit length",
	            [](const Ray& ray) { RC_ASSERT(approx_equal(ray.direction().length(), 1.0)); });

	suite.check("the direction is the normalised constructor argument", [](const Point3& origin) {
		const Vec3 direction = *nonzero_vector<double, 3>();
		RC_ASSERT(Ray(origin, direction).direction() == direction.normalised());
	});

	suite.check("scaling the direction by a positive factor gives the same ray", [](const Point3& origin) {
		const Vec3 direction = *nonzero_vector<double, 3>();
		const double factor = *finite_scalar<double>();
		RC_PRE(factor > 0.0);
		RC_PRE((direction * factor).length_squared() >= std::numeric_limits<double>::min());
		RC_ASSERT(approx_equal(Ray(origin, direction * factor).direction(), Ray(origin, direction).direction()));
	});

	suite.check("at(0) is the origin", [](const Ray& ray) { RC_ASSERT(ray.at(0.0) == ray.origin()); });

	suite.check("points along the ray differ by a multiple of the direction", [](const Ray& ray) {
		const double t = *finite_scalar<double>();
		const double s = *finite_scalar<double>();
		const double scale = ray.origin().length() + std::abs(t) + std::abs(s);
		RC_ASSERT(approx_equal(ray.at(t) - ray.at(s), ray.direction() * (t - s), tolerance(scale)));
	});

	suite.check("a non-finite t throws MathsError", [](const Ray& ray) {
		RC_ASSERT_THROWS_AS(static_cast<void>(ray.at(*non_finite_scalar<double>())), MathsError);
	});

	suite.check("a zero direction throws MathsError",
	            [](const Point3& origin) { RC_ASSERT_THROWS_AS(Ray(origin, Vec3::zero()), MathsError); });
}

void check_examples(Suite& suite) {
	suite.check("ray along the z axis", [] {
		const Ray ray{Point3{1, 2, 3}, Vec3{0, 0, 2}};
		RC_ASSERT(ray.origin() == Point3(1, 2, 3));
		RC_ASSERT(ray.direction() == Vec3::unit_z());
		RC_ASSERT(ray.at(2.0) == Point3(1, 2, 5));
		RC_ASSERT(ray.at(-1.0) == Point3(1, 2, 2));
	});

	suite.check("a point beyond the range of double throws MathsError", [] {
		constexpr double huge = std::numeric_limits<double>::max();
		const Ray ray{Point3{huge, 0, 0}, Vec3::unit_x()};
		RC_ASSERT_THROWS_AS(static_cast<void>(ray.at(huge)), MathsError);
	});

	suite.check("stream output", [] {
		std::ostringstream stream;
		stream << Ray{Point3{1, 2, 3}, Vec3{0, 0, 2}};
		RC_ASSERT(stream.str() == "Ray{origin: (1, 2, 3), direction: (0, 0, 1)}");
	});
}

} // namespace
} // namespace rt::test

int main() {
	rt::test::Suite suite;
	rt::test::check_examples(suite);
	rt::test::check_properties(suite);
	return suite.exit_code();
}

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
