#ifndef RT_RANDOM_SAMPLER_HPP
#define RT_RANDOM_SAMPLER_HPP

#include "rt/errors.hpp"
#include "rt/maths/vector.hpp"

#include <pcg_random.hpp>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>

namespace rt {

/// Random numbers for rendering, drawn from a `pcg32` stream (README.md § 6.5).
///
/// The generator is a private detail: no member names its type, so it can be replaced without touching any caller.
/// Every draw is a pure function of the seed, the stream and the number of earlier draws, which makes renders
/// reproducible regardless of threading.
class Sampler {
public:
	/// Bit width of a stream number: `pcg32` keeps only the low 63 bits, so larger numbers would repeat streams.
	static constexpr unsigned stream_bits = 63;
	static constexpr std::uint64_t max_stream = (std::uint64_t{1} << stream_bits) - 1;

	/// Generator `stream` of the given `seed`; different streams give independent sequences.
	///
	/// @throws RangeError if `stream` exceeds `max_stream`.
	explicit Sampler(std::uint64_t seed, std::uint64_t stream = 0) : engine_{seed, checked_stream(stream)} {}

	/// The stream of pixel `(x, y)` in an image `width` pixels wide: `pcg32(seed, y · width + x)`.
	///
	/// @throws RangeError if `x` is not less than `width`, or the pixel index exceeds `max_stream`.
	[[nodiscard]] static Sampler for_pixel(std::uint64_t seed, std::size_t x, std::size_t y, std::size_t width) {
		if (x >= width) {
			throw RangeError{"the pixel column must be less than the image width"};
		}
		const std::uint64_t column = x;
		const std::uint64_t row = y;
		const std::uint64_t row_length = width;
		if (row > (max_stream - column) / row_length) {
			throw RangeError{"the pixel index exceeds the number of generator streams"};
		}
		return Sampler{seed, (row * row_length) + column};
	}

	/// Uniform in `[0, 1)`, with every bit of the significand random.
	[[nodiscard]] double uniform() noexcept { return canonical<double>(); }

	/// Uniform in `[lo, hi)`.
	///
	/// @throws MathsError unless both bounds are finite and `lo < hi`.
	template <std::floating_point T>
	[[nodiscard]] T uniform(T lo, T hi) {
		if (!(std::isfinite(lo) && std::isfinite(hi) && lo < hi)) {
			throw MathsError{"uniform requires finite bounds with lo < hi"};
		}
		// `std::lerp` cannot overflow even for the widest finite range, but rounding may still land on `hi`,
		// hence the clamp to the last value below it.
		return std::clamp(std::lerp(lo, hi, canonical<T>()), lo, std::nextafter(hi, lo));
	}

	/// Uniformly distributed over the unit sphere.
	[[nodiscard]] Vec3 unit_vector() {
		// Archimedes' hat-box theorem: on a unit sphere, z is uniform in [-1, 1], independently of the azimuth.
		const double z = uniform(-1.0, 1.0);
		const double azimuth = uniform(0.0, 2.0 * std::numbers::pi);
		const double radius = std::sqrt(1.0 - (z * z));
		return Vec3{radius * std::cos(azimuth), radius * std::sin(azimuth), z};
	}

	/// Uniformly distributed over the open unit disk in the xy plane (`z == 0`), e.g. a point on the lens.
	[[nodiscard]] Vec3 in_unit_disk() {
		// Rejection sampling from the enclosing square: unlike the polar method, rounding can never carry the
		// result onto the boundary circle. A point is accepted with probability π/4.
		for (;;) {
			const Vec3 point{uniform(-1.0, 1.0), uniform(-1.0, 1.0), 0.0};
			if (point.length_squared() < 1.0) {
				return point;
			}
		}
	}

private:
	pcg32 engine_;

	[[nodiscard]] static std::uint64_t checked_stream(std::uint64_t stream) {
		if (stream > max_stream) {
			throw RangeError{"the generator stream number is too large"};
		}
		return stream;
	}

	/// 2^−n, exactly.
	template <std::floating_point T>
	[[nodiscard]] static constexpr T inverse_power_of_two(unsigned n) noexcept {
		T result{1};
		for (unsigned i = 0; i < n; ++i) {
			result /= 2;
		}
		return result;
	}

	/// Uniform in `[0, 1)` on the grid of multiples of 2^−digits, every value of which `T` represents exactly.
	template <std::floating_point T>
	[[nodiscard]] T canonical() noexcept {
		constexpr auto digits = static_cast<unsigned>(std::numeric_limits<T>::digits);
		constexpr auto word_bits = static_cast<unsigned>(std::numeric_limits<pcg32::result_type>::digits);
		static_assert(digits <= 2 * word_bits, "at most two outputs are drawn per value");
		constexpr unsigned drawn = digits > word_bits ? 2 * word_bits : word_bits;
		std::uint64_t bits = engine_();
		if constexpr (drawn > word_bits) {
			bits = (bits << word_bits) | engine_();
		}
		bits >>= drawn - digits;
		return static_cast<T>(bits) * inverse_power_of_two<T>(digits);
	}
};

} // namespace rt

#endif // RT_RANDOM_SAMPLER_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
