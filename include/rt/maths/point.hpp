#ifndef RT_MATHS_POINT_HPP
#define RT_MATHS_POINT_HPP

#include "rt/maths/vector.hpp"

#include <concepts>
#include <cstddef>
#include <format>
#include <ostream>
#include <print>

namespace rt {

/// Position in space, as opposed to a displacement (`Vector`): the two form an affine space.
///
/// A point has only the operations that are meaningful for positions: `point − point` gives the vector between
/// them, and `point ± vector` moves the point. Points cannot be added, scaled, normalised or multiplied together;
/// to treat a position as a vector, take it relative to a reference point explicitly, as in `p − Point::origin()`.
///
/// Invariant: every coordinate is finite, kept by the invariant of `Vector` (README.md § 7.2). Each operation that
/// could produce an infinite or NaN coordinate throws `MathsError` instead, leaving its operands unchanged.
template <std::floating_point T, std::size_t N>
	requires(N > 0)
class Point {
public:
	using Displacement = Vector<T, N>;

	/// The origin.
	constexpr Point() noexcept = default;

	/// Point with the given coordinates, one per dimension.
	///
	/// @throws MathsError if a coordinate is infinite or NaN (after conversion to `T`).
	template <std::convertible_to<T>... Args>
		requires(sizeof...(Args) == N)
	constexpr explicit(N == 1) Point(Args... coordinates) : position_{coordinates...} {}

	constexpr Point(const Point&) noexcept = default;
	constexpr Point(Point&&) noexcept = default;
	constexpr Point& operator=(const Point&) noexcept = default;
	constexpr Point& operator=(Point&&) noexcept = default;
	constexpr ~Point() noexcept = default;

	[[nodiscard]] static constexpr Point origin() noexcept { return {}; }

	/// Read-only access; use `set` to change a coordinate. Unchecked index, as for `std::array`.
	[[nodiscard]] constexpr const T& operator[](std::size_t index) const noexcept { return position_[index]; }

	/// @throws MathsError if `value` is infinite or NaN.
	constexpr void set(std::size_t index, T value) { position_.set(index, value); }

	/// Read-only iteration over the coordinates.
	[[nodiscard]] constexpr auto begin() const noexcept { return position_.begin(); }
	[[nodiscard]] constexpr auto end() const noexcept { return position_.end(); }

	/// @throws MathsError if the result lies beyond the range of `T`.
	constexpr Point& operator+=(const Displacement& offset) {
		position_ += offset;
		return *this;
	}

	/// @throws MathsError if the result lies beyond the range of `T`.
	constexpr Point& operator-=(const Displacement& offset) {
		position_ -= offset;
		return *this;
	}

	[[nodiscard]] friend constexpr Point operator+(Point lhs, const Displacement& rhs) {
		lhs += rhs;
		return lhs;
	}

	[[nodiscard]] friend constexpr Point operator-(Point lhs, const Displacement& rhs) {
		lhs -= rhs;
		return lhs;
	}

	/// The displacement from `rhs` to `lhs`.
	///
	/// @throws MathsError if a component of the difference lies beyond the range of `T`.
	[[nodiscard]] friend constexpr Displacement operator-(const Point& lhs, const Point& rhs) {
		return lhs.position_ - rhs.position_;
	}

	/// Exact coordinate-wise equality; use a tolerance when comparing computed results.
	[[nodiscard]] friend constexpr bool operator==(const Point&, const Point&) noexcept = default;

	friend std::ostream& operator<<(std::ostream& stream, const Point& point) {
		std::print(stream, "{}", point);
		return stream;
	}

private:
	/// Displacement from the origin.
	Displacement position_;
};

/// Deduces the coordinate type from the first argument; all arguments must share it: `Point{1.0, 2.0, 3.0}`.
template <typename... Args>
	requires(sizeof...(Args) > 0 && (std::same_as<Args...[0], Args> && ...))
Point(Args...) -> Point<Args...[0], sizeof...(Args)>;

using Point3 = Point<double, 3>;

} // namespace rt

/// Formats as `(x, y, z)`, exactly like `Vector`.
template <std::floating_point T, std::size_t N, typename CharT>
struct std::formatter<rt::Point<T, N>, CharT> : rt::detail::ComponentsFormatter<T, CharT> {};

#endif // RT_MATHS_POINT_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
