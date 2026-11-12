#include "rt/colour.hpp"
#include "rt/errors.hpp"
#include "rt/geometry/hit_record.hpp"
#include "rt/materials/glass.hpp"
#include "rt/materials/lambertian.hpp"
#include "rt/materials/material.hpp"
#include "rt/materials/metal.hpp"
#include "rt/materials/plastic.hpp"
#include "rt/materials/specular_coat.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"
#include "rt/random/sampler.hpp"

#include <rapidcheck/Assertions.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

static_assert(std::is_abstract_v<Material>);
static_assert(std::is_base_of_v<IScatterer, Material> && std::is_base_of_v<INamed, Material>);
static_assert(!std::is_final_v<Lambertian>, "Plastic derives from it");
static_assert(std::is_final_v<Metal> && std::is_final_v<Glass> && std::is_final_v<Plastic>);
static_assert(std::is_convertible_v<Plastic*, Lambertian*>, "plastic is a Lambertian surface");
static_assert(std::is_base_of_v<SpecularCoat, Plastic> && !std::is_convertible_v<Plastic*, SpecularCoat*>,
              "plastic is implemented in terms of a coat, privately");
static_assert(!std::is_polymorphic_v<SpecularCoat>, "a mixin, not an interface");

/// Cosine of the angle between the incoming ray and the normal, which faces against it.
[[nodiscard]] double incidence_cosine(const Incidence& incidence) {
	return std::min(-dot(incidence.ray.direction(), incidence.record.normal), 1.0);
}

/// Sine of the angle between unit vectors.
[[nodiscard]] double sine(const Vec3& a, const Vec3& b) { return cross(a, b).length(); }

[[nodiscard]] bool within_unit_range(const Colour& colour) {
	return colour.r() <= 1.0 && colour.g() <= 1.0 && colour.b() <= 1.0;
}

[[nodiscard]] Vec3 mirrored(const Incidence& incidence) {
	return SpecularCoat::reflect(incidence.ray.direction(), incidence.record.normal);
}

/// Whether `hits` out of `draws` trials are consistent with probability `probability`, within six standard errors.
[[nodiscard]] bool frequency_is_close(int hits, double probability) {
	const double frequency = static_cast<double>(hits) / draws;
	const double standard_error = std::sqrt(probability * (1.0 - probability) / draws);
	return std::abs(frequency - probability) <= (6.0 * standard_error) + (1.0 / draws);
}

/// Incidence from inside a sphere of the given refractive index, at an angle beyond the critical one.
[[nodiscard]] Incidence beyond_critical_angle(double ior) {
	const Vec3 outward = *unit_vector<double, 3>();
	const Vec3 tangent = cross(outward, *nonzero_vector<double, 3>());
	RC_PRE(tangent.length_squared() > 1e-12);
	const double critical = std::asin(1.0 / ior);
	const double fraction = *unit_channel();
	RC_PRE(fraction > 0.0);
	const double angle = critical + ((std::numbers::pi / 2 - critical) * fraction);
	const Ray ray{*finite_vector<double, 3>(), (outward * std::cos(angle)) + (tangent.normalised() * std::sin(angle))};
	return Incidence{.ray = ray, .record = HitRecord::from_outward_normal(ray, 1.0, outward, nullptr)};
}

/// Properties every material must have (README.md § 8.2), checked through the `Material` interface.
template <typename Concrete>
void check_common(Suite& suite, const std::string& kind) {
	suite.check(kind + ": scattered light starts at the hit point, along a unit direction, attenuated into [0, 1]",
	            [](const Concrete& concrete, const Incidence& incidence, Sampler sampler) {
								const Material& material = concrete;
								const auto scattered = material.scatter(incidence.ray, incidence.record, sampler);
								if (scattered.has_value()) {
									RC_ASSERT(scattered->ray.origin() == incidence.record.point);
									RC_ASSERT(approx_equal(scattered->ray.direction().length(), 1.0));
									RC_ASSERT(within_unit_range(scattered->attenuation));
								}
							});

	suite.check(kind + ": the name is kept", [](const Concrete& concrete) {
		const INamed& named = concrete;
		RC_ASSERT(!named.name().empty());
	});
}

