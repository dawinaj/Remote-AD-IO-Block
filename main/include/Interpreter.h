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
	enum class OPCode : uint8_t
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

	class Instruction
	{
	public:
		OPCode opc = OPCode::NOP;
		uint8_t port = 0;
		union
		{
			uint32_t u = 0;
			float f;
		} arg;

		Instruction() = default;
		~Instruction() = default;
		// DEFAULT_CP_CTOR(Instruction)
		// DEFAULT_MV_CTOR(Instruction)
	};

	//

	using InstrPtr = const Instruction *;
	constexpr InstrPtr nullinstr = nullptr;

	//

	class Loop;
	using LoopPtr = std::unique_ptr<Loop>;
	using Statement = std::variant<std::monostate, Instruction, LoopPtr>;

	//

	class Scope
	{
		std::vector<Statement> statements;
		mutable size_t index = 0;

	public:
		Scope();
		~Scope();
		// DEFAULT_CP_CTOR(Scope) // illegal, cant copy vector with uniqueptrs
		DEFAULT_MV_CTOR(Scope)

		InstrPtr getInstr() const;
		bool finished() const;
		void reset() const;
		void restart() const;

		Instruction &appendInstr(const Instruction & = Instruction());
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

		InstrPtr getInstr() const;
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

		InstrPtr getInstr() const;
		void reset() const;

		size_t size() const;
	};

	//////

	using ParseCb = std::function<bool(const std::vector<std::string> &, Instruction &)>; // in args, inout instr

	struct InstrLUTRow
	{
		Interpreter::OPCode opc;
		const char *namestr;
		const char *argstr;
		const char *descstr;
		size_t argcnt;
		ParseCb parser;
	};

	extern const std::vector<InstrLUTRow> CS_LUT;
};