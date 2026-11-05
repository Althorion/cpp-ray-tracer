#include "rt/errors.hpp"
#include "rt/render/worker_pool.hpp"

#include <rapidcheck/Assertions.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

static_assert(!std::is_copy_constructible_v<WorkerPool>);
static_assert(!std::is_copy_assignable_v<WorkerPool>);

/// Thrown by test tasks, carrying the index of the task.
struct TaskFailure : std::exception {
	explicit TaskFailure(std::size_t failed) : index{failed} {}

	std::size_t index;
};

/// Up to this many tasks per test case; enough for the threads to contend for them.
constexpr std::size_t max_tasks = 2000;

void check_properties(Suite& suite) {
	suite.check("every task runs exactly once", [] {
		const WorkerPool pool{*thread_count()};
		const std::size_t task_count = *index_below(max_tasks);
		std::vector<std::atomic<int>> runs(task_count);
		pool.run(task_count, [&](std::size_t i) { runs[i].fetch_add(1, std::memory_order_relaxed); });
		for (const auto& count : runs) {
			RC_ASSERT(count.load() == 1);
		}
	});

	suite.check("tasks run on at most the given number of threads, and never on the caller's", [] {
		const WorkerPool pool{*thread_count()};
		const std::size_t task_count = *index_below(max_tasks);
		std::mutex mutex;
		std::set<std::thread::id> threads;
		pool.run(task_count, [&](std::size_t /*index*/) {
			const std::scoped_lock lock{mutex};
			threads.insert(std::this_thread::get_id());
		});
		RC_ASSERT(threads.size() <= std::min(pool.threads(), task_count));
		RC_ASSERT(!threads.contains(std::this_thread::get_id()));
	});

	suite.check("a task's exception is re-thrown by run", [] {
		const WorkerPool pool{*thread_count()};
		const std::size_t task_count = *index_below(max_tasks) + 1;
		const std::size_t failing = *index_below(task_count);
		try {
			pool.run(task_count, [&](std::size_t i) {
				if (i == failing) {
					throw TaskFailure{i};
				}
			});
			RC_FAIL("no exception thrown");
		} catch (const TaskFailure& failure) {
			RC_ASSERT(failure.index == failing);
		}
	});

	suite.check("after a failure each thread stops: when every task throws, each runs at most one", [] {
		const WorkerPool pool{*thread_count()};
		const std::size_t task_count = *index_below(max_tasks) + 1;
		std::atomic<std::size_t> started{0};
		RC_ASSERT_THROWS_AS(pool.run(task_count,
		                             [&](std::size_t i) {
																	 started.fetch_add(1, std::memory_order_relaxed);
																	 throw TaskFailure{i};
																 }),
		                    TaskFailure);
		RC_ASSERT(started.load() >= 1U);
		RC_ASSERT(started.load() <= pool.threads());
	});

	suite.check("with one thread, tasks run in order and stop at the first failure", [] {
		const WorkerPool pool{1};
		const std::size_t task_count = *index_below(max_tasks) + 1;
		const std::size_t failing = *index_below(task_count);
		std::vector<std::size_t> order;
		RC_ASSERT_THROWS_AS(pool.run(task_count,
		                             [&](std::size_t i) {
																	 order.push_back(i);
																	 if (i == failing) {
																		 throw TaskFailure{i};
																	 }
																 }),
		                    TaskFailure);
		RC_ASSERT(order.size() == failing + 1);
		for (std::size_t i = 0; i < order.size(); ++i) {
			RC_ASSERT(order[i] == i);
		}
	});

	suite.check("a pool can run again after a failure", [] {
		const WorkerPool pool{*thread_count()};
		RC_ASSERT_THROWS_AS(pool.run(10, [](std::size_t i) { throw TaskFailure{i}; }), TaskFailure);
		std::atomic<std::size_t> completed{0};
		pool.run(100, [&](std::size_t /*index*/) { completed.fetch_add(1, std::memory_order_relaxed); });
		RC_ASSERT(completed.load() == 100U);
	});
}

void check_examples(Suite& suite) {
	suite.check("zero threads throw RangeError", [] { RC_ASSERT_THROWS_AS(WorkerPool{0}, RangeError); });

	suite.check("no tasks: nothing runs and nothing is thrown", [] {
		const WorkerPool pool{4};
		std::atomic<int> runs{0};
		pool.run(0, [&](std::size_t /*index*/) { runs.fetch_add(1); });
		RC_ASSERT(runs.load() == 0);
	});

	suite.check("exceptions of any type are re-thrown as they are", [] {
		const WorkerPool pool{3};
		RC_ASSERT_THROWS_AS(pool.run(5, [](std::size_t /*index*/) { throw std::runtime_error{"boom"}; }),
		                    std::runtime_error);
		RC_ASSERT_THROWS_AS(pool.run(5, [](std::size_t /*index*/) { throw RangeError{"boom"}; }), RangeError);
	});

	suite.check("independent tasks may write their own slots without synchronisation", [] {
		// Run under ThreadSanitizer, this checks that `run` makes the tasks' writes visible to the caller.
		const WorkerPool pool{8};
		std::vector<std::size_t> squares(1000);
		pool.run(squares.size(), [&](std::size_t i) { squares[i] = i * i; });
		for (std::size_t i = 0; i < squares.size(); ++i) {
			RC_ASSERT(squares[i] == i * i);
		}
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