void check_lambertian(Suite& suite) {
	suite.check("Lambertian: never absorbs, scatters into the normal half-space, attenuates by the albedo",
	            [](const Lambertian& lambertian, const Incidence& incidence, Sampler sampler) {
								const auto scattered = lambertian.scatter(incidence.ray, incidence.record, sampler);
								RC_ASSERT(scattered.has_value());
								RC_ASSERT(dot(scattered->ray.direction(), incidence.record.normal) >= 0.0);
								RC_ASSERT(scattered->attenuation == lambertian.albedo());
							});

	// Lambert's law: the density is proportional to cos θ, so cos θ has mean 2/3 and variance 1/2 − 4/9 = 1/18.
	suite.check("Lambertian: directions follow Lambert's cosine law",
	            [](const Lambertian& lambertian, const Incidence& incidence, Sampler sampler) {
								const auto cosine = [&] {
									return dot(lambertian.scatter(incidence.ray, incidence.record, sampler)->ray.direction(),
									           incidence.record.normal);
								};
								RC_ASSERT(mean_is_close(cosine, 2.0 / 3.0, 1.0 / 18.0));
							});
}

void check_metal(Suite& suite) {
	suite.check("Metal without fuzz: the angle of reflection equals the angle of incidence",
	            [](const Incidence& incidence, Sampler sampler) {
								const Metal metal{*material_name(), *unit_colour(), 0.0};
								const auto scattered = metal.scatter(incidence.ray, incidence.record, sampler);
								if (!scattered.has_value()) {
									// Only a ray grazing the surface leaves along it, which counts as absorbed.
									RC_ASSERT(approx_equal(incidence_cosine(incidence), 0.0));
									return;
								}
								const Vec3& direction = scattered->ray.direction();
								RC_ASSERT(approx_equal(dot(direction, incidence.record.normal), incidence_cosine(incidence)));
								RC_ASSERT(approx_equal(direction, mirrored(incidence)));
								RC_ASSERT(scattered->attenuation == metal.albedo());
							});

	// The direction is that of r + f·u for the unit mirror direction r and a unit u, so it is at most asin(f) off r.
	suite.check("Metal: absorbs, or reflects above the surface within the fuzz of the mirror direction",
	            [](const Metal& metal, const Incidence& incidence, Sampler sampler) {
								const auto scattered = metal.scatter(incidence.ray, incidence.record, sampler);
								if (scattered.has_value()) {
									const Vec3& direction = scattered->ray.direction();
									RC_ASSERT(dot(direction, incidence.record.normal) > 0.0);
									const double fuzz = metal.fuzz();
									RC_ASSERT(dot(direction, mirrored(incidence)) >= std::sqrt(1.0 - (fuzz * fuzz)) - 1e-9);
									RC_ASSERT(scattered->attenuation == metal.albedo());
								}
							});
}

void check_glass(Suite& suite) {
	suite.check("Glass: never absorbs nor tints", [](const Glass& glass, const Incidence& incidence, Sampler sampler) {
		const auto scattered = glass.scatter(incidence.ray, incidence.record, sampler);
		RC_ASSERT(scattered.has_value());
		RC_ASSERT(scattered->attenuation == Colour::white());
	});

	suite.check("Glass with ior 1 lets light through unchanged", [](const Incidence& incidence, Sampler sampler) {
		const Glass glass{*material_name(), 1.0};
		const auto scattered = glass.scatter(incidence.ray, incidence.record, sampler);
		RC_ASSERT(approx_equal(scattered->ray.direction(), incidence.ray.direction()));
	});

	suite.check("Glass: light is either mirror-reflected or refracted by Snell's law",
	            [](const Glass& glass, const Incidence& incidence, Sampler sampler) {
								RC_PRE(incidence_cosine(incidence) > 1e-6);
								const auto scattered = glass.scatter(incidence.ray, incidence.record, sampler);
								const Vec3& normal = incidence.record.normal;
								const Vec3& direction = scattered->ray.direction();
								if (dot(direction, normal) > 0.0) {
									RC_ASSERT(approx_equal(direction, mirrored(incidence)));
									return;
								}
								// Snell's law, n₁ sin θ₁ = n₂ sin θ₂, in the plane of incidence.
								const double ratio = incidence.record.front_face ? 1.0 / glass.ior() : glass.ior();
								RC_ASSERT(approx_equal(ratio * sine(incidence.ray.direction(), normal), sine(direction, normal)));
								RC_ASSERT(approx_equal(dot(cross(incidence.ray.direction(), normal), direction), 0.0));
							});

	suite.check("Glass: beyond the critical angle, light from the inside is always reflected", [](Sampler sampler) {
		const Glass glass{*material_name(), 1.0 + (2.0 * *positive_scalar() / coordinate_bound<double>)};
		const Incidence incidence = beyond_critical_angle(glass.ior());
		for (int i = 0; i < 64; ++i) {
			const auto scattered = glass.scatter(incidence.ray, incidence.record, sampler);
			RC_ASSERT(approx_equal(scattered->ray.direction(), mirrored(incidence)));
		}
	});

	suite.check("Glass: otherwise light is reflected with Schlick's probability",
	            [](const Glass& glass, const Incidence& incidence, Sampler sampler) {
								const double ratio = incidence.record.front_face ? 1.0 / glass.ior() : glass.ior();
								const double cosine = incidence_cosine(incidence);
								RC_PRE(ratio * std::sqrt(1.0 - (cosine * cosine)) < 1.0);
								int reflections = 0;
								for (int i = 0; i < draws; ++i) {
									const auto scattered = glass.scatter(incidence.ray, incidence.record, sampler);
									reflections += dot(scattered->ray.direction(), incidence.record.normal) > 0.0 ? 1 : 0;
								}
								RC_ASSERT(frequency_is_close(reflections, SpecularCoat::schlick(cosine, ratio)));
							});
}

