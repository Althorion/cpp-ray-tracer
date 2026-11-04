#include "rt/errors.hpp"
#include "rt/geometry/hit_record.hpp"
#include "rt/geometry/hittable.hpp"
#include "rt/geometry/sphere.hpp"
#include "rt/materials/material.hpp"
#include "rt/maths/interval.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"

#include <rapidcheck/Assertions.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

static_assert(std::is_base_of_v<IHittable, Sphere>);
static_assert(std::is_final_v<Sphere>);

constexpr double infinity = std::numeric_limits<double>::infinity();

/// The range the renderer uses for primary rays, less the self-intersection margin: everything ahead of the origin.
constexpr Interval<double> ahead{0.0, infinity};

/// Tolerance for positions in a scene with `sphere` and `ray`, whose rounding errors grow with the coordinates.
[[nodiscard]] double scene_tolerance(const Sphere& sphere, const Ray& ray) {
	return tolerance(ray.origin().length() + sphere.centre().length() + sphere.radius());
}

[[nodiscard]] double distance_from_centre(const Sphere& sphere, const Point3& point) {
	return (point - sphere.centre()).length();
}

/// Ray from an arbitrary origin through an arbitrary point well inside `sphere`, so it crosses the surface twice.
[[nodiscard]] Ray aimed_ray(const Sphere& sphere) {
	const Point3 origin = *finite_vector<double, 3>();
	const Point3 target = sphere.centre() + (*unit_vector<double, 3>() * (sphere.radius() * 0.99 * *unit_channel()));
	RC_PRE((target - origin).length_squared() > 0.0);
	return Ray{origin, target - origin};
}

void check_properties(Suite& suite) {
	suite.check("a ray from outside aimed at the centre hits at |c - o| - r", [](const Sphere& sphere) {
		const Point3 origin = *finite_vector<double, 3>();
		const double distance = distance_from_centre(sphere, origin);
		RC_PRE(distance > sphere.radius());
		const Ray ray{origin, sphere.centre() - origin};
		const auto record = sphere.hit(ray, ahead);
		RC_ASSERT(record.has_value());
		RC_ASSERT(approx_equal(record->t, distance - sphere.radius(), scene_tolerance(sphere, ray)));
	});

	suite.check("hits lie on the sphere, with a unit normal facing the ray", [](const Sphere& sphere) {
		const Ray ray = aimed_ray(sphere);
		// Through an owning interface pointer, as the scene holds its objects; under ASan this also checks that
		// deleting through `IHittable` is sound.
		const std::unique_ptr<const IHittable> object = std::make_unique<Sphere>(sphere);
		const auto record = object->hit(ray, ahead);
		RC_ASSERT(record.has_value());
		RC_ASSERT(approx_equal(distance_from_centre(sphere, record->point), sphere.radius(), scene_tolerance(sphere, ray)));
		RC_ASSERT(approx_equal(record->normal.length(), 1.0));
		RC_ASSERT(dot(record->normal, ray.direction()) <= 0.0);
	});

	suite.check("front_face tells whether the ray starts outside the sphere", [](const Sphere& sphere) {
		const Ray ray = aimed_ray(sphere);
		const double gap = distance_from_centre(sphere, ray.origin()) - sphere.radius();
		RC_PRE(std::abs(gap) > scene_tolerance(sphere, ray));
		const auto record = sphere.hit(ray, ahead);
		RC_ASSERT(record.has_value());
		RC_ASSERT(record->front_face == (gap > 0.0));
	});

	suite.check("an unbounded range gives the entry point, and excluding it the exit point", [](const Sphere& sphere) {
		const Ray ray = aimed_ray(sphere);
		const auto entry = sphere.hit(ray, Interval<double>::universe());
		RC_ASSERT(entry.has_value());
		RC_ASSERT(entry->front_face);
		const auto exit = sphere.hit(ray, Interval{std::nextafter(entry->t, infinity), infinity});
		RC_ASSERT(exit.has_value());
		RC_ASSERT(!exit->front_face);
		RC_ASSERT(entry->t < exit->t);
	});

	suite.check("a ray from outside heading away misses", [](const Sphere& sphere) {
		const Point3 origin = *finite_vector<double, 3>();
		RC_PRE(distance_from_centre(sphere, origin) > sphere.radius());
		const Vec3 direction = *nonzero_vector<double, 3>();
		const double approach = dot(direction, sphere.centre() - origin);
		RC_PRE(approach != 0.0);
		const Ray ray{origin, approach > 0.0 ? -direction : direction};
		RC_ASSERT(!sphere.hit(ray, ahead).has_value());
	});

	suite.check("the returned t always lies in the range", [](const Sphere& sphere, const Ray& ray) {
		const auto range = *any_interval<double>();
		const auto record = sphere.hit(ray, range);
		if (record.has_value()) {
			RC_ASSERT(range.contains(record->t));
		}
	});

	suite.check("the returned t always lies in the range, for rays that cross the sphere", [](const Sphere& sphere) {
		const Ray ray = aimed_ray(sphere);
		const auto range = *any_interval<double>();
		const auto record = sphere.hit(ray, range);
		if (record.has_value()) {
			RC_ASSERT(range.contains(record->t));
		}
	});

	suite.check("nothing is hit in an empty range",
	            [](const Sphere& sphere, const Ray& ray) { RC_ASSERT(!sphere.hit(ray, Interval<double>::empty())); });

	suite.check("a radius that is not finite and positive throws MathsError", [](const Point3& centre) {
		RC_ASSERT_THROWS_AS(Sphere(centre, *not_finite_positive(), placeholder_material()), MathsError);
	});

	suite.check("every construction is counted, failed ones and copies are not", [](const Sphere& sphere) {
		const std::size_t before = Sphere::instance_count();
		const Sphere constructed{sphere.centre(), sphere.radius(), sphere.material()};
		RC_ASSERT(Sphere::instance_count() == before + 1);
		const auto copy = std::make_unique<Sphere>(constructed);
		RC_ASSERT_THROWS_AS(Sphere(sphere.centre(), -sphere.radius(), sphere.material()), MathsError);
		RC_ASSERT(Sphere::instance_count() == before + 1);
		RC_ASSERT(copy->radius() == constructed.radius());
	});
}

