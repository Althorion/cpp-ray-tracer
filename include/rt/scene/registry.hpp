#ifndef RT_SCENE_REGISTRY_HPP
#define RT_SCENE_REGISTRY_HPP

#include "rt/errors.hpp"
#include "rt/materials/material.hpp"

#include <concepts>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace rt {

/// Named objects, looked up by the name they report themselves (README.md § 7.2).
///
/// The registry shares ownership of its objects with everyone who looks them up, e.g. the spheres made of a
/// material.
template <typename T>
	requires std::derived_from<T, INamed>
class Registry {
public:
	/// Registers `item` under `item->name()`; on failure the registry is left unchanged.
	///
	/// @throws Error if `item` is null.
	/// @throws DuplicateNameError if the name is already taken.
	void add(const std::shared_ptr<T>& item) {
		if (item == nullptr) {
			throw Error{"cannot register a null object"};
		}
		const std::string_view name = item->name();
		if (!items_.try_emplace(std::string{name}, item).second) {
			throw DuplicateNameError{name};
		}
	}

	/// @throws UnknownNameError if nothing is registered under `name`.
	[[nodiscard]] const std::shared_ptr<T>& get(std::string_view name) const {
		const auto found = items_.find(name);
		if (found == items_.end()) {
			throw UnknownNameError{name};
		}
		return found->second;
	}

	[[nodiscard]] bool contains(std::string_view name) const { return items_.contains(name); }

	/// Number of registered (hence distinct) names.
	[[nodiscard]] std::size_t size() const noexcept { return items_.size(); }

private:
	// `std::less<>` allows lookups by `std::string_view` without building a `std::string`.
	std::map<std::string, std::shared_ptr<T>, std::less<>> items_;
};

} // namespace rt

#endif // RT_SCENE_REGISTRY_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
