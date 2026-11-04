#ifndef RT_TESTS_GENERATORS_HPP
#define RT_TESTS_GENERATORS_HPP

#include "rt/colour.hpp"
#include "rt/geometry/hit_record.hpp"
#include "rt/geometry/sphere.hpp"
#include "rt/materials/glass.hpp"
#include "rt/materials/lambertian.hpp"
#include "rt/materials/material.hpp"
#include "rt/materials/metal.hpp"
#include "rt/materials/plastic.hpp"
#include "rt/maths/interval.hpp"
#include "rt/maths/ray.hpp"
#include "rt/maths/vector.hpp"
#include "rt/random/sampler.hpp"
#include "rt/scene/camera.hpp"

#include <rapidcheck.h>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace rt::test {

/// Largest coordinate magnitude produced by the generators; generously covers any sensible scene.
template <std::floating_point T>
inline constexpr T coordinate_bound = T{1e4};

/// Finite number in `(-coordinate_bound, coordinate_bound)`, shrinking towards zero.
template <std::floating_point T>
[[nodiscard]] rc::Gen<T> finite_scalar() {
	return rc::gen::map(rc::gen::arbitrary<T>(),
	                    [](T value) { return std::isfinite(value) ? std::fmod(value, coordinate_bound<T>) : T{0}; });
}

/// Vector with finite components in `(-coordinate_bound, coordinate_bound)`.
template <std::floating_point T, std::size_t N>
[[nodiscard]] rc::Gen<Vector<T, N>> finite_vector() {
	return []<std::size_t... Indices>(std::index_sequence<Indices...>) {
		return rc::gen::construct<Vector<T, N>>(((void)Indices, finite_scalar<T>())...);
	}(std::make_index_sequence<N>{});
}

/// Finite vector other than the zero vector; it may be very short.
template <std::floating_point T, std::size_t N>
[[nodiscard]] rc::Gen<Vector<T, N>> nonzero_vector() {
	return rc::gen::suchThat(finite_vector<T, N>(),
	                         [](const Vector<T, N>& vector) { return vector != Vector<T, N>::zero(); });
}

template <std::floating_point T, std::size_t N>
[[nodiscard]] rc::Gen<Vector<T, N>> unit_vector() {
	return rc::gen::map(nonzero_vector<T, N>(), [](const Vector<T, N>& vector) { return vector.normalised(); });
}

/// Index of a component of an `N`-dimensional vector.
template <std::size_t N>
[[nodiscard]] rc::Gen<std::size_t> component_index() {
	return rc::gen::inRange<std::size_t>(0, N);
}

/// Exponent `e` for which 2^e times a `finite_vector` stays finite, reaching down into the subnormal range.
template <std::floating_point T>
[[nodiscard]] rc::Gen<int> scaling_exponent() {
	constexpr int headroom = 14; // coordinate_bound = 1e4 < 2^14
	return rc::gen::inRange(std::numeric_limits<T>::min_exponent - std::numeric_limits<T>::digits,
	                        std::numeric_limits<T>::max_exponent - headroom + 1);
}

/// Positive or negative infinity, or NaN: the values a `Vector` must reject.
template <std::floating_point T>
[[nodiscard]] rc::Gen<T> non_finite_scalar() {
	return rc::gen::element(std::numeric_limits<T>::infinity(), -std::numeric_limits<T>::infinity(),
	                        std::numeric_limits<T>::quiet_NaN());
}

/// Finite number or, now and then, an infinity; the values an interval bound or a ray parameter can take.
template <std::floating_point T>
[[nodiscard]] rc::Gen<T> extended_scalar() {
	constexpr T infinity = std::numeric_limits<T>::infinity();
	return rc::gen::weightedOneOf<T>({
			{8, finite_scalar<T>()},
			{1, rc::gen::just(-infinity)},
			{1, rc::gen::just(infinity)},
	});
}

/// Interval with arbitrary bounds; empty whenever the lower bound exceeds the upper one.
template <std::floating_point T>
[[nodiscard]] rc::Gen<Interval<T>> any_interval() {
	return rc::gen::construct<Interval<T>>(extended_scalar<T>(), extended_scalar<T>());
}

/// Interval with `min <= max`, possibly degenerate (`min == max`) or unbounded.
template <std::floating_point T>
[[nodiscard]] rc::Gen<Interval<T>> nonempty_interval() {
	return rc::gen::map(rc::gen::pair(extended_scalar<T>(), extended_scalar<T>()), [](const std::pair<T, T>& bounds) {
		const auto [lower, upper] = std::minmax(bounds.first, bounds.second);
		return Interval<T>{lower, upper};
	});
}