void check_plastic(Suite& suite) {
	suite.check("Plastic: either an untinted mirror reflection or Lambertian scattering tinted by the albedo",
	            [](const Plastic& plastic, const Incidence& incidence, Sampler sampler) {
								const auto scattered = plastic.scatter(incidence.ray, incidence.record, sampler);
								RC_ASSERT(scattered.has_value());
								const bool mirror = scattered->attenuation == Colour::white() &&
								                    approx_equal(scattered->ray.direction(), mirrored(incidence));
								const bool diffuse = scattered->attenuation == plastic.albedo() &&
								                     dot(scattered->ray.direction(), incidence.record.normal) >= 0.0;
								RC_ASSERT(mirror || diffuse);
							});

	suite.check("Plastic: the coat reflects with Schlick's probability", [](const Plastic& plastic,
	                                                                        const Incidence& incidence, Sampler sampler) {
		int reflections = 0;
		for (int i = 0; i < draws; ++i) {
			const auto scattered = plastic.scatter(incidence.ray, incidence.record, sampler);
			// A diffuse direction coincides with the mirror one with probability 0.
			reflections += approx_equal(scattered->ray.direction(), mirrored(incidence), 1e-12) ? 1 : 0;
		}
		RC_ASSERT(frequency_is_close(reflections, SpecularCoat::schlick(incidence_cosine(incidence), plastic.ior())));
	});
}

void check_specular_coat(Suite& suite) {
	suite.check("schlick lies in [0, 1]", [] {
		const double value = SpecularCoat::schlick(*unit_channel(), *refractive_index());
		RC_ASSERT(0.0 <= value);
		RC_ASSERT(value <= 1.0);
	});

	suite.check("schlick grows as cos θ falls", [] {
		const double ior = *refractive_index();
		const auto [lower, higher] = std::minmax({*unit_channel(), *unit_channel()});
		RC_ASSERT(SpecularCoat::schlick(lower, ior) >= SpecularCoat::schlick(higher, ior));
	});

	suite.check("schlick is the same from either side of the boundary", [] {
		const double cosine = *unit_channel();
		const double ior = *refractive_index();
		RC_ASSERT(approx_equal(SpecularCoat::schlick(cosine, ior), SpecularCoat::schlick(cosine, 1.0 / ior)));
	});

	suite.check("reflect keeps the length and reverses the normal component", [](const Vec3& direction) {
		const Vec3 normal = *unit_vector<double, 3>();
		const Vec3 reflected = SpecularCoat::reflect(direction, normal);
		const double scale = direction.length();
		RC_ASSERT(approx_equal(reflected.length(), scale, tolerance(scale)));
		RC_ASSERT(approx_equal(dot(reflected, normal), -dot(direction, normal), tolerance(scale)));
		RC_ASSERT(approx_equal(SpecularCoat::reflect(reflected, normal), direction, tolerance(scale)));
	});

	suite.check("the coat's reflectance is Schlick's for its index", [] {
		const double ior = *refractive_index();
		const double cosine = *unit_channel();
		RC_ASSERT(SpecularCoat{ior}.reflectance(cosine) == SpecularCoat::schlick(cosine, ior));
	});
}

