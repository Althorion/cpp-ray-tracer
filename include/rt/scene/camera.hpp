#ifndef RT_SCENE_CAMERA_HPP
#define RT_SCENE_CAMERA_HPP

#include "rt/errors.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"
#include "rt/random/sampler.hpp"

#include <cmath>
#include <cstddef>
#include <format>
#include <numbers>
#include <optional>

namespace rt {

/// Parameters of the `camera` directive (README.md § 5.2), unvalidated; `Camera` checks them.
struct CameraSettings {
	Point3 from;
	Point3 to;
	Vec3 up = Vec3::unit_y();
	/// Vertical field of view, in degrees.
	double fov = 90.0;
	/// Diameter of the lens; 0 for a pinhole camera.
	double aperture = 0.0;
	/// Distance of the plane in focus; `‖to − from‖` when absent.
	std::optional<double> focus = std::nullopt;

	[[nodiscard]] friend bool operator==(const CameraSettings&, const CameraSettings&) = default;
};

/// Size of the image in pixels; it comes from the command line, not from the scene, so the camera takes it per ray.
struct Resolution {
	std::size_t width = 0;
	std::size_t height = 0;

	[[nodiscard]] friend constexpr bool operator==(const Resolution&, const Resolution&) noexcept = default;
};

/// A thin-lens camera (README.md § 6.1).
///
/// It looks from `from` towards `to`, with `u` pointing right, `v` up and `w` backwards (`u × v = w`). Every ray
/// starts on the lens, a disc of diameter `aperture` around `from` perpendicular to `w`, and passes through a point
/// of the pixel's footprint on the plane in focus, `focus` ahead of `from`.
class Camera {
public:
	/// @throws SceneError unless `from ≠ to`, `up` is non-zero and not parallel to `to − from`, `fov ∈ (0, 180)`,
	/// `aperture ≥ 0` and `focus > 0`, all finite, and unless the derived geometry stays finite as well.
	explicit Camera(const CameraSettings& settings) : settings_{settings} {
		validate(settings);
		try {
			const Vec3 backwards = settings.from - settings.to;
			w_ = backwards.normalised();
			const Vec3 right = cross(settings.up.normalised(), w_);
			if (right.length() < min_sine) {
				throw SceneError{"the camera's up vector must not be parallel to its viewing direction"};
			}
			u_ = right.normalised();
			v_ = cross(w_, u_);
			focus_ = settings.focus.value_or(backwards.length());
			focal_centre_ = settings.from - (w_ * focus_);
		} catch (const MathsError&) {
			throw SceneError{"the camera's geometry exceeds the range of floating-point numbers"};
		}
		half_height_ = std::tan(settings.fov * std::numbers::pi / 360.0) * focus_;
		if (!std::isfinite(half_height_)) {
			throw SceneError{"the camera's field of view is too wide for its focus distance"};
		}
	}

	[[nodiscard]] const CameraSettings& settings() const noexcept { return settings_; }

	/// Distance of the plane in focus, with the default resolved.
	[[nodiscard]] double focus() const noexcept { return focus_; }

	/// A ray through a uniformly random point of pixel `(x, y)` of an image of the given `resolution`, counted from
	/// the top-left corner; it starts at a uniformly random point of the lens. A pinhole camera draws only the two
	/// offsets within the pixel from `sampler`, otherwise it then draws the point on the lens.
	///
	/// @throws RangeError unless `x < width` and `y < height`.
	/// @throws MathsError if the image plane is too large to be represented, which takes an extreme aspect ratio
	/// combined with an extreme field of view or focus distance.
	[[nodiscard]] Ray ray_for(std::size_t x, std::size_t y, Resolution resolution, Sampler& sampler) const {
		if (x >= resolution.width || y >= resolution.height) {
			throw RangeError{
					std::format("pixel ({}, {}) lies outside a {}x{} image", x, y, resolution.width, resolution.height)};
		}
		const auto width = static_cast<double>(resolution.width);
		const auto height = static_cast<double>(resolution.height);
		const double half_width = half_height_ * (width / height);

		// The pixel spans [x, x + 1) × [y, y + 1); the screen coordinates run from −1 to 1, left to right and bottom
		// to top.
		const double column = static_cast<double>(x) + 0.5 + sampler.uniform(-0.5, 0.5);
		const double row = static_cast<double>(y) + 0.5 + sampler.uniform(-0.5, 0.5);
		const double horizontal = ((2.0 * column) / width) - 1.0;
		const double vertical = 1.0 - ((2.0 * row) / height);
		const Point3 target = focal_centre_ + (u_ * (horizontal * half_width)) + (v_ * (vertical * half_height_));

		Point3 origin = settings_.from;
		if (settings_.aperture > 0.0) {
			const Vec3 lens = sampler.in_unit_disk() * (settings_.aperture / 2.0);
			origin += (u_ * lens[0]) + (v_ * lens[1]);
		}
		return Ray{origin, target - origin};
	}

private:
	/// Below this sine of the angle between `up` and the viewing direction, rounding errors would dominate the
	/// direction of `u`, and with it the orientation of the image.
	static constexpr double min_sine = 1e-9;

	CameraSettings settings_;
	Vec3 u_;
	Vec3 v_;
	Vec3 w_;
	double focus_ = 0.0;
	Point3 focal_centre_;
	/// Half the height of the image plane, at the focus distance.
	double half_height_ = 0.0;

	static void validate(const CameraSettings& settings) {
		if (settings.from == settings.to) {
			throw SceneError{"the camera must not look at its own position"};
		}
		if (settings.up == Vec3::zero()) {
			throw SceneError{"the camera's up vector must not be zero"};
		}
		if (std::isnan(settings.fov) || settings.fov <= 0.0 || settings.fov >= 180.0) {
			throw SceneError{std::format("the camera's field of view must lie in (0, 180) degrees, got {}", settings.fov)};
		}
		if (!std::isfinite(settings.aperture) || settings.aperture < 0.0) {
			throw SceneError{std::format("the camera's aperture must be finite and non-negative, got {}", settings.aperture)};
		}
		if (settings.focus && (!std::isfinite(*settings.focus) || *settings.focus <= 0.0)) {
			throw SceneError{std::format("the camera's focus distance must be finite and positive, got {}", *settings.focus)};
		}
	}
};

} // namespace rt

#endif // RT_SCENE_CAMERA_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
