#pragma once

/// @file
/// Generic and safe strtol wrapper for Modern C++.

#ifndef TO_INTEGER_HPP_
#define TO_INTEGER_HPP_

#include <cinttypes>
#include <cwchar>
#include <cctype>
#include <string>
#include <limits>
#include <type_traits>

/// Converts the string representation of a number to its integer equivalent.
/// @param str the string containing a number to convert.
/// @param base the number base.
/// @tparam TNumber an integer type.
/// @return the integer value equivalent of the number contained in str.
/// @throws std::invalid_argument if str contains an invalid character which cannot be interpreted.
/// @throws std::out_of_range if the converted value would fall out of the range of the result type.
template <typename TNumber>
TNumber to_integer(
	const std::string& str,
	int base = 10) {
	const char* ptr = str.c_str();
	char* endptr;
	errno = 0;
	std::intmax_t ans = std::strtoimax(ptr, &endptr, base);

	if (ptr == endptr) {
		throw std::invalid_argument("invalid to_integer argument");
	}

	while (*endptr != '\0') {
		if (std::isspace(*endptr) == 0) {
			throw std::invalid_argument("invalid to_integer argument");
		}
		++endptr;
	}

	if (errno == ERANGE ||
		ans < std::numeric_limits<TNumber>::lowest() ||
		ans > std::numeric_limits<TNumber>::max()) {
		throw std::out_of_range("to_integer argument out of range");
	}

	return static_cast<TNumber>(ans);
}

/// Converts the string representation of a number to its integer equivalent.
/// @param str the string containing a number to convert.
/// @param base the number base.
/// @tparam TNumber an integer type.
/// @return the integer value equivalent of the number contained in str.
/// @throws std::invalid_argument if str contains an invalid character which cannot be interpreted.
/// @throws std::out_of_range if the converted value would fall out of the range of the result type.
template <typename TNumber, std::enable_if_t<std::is_unsigned<TNumber>::value>*& = nullptr>
TNumber to_integer(
	const std::string& str,
	int base = 10) {
	const char* ptr = str.c_str();
	char* endptr;
	errno = 0;
	std::uintmax_t ans = std::strtoumax(ptr, &endptr, base);

	if (ptr == endptr) {
		throw std::invalid_argument("invalid to_integer argument");
	}

	while (*endptr != '\0') {
		if (std::isspace(*endptr) == 0) {
			throw std::invalid_argument("invalid to_integer argument");
		}
		++endptr;
	}

	if (errno == ERANGE ||
		ans < std::numeric_limits<TNumber>::lowest() ||
		ans > std::numeric_limits<TNumber>::max()) {
		throw std::out_of_range("to_integer argument out of range");
	}

	return static_cast<TNumber>(ans);
}