/// The unit sphere five units down the negative z axis, in front of a camera at the origin.
[[nodiscard]] Sphere example_sphere() { return Sphere{Point3{0, 0, -5}, 1, placeholder_material()}; }

void check_examples(Suite& suite) {
	suite.check("a ray from outside along the axis", [] {
		const Sphere sphere = example_sphere();
		const auto record = sphere.hit(Ray{Point3{}, -Vec3::unit_z()}, Interval{0.001, infinity});
		RC_ASSERT(record.has_value());
		RC_ASSERT(record->t == 4.0);
		RC_ASSERT(record->point == Point3(0, 0, -4));
		RC_ASSERT(record->normal == Vec3::unit_z());
		RC_ASSERT(record->front_face);
		RC_ASSERT(record->material == sphere.material().get());
	});

	suite.check("a ray from the centre hits from the inside", [] {
		const Sphere sphere = example_sphere();
		const auto record = sphere.hit(Ray{sphere.centre(), Vec3::unit_x()}, Interval{0.001, infinity});
		RC_ASSERT(record.has_value());
		RC_ASSERT(record->t == 1.0);
		RC_ASSERT(record->point == Point3(1, 0, -5));
		RC_ASSERT(record->normal == -Vec3::unit_x());
		RC_ASSERT(!record->front_face);
	});

	suite.check("the range selects between the two crossings", [] {
		const Sphere sphere = example_sphere();
		const Ray ray{Point3{}, -Vec3::unit_z()};
		RC_ASSERT(!sphere.hit(ray, Interval{0.001, 3.9}).has_value());
		RC_ASSERT(sphere.hit(ray, Interval{4.5, infinity})->t == 6.0);
		RC_ASSERT(!sphere.hit(ray, Interval{6.5, infinity}).has_value());
		RC_ASSERT(!sphere.hit(Ray{Point3{}, Vec3::unit_z()}, Interval{0.001, infinity}).has_value());
	});

	suite.check("a tangent ray touches the sphere once, perpendicular to the normal", [] {
		const Sphere sphere = example_sphere();
		const auto record = sphere.hit(Ray{Point3{1, 0, 0}, -Vec3::unit_z()}, Interval{0.001, infinity});
		RC_ASSERT(record.has_value());
		// Both roots are 5, but the nearer one goes through |o − c|² − r² with |o − c| = √26, so it is only close;
		// which side the rounded point falls on, and hence `front_face`, is down to rounding.
		RC_ASSERT(approx_equal(record->t, 5.0));
		RC_ASSERT(approx_equal(record->point, Point3(1, 0, -5)));
		RC_ASSERT(approx_equal(dot(record->normal, -Vec3::unit_z()), 0.0));
		RC_ASSERT(dot(record->normal, -Vec3::unit_z()) <= 0.0);
		RC_ASSERT(!sphere.hit(Ray{Point3{1.000001, 0, 0}, -Vec3::unit_z()}, Interval{0.001, infinity}).has_value());
	});

	suite.check("a ray barely above a huge ground sphere", [] {
		const Sphere ground{Point3{0, -1000, 0}, 1000, placeholder_material()};
		const auto record = ground.hit(Ray{Point3{0, 1e-3, 0}, -Vec3::unit_y()}, Interval{1e-6, infinity});
		RC_ASSERT(record.has_value());
		RC_ASSERT(approx_equal(record->t, 1e-3, 1e-10));
		RC_ASSERT(record->front_face);
	});

	suite.check("spheres share their material, and hits carry it", [] {
		const std::shared_ptr<const Material> material = placeholder_material();
		const Sphere first{Point3{}, 1, material};
		const Sphere second{Point3{5, 0, 0}, 2, material};
		RC_ASSERT(first.material() == material);
		RC_ASSERT(second.material() == material);
		RC_ASSERT(material.use_count() == 3);
		RC_ASSERT(first.hit(Ray{Point3{0, 0, 5}, -Vec3::unit_z()}, Interval{0.001, infinity})->material == material.get());
	});

	suite.check("a sphere without a material throws Error",
	            [] { RC_ASSERT_THROWS_AS(Sphere(Point3{}, 1, nullptr), Error); });
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
