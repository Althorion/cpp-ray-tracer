#include "rt/errors.hpp"
#include "rt/maths/vector.hpp"
#include "rt/random/sampler.hpp"

#include <rapidcheck/Assertions.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

static_assert(!std::is_default_constructible_v<Sampler>, "a sampler always has an explicit seed");
static_assert(!std::is_convertible_v<std::uint64_t, Sampler>, "a seed becomes a sampler only explicitly");
static_assert(std::is_nothrow_copy_constructible_v<Sampler>);

/// Draws per property case; enough for the statistical checks below to be sharp.
constexpr int draws = 4096;

/// Whether the mean of `draws` values produced by `draw` lies within six standard errors of `expected`, for values of
/// the given `variance`. A correct sampler fails with probability about 2·10⁻⁹.
[[nodiscard]] bool mean_is_close(const std::function<double()>& draw, double expected, double variance) {
	double sum = 0.0;
	for (int i = 0; i < draws; ++i) {
		sum += draw();
	}
	const double standard_error = std::sqrt(variance / draws);
	return std::abs((sum / draws) - expected) <= 6.0 * standard_error;
}

/// The first ten values of `sampler`.
[[nodiscard]] std::array<double, 10> first_values(Sampler sampler) {
	std::array<double, 10> values{};
	std::ranges::generate(values, [&sampler] { return sampler.uniform(); });
	return values;
}

template <std::floating_point T>
void check_ranges(Suite& suite) {
	suite.check("uniform(lo, hi) lies in [lo, hi)", [](Sampler sampler) {
		const T first = *range_bound<T>();
		const T second = *range_bound<T>();
		const auto [lo, hi] = std::minmax(first, second);
		RC_PRE(lo < hi);
		for (int i = 0; i < draws; ++i) {
			const T value = sampler.uniform(lo, hi);
			RC_ASSERT(lo <= value);
			RC_ASSERT(value < hi);
		}
	});

	suite.check("uniform over adjacent numbers always gives the lower one", [](Sampler sampler) {
		const T lo = *range_bound<T>();
		RC_PRE(lo < std::numeric_limits<T>::max());
		const T hi = std::nextafter(lo, std::numeric_limits<T>::infinity());
		for (int i = 0; i < draws; ++i) {
			RC_ASSERT(sampler.uniform(lo, hi) == lo);
		}
	});

	suite.check("uniform with lo >= hi throws MathsError", [](Sampler sampler) {
		const T first = *range_bound<T>();
		const T second = *range_bound<T>();
		const auto [lo, hi] = std::minmax(first, second);
		RC_ASSERT_THROWS_AS(static_cast<void>(sampler.uniform(hi, lo)), MathsError);
		RC_ASSERT_THROWS_AS(static_cast<void>(sampler.uniform(lo, lo)), MathsError);
	});

	suite.check("uniform with a non-finite bound throws MathsError", [](Sampler sampler) {
		const T bound = *range_bound<T>();
		const T non_finite = *non_finite_scalar<T>();
		RC_ASSERT_THROWS_AS(static_cast<void>(sampler.uniform(bound, non_finite)), MathsError);
		RC_ASSERT_THROWS_AS(static_cast<void>(sampler.uniform(non_finite, bound)), MathsError);
	});
}

void check_distributions(Suite& suite) {
	suite.check("uniform() lies in [0, 1)", [](Sampler sampler) {
		for (int i = 0; i < draws; ++i) {
			const double value = sampler.uniform();
			RC_ASSERT(0.0 <= value);
			RC_ASSERT(value < 1.0);
		}
	});

	suite.check("uniform() has mean 1/2", [](Sampler sampler) {
		RC_ASSERT(mean_is_close([&sampler] { return sampler.uniform(); }, 0.5, 1.0 / 12.0));
	});

	suite.check("unit_vector has unit length", [](Sampler sampler) {
		for (int i = 0; i < draws; ++i) {
			RC_ASSERT(approx_equal(sampler.unit_vector().length(), 1.0));
		}
	});

	// On the unit sphere every coordinate has mean 0, mean square 1/3 and variance of the square 1/5 − 1/9 = 4/45.
	suite.check("unit_vector is spread evenly over the sphere", [](Sampler sampler) {
		const std::size_t axis = *component_index<3>();
		RC_ASSERT(mean_is_close([&] { return sampler.unit_vector()[axis]; }, 0.0, 1.0 / 3.0));
		RC_ASSERT(mean_is_close(
				[&] {
					const double coordinate = sampler.unit_vector()[axis];
					return coordinate * coordinate;
				},
				1.0 / 3.0, 4.0 / 45.0));
	});

	suite.check("in_unit_disk lies inside the unit disk in the xy plane", [](Sampler sampler) {
		for (int i = 0; i < draws; ++i) {
			const Vec3 point = sampler.in_unit_disk();
			RC_ASSERT(point.length() < 1.0);
			RC_ASSERT(point[2] == 0.0);
		}
	});

	// In the unit disk x and y have mean 0 and variance 1/4; the squared radius is uniform in [0, 1).
	suite.check("in_unit_disk is spread evenly over the disk", [](Sampler sampler) {
		const std::size_t axis = *component_index<2>();
		RC_ASSERT(mean_is_close([&] { return sampler.in_unit_disk()[axis]; }, 0.0, 1.0 / 4.0));
		RC_ASSERT(mean_is_close([&] { return sampler.in_unit_disk().length_squared(); }, 0.5, 1.0 / 12.0));
	});
}

