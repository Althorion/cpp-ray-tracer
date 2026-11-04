#include "rt/errors.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"
#include "rt/random/sampler.hpp"
#include "rt/scene/camera.hpp"

#include <rapidcheck/Assertions.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

/// The camera's geometry, derived here independently of `Camera` from README.md § 6.1: `u` right, `v` up, `w`
/// backwards, and the half-extents of the image plane at the focus distance.
struct Frame {
	Vec3 u;
	Vec3 v;
	Vec3 w;
	double focus = 0.0;
	double half_width = 0.0;
	double half_height = 0.0;
};

[[nodiscard]] Frame frame_of(const CameraSettings& settings, Resolution resolution) {
	const Vec3 w = (settings.from - settings.to).normalised();
	const Vec3 u = cross(settings.up, w).normalised();
	const double focus = settings.focus.value_or((settings.to - settings.from).length());
	const double half_height = std::tan(settings.fov * std::numbers::pi / 360.0) * focus;
	return Frame{
			.u = u,
			.v = cross(w, u),
			.w = w,
			.focus = focus,
			.half_width = half_height * static_cast<double>(resolution.width) / static_cast<double>(resolution.height),
			.half_height = half_height,
	};
}

/// Where `ray` crosses the plane in focus, in pixel units: the column and the row, counted from the top-left corner.
[[nodiscard]] std::pair<double, double> pixel_position(const CameraSettings& settings, const Frame& frame,
                                                       Resolution resolution, const Ray& ray) {
	const double ahead = -dot(ray.direction(), frame.w);
	const double depth = -dot(ray.origin() - settings.from, frame.w);
	const Vec3 on_plane = ray.at((frame.focus - depth) / ahead) - settings.from + (frame.w * frame.focus);
	const double horizontal = dot(on_plane, frame.u) / frame.half_width;
	const double vertical = dot(on_plane, frame.v) / frame.half_height;
	return {(horizontal + 1.0) / 2.0 * static_cast<double>(resolution.width),
	        (1.0 - vertical) / 2.0 * static_cast<double>(resolution.height)};
}

/// Rounding allowance for `pixel_position`, in pixels: the coordinates are rounded relative to the size of the
/// scene, which a small pixel magnifies.
[[nodiscard]] double pixel_slack(const CameraSettings& settings, const Frame& frame, Resolution resolution) {
	const double scale = settings.from.length() + frame.focus + frame.half_width + frame.half_height;
	const double pixel = std::min(2.0 * frame.half_width / static_cast<double>(resolution.width),
	                              2.0 * frame.half_height / static_cast<double>(resolution.height));
	return 1e-12 * std::max(1.0, scale) / pixel;
}

/// Settings with the aperture closed: a pinhole camera.
[[nodiscard]] CameraSettings pinhole(CameraSettings settings) {
	settings.aperture = 0.0;
	return settings;
}

