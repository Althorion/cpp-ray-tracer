#include "rt/colour.hpp"
#include "rt/errors.hpp"
#include "rt/materials/lambertian.hpp"
#include "rt/materials/material.hpp"
#include "rt/scene/registry.hpp"

#include <rapidcheck/Assertions.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "generators.hpp"
#include "harness.hpp"

namespace rt::test {
namespace {

/// Whether `Registry<T>` names a valid type, i.e. `T` satisfies the registry's constraint.
template <typename T>
concept registrable = requires { typename Registry<T>; };

static_assert(registrable<Material>);
static_assert(registrable<Lambertian>);
static_assert(!registrable<int>);
static_assert(!registrable<Colour>);

[[nodiscard]] std::shared_ptr<Material> named(const std::string& name) {
	return std::make_shared<Lambertian>(name, Colour{0.5, 0.5, 0.5});
}

void check_properties(Suite& suite) {
	suite.check("get returns the very object added", [] {
		const std::string name = *identifier();
		const auto extra_names = *identifiers();
		Registry<Material> registry;
		const auto material = named(name);
		registry.add(material);
		for (const auto& extra : extra_names) {
			if (extra != name && !registry.contains(extra)) {
				registry.add(named(extra));
			}
		}
		RC_ASSERT(registry.contains(name));
		RC_ASSERT(registry.get(name) == material);
	});

	suite.check("adding a taken name throws DuplicateNameError and changes nothing", [] {
		const std::string name = *identifier();
		Registry<Material> registry;
		const auto original = named(name);
		registry.add(original);
		const auto duplicate = named(name);
		try {
			registry.add(duplicate);
			RC_FAIL("no exception thrown");
		} catch (const DuplicateNameError& error) {
			RC_ASSERT(error.name() == name);
		}
		RC_ASSERT(registry.size() == 1U);
		RC_ASSERT(registry.get(name) == original);
		RC_ASSERT(duplicate.use_count() == 1); // not kept by the registry
	});

	suite.check("getting an unknown name throws UnknownNameError", [] {
		const auto names = *identifiers();
		const std::string missing = *identifier();
		RC_PRE(std::ranges::find(names, missing) == names.end());
		Registry<Material> registry;
		for (const auto& name : names) {
			if (!registry.contains(name)) {
				registry.add(named(name));
			}
		}
		RC_ASSERT(!registry.contains(missing));
		try {
			(void)registry.get(missing);
			RC_FAIL("no exception thrown");
		} catch (const UnknownNameError& error) {
			RC_ASSERT(error.name() == missing);
		}
	});

	suite.check("size equals the number of distinct names added", [] {
		const auto names = *identifiers();
		Registry<Material> registry;
		std::size_t duplicates = 0;
		for (const auto& name : names) {
			try {
				registry.add(named(name));
			} catch (const DuplicateNameError&) {
				++duplicates;
			}
		}
		const std::size_t distinct = std::ranges::to<std::set>(names).size();
		RC_ASSERT(registry.size() == distinct);
		RC_ASSERT(duplicates == names.size() - distinct);
		for (const auto& name : names) {
			RC_ASSERT(registry.contains(name));
		}
	});

	suite.check("the registry shares its objects rather than copying them", [] {
		const std::string name = *identifier();
		Registry<Material> registry;
		const auto material = named(name);
		registry.add(material);
		RC_ASSERT(registry.get(name) == material);
		RC_ASSERT(material.use_count() == 2);
	});
}

void check_examples(Suite& suite) {
	suite.check("an empty registry", [] {
		const Registry<Material> registry;
		RC_ASSERT(registry.size() == 0U);
		RC_ASSERT(!registry.contains("ground"));
		RC_ASSERT_THROWS_AS((void)registry.get("ground"), UnknownNameError);
	});

	suite.check("names are case-sensitive", [] {
		Registry<Material> registry;
		registry.add(named("gold"));
		registry.add(named("Gold"));
		RC_ASSERT(registry.size() == 2U);
		RC_ASSERT(registry.get("gold")->name() == std::string_view{"gold"});
		RC_ASSERT(registry.get("Gold")->name() == std::string_view{"Gold"});
	});

	suite.check("registering null throws Error", [] {
		Registry<Material> registry;
		RC_ASSERT_THROWS_AS(registry.add(nullptr), Error);
		RC_ASSERT(registry.size() == 0U);
	});

	suite.check("name errors are scene errors and report the name", [] {
		const DuplicateNameError duplicate{"red"};
		const UnknownNameError unknown{"blue"};
		RC_ASSERT(duplicate.name() == "red");
		RC_ASSERT(unknown.name() == "blue");
		RC_ASSERT(std::string_view{duplicate.what()}.contains("red"));
		RC_ASSERT(std::string_view{unknown.what()}.contains("blue"));
		RC_ASSERT(dynamic_cast<const SceneError*>(&duplicate) != nullptr);
		RC_ASSERT(dynamic_cast<const SceneError*>(&unknown) != nullptr);
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
