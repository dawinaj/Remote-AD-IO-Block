#include <stack>
#include <cmath>

#include "Interpreter.h"

#include "to_integer.h"
#include "to_floating_point.h"

using namespace std::string_literals;

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

#define PARSE_ERR(msg)      \
	{                       \
		prgValid = false;   \
		err.push_back(msg); \
		continue;           \
	}

#define ARG_CNT_CHK(name, cnt) \
	if (args.size() != cnt)    \
	PARSE_ERR("Stmt #"s + std::to_string(line) + "(" name "): expected " #cnt " args!")
}

void Program::parse(const std::string &str, std::vector<std::string> &err)
{
	prgValid = true;

	std::stack<Scope *, std::vector<Scope *>> scopes; // underlying storage type
	scopes.push(&scope);							  // main

	size_t beg = 0;
	size_t end = 0;
	size_t line = 0;

	while (beg < str.length())
	{
		end = str.find(';', beg);

		if (end == std::string::npos)
			end = str.length();

		++line;

		if (end - beg > 20) // sanity check
		{
			beg = end + 1;
			PARSE_ERR("Stmt #"s + std::to_string(line) + ": malformed (too long)!");
		}

		std::string stmt = str.substr(beg, end - beg);
		beg = end + 1;

		std::vector<std::string> args = str_split_on_whitespace(stmt);

		// for (const auto& w : args)
		// 	cout << '`' << w << '\'' << '\t';
		// cout << endl;

		if (args.empty()) // no command
			continue;

		std::string cmd = std::move(args[0]);
		args.erase(args.begin());

		if (cmd == "LOOP")
		{
			ARG_CNT_CHK("LOOP", 1);

			size_t iters = 0;
			if (!try_parse_integer(args[0], iters))
				PARSE_ERR("Stmt #"s + std::to_string(line) + " (LOOP): arg #1 expected +int!");

			Loop &l = scopes.top()->appendLoop(iters);
			scopes.push(&l.getScope());
		}

		else if (cmd == "ENDLOOP")
		{
			ARG_CNT_CHK("ENDLOOP", 0);

			if (scopes.size() == 1)
				PARSE_ERR("Stmt #"s + std::to_string(line) + " (ENDLOOP): no matching LOOP!");

			scopes.pop();
		}

		else // regular command
		{
			CmdStatement cs;

			if (cmd == "ANIN")
			{
				ARG_CNT_CHK("ANIN", 1);
				cs.cmd = Command::ANIN;

				if (!try_parse_integer(args[0], cs.port) || !(cs.port >= 1 && cs.port <= 4))
					PARSE_ERR("Stmt #"s + std::to_string(line) + " (ANIN): arg #1 expected input port (1|2|3|4)!");
			}
			else if (cmd == "ANOUT")
			{
				ARG_CNT_CHK("ANOUT", 2);
				cs.cmd = Command::ANOUT;

				if (!try_parse_integer(args[0], cs.port) || !(cs.port >= 1 && cs.port <= 2))
					PARSE_ERR("Stmt #"s + std::to_string(line) + " (ANOUT): arg #1 expected output port (1|2)!");

				if (!try_parse_floating_point(args[1], cs.arg.f) || !std::isfinite(cs.arg.f))
					PARSE_ERR("Stmt #"s + std::to_string(line) + " (ANOUT): arg #2 expected float!");
			}
			else if (cmd == "ANGEN")
			{
				ARG_CNT_CHK("ANGEN", 2);
				cs.cmd = Command::ANGEN;

				if (!try_parse_integer(args[0], cs.port) || !(cs.port >= 1 && cs.port <= 2))
					PARSE_ERR("Stmt #"s + std::to_string(line) + " (ANGEN): arg #1 expected output port (1|2)!");

				if (!try_parse_integer(args[1], cs.arg.i) || !std::isfinite(cs.arg.f))
					PARSE_ERR("Stmt #"s + std::to_string(line) + " (ANGEN): arg #2 expected Generator index!");
			}

			else if (cmd == "DGIN")
			{
				ARG_CNT_CHK("DGIN", 0);
				cs.cmd = Command::DGIN;
			}
			else if (cmd == "DGOUT")
			{
				ARG_CNT_CHK("DGOUT", 1);
				cs.cmd = Command::DGOUT;

				if (!try_parse_integer(args[0], cs.arg.i, 0) || !(cs.arg.i <= 0b1111))
					PARSE_ERR("Stmt #"s + std::to_string(line) + " (DGOUT): arg #1 expected 4-bit hex/oct/dec int!");
			}

			else if (cmd == "DELAY")
			{
				ARG_CNT_CHK("DELAY", 1);
				cs.cmd = Command::DELAY;

				if (!try_parse_integer(args[0], cs.arg.i) || !(cs.arg.i > 0))
					PARSE_ERR("Stmt #"s + std::to_string(line) + " (DELAY): arg #1 expected +int!");
			}
			else if (cmd == "RANGE")
			{
				ARG_CNT_CHK("RANGE", 2);
				cs.cmd = Command::RANGE;

				if (!try_parse_integer(args[0], cs.port) || !(cs.port >= 1 && cs.port <= 4))
					PARSE_ERR("Stmt #"s + std::to_string(line) + " (RANGE): arg #1 expected int 1-4!");

				if (args[1] == "OFF")
					cs.arg.i = 0;
				else if (args[1] == "MIN")
					cs.arg.i = 1;
				else if (args[1] == "MED")
					cs.arg.i = 2;
				else if (args[1] == "MAX")
					cs.arg.i = 3;
				else
					PARSE_ERR("Stmt #"s + std::to_string(line) + " (RANGE): arg #2 expected (OFF|MIN|MED|MAX)!");
			}

			else
				PARSE_ERR("Stmt #"s + std::to_string(line) + ": unknown command!");

			// no error, command finished, append to highest scope
			scopes.top()->appendCmd(cs);
		}
	}
	//*/

	scopes.pop(); // main

	if (scopes.size())
	{
		valid = false;
		err.push_back("Statement #"s + std::to_string(line) + ": LOOP missing matching ENDLOOP! x" + std::to_string(scopes.size()));
	}

	// out.appendCmd(cmd);
}
}
;