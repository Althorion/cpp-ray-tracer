#include "rt/errors.hpp"
#include "rt/render/render_settings.hpp"
#include "rt/scene/camera.hpp"
#include "rt/validation.hpp"

#include <rapidcheck/Assertions.h>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

void check_properties(Suite& suite) {
	suite.check("valid parameters are kept as given", [](const RenderSettings::Parameters& parameters) {
		const RenderSettings settings{parameters};
		RC_ASSERT(settings.resolution() == parameters.resolution);
		RC_ASSERT(settings.width() == parameters.resolution.width);
		RC_ASSERT(settings.height() == parameters.resolution.height);
		RC_ASSERT(settings.samples() == parameters.samples);
		RC_ASSERT(settings.max_depth() == parameters.max_depth);
		RC_ASSERT(settings.seed() == parameters.seed);
		RC_ASSERT(settings.threads() == parameters.threads);
	});

	suite.check("a width outside [1, 16384] throws RangeError", [](RenderSettings::Parameters parameters) {
		parameters.resolution.width = *invalid_render_side();
		RC_ASSERT_THROWS_AS(RenderSettings{parameters}, RangeError);
	});

	suite.check("a height outside [1, 16384] throws RangeError", [](RenderSettings::Parameters parameters) {
		parameters.resolution.height = *invalid_render_side();
		RC_ASSERT_THROWS_AS(RenderSettings{parameters}, RangeError);
	});

	suite.check("zero samples throw RangeError", [](RenderSettings::Parameters parameters) {
		parameters.samples = 0;
		RC_ASSERT_THROWS_AS(RenderSettings{parameters}, RangeError);
	});

	suite.check("zero threads throw RangeError", [](RenderSettings::Parameters parameters) {
		parameters.threads = 0;
		RC_ASSERT_THROWS_AS(RenderSettings{parameters}, RangeError);
	});

	suite.check("settings built from equal parameters are equal", [](const RenderSettings::Parameters& parameters) {
		RC_ASSERT(RenderSettings{parameters} == RenderSettings{parameters});
	});
}

void check_examples(Suite& suite) {
	suite.check("the defaults of the command line", [] {
		const RenderSettings settings{{.resolution = Resolution{.width = 800, .height = 450}}};
		RC_ASSERT(settings.samples() == 100U);
		RC_ASSERT(settings.max_depth() == 50U);
		RC_ASSERT(settings.seed() == 0U);
		RC_ASSERT(settings.threads() == default_thread_count());
		RC_ASSERT(settings.threads() >= 1U);
	});

	suite.check("the extreme valid values", [] {
		const RenderSettings smallest{
				{.resolution = Resolution{.width = 1, .height = 1}, .samples = 1, .max_depth = 0, .threads = 1}};
		RC_ASSERT(smallest.width() == 1U);
		RC_ASSERT(smallest.max_depth() == 0U);
		const RenderSettings largest{
				{.resolution = Resolution{.width = validation::max_image_side, .height = validation::max_image_side}}};
		RC_ASSERT(largest.width() == 16384U);
	});

	suite.check("a missing resolution throws RangeError",
	            [] { RC_ASSERT_THROWS_AS(RenderSettings{RenderSettings::Parameters{}}, RangeError); });
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
