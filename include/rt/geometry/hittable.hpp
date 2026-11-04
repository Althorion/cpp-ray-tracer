#ifndef RT_GEOMETRY_HITTABLE_HPP
#define RT_GEOMETRY_HITTABLE_HPP

#include "rt/geometry/hit_record.hpp"
#include "rt/maths/interval.hpp"
#include "rt/maths/ray.hpp"

#include <optional>

namespace rt {

/// Anything a ray can hit (README.md § 7.2); the scene holds its objects through this interface.
class IHittable {
public:
	virtual ~IHittable() = default;

	/// The nearest intersection of `ray` with the object whose `t` lies in `range`, or nothing if there is none.
	///
	/// Implementations fill the record through `HitRecord::from_outward_normal` and never return a `t` outside
	/// `range`.
	[[nodiscard]] virtual std::optional<HitRecord> hit(const Ray& ray, Interval<double> range) const = 0;
};

} // namespace rt

#endif // RT_GEOMETRY_HITTABLE_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