/// Ray with a finite origin and a direction that is not (numerically) zero.
[[nodiscard]] inline rc::Gen<Ray> any_ray() {
	return rc::gen::construct<Ray>(finite_vector<double, 3>(), nonzero_vector<double, 3>());
}

/// Number in `(0, coordinate_bound)`.
[[nodiscard]] inline rc::Gen<double> positive_scalar() {
	return rc::gen::suchThat(rc::gen::map(finite_scalar<double>(), [](double value) { return std::abs(value); }),
	                         [](double value) { return value > 0.0; });
}

/// Number of samples averaged into one pixel in the tests.
[[nodiscard]] inline rc::Gen<int> sample_count() { return rc::gen::inRange(1, 101); }

/// Colour channel in `[0, 1]`, with both ends generated often.
[[nodiscard]] inline rc::Gen<double> unit_channel() {
	return rc::gen::weightedOneOf<double>({
			{8, rc::gen::map(finite_scalar<double>(), [](double value) { return std::fmod(std::abs(value), 1.0); })},
			{1, rc::gen::just(0.0)},
			{1, rc::gen::just(1.0)},
	});
}

/// Colour with components in `[0, 1]`, such as an albedo or a sky colour.
[[nodiscard]] inline rc::Gen<Colour> unit_colour() {
	return rc::gen::construct<Colour>(unit_channel(), unit_channel(), unit_channel());
}

/// Colour with components in `[0, coordinate_bound)`, such as accumulated light before averaging.
[[nodiscard]] inline rc::Gen<Colour> bright_colour() {
	const auto channel = rc::gen::map(finite_scalar<double>(), [](double value) { return std::abs(value); });
	return rc::gen::construct<Colour>(channel, channel, channel);
}

/// Radius of a test sphere, in `[1e-3, coordinate_bound)`: a sphere much smaller than its distance from the origin
/// would collapse to a point once its coordinates are rounded, and nothing can be said about where it is hit.
[[nodiscard]] inline rc::Gen<double> sphere_radius() {
	return rc::gen::map(positive_scalar(), [](double value) { return std::max(value, 1e-3); });
}

/// Number that is not finite and positive: negative, zero, infinite or NaN; e.g. an invalid radius or refractive
/// index.
[[nodiscard]] inline rc::Gen<double> not_finite_positive() {
	return rc::gen::weightedOneOf<double>({
			{1, rc::gen::map(positive_scalar(), [](double value) { return -value; })},
			{1, rc::gen::just(0.0)},
			{1, non_finite_scalar<double>()},
	});
}

/// Longest identifier the scene language allows (README.md § 5.1).
inline constexpr std::size_t max_identifier_length = 20;

/// Identifier of the scene language: an ASCII letter followed by up to 19 letters, digits, `_` or `-`.
[[nodiscard]] inline rc::Gen<std::string> identifier() {
	constexpr std::string_view letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	const std::string others = std::string{letters} + "0123456789_-";
	return rc::gen::mapcat(rc::gen::inRange<std::size_t>(0, max_identifier_length), [=](std::size_t tail_length) {
		return rc::gen::map(rc::gen::pair(rc::gen::elementOf(std::string{letters}),
		                                  rc::gen::container<std::string>(tail_length, rc::gen::elementOf(others))),
		                    [](const std::pair<char, std::string>& parts) { return parts.first + parts.second; });
	});
}

/// Several identifiers, repetitions included.
[[nodiscard]] inline rc::Gen<std::vector<std::string>> identifiers() {
	return rc::gen::container<std::vector<std::string>>(identifier());
}

/// Material name: an identifier, as the parser would accept it.
[[nodiscard]] inline rc::Gen<std::string> material_name() { return identifier(); }

/// Metal fuzz, in [0, 1] with both ends generated often.
[[nodiscard]] inline rc::Gen<double> fuzz() { return unit_channel(); }

/// Fuzz outside [0, 1], or not finite.
[[nodiscard]] inline rc::Gen<double> invalid_fuzz() {
	return rc::gen::weightedOneOf<double>({
			{1, rc::gen::map(positive_scalar(), [](double value) { return -value; })},
			{1, rc::gen::map(positive_scalar(), [](double value) { return 1.0 + value; })},
			{1, non_finite_scalar<double>()},
	});
}

