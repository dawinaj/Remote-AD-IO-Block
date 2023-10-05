#pragma once

#include <vector>
#include <utility> //pair

#include "nlohmann/json.hpp"

#include "GenSignal.h"

class Generator
{
	static constexpr const char* const TAG = "Generator";
	using Signals = std::vector<std::pair<float, SignalHdl>>;

	Signals signals;

public:
	Generator() = default;
	~Generator() = default;

	Generator(Generator &&) = default;
	Generator &operator=(Generator &&) = default;

	void add(float, SignalHdl &&);
	float get(size_t);

	friend void to_json(json &, const Generator &);
	friend void from_json(const json &, Generator &);
};
