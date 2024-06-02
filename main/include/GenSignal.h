#pragma once

#include <memory>
#include <vector>

#include <cmath>
#include <numbers>
#include <utility>

#define JSON_DISABLE_ENUM_SERIALIZATION 1
#include "nlohmann/json.hpp"
using namespace nlohmann;

#include "CTOR.h"
//

enum SignalType : int8_t
{
	Virtual = 0,
	Const,
	Impulse,
	Sine,
	Square,
	Triangle,
	Chirp,
	ChirpLog,

	Delay = std::numeric_limits<int8_t>::min(),
	Absolute,
	Clamp,
	LinearMap,

	Multiply,
};

NLOHMANN_JSON_SERIALIZE_ENUM(SignalType, {
											 {SignalType::Virtual, nullptr},
											 {SignalType::Const, "Const"},
											 {SignalType::Impulse, "Impulse"},
											 {SignalType::Sine, "Sine"},
											 {SignalType::Square, "Square"},
											 {SignalType::Triangle, "Triangle"},
											 {SignalType::Chirp, "Chirp"},
											 {SignalType::ChirpLog, "ChirpLog"},
											 {SignalType::Delay, "Delay"},
											 {SignalType::Absolute, "Absolute"},
											 {SignalType::Clamp, "Clamp"},
											 {SignalType::LinearMap, "LinearMap"},
											 {SignalType::Multiply, "Multiply"},
										 })

//
class Signal;

class SignalHdl
{
	using SignalPtr = std::unique_ptr<Signal>;
	SignalPtr ptr;

public:
	DEFAULT_CTOR(SignalHdl);
	DELETE_CP_CTOR(SignalHdl);
	DEFAULT_MV_CTOR(SignalHdl);

	SignalHdl(SignalPtr &&p) : ptr(std::move(p)) {}

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
protected:
	static inline float ang_to_sine(float ang) // Full sine cycle on <0, 1>, duplicated to negatives for simplicity
	{
		bool flip = std::signbit(ang);
		ang = std::abs(ang);
		if (ang > 0.5f)
		{
			ang -= 0.5f;
			flip = !flip;
		}

		float smpl = 16.0f * ang * (0.5f - ang);
		return flip ? -smpl : smpl;
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
	SignalImpulse() = default;
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
	index_t P;

public:
	SignalTriangle(index_t t = 1, index_t p = 1) : T(t), P(p) {}
	~SignalTriangle() = default;

	float get(index_t i) const override
	{
		i = i % T;

		if (i == 0) // to avoid division when P=0
			return 0.0f;

		if (i <= P) // rising pos edge
			return static_cast<float>(i) / P;

		if (i >= T - P) // rising neg edge
			return static_cast<float>(i - T) / P;

		// falling edge
		return static_cast<float>(T / 2 - i) / (T / 2 - P);
	}
	SignalType type() const override
	{
		return SignalType::Triangle;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalTriangle, T, P);
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

		float phs = i * (FS + FD * fr * 0.5f);

		float ang = std::fmod(phs, 1.0f);
		return ang_to_sine(ang);
	}
	SignalType type() const override
	{
		return SignalType::Chirp;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalChirp, T, FS, FD);
};

class SignalChirpLog : public Signal
{
	index_t T;
	float FS;
	float FR;

public:
	SignalChirpLog(index_t t = 1, float fs = 0, float fr = 0) : T(t), FS(fs), FR(fr) {}
	~SignalChirpLog() = default;

	float get(index_t i) const override
	{
		i %= T;
		float fr = static_cast<float>(i) / T;

		float phs = FS * T / std::log(FR) * (std::pow(FR, fr) - 1.0f) + FS;
		//float phs = FS * T / std::log(FR) * std::pow(FR, fr);

		float ang = std::fmod(phs, 1.0f);
		return ang_to_sine(ang);
	}
	SignalType type() const override
	{
		return SignalType::ChirpLog;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalChirpLog, T, FS, FR);
};

//

class SignalDelay : public Signal
{
	index_t D = 0;
	SignalHdl S;

public:
	// SignalDelay() = default;
	SignalDelay(index_t d = 0, SignalHdl &&s = SignalHdl()) : D(d), S(std::move(s)) {}
	~SignalDelay() = default;

	// DELETE_CP_CTOR(SignalDelay);
	DEFAULT_MV_CTOR(SignalDelay);

	float get(index_t i) const override
	{
		//	if (i < D)
		//		return 0;
		return S->get(i - D);
	}
	SignalType type() const override
	{
		return SignalType::Delay;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalDelay, D, S);
};

class SignalAbsolute : public Signal
{
	SignalHdl S;

public:
	SignalAbsolute(SignalHdl &&s = SignalHdl()) : S(std::move(s)) {}
	~SignalAbsolute() = default;

	// DELETE_CP_CTOR(SignalAbsolute);
	DEFAULT_MV_CTOR(SignalAbsolute);

