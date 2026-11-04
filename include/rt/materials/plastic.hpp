#ifndef RT_MATERIALS_PLASTIC_HPP
#define RT_MATERIALS_PLASTIC_HPP

#include "rt/colour.hpp"
#include "rt/geometry/hit_record.hpp"
#include "rt/materials/lambertian.hpp"
#include "rt/materials/material.hpp"
#include "rt/materials/specular_coat.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"
#include "rt/random/sampler.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace rt {

/// A matte surface under a clear, glossy coat, such as plastic or varnish (README.md § 5.2, § 6.3).
///
/// Multiple inheritance of implementation: it *is* a Lambertian surface, and is *implemented in terms of* a
/// specular coat, which is why that base is private.
///
/// clang-tidy discourages inheriting from more than one class with an implementation; here it is deliberate, as
/// README.md § 12 requires multiple inheritance of implementation, and `SpecularCoat` has no virtual functions nor
/// any base, so no diamond or ambiguity can arise.
// NOLINTNEXTLINE(misc-multiple-inheritance): required by the specification, see above
class Plastic final : public Lambertian, private SpecularCoat {
public:
	static constexpr double default_ior = 1.5;

	/// @throws Error if `name` is empty.
	/// @throws MathsError if a component of `albedo` exceeds 1, or `ior` is not finite and positive.
	Plastic(std::string name, const Colour& albedo, double ior = default_ior)
			: Lambertian{std::move(name), albedo}, SpecularCoat{ior} {}

	using SpecularCoat::ior;

	/// A mirror reflection off the coat with Schlick's probability, untinted; otherwise the light passes the coat
	/// and is scattered by the Lambertian base beneath.
	[[nodiscard]] std::optional<Scatter> scatter(const Ray& ray, const HitRecord& record,
	                                             Sampler& sampler) const override {
		const double cos_theta = std::min(-dot(ray.direction(), record.normal), 1.0);
		if (sampler.uniform() < reflectance(cos_theta)) {
			return Scatter{
					.ray = Ray{record.point, reflect(ray.direction(), record.normal)},
					.attenuation = Colour::white(),
			};
		}
		return Lambertian::scatter(ray, record, sampler);
	}
};

} // namespace rt

#endif // RT_MATERIALS_PLASTIC_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
