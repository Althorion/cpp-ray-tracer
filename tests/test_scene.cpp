#include "rt/colour.hpp"
#include "rt/errors.hpp"
#include "rt/geometry/hit_record.hpp"
#include "rt/geometry/sphere.hpp"
#include "rt/materials/lambertian.hpp"
#include "rt/materials/metal.hpp"
#include "rt/maths/interval.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"
#include "rt/scene/camera.hpp"
#include "rt/scene/scene.hpp"

#include <rapidcheck/Assertions.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

static_assert(!std::is_copy_constructible_v<Scene>);
static_assert(!std::is_copy_assignable_v<Scene>);
static_assert(std::is_nothrow_move_constructible_v<Scene>);
static_assert(std::is_nothrow_move_assignable_v<Scene>);

constexpr double infinity = std::numeric_limits<double>::infinity();

/// The range the renderer searches (README.md § 6.2).
constexpr Interval<double> ahead{0.001, infinity};

[[nodiscard]] Camera example_camera() {
	return Camera{CameraSettings{.from = Point3{0, 0, 0}, .to = Point3{0, 0, -1}}};
}

[[nodiscard]] Scene scene_with(const std::vector<Sphere>& spheres) {
	Scene scene{example_camera()};
	for (const auto& sphere : spheres) {
		scene.add_object(std::make_unique<Sphere>(sphere));
	}
	return scene;
}

/// The nearest hit, found by asking every sphere on its own.
[[nodiscard]] std::optional<HitRecord> nearest_hit(const std::vector<Sphere>& spheres, const Ray& ray) {
	std::optional<HitRecord> nearest;
	for (const auto& sphere : spheres) {
		const auto record = sphere.hit(ray, ahead);
		if (record && (!nearest || record->t < nearest->t)) {
			nearest = record;
		}
	}
	return nearest;
}

/// Whether `scene` finds the same nearest hit as asking each of `spheres` separately.
[[nodiscard]] bool agrees_with_spheres(const Scene& scene, const std::vector<Sphere>& spheres, const Ray& ray) {
	const auto expected = nearest_hit(spheres, ray);
	const auto actual = scene.hit(ray, ahead);
	if (!expected || !actual) {
		return expected.has_value() == actual.has_value();
	}
	return actual->t == expected->t && actual->point == expected->point;
}

[[nodiscard]] bool within(double value, double a, double b) {
	return std::min(a, b) - default_eps<double> <= value && value <= std::max(a, b) + default_eps<double>;
}

void check_hits(Suite& suite) {
	suite.check("the scene finds the nearest of the spheres' hits",
	            [](const std::vector<Sphere>& spheres, const Ray& ray) {
								RC_ASSERT(agrees_with_spheres(scene_with(spheres), spheres, ray));
							});

	suite.check("the scene finds the nearest of the spheres' hits, for rays aimed at one of them",
	            [](const std::vector<Sphere>& spheres) {
								RC_PRE(!spheres.empty());
								const Sphere& target = spheres[*index_below(spheres.size())];
								const Point3 origin = *finite_vector<double, 3>();
								RC_PRE(origin != target.centre());
								RC_ASSERT(agrees_with_spheres(scene_with(spheres), spheres, Ray{origin, target.centre() - origin}));
							});

	suite.check("an empty scene is never hit", [](const Ray& ray) {
		const Scene scene{example_camera()};
		RC_ASSERT(!scene.hit(ray, Interval<double>::universe()).has_value());
	});

	suite.check("a moved scene keeps its contents", [](const std::vector<Sphere>& spheres, const Ray& ray) {
		Scene original = scene_with(spheres);
		original.add_material(std::make_shared<Lambertian>("matte", Colour{0.5, 0.5, 0.5}));
		const Scene moved{std::move(original)};
		RC_ASSERT(moved.object_count() == spheres.size());
		RC_ASSERT(moved.materials().contains("matte"));
		RC_ASSERT(agrees_with_spheres(moved, spheres, ray));
	});
}

void check_sky(Suite& suite) {
	suite.check("the sky is top straight up and bottom straight down", [](const Colour& top, const Colour& bottom) {
		const Sky sky{.top = top, .bottom = bottom};
		RC_ASSERT(sky.colour(Vec3::unit_y()) == top);
		RC_ASSERT(sky.colour(-Vec3::unit_y()) == bottom);
	});

	suite.check("the sky colour lies between bottom and top",
	            [](const Colour& top, const Colour& bottom, const Vec3& direction) {
								RC_PRE(direction != Vec3::zero());
								const Colour colour = Sky{.top = top, .bottom = bottom}.colour(direction);
								RC_ASSERT(within(colour.r(), top.r(), bottom.r()));
								RC_ASSERT(within(colour.g(), top.g(), bottom.g()));
								RC_ASSERT(within(colour.b(), top.b(), bottom.b()));
							});

	suite.check("the sky colour depends only on the direction, not on the length",
	            [](const Colour& top, const Colour& bottom) {
								const Vec3 direction = *unit_vector<double, 3>();
								const double length = *positive_scalar();
								const Sky sky{.top = top, .bottom = bottom};
								const Colour a = sky.colour(direction);
								const Colour b = sky.colour(direction * length);
								RC_ASSERT(approx_equal(a.r(), b.r()));
								RC_ASSERT(approx_equal(a.g(), b.g()));
								RC_ASSERT(approx_equal(a.b(), b.b()));
							});

	suite.check("the scene shows its sky", [](const Colour& top, const Colour& bottom, const Vec3& direction) {
		RC_PRE(direction != Vec3::zero());
		const Sky sky{.top = top, .bottom = bottom};
		const Scene scene{example_camera(), sky};
		RC_ASSERT(scene.sky() == sky);
		RC_ASSERT(scene.sky_colour(direction) == sky.colour(direction));
	});
}

