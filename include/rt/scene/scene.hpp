#ifndef RT_SCENE_SCENE_HPP
#define RT_SCENE_SCENE_HPP

#include "rt/colour.hpp"
#include "rt/errors.hpp"
#include "rt/geometry/hit_record.hpp"
#include "rt/geometry/hittable.hpp"
#include "rt/materials/material.hpp"
#include "rt/maths/interval.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"
#include "rt/scene/camera.hpp"
#include "rt/scene/registry.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace rt {

/// The background light: a vertical gradient from `bottom`, straight down, to `top`, straight up (README.md § 6.2).
struct Sky {
	Colour top{0.5, 0.7, 1.0};
	Colour bottom = Colour::white();

	/// `lerp(bottom, top, (ŷ + 1) / 2)` for the unit vector `ŷ` along `direction`.
	///
	/// @throws MathsError if `direction` is zero.
	[[nodiscard]] Colour colour(const Vec3& direction) const {
		// Clamped, because rounding can leave a normalised component a hair outside [−1, 1], and a negative weight
		// would make a negative colour.
		const double weight = std::clamp(0.5 * (direction.normalised()[1] + 1.0), 0.0, 1.0);
		return (bottom * (1.0 - weight)) + (top * weight);
	}

	[[nodiscard]] friend bool operator==(const Sky&, const Sky&) noexcept = default;
};

/// Everything to be rendered (README.md § 7.2): it owns the camera, the sky and the objects (composition), and keeps
/// the materials, which the objects share, in a registry.
///
/// Move-only: copying would have to clone every object.
class Scene {
public:
	explicit Scene(const Camera& camera, const Sky& sky = {}) : camera_{camera}, sky_{sky} {}

	Scene(const Scene&) = delete ("a scene owns its objects, which cannot be copied; move the scene instead");
	Scene& operator=(const Scene&) = delete ("a scene owns its objects, which cannot be copied; move the scene instead");
	Scene(Scene&&) noexcept = default;
	Scene& operator=(Scene&&) noexcept = default;
	~Scene() = default;

	[[nodiscard]] const Camera& camera() const noexcept { return camera_; }
	[[nodiscard]] const Sky& sky() const noexcept { return sky_; }
	[[nodiscard]] const Registry<Material>& materials() const noexcept { return materials_; }

	/// @throws Error if `material` is null.
	/// @throws DuplicateNameError if a material of the same name is already in the scene.
	void add_material(const std::shared_ptr<Material>& material) { materials_.add(material); }

	/// @throws Error if `object` is null.
	void add_object(std::unique_ptr<IHittable> object) {
		if (object == nullptr) {
			throw Error{"cannot add a null object to a scene"};
		}
		objects_.push_back(std::move(object));
	}

	[[nodiscard]] std::size_t object_count() const noexcept { return objects_.size(); }

	/// The nearest hit with `t` in `range` among all objects, found by a linear search.
	[[nodiscard]] std::optional<HitRecord> hit(const Ray& ray, Interval<double> range) const {
		std::optional<HitRecord> nearest;
		for (const auto& object : objects_) {
			if (auto record = object->hit(ray, range)) {
				// Only something nearer can still matter.
				range = Interval{range.min(), record->t};
				nearest = record;
			}
		}
		return nearest;
	}

	/// The colour of the sky seen in `direction`.
	///
	/// @throws MathsError if `direction` is zero.
	[[nodiscard]] Colour sky_colour(const Vec3& direction) const { return sky_.colour(direction); }

private:
	Camera camera_;
	Sky sky_;
	Registry<Material> materials_;
	std::vector<std::unique_ptr<IHittable>> objects_;
};

} // namespace rt

#endif // RT_SCENE_SCENE_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
