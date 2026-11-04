#ifndef RT_MATERIALS_LAMBERTIAN_HPP
#define RT_MATERIALS_LAMBERTIAN_HPP

#include "rt/colour.hpp"
#include "rt/geometry/hit_record.hpp"
#include "rt/materials/material.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"
#include "rt/random/sampler.hpp"

#include <optional>
#include <string>
#include <utility>

namespace rt {

/// An ideally matte surface, scattering light in all directions with the cosine-weighted distribution of Lambert's
/// law (README.md § 6.3).
///
/// Not `final`: `Plastic` is a Lambertian surface under a specular coat.
class Lambertian : public Material {
public:
	/// @throws Error if `name` is empty.
	/// @throws MathsError if a component of `albedo` exceeds 1.
	Lambertian(std::string name, const Colour& albedo) : Material{std::move(name)}, albedo_{validated_albedo(albedo)} {}

	[[nodiscard]] const Colour& albedo() const noexcept { return albedo_; }

	/// Never absorbs; the new direction is `normal + a random unit vector`, which is cosine-distributed about the
	/// normal, or the normal itself when that sum (nearly) vanishes.
	[[nodiscard]] std::optional<Scatter> scatter(const Ray& /*ray*/, const HitRecord& record,
	                                             Sampler& sampler) const override {
		// Below this length the sum would be dominated by rounding errors, and its direction would be arbitrary.
		constexpr double degenerate_length_squared = 1e-12;
		const Vec3 direction = record.normal + sampler.unit_vector();
		return Scatter{
				.ray = Ray{record.point, direction.length_squared() < degenerate_length_squared ? record.normal : direction},
				.attenuation = albedo_,
		};
	}

private:
	Colour albedo_;
};

} // namespace rt

#endif // RT_MATERIALS_LAMBERTIAN_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