void check_validation(Suite& suite) {
	suite.check("an empty name throws Error", [] {
		RC_ASSERT_THROWS_AS(Lambertian("", Colour::white()), Error);
		RC_ASSERT_THROWS_AS(Metal("", Colour::white(), 0.0), Error);
		RC_ASSERT_THROWS_AS(Glass("", 1.5), Error);
		RC_ASSERT_THROWS_AS(Plastic("", Colour::white()), Error);
	});

	suite.check("an albedo component above 1 throws MathsError", [] {
		const Colour albedo = *excessive_albedo();
		RC_ASSERT_THROWS_AS(Lambertian("bright", albedo), MathsError);
		RC_ASSERT_THROWS_AS(Metal("bright", albedo, 0.0), MathsError);
		RC_ASSERT_THROWS_AS(Plastic("bright", albedo), MathsError);
	});

	suite.check("fuzz outside [0, 1] throws MathsError",
	            [] { RC_ASSERT_THROWS_AS(Metal("brushed", Colour::white(), *invalid_fuzz()), MathsError); });

	suite.check("a refractive index that is not finite and positive throws MathsError", [] {
		const double ior = *not_finite_positive();
		RC_ASSERT_THROWS_AS(Glass("clear", ior), MathsError);
		RC_ASSERT_THROWS_AS(Plastic("glossy", Colour::white(), ior), MathsError);
		RC_ASSERT_THROWS_AS(SpecularCoat{ior}, MathsError);
	});
}

void check_examples(Suite& suite) {
	suite.check("characteristic values of schlick", [] {
		RC_ASSERT(approx_equal(SpecularCoat::schlick(1.0, 1.5), 0.04));
		RC_ASSERT(SpecularCoat::schlick(0.0, 1.5) == 1.0);
		RC_ASSERT(SpecularCoat::schlick(1.0, 1.0) == 0.0);
		RC_ASSERT(SpecularCoat::schlick(0.0, 1.0) == 0.0);
	});

	suite.check("a polished metal mirrors light", [] {
		Sampler sampler{0};
		const Ray ray{Point3{-1, 1, 0}, Vec3{1, -1, 0}};
		const auto record = HitRecord::from_outward_normal(ray, std::numbers::sqrt2, Vec3::unit_y(), nullptr);
		const auto scattered = Metal{"silver", Colour{0.9, 0.9, 0.9}, 0.0}.scatter(ray, record, sampler);
		RC_ASSERT(scattered.has_value());
		RC_ASSERT(approx_equal(scattered->ray.direction(), Vec3{1, 1, 0}.normalised()));
		RC_ASSERT(approx_equal(scattered->ray.origin(), Point3{}));
		RC_ASSERT(scattered->attenuation == Colour(0.9, 0.9, 0.9));
	});

	suite.check("light leaving glass at 60° is totally reflected", [] {
		Sampler sampler{0};
		// Inside, heading out through the top at 60° to the normal; sin 60° · 1.5 ≈ 1.3 > 1.
		const Ray ray{Point3{}, Vec3{std::numbers::sqrt3, 1, 0}};
		const auto record = HitRecord::from_outward_normal(ray, 1.0, Vec3::unit_y(), nullptr);
		RC_ASSERT(!record.front_face);
		const auto scattered = Glass{"glass", 1.5}.scatter(ray, record, sampler);
		RC_ASSERT(approx_equal(scattered->ray.direction(), Vec3{std::numbers::sqrt3, -1, 0}.normalised()));
	});

	suite.check("plastic defaults to an index of 1.5", [] {
		RC_ASSERT(Plastic("red", Colour{0.7, 0.1, 0.1}).ior() == 1.5);
		RC_ASSERT(Plastic::default_ior == 1.5);
	});

	suite.check("materials are used through their interfaces", [] {
		const std::vector<std::shared_ptr<const Material>> materials{
				std::make_shared<const Lambertian>("ground", Colour{0.8, 0.8, 0.0}),
				std::make_shared<const Plastic>("red", Colour{0.7, 0.1, 0.1}),
				std::make_shared<const Glass>("glass", 1.5),
				std::make_shared<const Metal>("gold", Colour{0.8, 0.6, 0.2}, 0.1),
		};
		const std::vector<std::string> names{"ground", "red", "glass", "gold"};
		for (const auto& [material, name] : std::views::zip(materials, names)) {
			const INamed& named = *material;
			RC_ASSERT(named.name() == name);
		}
	});
}

} // namespace
} // namespace rt::test

int main() {
	rt::test::Suite suite;
	rt::test::check_examples(suite);
	rt::test::check_common<rt::Lambertian>(suite, "Lambertian");
	rt::test::check_common<rt::Metal>(suite, "Metal");
	rt::test::check_common<rt::Glass>(suite, "Glass");
	rt::test::check_common<rt::Plastic>(suite, "Plastic");
	rt::test::check_lambertian(suite);
	rt::test::check_metal(suite);
	rt::test::check_glass(suite);
	rt::test::check_plastic(suite);
	rt::test::check_specular_coat(suite);
	rt::test::check_validation(suite);
	return suite.exit_code();
}

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