	float get(index_t i) const override
	{
		return std::abs(S->get(i));
	}
	SignalType type() const override
	{
		return SignalType::Absolute;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalAbsolute, S);
};

class SignalClamp : public Signal
{
	float L;
	float H;
	SignalHdl S;

public:
	SignalClamp(float l = -1, float h = 1, SignalHdl &&s = SignalHdl()) : L(l), H(h), S(std::move(s)) {}
	~SignalClamp() = default;

	// DELETE_CP_CTOR(SignalClamp);
	DEFAULT_MV_CTOR(SignalClamp);

	float get(index_t i) const override
	{
		return std::clamp(S->get(i), L, H);
	}
	SignalType type() const override
	{
		return SignalType::Clamp;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalClamp, L, H, S);
};

class SignalLinearMap : public Signal
{
	float A;
	float B;
	SignalHdl S;

public:
	SignalLinearMap(float a = 1, float b = 0, SignalHdl &&s = SignalHdl()) : A(a), B(b), S(std::move(s)) {}
	~SignalLinearMap() = default;

	// DELETE_CP_CTOR(SignalLinearMap);
	DEFAULT_MV_CTOR(SignalLinearMap);

	float get(index_t i) const override
	{
		return A * S->get(i) + B;
	}
	SignalType type() const override
	{
		return SignalType::LinearMap;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalLinearMap, A, B, S);
};

class SignalMultiply : public Signal
{
	SignalHdl S1;
	SignalHdl S2;

public:
	SignalMultiply(SignalHdl &&s1 = SignalHdl(), SignalHdl &&s2 = SignalHdl()) : S1(std::move(s1)), S2(std::move(s2)) {}
	~SignalMultiply() = default;

	// DELETE_CP_CTOR(SignalMultiply);
	DEFAULT_MV_CTOR(SignalMultiply);

	float get(index_t i) const override
	{
		return S1->get(i) * S2->get(i);
	}
	SignalType type() const override
	{
		return SignalType::Multiply;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SignalMultiply, S1, S2);
};

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
	SignalType t = j.at("WF").get<SignalType>();
	switch (t)
	{
	case SignalType::Const:
		o = make_signal<SignalConst>(j);
		break;
	case SignalType::Impulse:
		o = make_signal<SignalImpulse>(j);
		break;
	case SignalType::Sine:
		o = make_signal<SignalSine>(j);
		break;
	case SignalType::Square:
		o = make_signal<SignalSquare>(j);
		break;
	case SignalType::Triangle:
		o = make_signal<SignalTriangle>(j);
		break;
	case SignalType::Chirp:
		o = make_signal<SignalChirp>(j);
		break;
	case SignalType::ChirpLog:
		o = make_signal<SignalChirpLog>(j);
		break;

	case SignalType::Delay:
		o = make_signal<SignalDelay>(j);
		break;
	case SignalType::Absolute:
		o = make_signal<SignalAbsolute>(j);
		break;
	case SignalType::Clamp:
		o = make_signal<SignalClamp>(j);
		break;
	case SignalType::LinearMap:
		o = make_signal<SignalLinearMap>(j);
		break;

	case SignalType::Multiply:
		o = make_signal<SignalMultiply>(j);
		break;

	default:
		throw std::invalid_argument("Unknown generator type!");
	}
}
inline void to_json(json &j, const SignalHdl &o)
{
	SignalType t = o.ptr->type();

	switch (t)
	{
	case SignalType::Virtual:
		break;
	case SignalType::Const:
		j = *static_cast<SignalConst *>(o.ptr.get());
		break;
	case SignalType::Impulse:
		j = *static_cast<SignalImpulse *>(o.ptr.get());
		break;
	case SignalType::Sine:
		j = *static_cast<SignalSine *>(o.ptr.get());
		break;
	case SignalType::Square:
		j = *static_cast<SignalSquare *>(o.ptr.get());
		break;
	case SignalType::Triangle:
		j = *static_cast<SignalTriangle *>(o.ptr.get());
		break;
	case SignalType::Chirp:
		j = *static_cast<SignalChirp *>(o.ptr.get());
		break;
	case SignalType::ChirpLog:
		j = *static_cast<SignalChirpLog *>(o.ptr.get());
		break;

	case SignalType::Delay:
		j = *static_cast<SignalDelay *>(o.ptr.get());
		break;
	case SignalType::Absolute:
		j = *static_cast<SignalAbsolute *>(o.ptr.get());
		break;
	case SignalType::Clamp:
		j = *static_cast<SignalClamp *>(o.ptr.get());
		break;
	case SignalType::LinearMap:
		j = *static_cast<SignalLinearMap *>(o.ptr.get());
		break;

	case SignalType::Multiply:
		j = *static_cast<SignalMultiply *>(o.ptr.get());
		break;

	default:
		throw std::invalid_argument("Unknown generator type!");
	}

	j["WF"] = t;
}
