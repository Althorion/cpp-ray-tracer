#include "rt/errors.hpp"

#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rt {

Error::Error(const std::string& message) : std::runtime_error{message} {}

MathsError::MathsError(const std::string& message) : Error{message} {}

RangeError::RangeError(const std::string& message) : Error{message} {}

SceneError::SceneError(const std::string& message) : Error{message} {}

DuplicateNameError::DuplicateNameError(std::string_view name)
		: SceneError{std::format("duplicate name '{}'", name)}, name_{name} {}

UnknownNameError::UnknownNameError(std::string_view name)
		: SceneError{std::format("unknown name '{}'", name)}, name_{name} {}

} // namespace rt

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
