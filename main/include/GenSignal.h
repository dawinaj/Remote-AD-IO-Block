#pragma once

#include <memory>
#include <vector>

#include <cmath>
#include <numbers>
#include <utility>

#include "nlohmann/json.hpp"
using namespace nlohmann;

using SignalEnumT = int8_t;

enum SignalType : SignalEnumT
{
	Virtual = 0,
	Const,
	Impulse,
	Sine,
	Square,
	Triangle,

	Delay = std::numeric_limits<SignalEnumT>::min(),
};

NLOHMANN_JSON_SERIALIZE_ENUM(SignalType,
	{
		{ SignalType::Virtual, "Virtual" },
		{ SignalType::Const, "Const" },
		{ SignalType::Impulse, "Impulse" },
		{ SignalType::Sine, "Sine" },
		{ SignalType::Square, "Square" },
		{ SignalType::Triangle, "Triangle" },
		{ SignalType::Delay, "Delay" },
	}
)

class Signal;

class SignalHdl
{
	static constexpr const char *const TAG = "SignalHdl";
	using SignalPtr = std::unique_ptr<Signal>;
	SignalPtr ptr;

public:
	SignalHdl() = default;
	SignalHdl(const SignalHdl &) = delete;
	SignalHdl &operator=(const SignalHdl &) = delete;
	SignalHdl(SignalHdl &&) = default;
	SignalHdl &operator=(SignalHdl &&) = default;

	SignalHdl(SignalPtr &&p) : ptr(std::move(p)) {}
	~SignalHdl() = default;

	Signal &operator*() const;
	Signal *operator->() const;

	friend void to_json(json &, const SignalHdl &);
	friend void from_json(const json &, SignalHdl &);

	template <typename T, typename... Args>
	static SignalHdl make(Args &&...args)
	{
		// SignalPtr sp = std::unique_ptr<T>(new T(std::forward<Args>(args)...));
		SignalPtr sp = std::make_unique<T>(std::forward<Args>(args)...);
		return SignalHdl(std::move(sp));
	}
};

class Signal
{
	static constexpr const char *const TAG = "Signal";
	friend SignalHdl;

public:
	Signal() = default;
	virtual ~Signal() = default;

	virtual float get(size_t) const = 0;
	virtual SignalType type() const = 0;
};

class SignalConst : public Signal
{
public:
	SignalConst() = default;
	~SignalConst() = default;

	float get(size_t) const override
	{
		return 1;
	}
	SignalType type() const override
	{
		return SignalType::Const;
	}

	friend void to_json(json &j, const SignalConst &o) {}
	friend void from_json(const json &j, SignalConst &o) {}
};

class SignalImpulse : public Signal
{
public:
	SignalImpulse() {}
	~SignalImpulse() = default;

	float get(size_t i) const override
	{
		return i == 0;
	}
	SignalType type() const override
	{
		return SignalType::Impulse;
	}

	friend void to_json(json &j, const SignalImpulse &o) {}
	friend void from_json(const json &j, SignalImpulse &o) {}
};

class SignalSine : public Signal
{
	float T;

public:
	SignalSine(float t) : T(t) {}
	~SignalSine() = default;

	float get(size_t i) const override
	{
		return std::sin(i * 2 * std::numbers::pi / T);
	}
	SignalType type() const override
	{
		return SignalType::Sine;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalSine, T);
};

class SignalSquare : public Signal
{
	float T;
	float D;

public:
	SignalSquare(float t, float d = 0.5) : T(t), D(d) {}
	~SignalSquare() = default;

	float get(size_t i) const override
	{
		float trash;
		float x = std::modf(i / T, &trash);
		return (x < D) ? 1 : 0;
	}
	SignalType type() const override
	{
		return SignalType::Square;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalSquare, T, D);
};

class SignalTriangle : public Signal
{
	float T;

public:
	SignalTriangle(float t) : T(t) {}
	~SignalTriangle() = default;

	float get(size_t i) const override
	{
		float trash;
		float x = std::modf(i / T, &trash);
		if (x < 0.25)
			return x * 4;
		if (x > 0.75)
			return (x - 1) * 4;
		return (0.5 - x) * 4;
	}
	SignalType type() const override
	{
		return SignalType::Triangle;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalTriangle, T);
};

class SignalDelay : public Signal
{
	size_t D = 0;
	SignalHdl S;

public:
	SignalDelay() = default;
	SignalDelay(size_t d, SignalHdl &&s) : D(d), S(std::move(s)) {}
	~SignalDelay() = default;

	SignalDelay(SignalDelay &&) = default;
	SignalDelay &operator=(SignalDelay &&) = default;

	float get(size_t i) const override
	{
		if (i < D)
			return 0;
		return S->get(i - D);
	}
	SignalType type() const override
	{
		return SignalType::Delay;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalDelay, D, S);
};
