#ifndef RT_ERRORS_HPP
#define RT_ERRORS_HPP

#include <stdexcept>
#include <string>
#include <string_view>

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

/// The scene is invalid, e.g. a camera looking along its own `up` vector.
class SceneError : public Error {
public:
	explicit SceneError(const std::string& message);
};

/// A second object was registered under a name already taken.
class DuplicateNameError : public SceneError {
public:
	explicit DuplicateNameError(std::string_view name);

	[[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
	std::string name_;
};

/// No object is registered under the name looked up.
class UnknownNameError : public SceneError {
public:
	explicit UnknownNameError(std::string_view name);

	[[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
	std::string name_;
};

} // namespace rt

#endif // RT_ERRORS_HPP

// Local Variables:
// ispell-local-dictionary: "en-GB"
// End:
