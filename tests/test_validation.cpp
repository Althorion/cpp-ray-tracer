#include "rt/colour.hpp"
#include "rt/errors.hpp"
#include "rt/geometry/sphere.hpp"
#include "rt/materials/glass.hpp"
#include "rt/materials/lambertian.hpp"
#include "rt/materials/metal.hpp"
#include "rt/materials/plastic.hpp"
#include "rt/maths/vector.hpp"
#include "rt/scene/camera.hpp"
#include "rt/validation.hpp"

#include <rapidcheck/Assertions.h>

#include <initializer_list>
#include <limits>
#include <optional>
#include <string>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

using validation::check_aperture;
using validation::check_camera_axes;
using validation::check_focus;
using validation::check_fov;
using validation::check_fuzz;
using validation::check_ior;
using validation::check_radius;
using validation::check_rgb;

constexpr double infinity = std::numeric_limits<double>::infinity();
constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double largest = std::numeric_limits<double>::max();
constexpr double tiniest = std::numeric_limits<double>::denorm_min();

// The checks are usable at compile time, e.g. for the defaults.
static_assert(!check_fov(CameraSettings{}.fov));
static_assert(!check_aperture(CameraSettings{}.aperture));
static_assert(!check_ior(Plastic::default_ior));

/// Whether constructing with `make` throws exactly when `problem` is set.
template <typename Make>
[[nodiscard]] bool throws_iff(const validation::Problem& problem, Make make) {
	try {
		(void)make();
	} catch (const Error&) {
		return problem.has_value();
	}
	return !problem.has_value();
}

void check_agreement(Suite& suite) {
	suite.check("Lambertian and Metal reject exactly the albedos check_rgb rejects", [] {
		const Colour albedo = *valid_or_not(unit_colour(), excessive_albedo());
		RC_ASSERT(throws_iff(check_rgb(albedo), [&] { return Lambertian{"a", albedo}; }));
		RC_ASSERT(throws_iff(check_rgb(albedo), [&] { return Metal{"a", albedo, 0.0}; }));
		RC_ASSERT(throws_iff(check_rgb(albedo), [&] { return Plastic{"a", albedo}; }));
	});

	suite.check("Metal rejects exactly the fuzz check_fuzz rejects", [] {
		const double fuzz = *valid_or_not(rt::test::fuzz(), invalid_fuzz());
		RC_ASSERT(throws_iff(check_fuzz(fuzz), [&] { return Metal{"a", Colour::white(), fuzz}; }));
	});

	suite.check("Glass and Plastic reject exactly the indices check_ior rejects", [] {
		const double ior = *valid_or_not(refractive_index(), not_finite_positive());
		RC_ASSERT(throws_iff(check_ior(ior), [&] { return Glass{"a", ior}; }));
		RC_ASSERT(throws_iff(check_ior(ior), [&] { return Plastic{"a", Colour::white(), ior}; }));
	});

	suite.check("Sphere rejects exactly the radii check_radius rejects", [] {
		const double radius = *valid_or_not(sphere_radius(), not_finite_positive());
		RC_ASSERT(throws_iff(check_radius(radius), [&] { return Sphere{Point3{}, radius, placeholder_material()}; }));
	});

	suite.check("the camera rejects exactly what the camera checks reject", [](CameraSettings settings, bool bad_fov,
	                                                                           bool bad_aperture, bool parallel_up) {
		if (bad_fov) {
			settings.fov = *invalid_fov();
		}
		if (bad_aperture) {
			settings.aperture = *not_finite_non_negative();
		}
		if (parallel_up) {
			settings.up = (settings.to - settings.from) * *nonzero_scalar();
		}
		const bool invalid = check_camera_axes(settings.from, settings.to, settings.up) || check_fov(settings.fov) ||
		                     check_aperture(settings.aperture) || (settings.focus && check_focus(*settings.focus));
		RC_ASSERT(throws_iff(invalid ? validation::Problem{"invalid"} : std::nullopt, [&] { return Camera{settings}; }));
	});

	suite.check("validated passes a valid value through and throws the requested type otherwise", [] {
		const double radius = *sphere_radius();
		RC_ASSERT(validation::validated<MathsError>(radius, check_radius) == radius);
		const double invalid = *not_finite_positive();
		RC_ASSERT_THROWS_AS((void)validation::validated<SceneError>(invalid, check_radius), SceneError);
	});
}

