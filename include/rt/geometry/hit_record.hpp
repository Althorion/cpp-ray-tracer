#ifndef RT_GEOMETRY_HIT_RECORD_HPP
#define RT_GEOMETRY_HIT_RECORD_HPP

#include "rt/maths/point.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"

#include <functional>

namespace rt {

class Material;

/// Where and how a ray meets a surface (README.md § 7.2).
///
/// A plain record: its fields are public, so their consistency rests with whoever fills them in. Build it with
/// `from_outward_normal`, which establishes the conventions below.
struct HitRecord {
	/// The point of intersection, `ray.at(t)`.
	Point3 point;
	/// Unit normal facing against the ray: `dot(normal, ray.direction()) <= 0`.
	Vec3 normal;
	/// Ray parameter of the intersection.
	double t = 0.0;
	/// Whether the ray arrived from the outside of the surface, i.e. against its outward normal.
	bool front_face = false;
	/// Material of the surface, not owned; it outlives the record (the scene keeps materials alive).
	///
	/// A reference, since every surface has a material and a reference cannot be null; wrapped, since a plain
	/// reference member would make the record unassignable, and `Scene::hit` reassigns the nearest record. Not
	/// `std::experimental::observer_ptr` (Library Fundamentals TS v2): it never made it into the standard, and it
	/// is nullable like a raw pointer, so it would document that the record does not own the material without
	/// ruling out a missing one.
	std::reference_wrapper<const Material> material;

	/// The record of `ray` meeting a surface at `t`, whose outward normal there is `outward_normal`.
	///
	/// The normal is normalised and, when the ray comes from the inside, flipped so that it faces the ray. A ray
	/// grazing the surface (perpendicular to the normal) counts as coming from the inside.
	///
	/// @throws MathsError if `t` is not finite, `ray.at(t)` overflows, or `outward_normal` is zero.
	[[nodiscard]] static constexpr HitRecord from_outward_normal(const Ray& ray, double t, const Vec3& outward_normal,
	                                                             const Material& material) {
		const Vec3 unit_normal = outward_normal.normalised();
		const bool front_face = dot(ray.direction(), unit_normal) < 0.0;
		return HitRecord{
				.point = ray.at(t),
				.normal = front_face ? unit_normal : -unit_normal,
				.t = t,
				.front_face = front_face,
				.material = material,
		};
	}
};

} // namespace rt

#endif // RT_GEOMETRY_HIT_RECORD_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
