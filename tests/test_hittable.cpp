#include "rt/errors.hpp"
#include "rt/geometry/hit_record.hpp"
#include "rt/geometry/hittable.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"

#include <rapidcheck/Assertions.h>

#include <type_traits>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

static_assert(std::is_abstract_v<IHittable>);
static_assert(std::has_virtual_destructor_v<IHittable>, "scenes delete their objects through IHittable pointers");
static_assert(std::is_aggregate_v<HitRecord>);
static_assert(std::is_trivially_copyable_v<HitRecord>);

// The behaviour of `IHittable` implementations is tested with the real ones (test_sphere.cpp); this file covers the
// interface's shape and `HitRecord`, which every implementation relies on.

void check_hit_record(Suite& suite) {
	suite.check("the normal faces against the ray", [](const Ray& ray) {
		const auto record =
				HitRecord::from_outward_normal(ray, *finite_scalar<double>(), *nonzero_vector<double, 3>(), nullptr);
		RC_ASSERT(dot(record.normal, ray.direction()) <= 0.0);
	});

	suite.check("the normal is the normalised outward normal, flipped for hits from the inside", [](const Ray& ray) {
		const Vec3 outward = *nonzero_vector<double, 3>();
		const auto record = HitRecord::from_outward_normal(ray, *finite_scalar<double>(), outward, nullptr);
		RC_ASSERT(approx_equal(record.normal.length(), 1.0));
		RC_ASSERT(record.normal == (record.front_face ? outward.normalised() : -outward.normalised()));
	});

	suite.check("front_face tells whether the ray goes against the outward normal", [](const Ray& ray) {
		const Vec3 outward = *nonzero_vector<double, 3>();
		const auto record = HitRecord::from_outward_normal(ray, *finite_scalar<double>(), outward, nullptr);
		RC_ASSERT(record.front_face == (dot(ray.direction(), outward.normalised()) < 0.0));
	});

	suite.check("the point and t come from the ray", [](const Ray& ray) {
		const double t = *finite_scalar<double>();
		const auto record = HitRecord::from_outward_normal(ray, t, *nonzero_vector<double, 3>(), nullptr);
		RC_ASSERT(record.t == t);
		RC_ASSERT(record.point == ray.at(t));
	});

	suite.check("a zero normal or a non-finite t throws MathsError", [](const Ray& ray) {
		RC_ASSERT_THROWS_AS(
				static_cast<void>(HitRecord::from_outward_normal(ray, *finite_scalar<double>(), Vec3::zero(), nullptr)),
				MathsError);
		RC_ASSERT_THROWS_AS(static_cast<void>(HitRecord::from_outward_normal(ray, *non_finite_scalar<double>(),
		                                                                     *nonzero_vector<double, 3>(), nullptr)),
		                    MathsError);
	});
}

void check_examples(Suite& suite) {
	suite.check("a grazing ray counts as coming from the inside", [] {
		const auto record = HitRecord::from_outward_normal(Ray{Point3{}, Vec3::unit_x()}, 1.0, Vec3::unit_z(), nullptr);
		RC_ASSERT(!record.front_face);
		RC_ASSERT(record.normal == -Vec3::unit_z());
	});
}

} // namespace
} // namespace rt::test

int main() {
	rt::test::Suite suite;
	rt::test::check_examples(suite);
	rt::test::check_hit_record(suite);
	return suite.exit_code();
}

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
