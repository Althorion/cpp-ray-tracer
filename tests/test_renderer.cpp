#include "rt/colour.hpp"
#include "rt/maths/vector.hpp"
#include "rt/random/sampler.hpp"
#include "rt/render/image.hpp"
#include "rt/render/render_settings.hpp"
#include "rt/render/renderer.hpp"
#include "rt/scene/camera.hpp"
#include "rt/scene/scene.hpp"
#include "rt/scene/scene_description.hpp"

#include <rapidcheck/Assertions.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <ranges>
#include <type_traits>
#include <vector>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

static_assert(!std::is_copy_constructible_v<Renderer>);
static_assert(!std::is_move_constructible_v<Renderer>);
static_assert(!std::is_constructible_v<Renderer, Scene&&, const RenderSettings&>,
              "a renderer cannot be made from a temporary scene");

[[nodiscard]] Image render(const Scene& scene, const RenderSettings::Parameters& parameters) {
	return Renderer{scene, RenderSettings{parameters}}.render();
}

/// The `[lo, hi]` range of one colour channel.
struct ChannelRange {
	double lo;
	double hi;
};

/// The range of each channel of the sky over all directions through the rectangle `[h0, h1] × [v0, v1]` of the
/// image plane of a camera at the origin looking down −z, with `y` up, at distance 1 and with the given half-sides.
///
/// The sky colour depends linearly on `ŷ`, which grows with `v`; for a fixed `v` its extremes in `h` lie at the
/// rectangle's sides or at `h = 0`. So its extremes over the rectangle are among the corners and the points at
/// `h = 0` on the top and bottom sides, and each channel lies between its values at the two extreme directions.
[[nodiscard]] std::array<ChannelRange, 3> sky_range(const Sky& sky, double half_width, double half_height, double h0,
                                                    double h1, double v0, double v1) {
	std::vector<double> horizontals{h0, h1};
	if (h0 < 0.0 && 0.0 < h1) {
		horizontals.push_back(0.0);
	}
	std::array<ChannelRange, 3> ranges{};
	ranges.fill({.lo = std::numeric_limits<double>::infinity(), .hi = -std::numeric_limits<double>::infinity()});
	for (const double h : horizontals) {
		for (const double v : {v0, v1}) {
			const Colour colour = sky.colour(Vec3{h * half_width, v * half_height, -1.0});
			for (auto&& [range, channel] : std::views::zip(ranges, std::array{colour.r(), colour.g(), colour.b()})) {
				range = {.lo = std::min(range.lo, channel), .hi = std::max(range.hi, channel)};
			}
		}
	}
	return ranges;
}

