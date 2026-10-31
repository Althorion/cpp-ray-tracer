#ifndef RT_MATH_INTERVAL_HPP
#define RT_MATH_INTERVAL_HPP

#include "rt/errors.hpp"

#include <cmath>
#include <concepts>
#include <limits>
#include <ostream>

namespace rt {

/// Closed interval `[min, max]` of real numbers; the bounds may be infinite.
///
/// An interval with `min > max` is empty: it contains nothing.
template <std::floating_point T>
class Interval {
public:
	/// Empty interval.
	constexpr Interval() noexcept = default;

	/// @throws MathError if either bound is NaN.
	constexpr Interval(T min, T max) : min_{min}, max_{max} {
		if (std::isnan(min) || std::isnan(max)) {
			throw MathError{"interval bounds must not be NaN"};
		}
	}

	[[nodiscard]] static constexpr Interval empty() noexcept { return {}; }

	[[nodiscard]] static constexpr Interval universe() noexcept {
		Interval result;
		result.min_ = -infinity;
		result.max_ = infinity;
		return result;
	}

	[[nodiscard]] constexpr T min() const noexcept { return min_; }
	[[nodiscard]] constexpr T max() const noexcept { return max_; }

	[[nodiscard]] constexpr bool is_empty() const noexcept { return min_ > max_; }

	/// Whether `min <= x <= max`.
	[[nodiscard]] constexpr bool contains(T x) const noexcept { return min_ <= x && x <= max_; }

	/// Whether `min < x < max`, i.e. `x` lies strictly inside.
	[[nodiscard]] constexpr bool surrounds(T x) const noexcept { return min_ < x && x < max_; }

	/// The point of the interval nearest to `x`; a NaN stays NaN.
	///
	/// @throws MathError if the interval is empty.
	[[nodiscard]] constexpr T clamp(T x) const {
		if (is_empty()) {
			throw MathError{"cannot clamp to an empty interval"};
		}
		if (x < min_) {
			return min_;
		}
		if (x > max_) {
			return max_;
		}
		return x;
	}

	[[nodiscard]] friend constexpr bool operator==(const Interval&, const Interval&) noexcept = default;

	friend std::ostream& operator<<(std::ostream& stream, const Interval& interval) {
		return stream << '[' << interval.min_ << ", " << interval.max_ << ']';
	}

private:
	static constexpr T infinity = std::numeric_limits<T>::infinity();

	T min_ = infinity;
	T max_ = -infinity;
};

} // namespace rt

#endif // RT_MATH_INTERVAL_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
