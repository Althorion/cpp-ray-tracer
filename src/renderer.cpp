#include "rt/render/renderer.hpp"

#include "rt/colour.hpp"
#include "rt/errors.hpp"
#include "rt/maths/ray.hpp"
#include "rt/random/sampler.hpp"
#include "rt/render/image.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>

namespace rt {

Image Renderer::render() const {
	Image image{settings_.width(), settings_.height()};
	// Every task writes only through its own row, so the threads never write to the same pixel.
	pool_.run(settings_.height(), [&](std::size_t y) {
		std::ranges::transform(std::views::iota(0UZ, settings_.width()), image.row(y).begin(),
		                       [&](std::size_t x) { return pixel(x, y); });
	});
	return image;
}

Colour Renderer::trace(Ray ray, Sampler& sampler) const {
	// Iterative rather than recursive (README.md § 6.2), so a large depth cannot overflow the stack.
	Colour throughput = Colour::white();
	for (std::size_t depth = 0; depth < settings_.max_depth(); ++depth) {
		const auto record = scene_->hit(ray, ahead);
		if (!record) {
			return throughput * scene_->sky_colour(ray.direction());
		}
		if (record->material == nullptr) {
			throw Error{"a surface was hit that has no material"};
		}
		const auto scattered = record->material->scatter(ray, *record, sampler);
		if (!scattered) {
			return Colour::black();
		}
		throughput *= scattered->attenuation;
		ray = scattered->ray;
	}
	return Colour::black();
}

Colour Renderer::pixel(std::size_t x, std::size_t y) const {
	Sampler sampler = Sampler::for_pixel(settings_.seed(), x, y, settings_.width());
	Colour sum;
	for (std::size_t sample = 0; sample < settings_.samples(); ++sample) {
		sum += trace(scene_->camera().ray_for(x, y, settings_.resolution(), sampler), sampler);
	}
	return sum / static_cast<double>(settings_.samples());
}

} // namespace rt

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