void check_streams(Suite& suite) {
	suite.check("the same seed and pixel give the same sequence", [](std::uint64_t seed) {
		const std::size_t width = *image_width();
		const std::size_t x = *index_below(width);
		const std::size_t y = *index_below(width);
		RC_ASSERT(first_values(Sampler::for_pixel(seed, x, y, width)) ==
		          first_values(Sampler::for_pixel(seed, x, y, width)));
	});

	suite.check("different pixels give different sequences", [](std::uint64_t seed) {
		const std::size_t width = *image_width();
		const std::pair first{*index_below(width), *index_below(width)};
		const std::pair second{*index_below(width), *index_below(width)};
		RC_PRE(first != second);
		RC_ASSERT(first_values(Sampler::for_pixel(seed, first.first, first.second, width)) !=
		          first_values(Sampler::for_pixel(seed, second.first, second.second, width)));
	});

	suite.check("different seeds give different sequences for the same pixel",
	            [](std::uint64_t seed, std::uint64_t other) {
								RC_PRE(seed != other);
								const std::size_t width = *image_width();
								const std::size_t x = *index_below(width);
								const std::size_t y = *index_below(width);
								RC_ASSERT(first_values(Sampler::for_pixel(seed, x, y, width)) !=
								          first_values(Sampler::for_pixel(other, x, y, width)));
							});

	suite.check("a pixel's stream is y · width + x", [](std::uint64_t seed) {
		const std::size_t width = *image_width();
		const std::size_t x = *index_below(width);
		const std::size_t y = *index_below(width);
		RC_ASSERT(first_values(Sampler::for_pixel(seed, x, y, width)) == first_values(Sampler{seed, (y * width) + x}));
	});

	suite.check("a column outside the image throws RangeError", [](std::uint64_t seed) {
		const std::size_t width = *image_width();
		const std::size_t x = width + *index_below(width);
		RC_ASSERT_THROWS_AS(static_cast<void>(Sampler::for_pixel(seed, x, 0, width)), RangeError);
	});

	suite.check("a copy continues with the same sequence", [](Sampler sampler) {
		static_cast<void>(sampler.uniform());
		const Sampler copy{sampler};
		RC_ASSERT(first_values(copy) == first_values(sampler));
	});
}

void check_examples(Suite& suite) {
	// The first two outputs of pcg32 with seed 42 and stream 54, as printed by pcg-cpp's own pcg32-demo.
	constexpr std::uint64_t first_output = 0xa15c02b7;
	constexpr std::uint64_t second_output = 0x7b47f409;

	suite.check("uniform() uses 53 bits of two consecutive outputs", [] {
		Sampler sampler{42, 54};
		const auto expected = static_cast<double>(((first_output << 32U) | second_output) >> 11U) * 0x1p-53;
		RC_ASSERT(sampler.uniform() == expected);
	});

	suite.check("uniform<float> uses the top 24 bits of one output", [] {
		Sampler sampler{42, 54};
		RC_ASSERT(sampler.uniform(0.0F, 1.0F) == static_cast<float>(first_output >> 8U) * 0x1p-24F);
	});

	suite.check("uniform over the widest finite range stays finite", [] {
		constexpr double largest = std::numeric_limits<double>::max();
		Sampler sampler{7};
		for (int i = 0; i < draws; ++i) {
			const double value = sampler.uniform(-largest, largest);
			RC_ASSERT(std::isfinite(value));
		}
	});

	suite.check("the last pixel index that fits in a stream", [] {
		constexpr std::size_t width = std::size_t{1} << 32U;
		constexpr std::size_t last_row = (std::size_t{1} << 31U) - 1;
		RC_ASSERT(first_values(Sampler::for_pixel(0, width - 1, last_row, width)) ==
		          first_values(Sampler{0, Sampler::max_stream}));
		RC_ASSERT_THROWS_AS(static_cast<void>(Sampler::for_pixel(0, 0, last_row + 1, width)), RangeError);
	});

	suite.check("invalid streams and widths throw RangeError", [] {
		RC_ASSERT_THROWS_AS(Sampler(0, Sampler::max_stream + 1), RangeError);
		RC_ASSERT_THROWS_AS(static_cast<void>(Sampler::for_pixel(0, 0, 0, 0)), RangeError);
	});
}

} // namespace
} // namespace rt::test

int main() {
	rt::test::Suite suite;
	rt::test::check_examples(suite);
	rt::test::check_distributions(suite);
	rt::test::check_ranges<double>(suite);
	rt::test::check_ranges<float>(suite);
	rt::test::check_streams(suite);
	return suite.exit_code();
}

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
