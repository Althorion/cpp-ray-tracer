#ifndef RT_MATERIALS_SPECULAR_COAT_HPP
#define RT_MATERIALS_SPECULAR_COAT_HPP

#include "rt/errors.hpp"
#include "rt/maths/vector.hpp"
#include "rt/validation.hpp"

#include <algorithm>
#include <cmath>

namespace rt {

/// A clear, mirror-like coating with a refractive index (README.md § 5.2, § 7.2).
///
/// A mixin, not a material: it supplies the reflection maths to the materials built on it, `Plastic` (by private
/// inheritance) and `Glass` (through the static members).
class SpecularCoat {
public:
	/// @throws MathsError unless `ior` is finite and positive.
	explicit SpecularCoat(double ior) : ior_{validation::validated<MathsError>(ior, validation::check_ior)} {}

	[[nodiscard]] double ior() const noexcept { return ior_; }

	/// Probability that light arriving at angle θ to the normal is reflected by the coat.
	[[nodiscard]] double reflectance(double cos_theta) const noexcept { return schlick(cos_theta, ior_); }

	/// Schlick's approximation of the Fresnel reflectance for light arriving at angle θ to the normal of a boundary
	/// between media whose refractive indices have the ratio `ior` (README.md § 5.2); it lies in [0, 1].
	///
	/// `ior = 1` means no boundary at all and is special-cased to 0: Schlick's formula would give
	/// (1 − cos θ)⁵ there, whereas the exact Fresnel reflectance is 0 for `ior = 1` and tends to 1 at grazing angles
	/// for every other `ior` — the discontinuity is physical. `cos_theta` is clamped to [0, 1].
	[[nodiscard]] static double schlick(double cos_theta, double ior) noexcept {
		if (ior == 1.0) {
			return 0.0;
		}
		const double r0_root = (1.0 - ior) / (1.0 + ior);
		const double r0 = r0_root * r0_root;
		const double complement = 1.0 - std::clamp(cos_theta, 0.0, 1.0);
		const double complement_squared = complement * complement;
		// The exact value never exceeds 1, but the rounded sum r0 + (1 − r0) might.
		return std::min(1.0, r0 + ((1.0 - r0) * complement_squared * complement_squared * complement));
	}

	/// Mirror image of `direction` in the plane with unit normal `normal`: the angle of reflection equals the
	/// angle of incidence, and the length is that of `direction`.
	[[nodiscard]] static Vec3 reflect(const Vec3& direction, const Vec3& normal) {
		return direction - (normal * (2.0 * dot(direction, normal)));
	}

private:
	double ior_;
};

} // namespace rt

#endif // RT_MATERIALS_SPECULAR_COAT_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
