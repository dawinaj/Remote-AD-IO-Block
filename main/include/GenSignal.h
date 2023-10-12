#pragma once

#include <memory>
#include <vector>

#include <cmath>
#include <numbers>
#include <utility>

#define JSON_DISABLE_ENUM_SERIALIZATION 1
#include "nlohmann/json.hpp"
using namespace nlohmann;

enum SignalType : int
{
	Virtual = 0,
	Const,
	Impulse,
	Sine,
	Square,
	Triangle,

	Delay = std::numeric_limits<int>::min(),

};

NLOHMANN_JSON_SERIALIZE_ENUM(SignalType, {
											 {SignalType::Virtual, nullptr},
											 {SignalType::Const, "Const"},
											 {SignalType::Impulse, "Impulse"},
											 {SignalType::Sine, "Sine"},
											 {SignalType::Square, "Square"},
											 {SignalType::Triangle, "Triangle"},
											 {SignalType::Delay, "Delay"},
										 })

//
class Signal;

class SignalHdl
{
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

	Signal &operator*() const
	{
		return *ptr.get();
	}
	Signal *operator->() const
	{
		return ptr.get();
	}

	friend void from_json(const json &j, SignalHdl &o);
	friend void to_json(json &j, const SignalHdl &o);
};

//

class Signal
{
	friend SignalHdl;

public:
	using index_t = int32_t;

	Signal() = default;
	virtual ~Signal() = default;

	virtual float get(index_t) const
	{
		return 0;
	}
	virtual SignalType type() const
	{
		return SignalType::Virtual;
	}

	friend void to_json(json &j, const Signal &o) {}
	friend void from_json(const json &j, Signal &o) {}
};

//

class SignalConst : public Signal
{
public:
	SignalConst() = default;
	~SignalConst() = default;

	float get(index_t) const override
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

	float get(index_t i) const override
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

/*/
constexpr uint32_t sineQuarter16[65] = {
	32768, 33572, 34376, 35178, 35980, 36779, 37576, 38370, 39161, 39947, 40730,
	41507, 42280, 43046, 43807, 44561, 45307, 46047, 46778, 47500, 48214, 48919,
	49614, 50298, 50972, 51636, 52287, 52927, 53555, 54171, 54773, 55362, 55938,
	56499, 57047, 57579, 58097, 58600, 59087, 59558, 60013, 60451, 60873, 61278,
	61666, 62036, 62389, 62724, 63041, 63339, 63620, 63881, 64124, 64348, 64553,
	64739, 64905, 65053, 65180, 65289, 65377, 65446, 65496, 65525, 65535};

inline int32_t sin16(uint32_t x)
{
	x = x % 65536;
	uint32_t quotient = x / 256;
	uint32_t remainder = x % 256;
	uint32_t a = 0;
	uint32_t b = 0;

	if (quotient < 64)
	{
		a = sineQuarter16[quotient];
		b = sineQuarter16[quotient + 1];
		return a + (b - a) * remainder / 256;
	}
	if (quotient < 128)
	{
		a = sineQuarter16[128 - quotient];
		b = sineQuarter16[127 - quotient];
		return a - (a - b) * remainder / 256;
	}
	if (quotient < 192)
	{
		a = 65536 - sineQuarter16[quotient - 128];
		b = 65536 - sineQuarter16[quotient - 127];
		return a - (a - b) * remainder / 256;
	}
	a = 65536 - sineQuarter16[256 - quotient];
	b = 65536 - sineQuarter16[255 - quotient];
	return a + (b - a) * remainder / 256;
}
//*/

class SignalSine : public Signal
{
	index_t T;
	inline float sine_half(index_t i) const
	{
		float f = 2.0f * i / T;
		return 4.0f * f * (1.0f - f);
	}

public:
	SignalSine(index_t t = 1) : T(t) {}
	~SignalSine() = default;

	float get(index_t i) const override
	{
		i %= T;
		if (2 * i <= T)
			return sine_half(i);
		else
			return -sine_half(i - T / 2);
	}
	SignalType type() const override
	{
		return SignalType::Sine;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalSine, T);
};

class SignalSquare : public Signal
{
	index_t T;
	index_t D;

public:
	SignalSquare(index_t t = 1, index_t d = 0) : T(t), D(d) {}
	~SignalSquare() = default;

	float get(index_t i) const override
	{
		i = i % T;
		return (i < D) ? 1 : 0;
	}
	SignalType type() const override
	{
		return SignalType::Square;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalSquare, T, D);
};

class SignalTriangle : public Signal
{
	index_t T;

public:
	SignalTriangle(index_t t = 1) : T(t) {}
	~SignalTriangle() = default;

	float get(index_t i) const override
	{
		i = i % T;
		float x = float(i) / T;

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
	index_t D = 0;
	SignalHdl S;

public:
	SignalDelay() = default;
	SignalDelay(index_t d, SignalHdl &&s) : D(d), S(std::move(s)) {}
	~SignalDelay() = default;

	SignalDelay(SignalDelay &&) = default;
	SignalDelay &operator=(SignalDelay &&) = default;

	float get(index_t i) const override
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

//

//

template <typename T>
SignalHdl make_signal(const json &j)
{
	std::unique_ptr<Signal> sp = std::make_unique<T>(j.get<T>());
	return SignalHdl(std::move(sp));
}

template <typename T, typename... Args>
SignalHdl make_signal(Args &&...args)
{
	std::unique_ptr<Signal> sp = std::make_unique<T>(std::forward<Args>(args)...);
	return SignalHdl(std::move(sp));
}

inline void from_json(const json &j, SignalHdl &o)
{
	SignalType t = j.at("t").get<SignalType>();
	switch (t)
	{
	case SignalType::Const:
		o = make_signal<SignalConst>(j.at("p"));
		break;
	case SignalType::Impulse:
		o = make_signal<SignalImpulse>(j.at("p"));
		break;
	case SignalType::Sine:
		o = make_signal<SignalSine>(j.at("p"));
		break;
	case SignalType::Square:
		o = make_signal<SignalSquare>(j.at("p"));
		break;
	case SignalType::Triangle:
		o = make_signal<SignalTriangle>(j.at("p"));
		break;
	case SignalType::Delay:
		o = make_signal<SignalDelay>(j.at("p"));
		break;
	default:
		o = make_signal<Signal>();
		break;
	}
}
inline void to_json(json &j, const SignalHdl &o)
{
	SignalType t = o.ptr->type();
	j["t"] = t;

	json obj;
	switch (t)
	{
	case SignalType::Virtual:
		break;
	case SignalType::Const:
		obj = *static_cast<SignalConst *>(o.ptr.get());
		break;
	case SignalType::Impulse:
		obj = *static_cast<SignalImpulse *>(o.ptr.get());
		break;
	case SignalType::Sine:
		obj = *static_cast<SignalSine *>(o.ptr.get());
		break;
	case SignalType::Square:
		obj = *static_cast<SignalSquare *>(o.ptr.get());
		break;
	case SignalType::Triangle:
		obj = *static_cast<SignalTriangle *>(o.ptr.get());
		break;
	case SignalType::Delay:
		obj = *static_cast<SignalDelay *>(o.ptr.get());
		break;
	default:
		break;
	}

	j["p"] = obj;
}