void check_properties(Suite& suite) {
	suite.check("rendering twice gives identical images", [] {
		const Scene scene = build_scene(*scene_description());
		const auto parameters = *small_render_parameters();
		RC_ASSERT(render(scene, parameters) == render(scene, parameters));
	});

	suite.check("the image does not depend on the number of threads", [] {
		const Scene scene = build_scene(*scene_description());
		auto parameters = *small_render_parameters();
		parameters.threads = 1;
		const Image single = render(scene, parameters);
		parameters.threads = *parallel_thread_count();
		RC_ASSERT(render(scene, parameters) == single);
	});

	suite.check("a depth of 0 renders a black image", [] {
		const Scene scene = build_scene(*scene_description());
		auto parameters = *small_render_parameters();
		parameters.max_depth = 0;
		const Image image = render(scene, parameters);
		RC_ASSERT(std::ranges::all_of(image.pixels(), [](const Colour& pixel) { return pixel == Colour::black(); }));
	});

	suite.check("no pixel is brighter than the sky, as no material adds light", [] {
		const Scene scene = build_scene(*scene_description());
		const Sky& sky = scene.sky();
		const Image image = render(scene, *small_render_parameters());
		const double eps = default_eps<double>;
		RC_ASSERT(std::ranges::all_of(image.pixels(), [&](const Colour& pixel) {
			return pixel.r() <= std::max(sky.top.r(), sky.bottom.r()) + eps &&
			       pixel.g() <= std::max(sky.top.g(), sky.bottom.g()) + eps &&
			       pixel.b() <= std::max(sky.top.b(), sky.bottom.b()) + eps;
		}));
	});

	suite.check("with no spheres and a pinhole camera, every pixel is the sky seen through it", [] {
		const double fov = 1.0 + (178.0 * *unit_channel());
		const Scene scene{Camera{CameraSettings{.from = Point3{0, 0, 0}, .to = Point3{0, 0, -1}, .fov = fov}}, *any_sky()};
		auto parameters = *small_render_parameters();
		parameters.max_depth = std::max<std::size_t>(parameters.max_depth, 1);
		const Image image = render(scene, parameters);

		const auto width = static_cast<double>(image.width());
		const auto height = static_cast<double>(image.height());
		const double half_height = std::tan(fov * std::numbers::pi / 360.0);
		const double half_width = half_height * (width / height);
		for (std::size_t y = 0; y < image.height(); ++y) {
			for (std::size_t x = 0; x < image.width(); ++x) {
				const double h0 = ((2.0 * static_cast<double>(x)) / width) - 1.0;
				const double h1 = ((2.0 * static_cast<double>(x + 1)) / width) - 1.0;
				const double v0 = 1.0 - ((2.0 * static_cast<double>(y + 1)) / height);
				const double v1 = 1.0 - ((2.0 * static_cast<double>(y)) / height);
				const auto ranges = sky_range(scene.sky(), half_width, half_height, h0, h1, v0, v1);
				const Colour& pixel = image[x, y];
				for (const auto& [range, channel] : std::views::zip(ranges, std::array{pixel.r(), pixel.g(), pixel.b()})) {
					RC_ASSERT(range.lo - default_eps<double> <= channel);
					RC_ASSERT(channel <= range.hi + default_eps<double>);
				}
			}
		}
	});
}

void check_examples(Suite& suite) {
	suite.check("the image has the size of the settings", [] {
		const Scene scene = build_scene(demo_scene_description());
		const Image image = render(scene, {.resolution = Resolution{.width = 7, .height = 3}, .samples = 1, .threads = 2});
		RC_ASSERT(image.width() == 7U);
		RC_ASSERT(image.height() == 3U);
	});

	suite.check("tracing with a depth of 0 gives black even towards the sky", [] {
		const Scene scene = build_scene(demo_scene_description());
		const Renderer renderer{scene, RenderSettings{{.resolution = Resolution{.width = 1, .height = 1}, .max_depth = 0}}};
		Sampler sampler{1};
		RC_ASSERT(renderer.trace(Ray{Point3{0, 10, 0}, Vec3{0, 1, 0}}, sampler) == Colour::black());
	});

	suite.check("a ray that misses everything gets the sky colour", [] {
		const Scene scene = build_scene(demo_scene_description());
		const Renderer renderer{scene, RenderSettings{{.resolution = Resolution{.width = 1, .height = 1}, .max_depth = 1}}};
		Sampler sampler{1};
		RC_ASSERT(renderer.trace(Ray{Point3{0, 10, 0}, Vec3{0, 1, 0}}, sampler) == scene.sky().top);
	});

	suite.check("a ray that runs out of depth on a surface gets black", [] {
		const Scene scene = build_scene(demo_scene_description());
		const Renderer renderer{scene, RenderSettings{{.resolution = Resolution{.width = 1, .height = 1}, .max_depth = 1}}};
		Sampler sampler{1};
		// Straight down onto the ground sphere: the one interaction allowed is spent there.
		RC_ASSERT(renderer.trace(Ray{Point3{0, 10, 0}, Vec3{0, -1, 0}}, sampler) == Colour::black());
	});

	suite.check("the demonstration scene renders with every pixel lit", [] {
		const Scene scene = build_scene(demo_scene_description());
		const Image image =
				render(scene, {.resolution = Resolution{.width = 16, .height = 9}, .samples = 4, .max_depth = 8, .seed = 7});
		RC_ASSERT(std::ranges::all_of(image.pixels(), [](const Colour& pixel) { return pixel != Colour::black(); }));
	});
}

} // namespace
} // namespace rt::test

int main() {
	rt::test::Suite suite;
	rt::test::check_examples(suite);
	rt::test::check_properties(suite);
	return suite.exit_code();
}

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
