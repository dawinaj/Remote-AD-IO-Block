#pragma once

#include <memory>
#include <vector>
#include <utility> //pair

#define JSON_DISABLE_ENUM_SERIALIZATION 1
#include "nlohmann/json.hpp"

#include "GenSignal.h"

class Generator
{
public:
	using index_t = Signal::index_t;

private:
	using Signals = std::vector<std::pair<float, SignalHdl>>;

	Signals signals;
	index_t current_step;

public:
	DEFAULT_CTOR(Generator);
	DEFAULT_MV_CTOR(Generator);

	void add(float a, SignalHdl &&s)
	{
		signals.emplace_back(a, std::move(s));
	}
	float get(index_t i)
	{
		current_step = i;
		return calculate();
	}
	float forward()
	{
		++current_step;
		return calculate();
	}

	bool empty() const
	{
		return signals.empty();
	}
	size_t size() const
	{
		return signals.size();
	}

	friend void to_json(json &j, const Generator &o)
	{
		j = json::array();

		for (const auto &s : o.signals)
		{
			json obj = json::object();
			obj["A"] = s.first;
			obj["S"] = s.second;
			j.push_back(obj);
		}
	}

	friend void from_json(const json &j, Generator &o)
	{
		for (const auto &it : j.items())
		{
			const auto &obj = it.value();

			float a = obj.at("A").get<float>();
			SignalHdl s = obj.at("S").get<SignalHdl>();

			o.add(a, std::move(s));
		}
	}

private:
	inline float calculate() const
	{
		float sum = 0;
		for (const auto &s : signals)
			sum += s.first * s.second->get(current_step);
		return sum;
	}
};
