#ifndef RT_COLOUR_HPP
#define RT_COLOUR_HPP

#include "rt/errors.hpp"
#include "rt/maths/vector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ostream>

namespace rt {

/// Linear RGB colour (or light intensity) with non-negative, finite components; values above 1 are allowed.
class Colour {
public:
	/// Black.
	constexpr Colour() noexcept = default;

	/// @throws MathsError if a component is negative, infinite or NaN.
	constexpr Colour(double r, double g, double b) : rgb_{validated(Vec3{r, g, b})} {}

	/// @throws MathsError if a component is negative.
	constexpr explicit Colour(const Vec3& rgb) : rgb_{validated(rgb)} {}

	[[nodiscard]] static constexpr Colour black() noexcept { return {}; }
	[[nodiscard]] static constexpr Colour white() noexcept { return unchecked(white_rgb); }

	[[nodiscard]] constexpr double r() const noexcept { return rgb_[0]; }
	[[nodiscard]] constexpr double g() const noexcept { return rgb_[1]; }
	[[nodiscard]] constexpr double b() const noexcept { return rgb_[2]; }

	/// @throws MathsError if the sum overflows.
	constexpr Colour& operator+=(const Colour& rhs) {
		rgb_ += rhs.rgb_;
		return *this;
	}

	/// Component-wise product, e.g. filtering light through a material's attenuation.
	///
	/// @throws MathsError if the product overflows.
	constexpr Colour& operator*=(const Colour& rhs) {
		rgb_ *= rhs.rgb_;
		return *this;
	}

	/// Component-wise quotient.
	///
	/// @throws MathsError if `rhs` has a zero component or the quotient overflows.
	constexpr Colour& operator/=(const Colour& rhs) {
		rgb_ /= rhs.rgb_;
		return *this;
	}

	/// @throws MathsError if `scalar` is negative, infinite or NaN, or the product overflows.
	constexpr Colour& operator*=(double scalar) {
		require_non_negative(scalar, "a colour cannot be scaled by a negative factor");
		rgb_ *= scalar;
		return *this;
	}

	/// @throws MathsError if `scalar` is negative, zero, infinite or NaN, or the quotient overflows.
	constexpr Colour& operator/=(double scalar) {
		require_non_negative(scalar, "a colour cannot be scaled by a negative factor");
		rgb_ /= scalar;
		return *this;
	}

	[[nodiscard]] friend constexpr Colour operator+(Colour lhs, const Colour& rhs) {
		lhs += rhs;
		return lhs;
	}

	[[nodiscard]] friend constexpr Colour operator*(Colour lhs, const Colour& rhs) {
		lhs *= rhs;
		return lhs;
	}

	[[nodiscard]] friend constexpr Colour operator/(Colour lhs, const Colour& rhs) {
		lhs /= rhs;
		return lhs;
	}

	[[nodiscard]] friend constexpr Colour operator*(Colour lhs, double scalar) {
		lhs *= scalar;
		return lhs;
	}

	[[nodiscard]] friend constexpr Colour operator*(double scalar, Colour rhs) {
		rhs *= scalar;
		return rhs;
	}

	[[nodiscard]] friend constexpr Colour operator/(Colour lhs, double scalar) {
		lhs /= scalar;
		return lhs;
	}

	[[nodiscard]] friend constexpr bool operator==(const Colour&, const Colour&) noexcept = default;

	/// Gamma 2 encoding: the square root of every component (README.md § 6.4).
	///
	/// `noexcept` despite going through the checking constructor of `Vec3`: the components are finite and
	/// non-negative (the class invariant), so their square roots are finite and non-negative too, and that check
	/// cannot fail. clang-tidy cannot follow this reasoning, hence the suppression below.
	// NOLINTNEXTLINE(bugprone-exception-escape): the `Vec3` constructor cannot throw here, see above
	[[nodiscard]] constexpr Colour gamma_corrected() const noexcept {
		return unchecked(Vec3{std::sqrt(r()), std::sqrt(g()), std::sqrt(b())});
	}

	/// 8-bit channel values: every component clamped to `[0, 1)`, then `⌊256 · c⌋` (README.md § 6.4).
	///
	/// Apply `gamma_corrected` first when producing pixels.
	[[nodiscard]] constexpr std::array<std::uint8_t, 3> quantised() const noexcept {
		// The largest double below 1, so that ⌊256 · c⌋ ≤ 255. Components are never negative, so only the top
		// needs clamping; multiplying by 256 is exact, so the product cannot round up to 256.
		const double max_intensity = std::nextafter(1.0, 0.0);
		std::array<std::uint8_t, 3> channels{};
		for (std::size_t i = 0; i < channels.size(); ++i) {
			channels.at(i) = static_cast<std::uint8_t>(256.0 * std::min(rgb_[i], max_intensity));
		}
		return channels;
	}

	friend std::ostream& operator<<(std::ostream& stream, const Colour& colour) { return stream << "rgb" << colour.rgb_; }

private:
	static constexpr Vec3 white_rgb{1, 1, 1};

	Vec3 rgb_;

	/// For results that keep the invariant by construction.
	[[nodiscard]] static constexpr Colour unchecked(const Vec3& rgb) noexcept {
		Colour colour;
		colour.rgb_ = rgb;
		return colour;
	}

	static constexpr void require_non_negative(double value, const char* message) {
		if (value < 0.0) {
			throw MathsError{message};
		}
	}

	[[nodiscard]] static constexpr Vec3 validated(const Vec3& rgb) {
		for (std::size_t i = 0; i < 3; ++i) {
			require_non_negative(rgb[i], "colour components must not be negative");
		}
		return rgb;
	}
};

} // namespace rt

#endif // RT_COLOUR_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
