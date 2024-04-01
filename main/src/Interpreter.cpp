#include "Interpreter.h"

#include <cmath>

#include <array>
#include <stack>

#include <functional>

#include "to_integer.h"
#include "to_floating_point.h"

using namespace std::string_literals;

#define PARSE_ERR(reason)                                                \
	{                                                                    \
		prgValid = false;                                                \
		err.push_back("Stmt #"s + std::to_string(line) + ": " + reason); \
		goto end_of_stmt;                                                \
	}

static const char *expstx = "expected syntax: ";

#define PARSE_ERR_SNTX() PARSE_ERR(expstx + syntax)

#define ARG_CNT_CHK(cnt)    \
	if (args.size() != cnt) \
	PARSE_ERR_SNTX()

static std::vector<std::string> str_split_on_whitespace(const std::string &str)
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

namespace Interpreter
{
#include "InterpreterLUT.h"

	// Scope

	Scope::Scope() = default;
	Scope::~Scope() = default;

	StmtPtr Scope::getStmt()
	{
		if (finished()) [[unlikely]] // never
			return nullstmt;

		AnyStatement &stmt = statements[index];

		switch (stmt.index()) // check type
		{
		case 1: // cmd
		{
			++index;
			return &std::get<Statement>(stmt);
		}
		case 2: // loop
		{
			Loop &loop = *std::get<LoopPtr>(stmt);
			StmtPtr ret = loop.getStmt();
			if (loop.finished())
			{
				++index;
				loop.restart();
			}
			return ret;
		}
		default:
			return nullstmt;
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
				break;
			}
			default:
				break;
			}
		}
	}
	void Scope::restart()
	{
		index = 0;
	}

	Statement &Scope::appendStmt(const Statement &c)
	{
		statements.emplace_back(std::in_place_type<Statement>, c);
		return std::get<Statement>(statements.back());
	}

	Loop &Scope::appendLoop(size_t iters)
	{
		statements.emplace_back(std::in_place_type<LoopPtr>, std::make_unique<Loop>(iters));
		return *std::get<LoopPtr>(statements.back());
	}

	// Loop

	Loop::Loop(size_t mi) : max_iter(mi) {}
	Loop::~Loop() = default;

	StmtPtr Loop::getStmt()
	{
		if (finished()) [[unlikely]] // never
			return nullstmt;

		StmtPtr ret = scope.getStmt();

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

	StmtPtr Program::getStmt()
	{
		if (scope.finished()) [[unlikely]]
			return nullstmt;
		return scope.getStmt();
	}
	void Program::reset()
	{
		scope.reset();
	}

	//

	// helper

	static bool try_parse_range(const std::string &arg, uint32_t &val)
	{
		if (arg == "OFF")
			val = 0;
		else if (arg == "MIN")
			val = 1;
		else if (arg == "MED")
			val = 2;
		else if (arg == "MAX")
			val = 3;
		else
			return false;

		return true;
	}

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
			//*/
			else // regular command
			{
				Statement cs;

				for (const auto &lut : CS_LUT)
				{
					if (cmd == lut.namestr)
					{
						cs.cmd = lut.cmd;

						ARG_CNT_CHK(2)

						if (!try_parse_integer(args[0], cs.port) || !(cs.port >= 1 && cs.port <= 2))
							PARSE_ERR_SNTX()

						if (!try_parse_floating_point(args[1], cs.arg.f) || !std::isfinite(cs.arg.f))
							PARSE_ERR_SNTX()
					}
				}
				PARSE_ERR("unknown command!")
			}
		//*/
		end_of_stmt:
		}
		//*/

		scopes.pop(); // main

		for (size_t i = 0; i < scopes.size(); ++i)
			PARSE_ERR("LOOP missing matching ENDLOOP!");

		return prgValid;
	}
};