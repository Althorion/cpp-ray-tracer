#ifndef RT_RENDER_RENDER_SETTINGS_HPP
#define RT_RENDER_RENDER_SETTINGS_HPP

#include "rt/errors.hpp"
#include "rt/scene/camera.hpp"
#include "rt/validation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <thread>

namespace rt {

/// The number of hardware threads, or 1 if it cannot be determined (README.md § 4).
[[nodiscard]] inline std::size_t default_thread_count() noexcept {
	return std::max<std::size_t>(1, std::thread::hardware_concurrency());
}

/// How to render: the image size, the sampling and the parallelism (README.md § 4, § 7.2).
///
/// Validated on construction and immutable afterwards, so every `RenderSettings` is usable as is.
class RenderSettings {
public:
	/// The raw values, with the defaults of the command line, for designated initialisation.
	struct Parameters {
		Resolution resolution;
		std::size_t samples = 100;
		std::size_t max_depth = 50;
		std::uint64_t seed = 0;
		std::size_t threads = default_thread_count();
	};

	/// @throws RangeError unless the width and the height lie in [1, 16384], and there are at least one sample per
	/// pixel and one thread. Any `max_depth` and `seed` are valid; a depth of 0 renders a black image.
	explicit RenderSettings(const Parameters& parameters)
			: resolution_{
						.width = validation::validated<RangeError>(parameters.resolution.width, validation::check_image_side),
						.height = validation::validated<RangeError>(parameters.resolution.height, validation::check_image_side),
				},
				samples_{validation::validated<RangeError>(parameters.samples, validation::check_samples)},
				max_depth_{parameters.max_depth},
				seed_{parameters.seed},
				threads_{validation::validated<RangeError>(parameters.threads, validation::check_threads)} {}

	[[nodiscard]] Resolution resolution() const noexcept { return resolution_; }
	[[nodiscard]] std::size_t width() const noexcept { return resolution_.width; }
	[[nodiscard]] std::size_t height() const noexcept { return resolution_.height; }
	[[nodiscard]] std::size_t samples() const noexcept { return samples_; }
	[[nodiscard]] std::size_t max_depth() const noexcept { return max_depth_; }
	[[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
	[[nodiscard]] std::size_t threads() const noexcept { return threads_; }

	[[nodiscard]] friend bool operator==(const RenderSettings&, const RenderSettings&) noexcept = default;

private:
	Resolution resolution_;
	std::size_t samples_;
	std::size_t max_depth_;
	std::uint64_t seed_;
	std::size_t threads_;
};

} // namespace rt

#endif // RT_RENDER_RENDER_SETTINGS_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
