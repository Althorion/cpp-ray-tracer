#ifndef RT_MATERIALS_METAL_HPP
#define RT_MATERIALS_METAL_HPP

#include "rt/colour.hpp"
#include "rt/errors.hpp"
#include "rt/geometry/hit_record.hpp"
#include "rt/materials/material.hpp"
#include "rt/materials/specular_coat.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"
#include "rt/random/sampler.hpp"

#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace rt {

/// A mirror, polished (`fuzz = 0`) or brushed (`fuzz` up to 1), tinted by its albedo (README.md § 6.3).
class Metal final : public Material {
public:
	/// @throws Error if `name` is empty.
	/// @throws MathsError if a component of `albedo` exceeds 1, or `fuzz` is not in [0, 1].
	Metal(std::string name, const Colour& albedo, double fuzz)
			: Material{std::move(name)}, albedo_{validated_albedo(albedo)}, fuzz_{validated_fuzz(fuzz)} {}

	[[nodiscard]] const Colour& albedo() const noexcept { return albedo_; }
	[[nodiscard]] double fuzz() const noexcept { return fuzz_; }

	/// The mirror reflection, displaced by up to `fuzz` in a random direction; absorbed if that leaves it pointing
	/// into (or along) the surface.
	[[nodiscard]] std::optional<Scatter> scatter(const Ray& ray, const HitRecord& record,
	                                             Sampler& sampler) const override {
		const Vec3 reflected = SpecularCoat::reflect(ray.direction(), record.normal);
		const Vec3 direction = fuzz_ == 0.0 ? reflected : reflected + (sampler.unit_vector() * fuzz_);
		// Also covers a zero `direction`, which no ray could take.
		if (dot(direction, record.normal) <= 0.0) {
			return std::nullopt;
		}
		return Scatter{.ray = Ray{record.point, direction}, .attenuation = albedo_};
	}

private:
	Colour albedo_;
	double fuzz_;

	[[nodiscard]] static double validated_fuzz(double fuzz) {
		if (std::isnan(fuzz) || fuzz < 0.0 || fuzz > 1.0) {
			throw MathsError{"the fuzz of a metal must lie in [0, 1]"};
		}
		return fuzz;
	}
};

} // namespace rt

#endif // RT_MATERIALS_METAL_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
