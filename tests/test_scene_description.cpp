#include "rt/colour.hpp"
#include "rt/errors.hpp"
#include "rt/materials/glass.hpp"
#include "rt/materials/lambertian.hpp"
#include "rt/materials/material.hpp"
#include "rt/materials/metal.hpp"
#include "rt/materials/plastic.hpp"
#include "rt/maths/interval.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"
#include "rt/scene/scene.hpp"
#include "rt/scene/scene_description.hpp"

#include <rapidcheck/Assertions.h>

#include <algorithm>
#include <concepts>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

static_assert(std::equality_comparable<SceneDescription>);
static_assert(std::is_copy_constructible_v<SceneDescription>);

constexpr double infinity = std::numeric_limits<double>::infinity();

/// Whether `material` is the material that `parameters` describe.
[[nodiscard]] bool matches(const Material& material, const MaterialParameters& parameters) {
	return std::visit(
			[&material]<typename Parameters>(const Parameters& expected) {
				if constexpr (std::same_as<Parameters, LambertianDescription>) {
					const auto* actual = dynamic_cast<const Lambertian*>(&material);
					// A `Plastic` is a `Lambertian` too, but not the material described.
					return actual != nullptr && dynamic_cast<const Plastic*>(&material) == nullptr &&
					       actual->albedo() == expected.albedo;
				} else if constexpr (std::same_as<Parameters, MetalDescription>) {
					const auto* actual = dynamic_cast<const Metal*>(&material);
					return actual != nullptr && actual->albedo() == expected.albedo && actual->fuzz() == expected.fuzz;
				} else if constexpr (std::same_as<Parameters, GlassDescription>) {
					const auto* actual = dynamic_cast<const Glass*>(&material);
					return actual != nullptr && actual->ior() == expected.ior;
				} else {
					const auto* actual = dynamic_cast<const Plastic*>(&material);
					return actual != nullptr && actual->albedo() == expected.albedo && actual->ior() == expected.ior;
				}
			},
			parameters);
}

/// Name of the material hit by `ray` in `scene`, or an empty view on a miss.
[[nodiscard]] std::string_view material_hit(const Scene& scene, const Ray& ray) {
	const auto record = scene.hit(ray, Interval{0.001, infinity});
	return record ? record->material->name() : std::string_view{};
}

void check_properties(Suite& suite) {
	suite.check("the built scene has the described camera, sky, materials and number of spheres",
	            [](const SceneDescription& description) {
								const Scene scene = build_scene(description);
								RC_ASSERT(scene.camera().settings() == description.camera);
								RC_ASSERT(scene.sky() == description.sky);
								RC_ASSERT(scene.materials().size() == description.materials.size());
								for (const auto& material : description.materials) {
									RC_ASSERT(matches(*scene.materials().get(material.name), material.parameters));
								}
								RC_ASSERT(scene.object_count() == description.spheres.size());
							});

	suite.check("a described sphere is built with its centre, radius and material", [](SceneDescription description) {
		RC_PRE(!description.spheres.empty());
		// Keep one sphere, so that nothing stands in the way of a ray aimed at it.
		const SphereDescription sphere = description.spheres[*index_below(description.spheres.size())];
		description.spheres = {sphere};
		const Scene scene = build_scene(description);
		const Point3 origin = sphere.centre + (Vec3::unit_x() * (2.0 * sphere.radius));
		const auto record = scene.hit(Ray{origin, -Vec3::unit_x()}, Interval{0.0, infinity});
		RC_ASSERT(record.has_value());
		RC_ASSERT(approx_equal(record->t, sphere.radius, tolerance(sphere.centre.length() + sphere.radius)));
		RC_ASSERT(record->material->name() == sphere.material);
	});

	suite.check("a sphere made of an undescribed material throws UnknownNameError", [](SceneDescription description) {
		RC_PRE(!description.spheres.empty());
		description.spheres.back().material = "undescribed-name-x";
		RC_PRE(!std::ranges::any_of(description.materials, [](const MaterialDescription& material) {
			return material.name == "undescribed-name-x";
		}));
		RC_ASSERT_THROWS_AS((void)build_scene(description), UnknownNameError);
	});

	suite.check("two materials of the same name throw DuplicateNameError", [](SceneDescription description) {
		RC_PRE(!description.materials.empty());
		MaterialDescription twin = description.materials[*index_below(description.materials.size())];
		twin.parameters = *material_parameters();
		description.materials.push_back(twin);
		RC_ASSERT_THROWS_AS((void)build_scene(description), DuplicateNameError);
	});

	suite.check("invalid camera settings throw SceneError", [](SceneDescription description) {
		description.camera.fov = *invalid_fov();
		RC_ASSERT_THROWS_AS((void)build_scene(description), SceneError);
	});

	suite.check("an invalid sphere radius throws SceneError", [](SceneDescription description) {
		RC_PRE(!description.spheres.empty());
		description.spheres[*index_below(description.spheres.size())].radius = *not_finite_positive();
		RC_ASSERT_THROWS_AS((void)build_scene(description), SceneError);
	});

	suite.check("invalid material parameters throw SceneError", [](SceneDescription description) {
		description.materials.push_back(
				{.name = "invalid", .parameters = MetalDescription{.albedo = Colour{0.5, 0.5, 0.5}, .fuzz = *invalid_fuzz()}});
		RC_PRE(std::ranges::count(description.materials, std::string{"invalid"}, &MaterialDescription::name) == 1);
		RC_ASSERT_THROWS_AS((void)build_scene(description), SceneError);
	});

	suite.check("a sky colour component above 1 throws SceneError", [](SceneDescription description) {
		description.sky.top = *excessive_albedo();
		RC_ASSERT_THROWS_AS((void)build_scene(description), SceneError);
	});

	suite.check("a description equals its copy, and not after a change", [](const SceneDescription& description) {
		SceneDescription copy = description;
		RC_ASSERT(copy == description);
		copy.camera.fov = description.camera.fov == 45.0 ? 46.0 : 45.0;
		RC_ASSERT(copy != description);
	});
}

