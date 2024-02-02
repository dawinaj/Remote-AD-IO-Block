/// @file
/// Generic and safe strtod wrapper for Modern C++.

#ifndef TO_FLOATING_POINT_HPP_
#define TO_FLOATING_POINT_HPP_

#include <cwchar>
#include <cctype>
#include <string>
#include <type_traits>

/// Converts the string representation of a number to its floating-point number equivalent.
/// @param str the string containing a number to convert.
/// @tparam TNumber an floating-point number type.
/// @return the floating-point number value equivalent of the number contained in str.
/// @throws std::invalid_argument if str contains an invalid character which cannot be interpreted.
/// @throws std::out_of_range if the converted value would fall out of the range of the result type.
template <typename TNumber>
TNumber to_floating_point(
	const std::string& str) {
	const char* ptr = str.c_str();
	char* endptr;
	errno = 0;
	double ans = std::strtod(ptr, &endptr);

	if (ptr == endptr) {
		throw std::invalid_argument("invalid to_floating_point argument");
	}

	while (*endptr != '\0') {
		if (std::isspace(*endptr) == 0) {
			throw std::invalid_argument("invalid to_floating_point argument");
		}
		++endptr;
	}

	if (errno == ERANGE ||
		ans < std::numeric_limits<TNumber>::lowest() ||
		ans > std::numeric_limits<TNumber>::max()) {
		throw std::out_of_range("to_floating_point argument out of range");
	}

	return static_cast<TNumber>(ans);
}

/// Converts the string representation of a number to its floating-point number equivalent. A return value indicates whether the conversion succeeded.
/// @param str the string containing a number to convert.
/// @param result an floating-point number variable to store the floating-point number value equivalent of the number contained in str.
/// @tparam TNumber an floating-point number type.
/// @return true if str was converted successfully.
template <typename TNumber>
bool try_parse_floating_point(
	const std::string& str,
	TNumber& result) noexcept {
	const char* ptr = str.c_str();
	char* endptr;
	errno = 0;
	double ans = std::strtod(ptr, &endptr);

	if (ptr == endptr) {
		return false;
	}

	while (*endptr != '\0') {
		if (std::isspace(*endptr) == 0) {
			return false;
		}
		++endptr;
	}

	if (errno == ERANGE ||
		ans < std::numeric_limits<TNumber>::lowest() ||
		ans > std::numeric_limits<TNumber>::max()) {
		return false;
	}

	result = static_cast<TNumber>(ans);
	return true;
}

/// Converts the string representation of a number to its floating-point number equivalent.
/// @param str the string containing a number to convert.
/// @tparam TNumber an floating-point number type.
/// @return the floating-point number value equivalent of the number contained in str.
/// @throws std::invalid_argument if str contains an invalid character which cannot be interpreted.
/// @throws std::out_of_range if the converted value would fall out of the range of the result type.
template <typename TNumber>
TNumber to_floating_point(
	const std::wstring& str) {
	const wchar_t* ptr = str.c_str();
	wchar_t* endptr;
	errno = 0;
	double ans = std::wcstod(ptr, &endptr);

	if (ptr == endptr) {
		throw std::invalid_argument("invalid to_floating_point argument");
	}

	while (*endptr != L'\0') {
		if (std::isspace(*endptr) == 0) {
			throw std::invalid_argument("invalid to_floating_point argument");
		}
		++endptr;
	}

	if (errno == ERANGE ||
		ans < std::numeric_limits<TNumber>::lowest() ||
		ans > std::numeric_limits<TNumber>::max()) {
		throw std::out_of_range("to_floating_point argument out of range");
	}

	return static_cast<TNumber>(ans);
}

/// Converts the string representation of a number to its floating-point number equivalent. A return value indicates whether the conversion succeeded.
/// @param str the string containing a number to convert.
/// @param result an floating-point number variable to store the floating-point number value equivalent of the number contained in str.
/// @tparam TNumber an floating-point number type.
/// @return true if str was converted successfully.
template <typename TNumber>
bool try_parse_floating_point(
	const std::wstring& str,
	TNumber& result) noexcept {
	const wchar_t* ptr = str.c_str();
	wchar_t* endptr;
	errno = 0;
	double ans = std::wcstod(ptr, &endptr);

	if (ptr == endptr) {
		return false;
	}

	while (*endptr != L'\0') {
		if (std::isspace(*endptr) == 0) {
			return false;
		}
		++endptr;
	}

	if (errno == ERANGE ||
		ans < std::numeric_limits<TNumber>::lowest() ||
		ans > std::numeric_limits<TNumber>::max()) {
		return false;
	}

	result = static_cast<TNumber>(ans);
	return true;
}

#endif // !TO_FLOATING_POINT_HPP_
