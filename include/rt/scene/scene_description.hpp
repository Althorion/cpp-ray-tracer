#ifndef RT_SCENE_SCENE_DESCRIPTION_HPP
#define RT_SCENE_SCENE_DESCRIPTION_HPP

#include "rt/colour.hpp"
#include "rt/errors.hpp"
#include "rt/geometry/sphere.hpp"
#include "rt/materials/glass.hpp"
#include "rt/materials/lambertian.hpp"
#include "rt/materials/material.hpp"
#include "rt/materials/metal.hpp"
#include "rt/materials/plastic.hpp"
#include "rt/maths/vector.hpp"
#include "rt/scene/camera.hpp"
#include "rt/scene/scene.hpp"
#include "rt/validation.hpp"

#include <concepts>
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace rt {

/// Parameters of `material <name> lambertian <rgb>`.
struct LambertianDescription {
	Colour albedo;

	[[nodiscard]] friend bool operator==(const LambertianDescription&, const LambertianDescription&) noexcept = default;
};

/// Parameters of `material <name> metal <rgb> [fuzz <f>]`.
struct MetalDescription {
	Colour albedo;
	double fuzz = 0.0;

	[[nodiscard]] friend bool operator==(const MetalDescription&, const MetalDescription&) noexcept = default;
};

/// Parameters of `material <name> glass <ior>`.
struct GlassDescription {
	double ior = 1.5;

	[[nodiscard]] friend bool operator==(const GlassDescription&, const GlassDescription&) noexcept = default;
};

/// Parameters of `material <name> plastic <rgb> [ior <n>]`.
struct PlasticDescription {
	Colour albedo;
	double ior = Plastic::default_ior;

	[[nodiscard]] friend bool operator==(const PlasticDescription&, const PlasticDescription&) noexcept = default;
};

using MaterialParameters = std::variant<LambertianDescription, MetalDescription, GlassDescription, PlasticDescription>;

/// A `material` directive.
struct MaterialDescription {
	std::string name;
	MaterialParameters parameters;

	[[nodiscard]] friend bool operator==(const MaterialDescription&, const MaterialDescription&) = default;
};

/// A `sphere` directive; the material is referred to by name.
struct SphereDescription {
	Point3 centre;
	double radius = 1.0;
	std::string material;

	[[nodiscard]] friend bool operator==(const SphereDescription&, const SphereDescription&) = default;
};

/// The contents of a scene file as plain, comparable data (README.md § 7.2), unvalidated: `build_scene` turns it
/// into a `Scene` and checks it on the way.
///
/// The parser produces it and `serialise` writes it back, which the round-trip tests compare. Materials and spheres
/// keep the order of their directives.
struct SceneDescription {
	CameraSettings camera;
	Sky sky;
	std::vector<MaterialDescription> materials;
	std::vector<SphereDescription> spheres;

	[[nodiscard]] friend bool operator==(const SceneDescription&, const SceneDescription&) = default;
};

namespace detail {

[[nodiscard]] inline std::shared_ptr<Material> make_material(const MaterialDescription& description) {
	return std::visit(
			[&name = description.name]<typename Parameters>(const Parameters& parameters) -> std::shared_ptr<Material> {
				if constexpr (std::same_as<Parameters, LambertianDescription>) {
					return std::make_shared<Lambertian>(name, parameters.albedo);
				} else if constexpr (std::same_as<Parameters, MetalDescription>) {
					return std::make_shared<Metal>(name, parameters.albedo, parameters.fuzz);
				} else if constexpr (std::same_as<Parameters, GlassDescription>) {
					return std::make_shared<Glass>(name, parameters.ior);
				} else {
					static_assert(std::same_as<Parameters, PlasticDescription>);
					return std::make_shared<Plastic>(name, parameters.albedo, parameters.ior);
				}
			},
			description.parameters);
}

} // namespace detail

/// Builds the scene described by `description`.
///
/// Every invalid value is reported as a `SceneError`, with the offending item named in the message; the parser is
/// expected to have rejected most of them already, together with their position in the file.
///
/// @throws SceneError for invalid camera settings (README.md § 5.2), a sky colour component above 1, invalid
/// material parameters or an empty material name, and a sphere radius that is not finite and positive.
/// @throws DuplicateNameError if two materials share a name.
/// @throws UnknownNameError if a sphere refers to a material not described.
[[nodiscard]] inline Scene build_scene(const SceneDescription& description) {
	for (const Colour& colour : {description.sky.top, description.sky.bottom}) {
		if (const auto problem = validation::check_rgb(colour)) {
			throw SceneError{std::format("sky: {}", *problem)};
		}
	}
	Scene scene{Camera{description.camera}, description.sky};

	for (const auto& material : description.materials) {
		std::shared_ptr<Material> built;
		try {
			built = detail::make_material(material);
		} catch (const Error& error) {
			throw SceneError{std::format("material '{}': {}", material.name, error.what())};
		}
		scene.add_material(built);
	}

	for (std::size_t i = 0; i < description.spheres.size(); ++i) {
		const SphereDescription& sphere = description.spheres[i];
		const auto& material = scene.materials().get(sphere.material);
		try {
			scene.add_object(std::make_unique<Sphere>(sphere.centre, sphere.radius, material));
		} catch (const Error& error) {
			throw SceneError{std::format("sphere #{}: {}", i + 1, error.what())};
		}
	}
	return scene;
}

/// The demonstration scene of README.md § 5.5: four spheres, one of each material, on a huge matte ground sphere,
/// seen through a lens focused on the middle one.
[[nodiscard]] inline SceneDescription demo_scene_description() {
	return SceneDescription{
			.camera =
					CameraSettings{
							.from = Point3{-2, 2, 1},
							.to = Point3{0, 0, -1},
							.up = Vec3{0, 1, 0},
							.fov = 40.0,
							.aperture = 0.1,
							.focus = 3.4,
					},
			.sky = Sky{.top = Colour{0.5, 0.7, 1.0}, .bottom = Colour{1, 1, 1}},
			.materials =
					{
							{.name = "ground", .parameters = LambertianDescription{.albedo = Colour{0.8, 0.8, 0.0}}},
							{.name = "red", .parameters = PlasticDescription{.albedo = Colour{0.7, 0.1, 0.1}, .ior = 1.5}},
							{.name = "glass", .parameters = GlassDescription{.ior = 1.5}},
							{.name = "gold", .parameters = MetalDescription{.albedo = Colour{0.8, 0.6, 0.2}, .fuzz = 0.1}},
					},
			.spheres =
					{
							{.centre = Point3{0, -100.5, -1}, .radius = 100, .material = "ground"},
							{.centre = Point3{0, 0, -1}, .radius = 0.5, .material = "red"},
							{.centre = Point3{-1, 0, -1}, .radius = 0.5, .material = "glass"},
							{.centre = Point3{1, 0, -1}, .radius = 0.5, .material = "gold"},
					},
	};
}

} // namespace rt

#endif // RT_SCENE_SCENE_DESCRIPTION_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