void check_rays(Suite& suite) {
	suite.check("every ray crosses the plane in focus within its pixel, wherever it leaves the lens",
	            [](const CameraSettings& settings, Resolution resolution, Sampler sampler) {
								const Camera camera{settings};
								const std::size_t x = *index_below(resolution.width);
								const std::size_t y = *index_below(resolution.height);
								const Frame frame = frame_of(settings, resolution);
								const auto [column, row] =
										pixel_position(settings, frame, resolution, camera.ray_for(x, y, resolution, sampler));
								const double slack = pixel_slack(settings, frame, resolution);
								RC_ASSERT(column >= static_cast<double>(x) - slack);
								RC_ASSERT(column <= static_cast<double>(x + 1) + slack);
								RC_ASSERT(row >= static_cast<double>(y) - slack);
								RC_ASSERT(row <= static_cast<double>(y + 1) + slack);
							});

	suite.check("rays start on the lens: within aperture / 2 of from, perpendicular to the viewing direction",
	            [](const CameraSettings& settings, Resolution resolution, Sampler sampler) {
								const Camera camera{settings};
								const Ray ray = camera.ray_for(*index_below(resolution.width), *index_below(resolution.height),
								                               resolution, sampler);
								const Vec3 offset = ray.origin() - settings.from;
								const double allowance = tolerance(settings.from.length());
								RC_ASSERT(offset.length() <= (settings.aperture / 2.0) + allowance);
								RC_ASSERT(std::abs(dot(offset, frame_of(settings, resolution).w)) <= allowance);
							});

	suite.check(
			"with the aperture closed, the centre pixel looks from `from` towards `to`",
			[](const CameraSettings& any_settings, Sampler sampler) {
				const CameraSettings settings = pinhole(any_settings);
				// Odd sides, so that one pixel lies at the centre.
				const Resolution resolution{.width = (2 * *index_below(32)) + 1, .height = (2 * *index_below(32)) + 1};
				const Ray ray = Camera{settings}.ray_for(resolution.width / 2, resolution.height / 2, resolution, sampler);
				RC_ASSERT(ray.origin() == settings.from);
				// The jitter within the pixel deflects the ray by at most the angle subtended by half its diagonal.
				const Frame frame = frame_of(settings, resolution);
				const double half_diagonal = std::hypot(frame.half_width / static_cast<double>(resolution.width),
				                                        frame.half_height / static_cast<double>(resolution.height));
				const double max_angle = std::atan(half_diagonal / frame.focus);
				RC_ASSERT(dot(ray.direction(), (settings.to - settings.from).normalised()) >= std::cos(max_angle) - 1e-12);
			});

	suite.check("with the aperture closed, every ray starts exactly at `from`",
	            [](const CameraSettings& settings, Resolution resolution, Sampler sampler) {
								const Ray ray = Camera{pinhole(settings)}.ray_for(*index_below(resolution.width),
								                                                  *index_below(resolution.height), resolution, sampler);
								RC_ASSERT(ray.origin() == settings.from);
							});

	suite.check("the same sampler state gives the same ray",
	            [](const CameraSettings& settings, Resolution resolution, const Sampler& sampler) {
								const Camera camera{settings};
								const std::size_t x = *index_below(resolution.width);
								const std::size_t y = *index_below(resolution.height);
								Sampler first = sampler;
								Sampler second = sampler;
								const Ray a = camera.ray_for(x, y, resolution, first);
								const Ray b = camera.ray_for(x, y, resolution, second);
								RC_ASSERT(a.origin() == b.origin());
								RC_ASSERT(a.direction() == b.direction());
							});

	suite.check("a pinhole camera draws only the two offsets within the pixel",
	            [](const CameraSettings& settings, Resolution resolution, const Sampler& sampler) {
								Sampler used = sampler;
								(void)Camera{pinhole(settings)}.ray_for(*index_below(resolution.width), *index_below(resolution.height),
								                                        resolution, used);
								Sampler expected = sampler;
								(void)expected.uniform(-0.5, 0.5);
								(void)expected.uniform(-0.5, 0.5);
								RC_ASSERT(used.uniform() == expected.uniform());
							});

	suite.check("a pixel outside the image throws RangeError", [](const CameraSettings& settings, Resolution resolution,
	                                                              Sampler sampler) {
		const Camera camera{settings};
		const std::size_t beyond = *index_below(1000);
		RC_ASSERT_THROWS_AS((void)camera.ray_for(resolution.width + beyond, 0, resolution, sampler), RangeError);
		RC_ASSERT_THROWS_AS((void)camera.ray_for(0, resolution.height + beyond, resolution, sampler), RangeError);
	});

	suite.check("the focus distance defaults to |to - from|", [](const CameraSettings& settings) {
		const Camera camera{settings};
		RC_ASSERT(camera.focus() == settings.focus.value_or((settings.to - settings.from).length()));
	});
}

void check_validation(Suite& suite) {
	suite.check("a field of view outside (0, 180) throws SceneError", [](CameraSettings settings) {
		settings.fov = *invalid_fov();
		RC_ASSERT_THROWS_AS(Camera{settings}, SceneError);
	});

	suite.check("looking at its own position throws SceneError", [](CameraSettings settings) {
		settings.to = settings.from;
		RC_ASSERT_THROWS_AS(Camera{settings}, SceneError);
	});

	suite.check("an up vector parallel to the viewing direction throws SceneError", [](CameraSettings settings) {
		const double factor = *nonzero_scalar();
		settings.up = (settings.to - settings.from) * factor;
		RC_ASSERT_THROWS_AS(Camera{settings}, SceneError);
	});

	suite.check("a zero up vector throws SceneError", [](CameraSettings settings) {
		settings.up = Vec3::zero();
		RC_ASSERT_THROWS_AS(Camera{settings}, SceneError);
	});

	suite.check("a negative or non-finite aperture throws SceneError", [](CameraSettings settings) {
		settings.aperture = *not_finite_non_negative();
		RC_ASSERT_THROWS_AS(Camera{settings}, SceneError);
	});

	suite.check("a focus distance that is not finite and positive throws SceneError", [](CameraSettings settings) {
		settings.focus = *not_finite_positive();
		RC_ASSERT_THROWS_AS(Camera{settings}, SceneError);
	});
}

