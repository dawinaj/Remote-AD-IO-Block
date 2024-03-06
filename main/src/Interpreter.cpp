#include <stack>
#include <cmath>

#include "Interpreter.h"

#include "to_integer.h"
#include "to_floating_point.h"

using namespace std::string_literals;

#define PARSE_ERR(reason)                                                \
	{                                                                    \
		prgValid = false;                                                \
		err.push_back("Stmt #"s + std::to_string(line) + ": " + reason); \
		continue;                                                        \
	}

static const char *expstx = "expected syntax: ";

#define PARSE_ERR_SNTX() PARSE_ERR(expstx + syntax)

#define ARG_CNT_CHK(cnt)    \
	if (args.size() != cnt) \
	PARSE_ERR_SNTX()

std::vector<std::string> str_split_on_whitespace(const std::string &str)
{
	std::vector<std::string> ret;

	size_t i = 0;
	size_t j = 0;
	size_t l = str.length();

	while (j < l)
	{
		while (i < l && std::isspace(str[i]))
			++i;

		j = i;

		while (j < l && !std::isspace(str[j]))
			++j;

		if (i != j)
			ret.push_back(str.substr(i, j - i));

		i = j;
	}

	return ret;
}

namespace Executing
{
	// Scope

	Scope::Scope() = default;
	Scope::~Scope() = default;

	CmdPtr Scope::getCmd()
	{
		if (finished()) [[unlikely]] // never
			return nullcmd;

		AnyStatement &stmt = statements[index];

		switch (stmt.index()) // check type
		{
		case 1: // cmd
		{
			++index;
			return &std::get<CmdStatement>(stmt);
		}
		case 2: // loop
		{
			Loop &loop = *std::get<LoopPtr>(stmt);
			CmdPtr ret = loop.getCmd();
			if (loop.finished())
			{
				++index;
				loop.restart();
			}
			return ret;
		}
		default:
			return nullcmd;
		}
	}

	bool Scope::finished()
	{
		return index >= statements.size();
	}
	void Scope::reset()
	{
		index = 0;
		for (AnyStatement &stmt : statements)
		{
			switch (stmt.index()) // check type
			{
			case 2:
			{
				Loop &loop = *std::get<LoopPtr>(stmt);
				loop.reset();
				continue;
			}
			default:
				continue;
			}
		}
	}
	void Scope::restart()
	{
		index = 0;
	}

	CmdStatement &Scope::appendCmd(const CmdStatement &c)
	{
		statements.emplace_back(std::in_place_type<CmdStatement>, c);
		return std::get<CmdStatement>(statements.back());
	}

	Loop &Scope::appendLoop(size_t iters)
	{
		statements.emplace_back(std::in_place_type<LoopPtr>, std::make_unique<Loop>(iters));
		return *std::get<LoopPtr>(statements.back());
	}

	// Loop

	Loop::Loop(size_t mi) : max_iter(mi) {}
	Loop::~Loop() = default;

	CmdPtr Loop::getCmd()
	{
		if (finished()) [[unlikely]] // never
			return nullcmd;

		CmdPtr ret = scope.getCmd();

		if (scope.finished())
		{
			++iter;
			scope.restart();
		}

		return ret;
	}

	bool Loop::finished()
	{
		return iter >= max_iter;
	}
	void Loop::reset()
	{
		iter = 0;
		scope.reset();
	}
	void Loop::restart()
	{
		iter = 0;
	}

	Scope &Loop::getScope()
	{
		return scope;
	}

	//

	// Program

	CmdPtr Program::getCmd()
	{
		if (scope.finished()) [[unlikely]]
			return nullcmd;
		return scope.getCmd();
	}
	void Program::reset()
	{
		scope.reset();
	}

	//

