#ifndef RT_MATHS_RAY_HPP
#define RT_MATHS_RAY_HPP

#include "rt/maths/vector.hpp"

#include <ostream>

namespace rt {

/// Half-line `origin + t · direction`, with the direction always of unit length.
///
/// Origin and direction are finite by the invariant of `Vector`.
class Ray {
public:
	/// Ray from `origin` along `direction`; the direction need not be normalised.
	///
	/// @throws MathsError if `direction` is the zero vector.
	constexpr Ray(const Point3& origin, const Vec3& direction) : origin_{origin}, direction_{direction.normalised()} {}

	[[nodiscard]] constexpr const Point3& origin() const noexcept { return origin_; }

	/// Unit vector.
	[[nodiscard]] constexpr const Vec3& direction() const noexcept { return direction_; }

	/// The point at distance `t` from the origin (behind it for negative `t`).
	///
	/// @throws MathsError if `t` is infinite or NaN, or the point lies beyond the range of `double`.
	[[nodiscard]] constexpr Point3 at(double t) const { return origin_ + direction_ * t; }

	friend std::ostream& operator<<(std::ostream& stream, const Ray& ray) {
		return stream << "Ray{origin: " << ray.origin_ << ", direction: " << ray.direction_ << '}';
	}

private:
	Point3 origin_;
	Vec3 direction_;
};

} // namespace rt

#endif // RT_MATHS_RAY_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
