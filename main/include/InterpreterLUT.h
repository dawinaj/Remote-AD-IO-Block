using ParseCb = std::function<bool(std::vector<std::string> &, Statement &)>;

struct StmtLUTRow
{
	Interpreter::Command cmd;
	const char *namestr;
	const char *argstr;
	size_t argcnt;
	ParseCb parser;
};

#define CB_HELP(COND) [](std::vector<std::string> &args, CmdStatement &cs) { return (COND); }
#define READ_IP (try_parse_integer(args[0], cs.port) && (cs.port >= 1 && cs.port <= 4))
#define READ_OP (try_parse_integer(args[0], cs.port) && (cs.port >= 1 && cs.port <= 2))
#define READ_DO (try_parse_integer(args[0], cs.arg.u, 0) && (cs.arg.u <= 0b1111))

#define SNTX_IP "<port (1|2|3|4)>"
#define SNTX_OP "<port (1|2)>"
#define SNTX_NO "<no args>"
#define SNTX_DO "<state (4 bits hex/oct/dec)>"

static const std::array CS_LUT = std::to_array<StmtLUTRow>({
	{
		Interpreter::Command::NOP,
		"NOP",
		SNTX_NO,
		0,
		CB_HELP(false),
	},
	//
	{
		Interpreter::Command::AIRDF,
		"AIRDF",
		SNTX_IP,
		1,
		CB_HELP(READ_IP),
	},
	{
		Interpreter::Command::AIRDM,
		"AIRDM",
		SNTX_IP,
		1,
		CB_HELP(READ_IP),
	},
	{
		Interpreter::Command::AIRDU,
		"AIRDU",
		SNTX_IP,
		1,
		CB_HELP(READ_IP),
	},
	//
	{
		Interpreter::Command::AIEN,
		"AIEN",
		SNTX_NO,
		0,
		CB_HELP(true),
	},
	{
		Interpreter::Command::AIDIS,
		"AIDIS",
		SNTX_NO,
		0,
		CB_HELP(true),
	},
	{
		Interpreter::Command::AIRNG,
		"AIRNG",
		SNTX_IP " <range (OFF|MIN|MED|MAX)>",
		2,
		CB_HELP(READ_IP && (try_parse_range(args[1], cs.arg.u))),
	},
	//
	{
		Interpreter::Command::AOVAL,
		"AOVAL",
		SNTX_OP " <voltage (float)>",
		2,
		CB_HELP(READ_OP && (try_parse_floating_point(args[1], cs.arg.f) && std::isfinite(cs.arg.f))),
	},
	{
		Interpreter::Command::AOGEN,
		"AOGEN",
		SNTX_OP " <generator_idx (uint)>",
		2,
		CB_HELP(READ_OP && (try_parse_integer(args[1], cs.arg.u))),
	},
	//
	{
		Interpreter::Command::DIRD,
		"DIRD",
		SNTX_NO,
		0,
		CB_HELP(true),
	},
	//
	{
		Interpreter::Command::DOWR,
		"DOWR",
		SNTX_DO,
		1,
		CB_HELP(READ_DO),
	},
	{
		Interpreter::Command::DOSET,
		"DOSET",
		SNTX_DO,
		1,
		CB_HELP(READ_DO),
	},
	{
		Interpreter::Command::DORST,
		"DORST",
		SNTX_DO,
		1,
		CB_HELP(READ_DO),
	},
	{
		Interpreter::Command::DOAND,
		"DOAND",
		SNTX_DO,
		1,
		CB_HELP(READ_DO),
	},
	{
		Interpreter::Command::DOXOR,
		"DOXOR",
		SNTX_DO,
		1,
		CB_HELP(READ_DO),
	},
	//
	{
		Interpreter::Command::DELAY,
		"DELAY",
		"<microseconds (uint32)>",
		1,
		CB_HELP(try_parse_integer(args[0], cs.arg.u)),
	},
	{
		Interpreter::Command::GETTM,
		"GETTM",
		SNTX_NO,
		1,
		CB_HELP(READ_DO),
	},
});

#undef CB_HELP
#undef READ_IP
#undef READ_OP
#undef READ_DO

#undef SNTX_IP
#undef SNTX_OP
#undef SNTX_NO
#undef SNTX_DO