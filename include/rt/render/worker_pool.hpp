#ifndef RT_RENDER_WORKER_POOL_HPP
#define RT_RENDER_WORKER_POOL_HPP

#include "rt/errors.hpp"
#include "rt/validation.hpp"

#include <cstddef>
#include <functional>

namespace rt {

/// Runs numbered tasks on a fixed number of threads (README.md § 6.6).
///
/// Each call to `run` starts its own `std::jthread`s, which take the next unclaimed index from a shared atomic
/// counter until none is left, so faster threads take more tasks. The first exception thrown by a task stops the
/// other threads from claiming further tasks and is re-thrown by `run` once all of them have finished.
class WorkerPool {
public:
	/// @throws RangeError if `threads` is 0.
	explicit WorkerPool(std::size_t threads)
			: threads_{validation::validated<RangeError>(threads, validation::check_threads)} {}

	WorkerPool(const WorkerPool&) = delete;
	WorkerPool& operator=(const WorkerPool&) = delete;
	WorkerPool(WorkerPool&&) = delete;
	WorkerPool& operator=(WorkerPool&&) = delete;
	~WorkerPool() = default;

	[[nodiscard]] std::size_t threads() const noexcept { return threads_; }

	/// Calls `task(i)` exactly once for every `i` in `[0, task_count)`, on at most `threads()` threads (never more
	/// threads than tasks), and returns when all calls have finished. `task` must be safe to call concurrently.
	///
	/// If a task throws, the remaining unclaimed tasks are skipped, the tasks already running finish, and the first
	/// exception is re-thrown here.
	///
	/// @throws std::system_error if a thread cannot be started.
	void run(std::size_t task_count, const std::function<void(std::size_t)>& task) const;

private:
	std::size_t threads_;
};

} // namespace rt

#endif // RT_RENDER_WORKER_POOL_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