	bool Program::parse(const std::string &str, std::vector<std::string> &err)
	{
		bool prgValid = true;

		std::stack<Scope *, std::vector<Scope *>> scopes; // underlying storage type
		scopes.push(&scope);							  // main

		size_t beg = 0;
		size_t end = 0;
		size_t line = 0;

		while (beg < str.length())
		{
			++line;

			end = str.find(';', beg);
			if (end == std::string::npos)
				end = str.length();

			if (end - beg > 32) // sanity check
			{
				beg = end + 1;
				PARSE_ERR("malformed (too long, max 32)!");
			}

			std::string stmt = str.substr(beg, end - beg);
			beg = end + 1;

			std::vector<std::string> args = str_split_on_whitespace(stmt); // split and strip WSs

			// for (const auto& w : args)
			// 	cout << '`' << w << '\'' << '\t';
			// cout << endl;

			if (args.empty()) // no command
				continue;

			std::string cmd = std::move(args[0]);
			args.erase(args.begin());

			if (cmd == "LOOP")
			{
				const char *syntax = "LOOP <iterations (uint32)>";

				ARG_CNT_CHK(1)

				size_t iters = 0;
				if (!try_parse_integer(args[0], iters))
					PARSE_ERR_SNTX()

				Loop &l = scopes.top()->appendLoop(iters);
				scopes.push(&l.getScope());
			}

			else if (cmd == "ENDLOOP")
			{
				const char *syntax = "ENDLOOP <no args>";

				ARG_CNT_CHK(0)

				if (scopes.size() == 1)
					PARSE_ERR("ENDLOOP has no matching LOOP!")

				scopes.pop();
			}

			else // regular command
			{
				CmdStatement cs;

				// analog
				if (cmd == "ANIN")
				{
					const char *syntax = "ANIN <port (1|2|3|4)>";
					cs.cmd = Command::ANIN;

					ARG_CNT_CHK(1)

					if (!try_parse_integer(args[0], cs.port) || !(cs.port >= 1 && cs.port <= 4))
						PARSE_ERR_SNTX()
				}

				else if (cmd == "ANOUT")
				{
					const char *syntax = "ANOUT <port (1|2)> <voltage (float)>";
					cs.cmd = Command::ANOUT;

					ARG_CNT_CHK(2)

					if (!try_parse_integer(args[0], cs.port) || !(cs.port >= 1 && cs.port <= 2))
						PARSE_ERR_SNTX()

					if (!try_parse_floating_point(args[1], cs.arg.f) || !std::isfinite(cs.arg.f))
						PARSE_ERR_SNTX()
				}

				else if (cmd == "ANGEN")
				{
					const char *syntax = "ANGEN <port (1|2)> <generator idx (uint<16)>";
					cs.cmd = Command::ANGEN;

					ARG_CNT_CHK(2)

					if (!try_parse_integer(args[0], cs.port) || !(cs.port >= 1 && cs.port <= 2))
						PARSE_ERR_SNTX()

					if (!try_parse_integer(args[1], cs.arg.u) || !(cs.port < 16))
						PARSE_ERR_SNTX()
				}

				// digital
				else if (cmd == "DGIN")
				{
					const char *syntax = "DGIN <no args>";
					cs.cmd = Command::DGIN;

					ARG_CNT_CHK(0)
				}

				else if (cmd == "DGOUT")
				{
					const char *syntax = "DGOUT <state (4 bits hex/oct/dec)>";
					cs.cmd = Command::DGOUT;

					ARG_CNT_CHK(1)

					if (!try_parse_integer(args[0], cs.arg.u, 0) || !(cs.arg.u <= 0b1111))
						PARSE_ERR_SNTX()
				}

				// other
				else if (cmd == "DELAY")
				{
					const char *syntax = "DELAY <microseconds (+uint32)>";
					cs.cmd = Command::DELAY;

					ARG_CNT_CHK(1)

					if (!try_parse_integer(args[0], cs.arg.u) || !(cs.arg.u > 0))
						PARSE_ERR_SNTX()
				}

				else if (cmd == "RANGE")
				{
					const char *syntax = "RANGE <port (1|2|3|4)> <range (OFF|MIN|MED|MAX)>";
					cs.cmd = Command::RANGE;

					ARG_CNT_CHK(2)

					if (!try_parse_integer(args[0], cs.port) || !(cs.port >= 1 && cs.port <= 4))
						PARSE_ERR_SNTX()

					if (args[1] == "OFF")
						cs.arg.u = 0;
					else if (args[1] == "MIN")
						cs.arg.u = 1;
					else if (args[1] == "MED")
						cs.arg.u = 2;
					else if (args[1] == "MAX")
						cs.arg.u = 3;
					else
						PARSE_ERR_SNTX()
				}

				else
					PARSE_ERR("unknown command!")

				// no error, command finished, append to highest scope
				scopes.top()->appendCmd(cs);
			}
		}
		//*/

		scopes.pop(); // main

		for (size_t i = 0; i < scopes.size(); ++i)
			PARSE_ERR("LOOP missing matching ENDLOOP!");

		// out.appendCmd(cmd);

		return prgValid;
	}
};