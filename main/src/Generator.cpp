#include "Generator.h"

#include <memory>

#include "Signal.h"

//

void Generator::add(float a, SignalHdl &&s)
{
	signals.emplace_back(a, std::move(s));
}

float Generator::get(size_t i)
{
	current_step = i;
	return calculate();
}

float Generator::forward()
{
	++current_step;
	return calculate();
}

float Generator::calculate() const
{
	float sum = 0;
	for (const auto &s : signals)
		sum += s.first * s.second->get(current_step);
	return sum;
}

bool Generator::empty() const
{
	return signals.empty();
}

size_t Generator::size() const
{
	return signals.size();
}

//

void to_json(json &j, const Generator &o)
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

void from_json(const json &j, Generator &o)
{
	for (const auto &it : j.items())
	{
		const auto &obj = it.value();

		float a = obj["A"];
		SignalHdl s = obj["S"];

		o.add(a, std::move(s));
	}
}
