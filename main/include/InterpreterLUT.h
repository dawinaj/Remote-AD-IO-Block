#pragma once

#define CB_HELP(COND) [](const std::vector<std::string> &args, Instruction &cs) { return (COND); }
#define READ_IP (try_parse_integer(args[0], cs.port) && (cs.port >= 1 && cs.port <= 4))
#define READ_OP (try_parse_integer(args[0], cs.port) && (cs.port >= 1 && cs.port <= 2))
#define READ_DO (try_parse_integer(args[0], cs.arg.u, 0) && (cs.arg.u <= 0b1111))

#define SNTX_IP "<port (1|2|3|4)>"
#define SNTX_OP "<port (1|2)>"
#define SNTX_NO "<no args>"
#define SNTX_DO "<state (4-bit hex/oct/dec int)>"

const std::vector<InstrLUTRow> CS_LUT = {
	{
		OPCode::NOP,
		"NOP",
		SNTX_NO,
		"No OPeration - does nothing, does not parse, do not use",
		0,
		CB_HELP(false),
	},
	//
	{
		OPCode::AIRDF,
		"AIRDF",
		SNTX_IP,
		"Analog Input ReaD Float - returns measurement (V, A) as 32-bit float",
		1,
		CB_HELP(READ_IP),
	},
	{
		OPCode::AIRDM,
		"AIRDM",
		SNTX_IP,
		"Analog Input ReaD Milli - returns measurement (mV, mA) as 32-bit int",
		1,
		CB_HELP(READ_IP),
	},
	{
		OPCode::AIRDU,
		"AIRDU",
		SNTX_IP,
		"Analog Input ReaD Micro - returns measurement (uV, uA) as 32-bit int",
		1,
		CB_HELP(READ_IP),
	},
	//
	{
		OPCode::AIEN,
		"AIEN",
		SNTX_NO,
		"Analog Input ENable - turns on input ports connections",
		0,
		CB_HELP(true),
	},
	{
		OPCode::AIDIS,
		"AIDIS",
		SNTX_NO,
		"Analog Input DISable - turns off input ports connections",
		0,
		CB_HELP(true),
	},
	{
		OPCode::AIRNG,
		"AIRNG",
		SNTX_IP " <range (OFF|MIN|MED|MAX)>",
		"Analog Input RaNGe - sets the range of port (selects divider/multiplier gain)",
		2,
		CB_HELP(READ_IP && (try_parse_range(args[1], cs.arg.u))),
	},
	//
	{
		OPCode::AOVAL,
		"AOVAL",
		SNTX_OP " <voltage (float)>",
		"Analog Output VALue - outputs value to port",
		2,
		CB_HELP(READ_OP && (try_parse_floating_point(args[1], cs.arg.f) && std::isfinite(cs.arg.f))),
	},
	{
		OPCode::AOGEN,
		"AOGEN",
		SNTX_OP " <generator_idx (uint)>",
		"Analog Output GENerator - outputs value created by the chosen Generator to port",
		2,
		CB_HELP(READ_OP && (try_parse_integer(args[1], cs.arg.u))),
	},
	//
	{
		OPCode::DIRD,
		"DIRD",
		SNTX_NO,
		"Digital Inputs ReaD - returns state of pins as 32-bit uint",
		0,
		CB_HELP(true),
	},
	//
	{
		OPCode::DOWR,
		"DOWR",
		SNTX_DO,
		"Digital Outputs WRite - directly writes the state to the pins",
		1,
		CB_HELP(READ_DO),
	},
	{
		OPCode::DOSET,
		"DOSET",
		SNTX_DO,
		"Digital Outputs SET - turns ON pins corresponding to set bits (bitwise OR)",
		1,
		CB_HELP(READ_DO),
	},
	{
		OPCode::DORST,
		"DORST",
		SNTX_DO,
		"Digital Outputs ReSeT - turns OFF pins corresponding to set bits",
		1,
		CB_HELP(READ_DO),
	},
	{
		OPCode::DOAND,
		"DOAND",
		SNTX_DO,
		"Digital Outputs AND - turns OFF pins corresponding to unset bits (bitwise AND)",
		1,
		CB_HELP(READ_DO),
	},
	{
		OPCode::DOXOR,
		"DOXOR",
		SNTX_DO,
		"Digital Outputs XOR - performs a bitwise eXclusive OR operation",
		1,
		CB_HELP(READ_DO),
	},
	//
	{
		OPCode::DELAY,
		"DELAY",
		"<microseconds (uint32)>",
		"DELAY - sets the next synchronization timestamp based on last timestamp",
		1,
		CB_HELP(try_parse_integer(args[0], cs.arg.u)),
	},
	{
		OPCode::GETTM,
		"GETTM",
		SNTX_NO,
		"GET TiMe - sets the next synchronization timestamp to now",
		0,
		CB_HELP(READ_DO),
	},
	//
	{
		OPCode::NOP,
		"LOOP",
		"<iterations (uint32)>",
		"LOOP - repeats the code between itself and matching ENDLOOP specified number of times",
		1,
		CB_HELP(false),
	},
	{
		OPCode::NOP,
		"ENDLOOP",
		SNTX_NO,
		"END LOOP - closing of previous LOOP",
		0,
		CB_HELP(false),
	},
};

#undef CB_HELP
#undef READ_IP
#undef READ_OP
#undef READ_DO

#undef SNTX_IP
#undef SNTX_OP
#undef SNTX_NO
#undef SNTX_DO