/// Converts the string representation of a number to its integer equivalent. A return value indicates whether the conversion succeeded.
/// @param str the string containing a number to convert.
/// @param result an integer variable to store the integer value equivalent of the number contained in str.
/// @param base the number base.
/// @tparam TNumber an integer type.
/// @return true if str was converted successfully.
template <typename TNumber>
bool try_parse_integer(
	const std::string& str,
	TNumber& result,
	int base = 10) noexcept {
	const char* ptr = str.c_str();
	char* endptr;
	errno = 0;
	std::intmax_t ans = std::strtoimax(ptr, &endptr, base);

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

/// Converts the string representation of a number to its integer equivalent. A return value indicates whether the conversion succeeded.
/// @param str the string containing a number to convert.
/// @param result an integer variable to store the integer value equivalent of the number contained in str.
/// @param base the number base.
/// @tparam TNumber an integer type.
/// @return true if str was converted successfully.
template <typename TNumber, std::enable_if_t<std::is_unsigned<TNumber>::value>*& = nullptr>
bool try_parse_integer(
	const std::string& str,
	TNumber& result,
	int base = 10) noexcept {
	const char* ptr = str.c_str();
	char* endptr;
	errno = 0;
	std::uintmax_t ans = std::strtoumax(ptr, &endptr, base);

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

/// Converts the string representation of a number to its integer equivalent.
/// @param str the string containing a number to convert.
/// @param base the number base.
/// @tparam TNumber an integer type.
/// @return the integer value equivalent of the number contained in str.
/// @throws std::invalid_argument if str contains an invalid character which cannot be interpreted.
/// @throws std::out_of_range if the converted value would fall out of the range of the result type.
template <typename TNumber>
TNumber to_integer(
	const std::wstring& str,
	int base = 10) {
	const wchar_t* ptr = str.c_str();
	wchar_t* endptr;
	errno = 0;
	std::intmax_t ans = std::wcstoimax(ptr, &endptr, base);

	if (ptr == endptr) {
		throw std::invalid_argument("invalid to_integer argument");
	}

	while (*endptr != L'\0') {
		if (std::isspace(*endptr) == 0) {
			throw std::invalid_argument("invalid to_integer argument");
		}
		++endptr;
	}

	if (errno == ERANGE ||
		ans < std::numeric_limits<TNumber>::lowest() ||
		ans > std::numeric_limits<TNumber>::max()) {
		throw std::out_of_range("to_integer argument out of range");
	}

	return static_cast<TNumber>(ans);
}

/// Converts the string representation of a number to its integer equivalent.
/// @param str the string containing a number to convert.
/// @param base the number base.
/// @tparam TNumber an integer type.
/// @return the integer value equivalent of the number contained in str.
/// @throws std::invalid_argument if str contains an invalid character which cannot be interpreted.
/// @throws std::out_of_range if the converted value would fall out of the range of the result type.
template <typename TNumber, std::enable_if_t<std::is_unsigned<TNumber>::value>*& = nullptr>
TNumber to_integer(
	const std::wstring& str,
	int base = 10) {
	const wchar_t* ptr = str.c_str();
	wchar_t* endptr;
	errno = 0;
	std::uintmax_t ans = std::wcstoumax(ptr, &endptr, base);

	if (ptr == endptr) {
		throw std::invalid_argument("invalid to_integer argument");
	}

	while (*endptr != L'\0') {
		if (std::isspace(*endptr) == 0) {
			throw std::invalid_argument("invalid to_integer argument");
		}
		++endptr;
	}

	if (errno == ERANGE ||
		ans < std::numeric_limits<TNumber>::lowest() ||
		ans > std::numeric_limits<TNumber>::max()) {
		throw std::out_of_range("to_integer argument out of range");
	}

	return static_cast<TNumber>(ans);
}

/// Converts the string representation of a number to its integer equivalent. A return value indicates whether the conversion succeeded.
/// @param str the string containing a number to convert.
/// @param result an integer variable to store the integer value equivalent of the number contained in str.
/// @param base the number base.
/// @tparam TNumber an integer type.
/// @return true if str was converted successfully.
template <typename TNumber>
bool try_parse_integer(
	const std::wstring& str,
	TNumber& result,
	int base = 10) noexcept {
	const wchar_t* ptr = str.c_str();
	wchar_t* endptr;
	errno = 0;
	std::intmax_t ans = std::wcstoimax(ptr, &endptr, base);

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

/// Converts the string representation of a number to its integer equivalent. A return value indicates whether the conversion succeeded.
/// @param str the string containing a number to convert.
/// @param result an integer variable to store the integer value equivalent of the number contained in str.
/// @param base the number base.
/// @tparam TNumber an integer type.
/// @return true if str was converted successfully.
template <typename TNumber, std::enable_if_t<std::is_unsigned<TNumber>::value>*& = nullptr>
bool try_parse_integer(
	const std::wstring& str,
	TNumber& result,
	int base = 10) noexcept {
	const wchar_t* ptr = str.c_str();
	wchar_t* endptr;
	errno = 0;
	std::uintmax_t ans = std::wcstoumax(ptr, &endptr, base);

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

#endif // !TO_INTEGER_HPP_

