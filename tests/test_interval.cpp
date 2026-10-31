#include "rt/errors.hpp"
#include "rt/math/interval.hpp"

#include <rapidcheck/Assertions.h>

#include <cmath>
#include <concepts>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

// Compile-time checks of construction, deduction and the static constants.
static_assert(std::same_as<decltype(Interval{0.0, 1.0}), Interval<double>>);
static_assert(Interval<double>{} == Interval<double>::empty());
static_assert(Interval<double>::empty().is_empty());
static_assert(!Interval<double>::universe().is_empty());
static_assert(Interval{0.0, 1.0}.clamp(2.0) == 1.0);
static_assert(std::is_trivially_copyable_v<Interval<double>>);

template <std::floating_point T>
void check_properties(Suite& suite, const std::string& label) {
	using I = Interval<T>;

	suite.check(label + ": clamp(x) always lies in the interval", [] {
		const I interval = *nonempty_interval<T>();
		const T x = *extended_scalar<T>();
		const T clamped = interval.clamp(x);
		RC_ASSERT(interval.min() <= clamped);
		RC_ASSERT(clamped <= interval.max());
	});

	suite.check(label + ": the interval contains clamp(x)", [] {
		const I interval = *nonempty_interval<T>();
		RC_ASSERT(interval.contains(interval.clamp(*extended_scalar<T>())));
	});

	suite.check(label + ": clamp leaves contained values unchanged", [] {
		const I interval = *nonempty_interval<T>();
		const T x = *extended_scalar<T>();
		RC_PRE(interval.contains(x));
		RC_ASSERT(interval.clamp(x) == x);
	});

	suite.check(label + ": clamp is idempotent", [] {
		const I interval = *nonempty_interval<T>();
		const T clamped = interval.clamp(*extended_scalar<T>());
		RC_ASSERT(interval.clamp(clamped) == clamped);
	});

	suite.check(label + ": clamp moves outside values to the nearest bound", [] {
		const I interval = *nonempty_interval<T>();
		const T x = *extended_scalar<T>();
		RC_PRE(!interval.contains(x));
		RC_ASSERT(interval.clamp(x) == (x < interval.min() ? interval.min() : interval.max()));
	});

	suite.check(label + ": surrounds implies contains", [](const I& interval) {
		const T x = *extended_scalar<T>();
		RC_PRE(interval.surrounds(x));
		RC_ASSERT(interval.contains(x));
	});

	suite.check(label + ": the bounds are contained but not surrounded", [] {
		const I interval = *nonempty_interval<T>();
		RC_ASSERT(interval.contains(interval.min()));
		RC_ASSERT(interval.contains(interval.max()));
		RC_ASSERT(!interval.surrounds(interval.min()));
		RC_ASSERT(!interval.surrounds(interval.max()));
	});

	suite.check(label + ": an empty interval contains nothing", [](const I& interval) {
		RC_PRE(interval.is_empty());
		RC_ASSERT(!interval.contains(*extended_scalar<T>()));
	});

	suite.check(label + ": the universe surrounds every finite number",
	            [] { RC_ASSERT(I::universe().surrounds(*finite_scalar<T>())); });

	suite.check(label + ": the empty constant contains nothing",
	            [] { RC_ASSERT(!I::empty().contains(*extended_scalar<T>())); });

	suite.check(label + ": clamping to an empty interval throws MathError",
	            [] { RC_ASSERT_THROWS_AS(static_cast<void>(I::empty().clamp(*extended_scalar<T>())), MathError); });
}

void check_examples(Suite& suite) {
	constexpr double infinity = std::numeric_limits<double>::infinity();
	constexpr double nan = std::numeric_limits<double>::quiet_NaN();

	suite.check("ray parameter range [0.001, inf)", [] {
		const Interval range{0.001, infinity};
		RC_ASSERT(range.contains(0.001));
		RC_ASSERT(!range.surrounds(0.001));
		RC_ASSERT(range.surrounds(1.0));
		RC_ASSERT(!range.contains(0.0));
		RC_ASSERT(range.contains(infinity));
		RC_ASSERT(!range.surrounds(infinity));
	});

	suite.check("clamp to the unit interval", [] {
		const Interval unit{0.0, 1.0};
		RC_ASSERT(unit.clamp(-0.5) == 0.0);
		RC_ASSERT(unit.clamp(0.25) == 0.25);
		RC_ASSERT(unit.clamp(1.5) == 1.0);
		RC_ASSERT(unit.clamp(infinity) == 1.0);
	});

	suite.check("the universe contains the infinities but does not surround them", [] {
		RC_ASSERT(Interval<double>::universe().contains(-infinity));
		RC_ASSERT(Interval<double>::universe().contains(infinity));
		RC_ASSERT(!Interval<double>::universe().surrounds(infinity));
	});

	suite.check("NaN bounds throw MathError", [] {
		RC_ASSERT_THROWS_AS(Interval(nan, 1.0), MathError);
		RC_ASSERT_THROWS_AS(Interval(0.0, nan), MathError);
	});

	suite.check("NaN is in no interval and clamps to NaN", [] {
		RC_ASSERT(!Interval<double>::universe().contains(nan));
		RC_ASSERT(std::isnan(Interval{0.0, 1.0}.clamp(nan)));
	});

	suite.check("stream output", [] {
		std::ostringstream stream;
		stream << Interval{-1.0, 2.5};
		RC_ASSERT(stream.str() == "[-1, 2.5]");
	});
}

} // namespace
} // namespace rt::test

int main() {
	rt::test::Suite suite;
	rt::test::check_examples(suite);
	rt::test::check_properties<double>(suite, "Interval<double>");
	rt::test::check_properties<float>(suite, "Interval<float>");
	return suite.exit_code();
}

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
