
#include <esp_log.h>

#include "GenSignal.h"

Signal &SignalHdl::operator*() const
{
	return *ptr.get();
}
Signal *SignalHdl::operator->() const
{
	return ptr.get();
}

void to_json(json &j, const SignalHdl &o)
{
	SignalType t = o.ptr->type();
	// j["t"] = static_cast<SignalEnumT>(t);
	j["t"] = t;

	json obj;
	switch (t)
	{
	case SignalType::Virtual:
		ESP_LOGE(SignalHdl::TAG, "Cannot serialize pure virtual class!");
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
		ESP_LOGE(SignalHdl::TAG, "Such generator does not exist!");
		break;
	}

	j["p"] = obj;
}

void from_json(const json &j, SignalHdl &o)
{
	// SignalType t = static_cast<SignalType>(j["t"]);
	SignalType t = j["t"];

	switch (t)
	{
	case SignalType::Virtual:
		ESP_LOGE(SignalHdl::TAG, "Cannot create pure virtual class!");
		break;
	case SignalType::Const:
		o = make_signal<SignalConst>(j["p"]);
		break;
	case SignalType::Impulse:
		o = make_signal<SignalImpulse>(j["p"]);
		break;
	case SignalType::Sine:
		o = make_signal<SignalSine>(j["p"]);
		break;
	case SignalType::Square:
		o = make_signal<SignalSquare>(j["p"]);
		break;
	case SignalType::Triangle:
		o = make_signal<SignalTriangle>(j["p"]);
		break;
	case SignalType::Delay:
		o = make_signal<SignalDelay>(j["p"]);
		break;
	default:
		ESP_LOGE(SignalHdl::TAG, "Such generator does not exist!");
		break;
	}
}
