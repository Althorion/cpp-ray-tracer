#ifndef RT_RENDER_RENDERER_HPP
#define RT_RENDER_RENDERER_HPP

#include "rt/colour.hpp"
#include "rt/maths/interval.hpp"
#include "rt/maths/ray.hpp"
#include "rt/random/sampler.hpp"
#include "rt/render/image.hpp"
#include "rt/render/render_settings.hpp"
#include "rt/render/worker_pool.hpp"
#include "rt/scene/scene.hpp"

#include <cstddef>
#include <limits>

namespace rt {

/// Renders a scene into an image of linear colours by path tracing (README.md § 6.2, § 7.2).
///
/// It refers to the scene (aggregation), which must outlive it, and owns the worker pool (composition). Each pixel
/// draws from its own generator stream, so the image depends only on the scene and the settings, not on the number
/// of threads or the order in which they work (README.md § 6.5).
class Renderer {
public:
	/// `scene` must outlive the renderer; `settings` is copied.
	Renderer(const Scene& scene, const RenderSettings& settings)
			: scene_{&scene}, settings_{settings}, pool_{settings.threads()} {}

	Renderer(const Scene&&, const RenderSettings&) = delete ("the renderer keeps a reference to the scene, which a "
	                                                         "temporary would not outlive");

	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;
	Renderer& operator=(Renderer&&) = delete;
	~Renderer() = default;

	[[nodiscard]] const RenderSettings& settings() const noexcept { return settings_; }

	/// The whole image: every pixel the average of `samples` traced paths, neither gamma-corrected nor clamped
	/// (README.md § 6.4). Rows are rendered in parallel, each one by a single thread.
	///
	/// @throws Error (or a class derived from it) if tracing fails, re-thrown from the thread it happened in.
	[[nodiscard]] Image render() const;

	/// The light carried back along `ray`, following at most `max_depth` surface interactions: the sky colour
	/// filtered by every attenuation on the way, or black if the light is absorbed or the depth runs out.
	///
	/// @throws Error if a surface hit has no material.
	[[nodiscard]] Colour trace(Ray ray, Sampler& sampler) const;

	/// Nearest distance along a ray at which a hit counts: a little above zero, so that a scattered ray does not hit
	/// the surface it starts on again because of rounding (README.md § 6.2).
	static constexpr Interval<double> ahead{0.001, std::numeric_limits<double>::infinity()};

private:
	const Scene* scene_;
	RenderSettings settings_;
	WorkerPool pool_;

	[[nodiscard]] Colour pixel(std::size_t x, std::size_t y) const;
};

} // namespace rt

#endif // RT_RENDER_RENDERER_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
