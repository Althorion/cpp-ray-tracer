#include "rt/errors.hpp"

#include <stdexcept>
#include <string>

namespace rt {

Error::Error(const std::string& message) : std::runtime_error{message} {}

MathsError::MathsError(const std::string& message) : Error{message} {}

RangeError::RangeError(const std::string& message) : Error{message} {}

} // namespace rt

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