/// Refractive index: usually in [0.5, 3], which spans an air bubble in glass to diamond, and now and then exactly
/// 1 (no boundary) or a common real value.
[[nodiscard]] inline rc::Gen<double> refractive_index() {
	return rc::gen::weightedOneOf<double>({
			{4, rc::gen::map(unit_channel(), [](double value) { return 0.5 + (2.5 * value); })},
			{1, rc::gen::element(1.0, 1.33, 1.5, 2.42)},
	});
}

/// Colour with a component above 1, which no albedo may have.
[[nodiscard]] inline rc::Gen<Colour> excessive_albedo() {
	const auto excess = rc::gen::map(positive_scalar(), [](double value) { return 1.0 + value; });
	return rc::gen::oneOf(rc::gen::construct<Colour>(excess, unit_channel(), unit_channel()),
	                      rc::gen::construct<Colour>(unit_channel(), excess, unit_channel()),
	                      rc::gen::construct<Colour>(unit_channel(), unit_channel(), excess));
}

/// Material for tests that need one but do not depend on which.
[[nodiscard]] inline std::shared_ptr<const Material> placeholder_material() {
	return std::make_shared<const Lambertian>("placeholder", Colour{0.5, 0.5, 0.5});
}

/// Sphere with a finite centre, a `sphere_radius` and the `placeholder_material`.
[[nodiscard]] inline rc::Gen<Sphere> any_sphere() {
	return rc::gen::construct<Sphere>(finite_vector<double, 3>(), sphere_radius(), rc::gen::just(placeholder_material()));
}

/// A ray meeting a surface: the record has a unit normal facing against the ray, and either `front_face`.
struct Incidence {
	Ray ray;
	HitRecord record;
};

[[nodiscard]] inline rc::Gen<Incidence> incidence() {
	return rc::gen::map(rc::gen::tuple(any_ray(), finite_scalar<double>(), nonzero_vector<double, 3>()),
	                    [](const std::tuple<Ray, double, Vec3>& parts) {
												const auto& [ray, t, outward_normal] = parts;
												return Incidence{
														.ray = ray,
														.record = HitRecord::from_outward_normal(ray, t, outward_normal, nullptr),
												};
											});
}

/// Bound of a sampling range: usually a moderate number, now and then an extreme one (the largest or the smallest
/// finite magnitude, or zero).
template <std::floating_point T>
[[nodiscard]] rc::Gen<T> range_bound() {
	constexpr T largest = std::numeric_limits<T>::max();
	constexpr T smallest = std::numeric_limits<T>::denorm_min();
	return rc::gen::weightedOneOf<T>({
			{8, finite_scalar<T>()},
			{1, rc::gen::element(-largest, largest, -smallest, smallest, T{0})},
	});
}

/// Sampler with an arbitrary seed and stream.
[[nodiscard]] inline rc::Gen<Sampler> any_sampler() {
	const auto stream = rc::gen::map(rc::gen::arbitrary<std::uint64_t>(),
	                                 [](std::uint64_t value) { return value & Sampler::max_stream; });
	return rc::gen::construct<Sampler>(rc::gen::arbitrary<std::uint64_t>(), stream);
}

