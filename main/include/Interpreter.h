#pragma once
#include "COMMON.h"

#include <string>
#include <vector>

#include <memory>
#include <variant>

#include <functional>

//

#define DEFAULT_CP_CTOR(Class)   \
	Class(Class &rhs) = default; \
	Class &operator=(Class &rhs) = default;

#define DEFAULT_MV_CTOR(Class)    \
	Class(Class &&rhs) = default; \
	Class &operator=(Class &&rhs) = default;

namespace Interpreter
{
	using CmdUT = uint8_t;

	// enum CmdDir : CmdUT
	// {
	// 	Analog = 0b10000000,
	// 	Digital = 0b01000000,
	// 	Misc = 0,
	// };

	// enum CmdTrait : CmdUT
	// {

	// 	Analog = 1 << 13,
	// 	Digital = 1 << 12,
	// 	Misc = 1 << 11,

	// 	Read = 1 << 10,
	// 	Write = 1 << 9,
	// 	Other = 1 << 8,
	// };

	enum class Command : CmdUT
	{
		NOP = 0,

		AIRDF,
		AIRDM,
		AIRDU,

		AIEN,
		AIDIS,
		AIRNG,

		AOVAL,
		AOGEN,

		DIRD,

		DOWR,
		DOSET,
		DORST,
		DOAND,
		DOXOR,

		DELAY,
		GETTM,

	};

	class Statement
	{
	public:
		Command cmd = Command::NOP;
		uint8_t port = 0;
		union
		{
			uint32_t u = 0;
			float f;
		} arg;

		Statement() = default;
		~Statement() = default;
		// DEFAULT_CP_CTOR(Statement)
		// DEFAULT_MV_CTOR(Statement)
	};

	//

	using StmtPtr = const Statement *;
	const StmtPtr nullstmt = nullptr;

	//

	class Loop;
	using LoopPtr = std::unique_ptr<Loop>;
	using AnyStatement = std::variant<std::monostate, Statement, LoopPtr>;

	//

	class Scope
	{
		std::vector<AnyStatement> statements;
		mutable size_t index = 0;

	public:
		Scope();
		~Scope();
		// DEFAULT_CP_CTOR(Scope) // illegal, cant copy vector with uniqueptrs
		DEFAULT_MV_CTOR(Scope)

		StmtPtr getStmt() const;
		bool finished() const;
		void reset() const;
		void restart() const;

		Statement &appendStmt(const Statement & = Statement());
		Loop &appendLoop(size_t);

		size_t size() const;
	};

	//

	class Loop
	{
		Scope scope;
		size_t max_iter = 0;
		mutable size_t iter = 0;

	public:
		Loop(size_t = 0);
		~Loop();
		// DEFAULT_CP_CTOR(Loop)
		// DEFAULT_MV_CTOR(Loop)

		StmtPtr getStmt() const;
		bool finished() const;

		void reset() const;
		void restart() const;

		Scope &getScope();
	};

	//

	class Program
	{
		Scope scope;

	public:
		Program() = default;
		~Program() = default;
		// DEFAULT_CP_CTOR(Program)
		DEFAULT_MV_CTOR(Program)

		bool parse(const std::string &, std::vector<std::string> &);

		StmtPtr getStmt() const;
		void reset() const;

		size_t size() const;
	};

	//////

	using ParseCb = std::function<bool(const std::vector<std::string> &, Statement &)>; // in args, out stmt

	struct StmtLUTRow
	{
		Interpreter::Command cmd;
		const char *namestr;
		const char *argstr;
		const char *descstr;
		size_t argcnt;
		ParseCb parser;
	};

	extern const std::vector<StmtLUTRow> CS_LUT;
};