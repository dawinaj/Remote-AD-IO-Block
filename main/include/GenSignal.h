#pragma once

#include <memory>
#include <vector>

#include <cmath>
#include <numbers>
#include <utility>

#define JSON_DISABLE_ENUM_SERIALIZATION 1
#include "nlohmann/json.hpp"
using namespace nlohmann;

enum SignalType : int8_t
{
	Virtual = 0,
	Const,
	Impulse,
	Sine,
	Square,
	Triangle,
	Chirp,

	Delay = std::numeric_limits<int8_t>::min(),

};

NLOHMANN_JSON_SERIALIZE_ENUM(SignalType, {
											 {SignalType::Virtual, nullptr},
											 {SignalType::Const, "Const"},
											 {SignalType::Impulse, "Impulse"},
											 {SignalType::Sine, "Sine"},
											 {SignalType::Square, "Square"},
											 {SignalType::Triangle, "Triangle"},
											 {SignalType::Chirp, "Chirp"},
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

protected:
	static inline float ang_to_sine(float ang) // Full sine cycle on <0, 1>, duplicated to negatives for simplicity
	{
		float aang = std::abs(ang);
		bool flip = std::signbit(ang) != (aang > 0.5f);

		float smpl = 16.0f * aang * (0.5f - aang);
		float bhskr = 4.0f * smpl / (5.0f - smpl);
		return flip ? -bhskr : bhskr;
	}

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

class SignalSine : public Signal
{
	index_t T;

public:
	SignalSine(index_t t = 1) : T(t) {}
	~SignalSine() = default;

	float get(index_t i) const override
	{
		i %= T;
		float ang = static_cast<float>(i) / T;
		return ang_to_sine(ang);
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

		if (4 * i < T)
			return x * 4.0f;
		if (4 * i > 3 * T)
			return (x - 1) * 4.0f;
		return (0.5f - x) * 4.0f;
	}
	SignalType type() const override
	{
		return SignalType::Triangle;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalTriangle, T);
};

class SignalChirp : public Signal
{
	index_t T;
	float FS;
	float FD;

public:
	SignalChirp(index_t t = 1, float fs = 0, float fd = 0) : T(t), FS(fs), FD(fd) {}
	~SignalChirp() = default;

	float get(index_t i) const override
	{
		i %= T;
		float fr = static_cast<float>(i) / T;

		float phs = i * (FS + 0.5f * FD * fr); // from the net, integral? idk

		float ang = std::fmod(phs, 1.0f);
		return ang_to_sine(ang);
	}
	SignalType type() const override
	{
		return SignalType::Chirp;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalChirp, T, FS, FD);
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
