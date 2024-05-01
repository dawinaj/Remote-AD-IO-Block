#include "Interpreter.h"

#include <cmath>
// #include <array>
#include <stack>

#include "to_integer.h"
#include "to_floating_point.h"

using namespace std::string_literals;

static const char *TAG = "Interpreter";

#define PARSE_ERR(reason)                                                \
	do                                                                   \
	{                                                                    \
		prgValid = false;                                                \
		err.push_back("Stmt #"s + std::to_string(line) + ": " + reason); \
	} while (0)

static const char *expstx = "expected syntax: ";

#define PARSE_ERR_SNTX(syntax) PARSE_ERR(expstx + syntax)

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

#include "InterpreterLUT.h"

	// Scope

	Scope::Scope() = default;
	Scope::~Scope() = default;

	StmtPtr Scope::getStmt() const
	{
		if (finished()) [[unlikely]] // never
			return nullstmt;

		const AnyStatement &stmt = statements[index];

		switch (stmt.index()) // check type
		{
		case 1: // cmd
		{
			++index;
			return &std::get<Statement>(stmt);
		}
		case 2: // loop
		{
			const Loop &loop = *std::get<LoopPtr>(stmt);
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

	bool Scope::finished() const
	{
		return index >= statements.size();
	}
	void Scope::reset() const
	{
		index = 0;
		for (const AnyStatement &stmt : statements)
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
	void Scope::restart() const
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

	size_t Scope::size() const
	{
		return statements.size();
	}

	// Loop

	Loop::Loop(size_t mi) : max_iter(mi) {}
	Loop::~Loop() = default;

	StmtPtr Loop::getStmt() const
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

	bool Loop::finished() const
	{
		return iter >= max_iter;
	}
	void Loop::reset() const
	{
		iter = 0;
		scope.reset();
	}
	void Loop::restart() const
	{
		iter = 0;
	}

	Scope &Loop::getScope()
	{
		return scope;
	}

	//

	// Program

	StmtPtr Program::getStmt() const
	{
		if (scope.finished()) [[unlikely]]
			return nullstmt;
		return scope.getStmt();
	}
	void Program::reset() const
	{
		scope.reset();
	}

	size_t Program::size() const
	{
		return scope.size();
	}

	// Parser

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
				continue;
			}

			std::string stmtstr = str.substr(beg, end - beg);
			beg = end + 1;

			// ESP_LOGD(TAG, "Cmd: `%s`", stmtstr.c_str());
			//  if (stmtstr.empty()) // no command
			//  	continue;

			std::vector<std::string> args = str_split_on_whitespace(stmtstr); // split and strip WSs

			if (args.empty()) // no command
				continue;

			// for (const auto& w : args)
			// 	cout << '`' << w << '\'' << '\t';
			// cout << endl;

			std::string cmd = std::move(args[0]);
			args.erase(args.begin());

			if (cmd == "LOOP")
			{
				size_t iters = 0;

				if (args.size() != 1 || !try_parse_integer(args[0], iters))
					PARSE_ERR_SNTX("LOOP <iterations (uint32)>");

				Loop &l = scopes.top()->appendLoop(iters);
				scopes.push(&l.getScope());
			}
			else if (cmd == "ENDLOOP")
			{
				if (args.size() != 0)
					PARSE_ERR_SNTX("ENDLOOP <no args>");

				if (scopes.size() == 1)
					PARSE_ERR("ENDLOOP: no previous LOOP to end!");

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

						if ((args.size() != lut.argcnt) || (lut.parser && !lut.parser(args, cs)))
							PARSE_ERR_SNTX(lut.namestr + ' ' + lut.argstr);

						break;
					}
				}

				if (cs.cmd == Command::NOP)
					PARSE_ERR("unknown command!");

				scopes.top()->appendStmt(cs);
			}
		}

		scopes.pop(); // main

		line = -1;
		for (size_t i = 0; i < scopes.size(); ++i)
			PARSE_ERR("LOOP missing matching ENDLOOP!");

		return prgValid;
	}
};