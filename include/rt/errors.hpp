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
class MathsError : public Error {
public:
	explicit MathsError(const std::string& message);
};

/// An index or a size outside the permitted range, e.g. a pixel outside the image.
class RangeError : public Error {
public:
	explicit RangeError(const std::string& message);
};

} // namespace rt

#endif // RT_ERRORS_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
