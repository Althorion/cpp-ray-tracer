#include "rt/colour.hpp"
#include "rt/errors.hpp"
#include "rt/maths/vector.hpp"

#include <rapidcheck/Assertions.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <type_traits>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

static_assert(Colour{} == Colour::black());
static_assert(Colour::white() == Colour(1, 1, 1));
static_assert(std::is_trivially_copyable_v<Colour>);
static_assert(!std::is_convertible_v<Vec3, Colour>, "a vector becomes a colour only explicitly");

/// Whether every channel of `lower` is at most the corresponding channel of `upper`.
[[nodiscard]] bool channels_ordered(const std::array<std::uint8_t, 3>& lower,
                                    const std::array<std::uint8_t, 3>& upper) {
	for (std::size_t i = 0; i < lower.size(); ++i) {
		if (lower.at(i) > upper.at(i)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool components_ordered(const Colour& lower, const Colour& upper) {
	return lower.r() <= upper.r() && lower.g() <= upper.g() && lower.b() <= upper.b();
}

[[nodiscard]] bool approx_equal(const Colour& a, const Colour& b, double eps = default_eps<double>) {
	return test::approx_equal(a.r(), b.r(), eps) && test::approx_equal(a.g(), b.g(), eps) &&
	       test::approx_equal(a.b(), b.b(), eps);
}

void check_properties(Suite& suite) {
	suite.check("quantised is monotonic in every component", [] {
		const Colour lower = *bright_colour();
		const Colour upper = lower + *bright_colour();
		RC_ASSERT(channels_ordered(lower.quantised(), upper.quantised()));
	});

	suite.check("gamma_corrected is monotonic in every component", [] {
		const Colour lower = *bright_colour();
		const Colour upper = lower + *bright_colour();
		RC_ASSERT(components_ordered(lower.gamma_corrected(), upper.gamma_corrected()));
	});

	suite.check("quantised channels are 255 exactly from 255/256 upwards", [] {
		const Colour colour = *bright_colour();
		const auto channels = colour.quantised();
		const std::array<double, 3> components{colour.r(), colour.g(), colour.b()};
		for (std::size_t i = 0; i < channels.size(); ++i) {
			RC_ASSERT((channels.at(i) == 255) == (components.at(i) >= 255.0 / 256.0));
		}
	});

	suite.check("gamma correction undoes squaring",
	            [](const Colour& colour) { RC_ASSERT(approx_equal((colour * colour).gamma_corrected(), colour)); });

	suite.check("a negative component throws MathsError", [] {
		const double negative = -*positive_scalar();
		RC_ASSERT_THROWS_AS(Colour(negative, 0, 0), MathsError);
		RC_ASSERT_THROWS_AS(Colour(0, negative, 0), MathsError);
		RC_ASSERT_THROWS_AS(Colour(0, 0, negative), MathsError);
		RC_ASSERT_THROWS_AS(Colour(Vec3(0, 0, negative)), MathsError);
	});

	suite.check("scaling by a negative factor throws MathsError and keeps the colour", [](const Colour& colour) {
		const double factor = -*positive_scalar();
		Colour changed{colour};
		RC_ASSERT_THROWS_AS(changed *= factor, MathsError);
		RC_ASSERT_THROWS_AS(changed /= factor, MathsError);
		RC_ASSERT(changed == colour);
	});

	suite.check("division by zero throws MathsError", [](const Colour& colour) {
		RC_ASSERT_THROWS_AS(static_cast<void>(colour / 0.0), MathsError);
		RC_ASSERT_THROWS_AS(static_cast<void>(colour / Colour::black()), MathsError);
	});

	suite.check("addition is commutative and black is its identity", [](const Colour& a, const Colour& b) {
		RC_ASSERT(a + b == b + a);
		RC_ASSERT(a + Colour::black() == a);
	});

	suite.check("white is the identity of multiplication", [](const Colour& colour) {
		RC_ASSERT(colour * Colour::white() == colour);
		RC_ASSERT(colour * 1.0 == colour);
	});

	suite.check("scaling by zero gives black", [](const Colour& colour) { RC_ASSERT(colour * 0.0 == Colour::black()); });

	suite.check("averaging n equal samples gives the sample", [](const Colour& colour) {
		const int samples = *sample_count();
		Colour sum;
		for (int i = 0; i < samples; ++i) {
			sum += colour;
		}
		RC_ASSERT(approx_equal(sum / static_cast<double>(samples), colour));
	});
}

void check_examples(Suite& suite) {
	suite.check("quantisation of characteristic values", [] {
		using Channels = std::array<std::uint8_t, 3>;
		RC_ASSERT((Colour::black().quantised() == Channels{0, 0, 0}));
		RC_ASSERT((Colour::white().quantised() == Channels{255, 255, 255}));
		RC_ASSERT((Colour(0.5, 0.25, 1.0 / 256).quantised() == Channels{128, 64, 1}));
		RC_ASSERT((Colour(255.0 / 256.0, std::nextafter(255.0 / 256.0, 0.0), 1000).quantised() == Channels{255, 254, 255}));
		RC_ASSERT((Colour(std::nextafter(1.0, 0.0), 1, 1).quantised() == Channels{255, 255, 255}));
		RC_ASSERT((Colour(0.0039, 0, 0).quantised() == Channels{0, 0, 0}));
	});

	suite.check("gamma correction of characteristic values", [] {
		RC_ASSERT(Colour(0.25, 1, 4).gamma_corrected() == Colour(0.5, 1, 2));
		RC_ASSERT(Colour::black().gamma_corrected() == Colour::black());
	});

	suite.check("component-wise and scalar operators", [] {
		RC_ASSERT(Colour(0.1, 0.2, 0.3) + Colour(0.4, 0.5, 0.6) == Colour(0.1 + 0.4, 0.2 + 0.5, 0.3 + 0.6));
		RC_ASSERT(Colour(0.5, 1, 2) * Colour(2, 0.5, 0.25) == Colour(1, 0.5, 0.5));
		RC_ASSERT(Colour(1, 0.5, 0.5) / Colour(2, 0.5, 0.25) == Colour(0.5, 1, 2));
		RC_ASSERT(Colour(1, 2, 3) * 2.0 == Colour(2, 4, 6));
		RC_ASSERT(2.0 * Colour(1, 2, 3) == Colour(2, 4, 6));
		RC_ASSERT(Colour(2, 4, 6) / 2.0 == Colour(1, 2, 3));
	});

	suite.check("non-finite components throw MathsError", [] {
		RC_ASSERT_THROWS_AS(Colour(std::numeric_limits<double>::infinity(), 0, 0), MathsError);
		RC_ASSERT_THROWS_AS(Colour(0, std::numeric_limits<double>::quiet_NaN(), 0), MathsError);
	});

	suite.check("negative zero is allowed", [] { RC_ASSERT(Colour(-0.0, 0, 0) == Colour::black()); });

	suite.check("stream output", [] {
		std::ostringstream stream;
		stream << Colour(0.5, 0.7, 1);
		RC_ASSERT(stream.str() == "rgb(0.5, 0.7, 1)");
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