/// Image width for pixel-stream tests: from a single column up to a very wide image.
[[nodiscard]] inline rc::Gen<std::size_t> image_width() { return rc::gen::inRange<std::size_t>(1, 100'000); }

/// Valid camera settings with moderate parameters: positions within `coordinate_bound`, a target at a distance in
/// [0.01, 100], an `up` at least about 6° away from the viewing direction, `fov` in [1, 179], `aperture` in [0, 2]
/// and, unless left to its default, `focus` in [0.1, 100].
[[nodiscard]] inline rc::Gen<CameraSettings> camera_settings() {
	const auto scaled = [](double lo, double hi) {
		return rc::gen::map(unit_channel(), [=](double value) { return lo + ((hi - lo) * value); });
	};
	const auto focus = rc::gen::weightedOneOf<std::optional<double>>({
			{1, rc::gen::just(std::optional<double>{})},
			{2, rc::gen::map(scaled(0.1, 100.0), [](double value) { return std::optional{value}; })},
	});
	const auto settings = rc::gen::apply(
			[](const Point3& from, const Vec3& heading, double distance, const Vec3& up, double fov, double aperture,
			   std::optional<double> focus_distance) {
				return CameraSettings{
						.from = from,
						.to = from + (heading * distance),
						.up = up,
						.fov = fov,
						.aperture = aperture,
						.focus = focus_distance,
				};
			},
			finite_vector<double, 3>(), unit_vector<double, 3>(), scaled(0.01, 100.0), unit_vector<double, 3>(),
			scaled(1.0, 179.0), scaled(0.0, 2.0), focus);
	return rc::gen::suchThat(settings, [](const CameraSettings& candidate) {
		return candidate.from != candidate.to &&
		       cross(candidate.up, (candidate.to - candidate.from).normalised()).length() >= 0.1;
	});
}

/// Finite number other than zero, of either sign.
[[nodiscard]] inline rc::Gen<double> nonzero_scalar() {
	return rc::gen::suchThat(finite_scalar<double>(), [](double value) { return value != 0.0; });
}

/// Field of view outside (0, 180) degrees, or not finite.
[[nodiscard]] inline rc::Gen<double> invalid_fov() {
	return rc::gen::weightedOneOf<double>({
			{2, rc::gen::map(finite_scalar<double>(), [](double value) { return -std::abs(value); })},
			{2, rc::gen::map(finite_scalar<double>(), [](double value) { return 180.0 + std::abs(value); })},
			{1, non_finite_scalar<double>()},
	});
}

/// Number that is not finite and non-negative: negative, infinite or NaN; e.g. an invalid aperture.
[[nodiscard]] inline rc::Gen<double> not_finite_non_negative() {
	return rc::gen::suchThat(not_finite_positive(), [](double value) { return value != 0.0; });
}

/// Image size for camera tests, from a single pixel up to 64×64.
[[nodiscard]] inline rc::Gen<Resolution> resolution() {
	const auto side = rc::gen::inRange<std::size_t>(1, 65);
	return rc::gen::construct<Resolution>(side, side);
}

/// Index in `[0, bound)`; `bound` must be positive.
[[nodiscard]] inline rc::Gen<std::size_t> index_below(std::size_t bound) {
	return rc::gen::inRange<std::size_t>(0, bound);
}

} // namespace rt::test

namespace rc {

template <>
struct Arbitrary<rt::Lambertian> {
	static Gen<rt::Lambertian> arbitrary() {
		return gen::construct<rt::Lambertian>(rt::test::material_name(), rt::test::unit_colour());
	}
};

template <>
struct Arbitrary<rt::Metal> {
	static Gen<rt::Metal> arbitrary() {
		return gen::construct<rt::Metal>(rt::test::material_name(), rt::test::unit_colour(), rt::test::fuzz());
	}
};

template <>
struct Arbitrary<rt::Glass> {
	static Gen<rt::Glass> arbitrary() {
		return gen::construct<rt::Glass>(rt::test::material_name(), rt::test::refractive_index());
	}
};

template <>
struct Arbitrary<rt::Plastic> {
	static Gen<rt::Plastic> arbitrary() {
		return gen::construct<rt::Plastic>(rt::test::material_name(), rt::test::unit_colour(),
		                                   rt::test::refractive_index());
	}
};

template <>
struct Arbitrary<rt::test::Incidence> {
	static Gen<rt::test::Incidence> arbitrary() { return rt::test::incidence(); }
};

template <>
struct Arbitrary<rt::Sphere> {
	static Gen<rt::Sphere> arbitrary() { return rt::test::any_sphere(); }
};

template <>
struct Arbitrary<rt::CameraSettings> {
	static Gen<rt::CameraSettings> arbitrary() { return rt::test::camera_settings(); }
};

template <>
struct Arbitrary<rt::Resolution> {
	static Gen<rt::Resolution> arbitrary() { return rt::test::resolution(); }
};

template <>
struct Arbitrary<rt::Sampler> {
	static Gen<rt::Sampler> arbitrary() { return rt::test::any_sampler(); }
};

template <>
struct Arbitrary<rt::Colour> {
	static Gen<rt::Colour> arbitrary() { return rt::test::unit_colour(); }
};

template <>
struct Arbitrary<rt::Ray> {
	static Gen<rt::Ray> arbitrary() { return rt::test::any_ray(); }
};

template <std::floating_point T>
struct Arbitrary<rt::Interval<T>> {
	static Gen<rt::Interval<T>> arbitrary() { return rt::test::any_interval<T>(); }
};

template <std::floating_point T, std::size_t N>
struct Arbitrary<rt::Vector<T, N>> {
	static Gen<rt::Vector<T, N>> arbitrary() { return rt::test::finite_vector<T, N>(); }
};

} // namespace rc

#endif // RT_TESTS_GENERATORS_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
