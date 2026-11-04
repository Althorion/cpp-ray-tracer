#ifndef RT_MATERIALS_GLASS_HPP
#define RT_MATERIALS_GLASS_HPP

#include "rt/colour.hpp"
#include "rt/geometry/hit_record.hpp"
#include "rt/materials/material.hpp"
#include "rt/materials/specular_coat.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"
#include "rt/random/sampler.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace rt {

/// A clear dielectric such as glass or water, surrounded by air (README.md § 5.2, § 6.3).
class Glass final : public Material {
public:
	/// @throws Error if `name` is empty.
	/// @throws MathsError unless `ior` is finite and positive.
	Glass(std::string name, double ior) : Material{std::move(name)}, ior_{SpecularCoat::validated_ior(ior)} {}

	[[nodiscard]] double ior() const noexcept { return ior_; }

	/// Never absorbs and never tints: the light is either refracted (Snell's law) or mirror-reflected, the latter
	/// always under total internal reflection and otherwise with Schlick's probability.
	[[nodiscard]] std::optional<Scatter> scatter(const Ray& ray, const HitRecord& record,
	                                             Sampler& sampler) const override {
		// Entering the sphere, light goes from air into the glass; leaving it, the other way round.
		const double ratio = record.front_face ? 1.0 / ior_ : ior_;
		const double cos_theta = std::min(-dot(ray.direction(), record.normal), 1.0);
		const double sin_theta = std::sqrt(1.0 - (cos_theta * cos_theta));

		const bool total_internal_reflection = ratio * sin_theta > 1.0;
		const Vec3 direction = total_internal_reflection || sampler.uniform() < SpecularCoat::schlick(cos_theta, ratio)
		                           ? SpecularCoat::reflect(ray.direction(), record.normal)
		                           : refract(ray.direction(), record.normal, cos_theta, ratio);
		return Scatter{.ray = Ray{record.point, direction}, .attenuation = Colour::white()};
	}

private:
	double ior_;

	/// The direction of unit `direction` after crossing a boundary with unit `normal` (facing against `direction`),
	/// `cos_theta = −dot(direction, normal)`, into a medium `ratio` times less refractive. Requires
	/// `ratio · sin θ ≤ 1`.
	[[nodiscard]] static Vec3 refract(const Vec3& direction, const Vec3& normal, double cos_theta, double ratio) {
		// The component along the surface scales with the ratio; the normal component makes the result unit length.
		const Vec3 tangential = (direction + (normal * cos_theta)) * ratio;
		const double normal_part = std::sqrt(std::abs(1.0 - tangential.length_squared()));
		return tangential - (normal * normal_part);
	}
};

} // namespace rt

#endif // RT_MATERIALS_GLASS_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