void check_examples(Suite& suite) {
	suite.check("the demo scene builds, with four materials and four spheres", [] {
		const Scene scene = build_scene(demo_scene_description());
		RC_ASSERT(scene.materials().size() == 4U);
		RC_ASSERT(scene.object_count() == 4U);
		RC_ASSERT(scene.camera().focus() == 3.4);
	});

	suite.check("the demo scene has one sphere of each material where README.md § 5.5 puts it", [] {
		const Scene scene = build_scene(demo_scene_description());
		// Rays along −z through the centres of the small spheres, one down onto the ground and one up into the sky.
		RC_ASSERT(material_hit(scene, Ray{Point3{0, 0, 5}, -Vec3::unit_z()}) == "red");
		RC_ASSERT(material_hit(scene, Ray{Point3{-1, 0, 5}, -Vec3::unit_z()}) == "glass");
		RC_ASSERT(material_hit(scene, Ray{Point3{1, 0, 5}, -Vec3::unit_z()}) == "gold");
		RC_ASSERT(material_hit(scene, Ray{Point3{3, 5, 3}, -Vec3::unit_y()}) == "ground");
		RC_ASSERT(material_hit(scene, Ray{Point3{0, 5, 0}, Vec3::unit_y()}).empty());
	});

	suite.check("the demo scene's materials have the described kinds", [] {
		const Scene scene = build_scene(demo_scene_description());
		RC_ASSERT(dynamic_cast<const Lambertian*>(scene.materials().get("ground").get()) != nullptr);
		RC_ASSERT(dynamic_cast<const Plastic*>(scene.materials().get("red").get()) != nullptr);
		RC_ASSERT(dynamic_cast<const Glass*>(scene.materials().get("glass").get()) != nullptr);
		RC_ASSERT(dynamic_cast<const Metal*>(scene.materials().get("gold").get()) != nullptr);
	});

	suite.check("an error names the offending sphere", [] {
		SceneDescription description = demo_scene_description();
		description.spheres[2].radius = -1.0;
		try {
			(void)build_scene(description);
			RC_FAIL("no exception thrown");
		} catch (const SceneError& error) {
			RC_ASSERT(std::string_view{error.what()}.contains("sphere #3"));
		}
	});

	suite.check("an error names the offending material", [] {
		SceneDescription description = demo_scene_description();
		description.materials[1].parameters = PlasticDescription{.albedo = Colour{0.5, 0.5, 0.5}, .ior = 0.0};
		try {
			(void)build_scene(description);
			RC_FAIL("no exception thrown");
		} catch (const SceneError& error) {
			RC_ASSERT(std::string_view{error.what()}.contains("'red'"));
		}
	});

	suite.check("an empty material name throws SceneError", [] {
		SceneDescription description = demo_scene_description();
		description.materials.push_back({.name = "", .parameters = GlassDescription{.ior = 1.5}});
		RC_ASSERT_THROWS_AS((void)build_scene(description), SceneError);
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