void check_examples(Suite& suite) {
	suite.check("check_rgb: [0, 1] per component", [] {
		RC_ASSERT(!check_rgb(Colour::black()));
		RC_ASSERT(!check_rgb(Colour::white()));
		RC_ASSERT(check_rgb(Colour{0, 0, 1.0000001}).has_value());
	});

	suite.check("check_fuzz: [0, 1]", [] {
		RC_ASSERT(!check_fuzz(0.0));
		RC_ASSERT(!check_fuzz(1.0));
		RC_ASSERT(check_fuzz(-0.1).has_value());
		RC_ASSERT(check_fuzz(1.1).has_value());
		RC_ASSERT(check_fuzz(nan).has_value());
	});

	suite.check("check_ior, check_radius, check_focus: finite and positive", [] {
		for (const auto check : {check_ior, check_radius, check_focus}) {
			RC_ASSERT(!check(tiniest));
			RC_ASSERT(!check(largest));
			RC_ASSERT(check(0.0).has_value());
			RC_ASSERT(check(-1.0).has_value());
			RC_ASSERT(check(infinity).has_value());
			RC_ASSERT(check(nan).has_value());
		}
	});

	suite.check("check_fov: (0, 180)", [] {
		RC_ASSERT(!check_fov(tiniest));
		RC_ASSERT(!check_fov(179.999));
		RC_ASSERT(check_fov(0.0).has_value());
		RC_ASSERT(check_fov(180.0).has_value());
		RC_ASSERT(check_fov(nan).has_value());
	});

	suite.check("check_aperture: finite and non-negative", [] {
		RC_ASSERT(!check_aperture(0.0));
		RC_ASSERT(!check_aperture(2.0));
		RC_ASSERT(check_aperture(-tiniest).has_value());
		RC_ASSERT(check_aperture(infinity).has_value());
		RC_ASSERT(check_aperture(nan).has_value());
	});

	suite.check("check_camera_axes", [] {
		const Point3 origin{0, 0, 0};
		const Point3 ahead{0, 0, -1};
		RC_ASSERT(!check_camera_axes(origin, ahead, Vec3::unit_y()));
		RC_ASSERT(check_camera_axes(origin, origin, Vec3::unit_y()).has_value());
		RC_ASSERT(check_camera_axes(origin, ahead, Vec3::zero()).has_value());
		RC_ASSERT(check_camera_axes(origin, ahead, Vec3{0, 0, 5}).has_value());
		RC_ASSERT(check_camera_axes(origin, ahead, Vec3{0, 0, -1e-300}).has_value());
	});

	suite.check("check_camera_axes does not overflow, nor lose the tiniest differences", [] {
		const Point3 far_left{-largest, 0, 0};
		const Point3 far_right{largest, 0, 0};
		RC_ASSERT(!check_camera_axes(far_left, far_right, Vec3::unit_y()));
		RC_ASSERT(check_camera_axes(far_left, far_right, Vec3::unit_x()).has_value());
		const Point3 nearly_origin{tiniest, 0, 0};
		RC_ASSERT(!check_camera_axes(Point3{0, 0, 0}, nearly_origin, Vec3::unit_y()));
		RC_ASSERT(check_camera_axes(Point3{0, 0, 0}, nearly_origin, Vec3::unit_x()).has_value());
	});

	suite.check("problems are phrased for error messages", [] {
		RC_ASSERT(std::string{*check_fov(200.0)}.contains("field of view"));
		RC_ASSERT(std::string{*check_radius(-1.0)}.contains("radius"));
	});
}

} // namespace
} // namespace rt::test

int main() {
	rt::test::Suite suite;
	rt::test::check_examples(suite);
	rt::test::check_agreement(suite);
	return suite.exit_code();
}

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
