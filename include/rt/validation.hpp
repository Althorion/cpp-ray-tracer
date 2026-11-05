#ifndef RT_VALIDATION_HPP
#define RT_VALIDATION_HPP

#include "rt/colour.hpp"
#include "rt/errors.hpp"
#include "rt/maths/vector.hpp"

#include <cmath>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

/// The limits on the parameters of a scene (README.md § 5.2), in one place.
///
/// Each `check_*` function returns nothing for a valid value, and otherwise the reason why it is invalid, phrased for
/// an error message. The constructors of the classes turn it into an exception with `validated`; the parser reports
/// it as a `SceneParseError` with the position of the offending token, before any object is built.
namespace rt::validation {

/// Why a value is invalid; empty if it is valid.
using Problem = std::optional<std::string_view>;

/// `value`, if `check(value)` finds no problem.
///
/// @throws Exception with the problem as its message otherwise.
template <std::derived_from<Error> Exception, typename T, typename Check>
	requires std::same_as<std::invoke_result_t<Check, const T&>, Problem>
[[nodiscard]] constexpr T validated(T value, Check check) {
	if (const Problem problem = check(value)) {
		throw Exception{std::string{*problem}};
	}
	return value;
}

/// An `<rgb>` value, such as an albedo or a sky colour: no component above 1, so that no surface creates light
/// (components below 0 are already ruled out by `Colour`).
[[nodiscard]] constexpr Problem check_rgb(const Colour& colour) noexcept {
	if (colour.r() > 1.0 || colour.g() > 1.0 || colour.b() > 1.0) {
		return "colour components must lie in [0, 1]";
	}
	return std::nullopt;
}

/// The fuzz of a metal: in [0, 1].
[[nodiscard]] constexpr Problem check_fuzz(double fuzz) noexcept {
	if (std::isnan(fuzz) || fuzz < 0.0 || fuzz > 1.0) {
		return "the fuzz of a metal must lie in [0, 1]";
	}
	return std::nullopt;
}

/// A refractive index, of glass or of a plastic's coat: finite and positive.
[[nodiscard]] constexpr Problem check_ior(double ior) noexcept {
	if (!std::isfinite(ior) || ior <= 0.0) {
		return "a refractive index must be finite and positive";
	}
	return std::nullopt;
}

/// The radius of a sphere: finite and positive.
[[nodiscard]] constexpr Problem check_radius(double radius) noexcept {
	if (!std::isfinite(radius) || radius <= 0.0) {
		return "the radius of a sphere must be finite and positive";
	}
	return std::nullopt;
}

/// The camera's vertical field of view, in degrees: in (0, 180).
[[nodiscard]] constexpr Problem check_fov(double fov) noexcept {
	if (std::isnan(fov) || fov <= 0.0 || fov >= 180.0) {
		return "the camera's field of view must lie in (0, 180) degrees";
	}
	return std::nullopt;
}

/// The diameter of the camera's lens: finite and non-negative.
[[nodiscard]] constexpr Problem check_aperture(double aperture) noexcept {
	if (!std::isfinite(aperture) || aperture < 0.0) {
		return "the camera's aperture must be finite and non-negative";
	}
	return std::nullopt;
}

/// The camera's focus distance, when given: finite and positive.
[[nodiscard]] constexpr Problem check_focus(double focus) noexcept {
	if (!std::isfinite(focus) || focus <= 0.0) {
		return "the camera's focus distance must be finite and positive";
	}
	return std::nullopt;
}

/// Below this sine of the angle between the camera's `up` and its viewing direction, rounding errors would dominate
/// the camera's horizontal axis, and with it the orientation of the image.
inline constexpr double min_up_sine = 1e-9;

/// The camera's position, target and up vector together: `from ≠ to`, and `up` non-zero and not (nearly) parallel
/// to the viewing direction (see `min_up_sine`).
[[nodiscard]] inline Problem check_camera_axes(const Point3& from, const Point3& to, const Vec3& up) {
	if (from == to) {
		return "the camera must not look at its own position";
	}
	if (up == Vec3::zero()) {
		return "the camera's up vector must not be zero";
	}
	// Halved first, so that the difference cannot overflow; only for the tiniest (subnormal) differences can the
	// halves round to the same value, and then the plain difference is exact.
	Vec3 view = (to * 0.5) - (from * 0.5);
	if (view == Vec3::zero()) {
		view = to - from;
	}
	if (cross(up.normalised(), view.normalised()).length() < min_up_sine) {
		return "the camera's up vector must not be parallel to its viewing direction";
	}
	return std::nullopt;
}

} // namespace rt::validation

#endif // RT_VALIDATION_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
