#include "rt/colour.hpp"
#include "rt/errors.hpp"
#include "rt/render/image.hpp"

#include <rapidcheck/Assertions.h>

#include <cstddef>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

static_assert(std::is_copy_constructible_v<Image>);
static_assert(std::is_nothrow_move_constructible_v<Image>);

void check_properties(Suite& suite) {
	suite.check("a new image is black and has the requested size", [] {
		const std::size_t width = *image_side();
		const std::size_t height = *image_side();
		const Image image{width, height};
		RC_ASSERT(image.width() == width);
		RC_ASSERT(image.height() == height);
		RC_ASSERT(image.pixels().size() == width * height);
		for (const Colour& pixel : image.pixels()) {
			RC_ASSERT(pixel == Colour::black());
		}
	});

	suite.check("a pixel reads back what was written, and nothing else changes", [](Image image) {
		const std::size_t x = *index_below(image.width());
		const std::size_t y = *index_below(image.height());
		const Colour colour = *bright_colour();
		const Image before = image;
		image[x, y] = colour;
		RC_ASSERT((std::as_const(image)[x, y] == colour));
		for (std::size_t j = 0; j < image.height(); ++j) {
			for (std::size_t i = 0; i < image.width(); ++i) {
				if (i != x || j != y) {
					RC_ASSERT((image[i, j] == before[i, j]));
				}
			}
		}
	});

	suite.check("pixels are stored row by row from the top-left corner", [](const Image& image) {
		const std::size_t x = *index_below(image.width());
		const std::size_t y = *index_below(image.height());
		RC_ASSERT((image.pixels()[(y * image.width()) + x] == image[x, y]));
	});

	suite.check("a row is a view of the image's pixels", [](Image image) {
		const std::size_t x = *index_below(image.width());
		const std::size_t y = *index_below(image.height());
		const std::span<Colour> row = image.row(y);
		RC_ASSERT(row.size() == image.width());
		row[x] = Colour{2, 3, 4};
		RC_ASSERT((image[x, y] == Colour{2, 3, 4}));
	});

	suite.check("access outside the image throws RangeError", [](Image image) {
		const std::size_t beyond = *index_below(1000);
		RC_ASSERT_THROWS_AS((void)(image[image.width() + beyond, 0]), RangeError);
		RC_ASSERT_THROWS_AS((void)(image[0, image.height() + beyond]), RangeError);
		RC_ASSERT_THROWS_AS((void)(std::as_const(image)[image.width() + beyond, 0]), RangeError);
		RC_ASSERT_THROWS_AS((void)image.row(image.height() + beyond), RangeError);
	});

	suite.check("a copy is independent of the original", [](const Image& image) {
		Image copy = image;
		RC_ASSERT(copy == image);
		copy[0, 0] = copy[0, 0] + Colour::white();
		RC_ASSERT(copy != image);
	});
}

void check_examples(Suite& suite) {
	suite.check("an image without pixels throws RangeError", [] {
		RC_ASSERT_THROWS_AS(Image(0, 1), RangeError);
		RC_ASSERT_THROWS_AS(Image(1, 0), RangeError);
	});

	suite.check("an image too large to address throws RangeError rather than allocating", [] {
		constexpr std::size_t huge = std::numeric_limits<std::size_t>::max() / 2;
		RC_ASSERT_THROWS_AS(Image(huge, huge), RangeError);
		RC_ASSERT_THROWS_AS(Image(std::numeric_limits<std::size_t>::max(), 1), RangeError);
	});

	suite.check("linear colours above 1 are kept as they are", [] {
		Image image{2, 1};
		image[1, 0] = Colour{1.5, 20, 0.25};
		RC_ASSERT((image[1, 0] == Colour{1.5, 20, 0.25}));
	});

	suite.check("images of different sizes differ, even when all black", [] {
		RC_ASSERT(Image(2, 3) != Image(3, 2));
		RC_ASSERT(Image(2, 3) == Image(2, 3));
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
