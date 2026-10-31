#ifndef RT_MATH_VECTOR_HPP
#define RT_MATH_VECTOR_HPP

#include "rt/errors.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <format>
#include <functional>
#include <numeric>
#include <ostream>

namespace rt {

/// Fixed-size mathematical vector with value semantics.
///
/// Components are stored together in one array (array-of-structures layout), see README.md § 2.
template <std::floating_point T, std::size_t N>
	requires(N > 0)
class Vector {
public:
	/// Zero vector.
	constexpr Vector() noexcept = default;

	/// Vector with the given components, one per dimension.
	template <std::convertible_to<T>... Args>
		requires(sizeof...(Args) == N)
	constexpr explicit(N == 1) Vector(Args... components) noexcept : components_{static_cast<T>(components)...} {}

	constexpr Vector(const Vector&) noexcept = default;
	constexpr Vector(Vector&&) noexcept = default;
	constexpr Vector& operator=(const Vector&) noexcept = default;
	constexpr Vector& operator=(Vector&&) noexcept = default;
	constexpr ~Vector() noexcept = default;

	[[nodiscard]] static constexpr Vector zero() noexcept { return {}; }

	[[nodiscard]] static constexpr Vector unit_x() noexcept { return unit<0>(); }

	[[nodiscard]] static constexpr Vector unit_y() noexcept
		requires(N >= 2)
	{
		return unit<1>();
	}

	[[nodiscard]] static constexpr Vector unit_z() noexcept
		requires(N >= 3)
	{
		return unit<2>();
	}

	// Unchecked access, as for `std::array` (debug builds check bounds with `_GLIBCXX_ASSERTIONS`).
	// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
	[[nodiscard]] constexpr T& operator[](std::size_t index) noexcept { return components_[index]; }
	[[nodiscard]] constexpr const T& operator[](std::size_t index) const noexcept { return components_[index]; }
	// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

	constexpr Vector& operator+=(const Vector& rhs) noexcept { return combine(rhs, std::plus{}); }
	constexpr Vector& operator-=(const Vector& rhs) noexcept { return combine(rhs, std::minus{}); }
	constexpr Vector& operator*=(const Vector& rhs) noexcept { return combine(rhs, std::multiplies{}); }
	constexpr Vector& operator/=(const Vector& rhs) noexcept { return combine(rhs, std::divides{}); }

	constexpr Vector& operator*=(T scalar) noexcept {
		for (T& component : components_) {
			component *= scalar;
		}
		return *this;
	}

	constexpr Vector& operator/=(T scalar) noexcept {
		for (T& component : components_) {
			component /= scalar;
		}
		return *this;
	}

	[[nodiscard]] constexpr Vector operator-() const noexcept {
		Vector result{*this};
		for (T& component : result.components_) {
			component = -component;
		}
		return result;
	}

	[[nodiscard]] friend constexpr Vector operator+(Vector lhs, const Vector& rhs) noexcept {
		lhs += rhs;
		return lhs;
	}

	[[nodiscard]] friend constexpr Vector operator-(Vector lhs, const Vector& rhs) noexcept {
		lhs -= rhs;
		return lhs;
	}

	/// Component-wise (Hadamard) product.
	[[nodiscard]] friend constexpr Vector operator*(Vector lhs, const Vector& rhs) noexcept {
		lhs *= rhs;
		return lhs;
	}

	/// Component-wise quotient.
	[[nodiscard]] friend constexpr Vector operator/(Vector lhs, const Vector& rhs) noexcept {
		lhs /= rhs;
		return lhs;
	}

	[[nodiscard]] friend constexpr Vector operator*(Vector lhs, T scalar) noexcept {
		lhs *= scalar;
		return lhs;
	}

	[[nodiscard]] friend constexpr Vector operator*(T scalar, Vector rhs) noexcept {
		rhs *= scalar;
		return rhs;
	}

	[[nodiscard]] friend constexpr Vector operator/(Vector lhs, T scalar) noexcept {
		lhs /= scalar;
		return lhs;
	}

	/// Exact component-wise equality; use a tolerance when comparing computed results.
	[[nodiscard]] friend constexpr bool operator==(const Vector&, const Vector&) noexcept = default;

	[[nodiscard]] friend constexpr T dot(const Vector& lhs, const Vector& rhs) noexcept {
		return std::inner_product(lhs.components_.begin(), lhs.components_.end(), rhs.components_.begin(), T{0});
	}

	[[nodiscard]] friend constexpr Vector cross(const Vector& lhs, const Vector& rhs) noexcept
		requires(N == 3)
	{
		const auto& a = lhs.components_;
		const auto& b = rhs.components_;
		return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
	}

	[[nodiscard]] constexpr T length_squared() const noexcept { return dot(*this, *this); }

	[[nodiscard]] constexpr T length() const noexcept { return std::sqrt(length_squared()); }

	/// Unit vector with the same direction.
	///
	/// @throws MathError if the vector has zero length.
	[[nodiscard]] constexpr Vector normalised() const {
		const T squared = length_squared();
		if (squared == T{0}) {
			throw MathError{"cannot normalise a zero-length vector"};
		}
		return *this / std::sqrt(squared);
	}

	friend std::ostream& operator<<(std::ostream& stream, const Vector& vector) {
		stream << '(';
		for (std::size_t i = 0; const T component : vector.components_) {
			stream << (i++ == 0 ? "" : ", ") << component;
		}
		return stream << ')';
	}

private:
	std::array<T, N> components_{};

	template <std::size_t Axis>
	[[nodiscard]] static constexpr Vector unit() noexcept {
		Vector result;
		std::get<Axis>(result.components_) = T{1};
		return result;
	}

	template <typename Operation>
	constexpr Vector& combine(const Vector& rhs, Operation operation) noexcept {
		std::ranges::transform(components_, rhs.components_, components_.begin(), operation);
		return *this;
	}
};

/// Deduces the component type from the first argument; all arguments must share it: `Vector{1.0, 2.0, 3.0}`.
template <typename... Args>
	requires(sizeof...(Args) > 0 && (std::same_as<Args...[0], Args> && ...))
Vector(Args...) -> Vector<Args...[0], sizeof...(Args)>;

using Vec3 = Vector<double, 3>;
using Point3 = Vec3;

} // namespace rt

/// Formats as `(x, y, z)`; the format specification applies to every component: `std::format("{:.2f}", v)`.
template <std::floating_point T, std::size_t N, typename CharT>
struct std::formatter<rt::Vector<T, N>, CharT> {
	constexpr auto parse(std::basic_format_parse_context<CharT>& context) { return component_.parse(context); }

	template <typename FormatContext>
	auto format(const rt::Vector<T, N>& vector, FormatContext& context) const {
		auto out = context.out();
		*out++ = CharT{'('};
		for (std::size_t i = 0; i < N; ++i) {
			if (i > 0) {
				*out++ = CharT{','};
				*out++ = CharT{' '};
			}
			context.advance_to(out);
			out = component_.format(vector[i], context);
		}
		*out++ = CharT{')'};
		return out;
	}

private:
	std::formatter<T, CharT> component_;
};

#endif // RT_MATH_VECTOR_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