void check_examples(Suite& suite) {
	suite.check("the default sky fades from white below to light blue above", [] {
		const Sky sky;
		RC_ASSERT(sky.top == (Colour{0.5, 0.7, 1.0}));
		RC_ASSERT(sky.bottom == Colour::white());
		const Colour horizon = sky.colour(Vec3::unit_x());
		RC_ASSERT(approx_equal(horizon.r(), 0.75));
		RC_ASSERT(approx_equal(horizon.g(), 0.85));
		RC_ASSERT(approx_equal(horizon.b(), 1.0));
	});

	suite.check("the sky of a zero direction throws MathsError",
	            [] { RC_ASSERT_THROWS_AS((void)Sky{}.colour(Vec3::zero()), MathsError); });

	suite.check("the nearer of two spheres in line is hit, whichever was added first", [] {
		const Sphere near{Point3{0, 0, -3}, 1, placeholder_material()};
		const Sphere far{Point3{0, 0, -10}, 1, placeholder_material()};
		const Ray ray{Point3{0, 0, 0}, -Vec3::unit_z()};
		for (const auto& order : {std::vector{near, far}, std::vector{far, near}}) {
			const auto record = scene_with(order).hit(ray, ahead);
			RC_ASSERT(record.has_value());
			RC_ASSERT(approx_equal(record->t, 2.0));
		}
	});

	suite.check("the range limits what the scene can hit", [] {
		const Scene scene = scene_with({Sphere{Point3{0, 0, -3}, 1, placeholder_material()}});
		const Ray ray{Point3{0, 0, 0}, -Vec3::unit_z()};
		RC_ASSERT(!scene.hit(ray, Interval{0.001, 1.5}).has_value());
		RC_ASSERT(approx_equal(scene.hit(ray, Interval{2.5, infinity})->t, 4.0));
	});

	suite.check("spheres share the scene's materials, and hits carry them", [] {
		Scene scene{example_camera()};
		scene.add_material(std::make_shared<Metal>("gold", Colour{0.8, 0.6, 0.2}, 0.1));
		const auto& gold = scene.materials().get("gold");
		scene.add_object(std::make_unique<Sphere>(Point3{0, 0, -3}, 1, gold));
		scene.add_object(std::make_unique<Sphere>(Point3{0, 0, 3}, 1, gold));
		RC_ASSERT(gold.use_count() == 3);
		RC_ASSERT(scene.hit(Ray{Point3{0, 0, 0}, -Vec3::unit_z()}, ahead)->material == gold.get());
	});

	suite.check("a duplicate material name throws DuplicateNameError and keeps the first", [] {
		Scene scene{example_camera()};
		const auto first = std::make_shared<Lambertian>("red", Colour{0.7, 0.1, 0.1});
		scene.add_material(first);
		RC_ASSERT_THROWS_AS(scene.add_material(std::make_shared<Lambertian>("red", Colour{0.1, 0.1, 0.7})),
		                    DuplicateNameError);
		RC_ASSERT(scene.materials().size() == 1U);
		RC_ASSERT(scene.materials().get("red") == first);
	});

	suite.check("an unknown material throws UnknownNameError", [] {
		const Scene scene{example_camera()};
		RC_ASSERT_THROWS_AS((void)scene.materials().get("missing"), UnknownNameError);
	});

	suite.check("adding a null object throws Error", [] {
		Scene scene{example_camera()};
		RC_ASSERT_THROWS_AS(scene.add_object(nullptr), Error);
		RC_ASSERT(scene.object_count() == 0U);
	});

	suite.check("the scene keeps its camera", [] {
		const CameraSettings settings{.from = Point3{1, 2, 3}, .to = Point3{0, 0, 0}, .fov = 40.0};
		const Scene scene{Camera{settings}};
		RC_ASSERT(scene.camera().settings() == settings);
	});
}

} // namespace
} // namespace rt::test

int main() {
	rt::test::Suite suite;
	rt::test::check_examples(suite);
	rt::test::check_hits(suite);
	rt::test::check_sky(suite);
	return suite.exit_code();
}

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
