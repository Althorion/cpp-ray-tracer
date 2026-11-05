#ifndef RT_RENDER_IMAGE_HPP
#define RT_RENDER_IMAGE_HPP

#include "rt/colour.hpp"
#include "rt/errors.hpp"

#include <cstddef>
#include <format>
#include <span>
#include <vector>

namespace rt {

/// A rectangular grid of linear colours, as the renderer produces them (README.md § 6.4, § 7.2): averages of samples,
/// neither gamma-corrected nor clamped, so components may exceed 1. Converting them for a file is the writer's job.
///
/// Pixels are addressed as `image[x, y]`, with `(0, 0)` the top-left corner, and stored row by row from the top.
class Image {
public:
	/// A black image.
	///
	/// @throws RangeError if either side is 0, or the image has more pixels than a `std::vector` can hold.
	Image(std::size_t width, std::size_t height) : width_{width}, height_{height}, pixels_(checked_size(width, height)) {}

	[[nodiscard]] std::size_t width() const noexcept { return width_; }
	[[nodiscard]] std::size_t height() const noexcept { return height_; }

	/// @throws RangeError unless `x < width` and `y < height`.
	[[nodiscard]] Colour& operator[](std::size_t x, std::size_t y) { return pixels_[index(x, y)]; }

	/// @throws RangeError unless `x < width` and `y < height`.
	[[nodiscard]] const Colour& operator[](std::size_t x, std::size_t y) const { return pixels_[index(x, y)]; }

	/// All pixels, row by row from the top, each row from the left.
	[[nodiscard]] std::span<const Colour> pixels() const noexcept { return pixels_; }

	/// Row `y`, from the left; writing through it touches no other row, so threads may fill different rows at once.
	///
	/// @throws RangeError unless `y < height`.
	[[nodiscard]] std::span<Colour> row(std::size_t y) { return std::span{pixels_}.subspan(index(0, y), width_); }

	[[nodiscard]] friend bool operator==(const Image&, const Image&) = default;

private:
	std::size_t width_;
	std::size_t height_;
	std::vector<Colour> pixels_;

	[[nodiscard]] static std::size_t checked_size(std::size_t width, std::size_t height) {
		if (width == 0 || height == 0) {
			throw RangeError{std::format("an image must have at least one pixel, not {}x{}", width, height)};
		}
		if (height > std::vector<Colour>{}.max_size() / width) {
			throw RangeError{std::format("a {}x{} image is too large", width, height)};
		}
		return width * height;
	}

	[[nodiscard]] std::size_t index(std::size_t x, std::size_t y) const {
		if (x >= width_ || y >= height_) {
			throw RangeError{std::format("pixel ({}, {}) lies outside a {}x{} image", x, y, width_, height_)};
		}
		return (y * width_) + x;
	}
};

} // namespace rt

#endif // RT_RENDER_IMAGE_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