/// A camera at the origin looking down −z, with the default settings otherwise.
[[nodiscard]] CameraSettings looking_down_z() {
	return CameraSettings{.from = Point3{0, 0, 0}, .to = Point3{0, 0, -1}};
}

void check_examples(Suite& suite) {
	suite.check("the top-left pixel looks up and to the left, the bottom-right one down and to the right", [] {
		const Camera camera{looking_down_z()};
		const Resolution resolution{.width = 2, .height = 2};
		Sampler sampler{1};
		const Vec3 top_left = camera.ray_for(0, 0, resolution, sampler).direction();
		const Vec3 bottom_right = camera.ray_for(1, 1, resolution, sampler).direction();
		RC_ASSERT(top_left[0] < 0.0);
		RC_ASSERT(top_left[1] > 0.0);
		RC_ASSERT(bottom_right[0] > 0.0);
		RC_ASSERT(bottom_right[1] < 0.0);
		RC_ASSERT(top_left[2] < 0.0);
		RC_ASSERT(bottom_right[2] < 0.0);
	});

	suite.check("a 90° field of view spans ±45°", [] {
		const Camera camera{looking_down_z()};
		// A single square pixel covers the whole field of view, ±45° both ways.
		const Resolution resolution{.width = 1, .height = 1};
		Sampler sampler{2};
		for (int i = 0; i < 1000; ++i) {
			const Vec3 direction = camera.ray_for(0, 0, resolution, sampler).direction();
			RC_ASSERT(std::abs(direction[1]) <= -direction[2] * (1.0 + 1e-12));
			RC_ASSERT(std::abs(direction[0]) <= -direction[2] * (1.0 + 1e-12));
		}
	});

	suite.check("the default settings: up is +y, 90° field of view, pinhole", [] {
		const CameraSettings settings{.from = Point3{1, 2, 3}, .to = Point3{1, 2, 0}};
		RC_ASSERT(settings.up == Vec3::unit_y());
		RC_ASSERT(settings.fov == 90.0);
		RC_ASSERT(settings.aperture == 0.0);
		RC_ASSERT(!settings.focus.has_value());
		RC_ASSERT(Camera{settings}.focus() == 3.0);
	});

	suite.check("geometry beyond the floating-point range throws SceneError, not MathsError", [] {
		constexpr double huge = std::numeric_limits<double>::max() / 2.0 * 1.5;
		const CameraSettings far_apart{.from = Point3{-huge, 0, 0}, .to = Point3{huge, 0, 0}};
		RC_ASSERT_THROWS_AS(Camera{far_apart}, SceneError);

		const CameraSettings too_wide{.from = Point3{0, 0, 0}, .to = Point3{0, 0, -1}, .fov = 179.999, .focus = huge};
		RC_ASSERT_THROWS_AS(Camera{too_wide}, SceneError);
	});

	suite.check("an empty image has no pixels", [] {
		const Camera camera{looking_down_z()};
		Sampler sampler{3};
		RC_ASSERT_THROWS_AS((void)camera.ray_for(0, 0, Resolution{.width = 0, .height = 0}, sampler), RangeError);
	});

	suite.check("an up vector at a tiny angle to the viewing direction is still accepted", [] {
		const CameraSettings settings{.from = Point3{0, 0, 0}, .to = Point3{0, 0, -1}, .up = Vec3{1e-6, 0, 1}};
		const Camera camera{settings};
		Sampler sampler{4};
		const Vec3 direction = camera.ray_for(0, 0, Resolution{.width = 2, .height = 1}, sampler).direction();
		// u = up × w points along −y here, so the left pixel looks towards +y.
		RC_ASSERT(direction[1] > 0.0);
	});
}

} // namespace
} // namespace rt::test

int main() {
	rt::test::Suite suite;
	rt::test::check_examples(suite);
	rt::test::check_rays(suite);
	rt::test::check_validation(suite);
	return suite.exit_code();
}

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
