#ifndef RT_MATERIALS_MATERIAL_HPP
#define RT_MATERIALS_MATERIAL_HPP

#include "rt/colour.hpp"
#include "rt/errors.hpp"
#include "rt/geometry/hit_record.hpp"
#include "rt/maths/ray.hpp"
#include "rt/random/sampler.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace rt {

/// The outcome of light scattering off a surface (README.md § 7.2): the ray it continues along, and the fraction
/// of each colour channel that survives.
struct Scatter {
	Ray ray;
	Colour attenuation;
};

/// Anything that scatters incoming light (README.md § 7.2).
class IScatterer {
public:
	virtual ~IScatterer() = default;

	/// How `ray`, having hit a surface as described by `record`, continues; nothing if the light is absorbed.
	///
	/// Random choices are drawn from `sampler`, so that the result depends only on its stream (README.md § 6.5).
	[[nodiscard]] virtual std::optional<Scatter> scatter(const Ray& ray, const HitRecord& record,
	                                                     Sampler& sampler) const = 0;
};

/// Anything with a name, e.g. to be found by it in a `Registry` (README.md § 7.2).
class INamed {
public:
	virtual ~INamed() = default;

	[[nodiscard]] virtual std::string_view name() const = 0;
};

/// A named surface material (README.md § 6.3, § 7.2); concrete materials implement `scatter`.
///
/// Materials are shared between the spheres that use them (`std::shared_ptr<const Material>`) and are used only
/// through pointers, so they are not copied.
class Material : public IScatterer, public INamed {
public:
	[[nodiscard]] std::string_view name() const noexcept override { return name_; }

protected:
	/// @throws Error if `name` is empty.
	explicit Material(std::string name) : name_{validated_name(std::move(name))} {}

	/// A reflectance, i.e. a colour with no component above 1 (README.md § 5.2), so that no surface creates light.
	///
	/// @throws MathsError if a component of `albedo` exceeds 1.
	[[nodiscard]] static Colour validated_albedo(const Colour& albedo) {
		if (albedo.r() > 1.0 || albedo.g() > 1.0 || albedo.b() > 1.0) {
			throw MathsError{"albedo components must not exceed 1"};
		}
		return albedo;
	}

private:
	std::string name_;

	/// A safety net only: names come from scene files, whose parser must already reject empty ones (README.md § 5.1
	/// requires an identifier, which has at least one character), and report them with a position in the file.
	[[nodiscard]] static std::string validated_name(std::string name) {
		if (name.empty()) {
			throw Error{"a material must have a name"};
		}
		return name;
	}
};

} // namespace rt

#endif // RT_MATERIALS_MATERIAL_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
