#ifndef RT_GEOMETRY_SPHERE_HPP
#define RT_GEOMETRY_SPHERE_HPP

#include "rt/errors.hpp"
#include "rt/geometry/hit_record.hpp"
#include "rt/geometry/hittable.hpp"
#include "rt/maths/interval.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <optional>
#include <utility>

namespace rt {

class Material;

/// A sphere with a finite, positive radius (README.md § 7.2).
class Sphere final : public IHittable {
public:
	/// @throws MathsError if `radius` is not a finite, positive number.
	Sphere(const Point3& centre, double radius, std::shared_ptr<const Material> material)
			: centre_{centre}, radius_{validated_radius(radius)}, material_{std::move(material)} {
		constructed.fetch_add(1, std::memory_order_relaxed);
	}

	[[nodiscard]] const Point3& centre() const noexcept { return centre_; }
	[[nodiscard]] double radius() const noexcept { return radius_; }

	/// Shared with every other user of the material (aggregation: the material outlives none of them in particular).
	[[nodiscard]] const std::shared_ptr<const Material>& material() const noexcept { return material_; }

	/// Number of spheres constructed so far by the constructor above (copies are not counted).
	[[nodiscard]] static std::size_t instance_count() noexcept { return constructed.load(std::memory_order_relaxed); }

	/// The nearer intersection with `t` in `range`, or the farther one if only that lies in `range`.
	///
	/// @throws MathsError if the computation overflows, which takes coordinates or a radius beyond about 10¹⁵⁰.
	[[nodiscard]] std::optional<HitRecord> hit(const Ray& ray, Interval<double> range) const override {
		// The direction has unit length, so the roots of |o + t·d − c|² = r² are t = h ± √(r² − p²), where h is
		// the parameter of the point on the ray closest to the centre and p the distance between the two. Both
		// differences of squares are evaluated as products, (a − b)(a + b), avoiding the cancellation that would
		// cost the classic formula its precision for large spheres such as a ground plane (README.md § 2).
		const Vec3 to_centre = centre_ - ray.origin();
		const double closest = dot(ray.direction(), to_centre);
		const double miss_distance = (to_centre - (ray.direction() * closest)).length();
		const double discriminant = (radius_ - miss_distance) * (radius_ + miss_distance);
		if (discriminant < 0.0) {
			return std::nullopt;
		}

		// Of the two roots, the one farther from zero is computed directly and the nearer one from their product,
		// (|o − c| − r)(|o − c| + r): subtracting two close numbers would lose the nearer root's precision.
		const double half_chord = std::sqrt(discriminant);
		const double far_root = closest + std::copysign(half_chord, closest);
		const double distance = to_centre.length();
		const double product = (distance - radius_) * (distance + radius_);
		const double near_root = far_root == 0.0 ? 0.0 : product / far_root;
		const auto [first, second] = std::minmax(near_root, far_root);

		for (const double t : {first, second}) {
			if (range.contains(t)) {
				return HitRecord::from_outward_normal(ray, t, ray.at(t) - centre_, material_.get());
			}
		}
		return std::nullopt;
	}

private:
	static inline std::atomic<std::size_t> constructed{0};

	Point3 centre_;
	double radius_;
	std::shared_ptr<const Material> material_;

	[[nodiscard]] static double validated_radius(double radius) {
		if (!std::isfinite(radius) || radius <= 0.0) {
			throw MathsError{"the radius of a sphere must be finite and positive"};
		}
		return radius;
	}
};

} // namespace rt

#endif // RT_GEOMETRY_SPHERE_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
