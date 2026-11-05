#include "rt/render/worker_pool.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace rt {

void WorkerPool::run(std::size_t task_count, const std::function<void(std::size_t)>& task) const {
	std::atomic<std::size_t> next{0};
	std::stop_source stop;
	std::mutex failure_mutex;
	std::exception_ptr failure;

	// Each thread checks the shared token, so that one failing task stops them all.
	const auto work = [&, token = stop.get_token()] {
		while (!token.stop_requested()) {
			const std::size_t index = next.fetch_add(1, std::memory_order_relaxed);
			if (index >= task_count) {
				return;
			}
			try {
				task(index);
			} catch (...) {
				const std::scoped_lock lock{failure_mutex};
				if (!failure) {
					failure = std::current_exception();
				}
				stop.request_stop();
				return;
			}
		}
	};

	{
		std::vector<std::jthread> workers;
		const std::size_t count = std::min(threads_, task_count);
		workers.reserve(count);
		try {
			for (std::size_t i = 0; i < count; ++i) {
				workers.emplace_back(work);
			}
		} catch (...) {
			// The threads already started stop after their current task and are joined as `workers` goes.
			stop.request_stop();
			throw;
		}
	} // Joins every worker; their writes, `failure` included, are visible from here on.

	if (failure) {
		std::rethrow_exception(failure);
	}
}

} // namespace rt

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
