#ifndef RT_MATHS_VECTOR_HPP
#define RT_MATHS_VECTOR_HPP

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
///
/// Invariant: every component is finite. Each operation that could produce an infinite or NaN component throws
/// `MathsError` instead, leaving its operands unchanged.
template <std::floating_point T, std::size_t N>
	requires(N > 0)
class Vector {
public:
	/// Zero vector.
	constexpr Vector() noexcept = default;

	/// Vector with the given components, one per dimension.
	///
	/// @throws MathsError if a component is infinite or NaN (after conversion to `T`).
	template <std::convertible_to<T>... Args>
		requires(sizeof...(Args) == N)
	constexpr explicit(N == 1) Vector(Args... components) : components_{static_cast<T>(components)...} {
		require_finite("vector components must be finite");
	}

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

	// Unchecked indices, as for `std::array` (debug builds check bounds with `_GLIBCXX_ASSERTIONS`).
	// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)

	/// Read-only access; use `set` to change a component.
	[[nodiscard]] constexpr const T& operator[](std::size_t index) const noexcept { return components_[index]; }

	/// @throws MathsError if `value` is infinite or NaN.
	constexpr void set(std::size_t index, T value) {
		require_finite(value, "vector components must be finite");
		components_[index] = value;
	}

	// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

	constexpr Vector& operator+=(const Vector& rhs) { return combine(rhs, std::plus{}); }
	constexpr Vector& operator-=(const Vector& rhs) { return combine(rhs, std::minus{}); }
	constexpr Vector& operator*=(const Vector& rhs) { return combine(rhs, std::multiplies{}); }
	constexpr Vector& operator/=(const Vector& rhs) { return combine(rhs, std::divides{}); }

	/// @throws MathsError if `scalar` is infinite or NaN, or the product overflows.
	constexpr Vector& operator*=(T scalar) { return combine(scalar, std::multiplies{}); }

	/// @throws MathsError if `scalar` is zero, infinite or NaN, or the quotient overflows.
	constexpr Vector& operator/=(T scalar) { return combine(scalar, std::divides{}); }

	[[nodiscard]] constexpr Vector operator-() const noexcept {
		Vector result{*this};
		for (T& component : result.components_) {
			component = -component;
		}
		return result;
	}

	[[nodiscard]] friend constexpr Vector operator+(Vector lhs, const Vector& rhs) {
		lhs += rhs;
		return lhs;
	}

	[[nodiscard]] friend constexpr Vector operator-(Vector lhs, const Vector& rhs) {
		lhs -= rhs;
		return lhs;
	}

	/// Component-wise (Hadamard) product.
	[[nodiscard]] friend constexpr Vector operator*(Vector lhs, const Vector& rhs) {
		lhs *= rhs;
		return lhs;
	}

	/// Component-wise quotient.
	[[nodiscard]] friend constexpr Vector operator/(Vector lhs, const Vector& rhs) {
		lhs /= rhs;
		return lhs;
	}

	[[nodiscard]] friend constexpr Vector operator*(Vector lhs, T scalar) {
		lhs *= scalar;
		return lhs;
	}

	[[nodiscard]] friend constexpr Vector operator*(T scalar, Vector rhs) {
		rhs *= scalar;
		return rhs;
	}

	[[nodiscard]] friend constexpr Vector operator/(Vector lhs, T scalar) {
		lhs /= scalar;
		return lhs;
	}

	/// Exact component-wise equality; use a tolerance when comparing computed results.
	[[nodiscard]] friend constexpr bool operator==(const Vector&, const Vector&) noexcept = default;

	/// May overflow to infinity: the result is a scalar, not a vector.
	[[nodiscard]] friend constexpr T dot(const Vector& lhs, const Vector& rhs) noexcept {
		return std::inner_product(lhs.components_.begin(), lhs.components_.end(), rhs.components_.begin(), T{0});
	}

	/// @throws MathsError if the result overflows.
	[[nodiscard]] friend constexpr Vector cross(const Vector& lhs, const Vector& rhs)
		requires(N == 3)
	{
		const auto& a = lhs.components_;
		const auto& b = rhs.components_;
		return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
	}

	/// May overflow to infinity (or underflow to zero) for very long (or very short) vectors; see `length`.
	[[nodiscard]] constexpr T length_squared() const noexcept { return dot(*this, *this); }

	/// Accurate for any vector; infinite only if the true length exceeds the largest finite `T`.
	[[nodiscard]] constexpr T length() const noexcept {
		const T squared = length_squared();
		if (std::isnormal(squared)) {
			return std::sqrt(squared);
		}
		// `squared` overflowed or (nearly) underflowed: rescale so that the largest component is 1.
		const T largest = max_magnitude();
		if (largest == T{0}) {
			return T{0};
		}
		T sum{0};
		for (const T component : components_) {
			const T scaled = component / largest;
			sum += scaled * scaled;
		}
		return largest * std::sqrt(sum);
	}

	/// Unit vector with the same direction; accurate for any non-zero vector, however long or short.
	///
	/// @throws MathsError if the vector is zero.
	[[nodiscard]] constexpr Vector normalised() const {
		const T squared = length_squared();
		if (std::isnormal(squared)) {
			return *this / std::sqrt(squared);
		}
		// `squared` overflowed or (nearly) underflowed: rescale first, so that the length lies in [1, sqrt(N)].
		const T largest = max_magnitude();
		if (largest == T{0}) {
			throw MathsError{"cannot normalise a zero-length vector"};
		}
		const Vector scaled = *this / largest;
		return scaled / std::sqrt(scaled.length_squared());
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

	static constexpr void require_finite(T value, const char* message) {
		if (!std::isfinite(value)) {
			throw MathsError{message};
		}
	}

	constexpr void require_finite(const char* message) const {
		for (const T component : components_) {
			require_finite(component, message);
		}
	}

	[[nodiscard]] constexpr T max_magnitude() const noexcept {
		T largest{0};
		for (const T component : components_) {
			largest = std::max(largest, std::abs(component));
		}
		return largest;
	}

	/// Applies `operation` component-wise; `*this` changes only if the result keeps the invariant.
	template <typename Operation>
	constexpr Vector& combine(const Vector& rhs, Operation operation) {
		Vector result;
		std::ranges::transform(components_, rhs.components_, result.components_.begin(), operation);
		return assign_checked(result);
	}

	/// Applies `operation` with `scalar` to every component; `*this` changes only if the result keeps the invariant.
	template <typename Operation>
	constexpr Vector& combine(T scalar, Operation operation) {
		require_finite(scalar, "vector operations require a finite scalar");
		Vector result;
		std::ranges::transform(components_, result.components_.begin(),
		                       [&](T component) { return operation(component, scalar); });
		return assign_checked(result);
	}

	constexpr Vector& assign_checked(const Vector& result) {
		result.require_finite("vector operation produced an infinite or NaN component");
		*this = result;
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

#endif // RT_MATHS_VECTOR_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
