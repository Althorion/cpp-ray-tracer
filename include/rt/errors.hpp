#ifndef RT_ERRORS_HPP
#define RT_ERRORS_HPP

#include <stdexcept>
#include <string>

namespace rt {

/// Base class of every exception thrown by the ray tracer.
class Error : public std::runtime_error {
public:
	explicit Error(const std::string& message);
};

/// A mathematical operation was applied outside its domain, e.g. normalising a zero vector.
class MathError : public Error {
public:
	explicit MathError(const std::string& message);
};

} // namespace rt

#endif // RT_ERRORS_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
