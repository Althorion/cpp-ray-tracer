#ifndef RT_MATH_RAY_HPP
#define RT_MATH_RAY_HPP

#include "rt/math/vector.hpp"

#include <ostream>

namespace rt {

/// Half-line `origin + t · direction`, with the direction always of unit length.
class Ray {
public:
	/// Ray from `origin` along `direction`; the direction need not be normalised.
	///
	/// @throws MathError if `direction` is the zero vector.
	constexpr Ray(const Point3& origin, const Vec3& direction) : origin_{origin}, direction_{direction.normalised()} {}

	[[nodiscard]] constexpr const Point3& origin() const noexcept { return origin_; }

	/// Unit vector.
	[[nodiscard]] constexpr const Vec3& direction() const noexcept { return direction_; }

	/// The point at distance `t` from the origin (behind it for negative `t`).
	[[nodiscard]] constexpr Point3 at(double t) const noexcept { return origin_ + direction_ * t; }

	friend std::ostream& operator<<(std::ostream& stream, const Ray& ray) {
		return stream << "Ray{origin: " << ray.origin_ << ", direction: " << ray.direction_ << '}';
	}

private:
	Point3 origin_;
	Vec3 direction_;
};

} // namespace rt

#endif // RT_MATH_RAY_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
