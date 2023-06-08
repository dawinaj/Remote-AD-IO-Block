#pragma once

#include <exception>

//

// virtual char const *what() const noexcept
// {
// 	return exception::what();
// }

class json_error : public std::logic_error
{
public:
	json_error(const char *message) noexcept : std::logic_error(message) {}
};

class socket_error : public std::runtime_error
{
public:
	socket_error(const char *message) noexcept : std::runtime_error(message) {}
};

class warning : public std::exception
{
	std::basic_string<char> _M_msg;

public:
	explicit warning(const std::string &__arg) _GLIBCXX_TXN_SAFE _GLIBCXX_USE_NOEXCEPT
	{
		_M_msg = __arg;
	}

	explicit warning(const char *__arg) _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_USE_NOEXCEPT
	{
		_M_msg = __arg;
	}

	warning(const warning &__arg) _GLIBCXX_USE_NOEXCEPT
	{
		_M_msg = __arg._M_msg;
	}
	warning &operator=(const warning &__arg) _GLIBCXX_USE_NOEXCEPT
	{
		_M_msg = __arg._M_msg;
		return *this;
	}

	virtual ~warning() _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_USE_NOEXCEPT
	{
	}

	virtual const char *what() const _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_USE_NOEXCEPT
	{
		return _M_msg.data();
	}
};

class disconnect_warning : public warning
{
public:
	disconnect_warning(const char *message) noexcept : warning(message) {}
};