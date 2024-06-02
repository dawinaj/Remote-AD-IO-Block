#pragma once
#include "COMMON.h"

#include <string>
#include <vector>

#include <memory>
#include <variant>

#include <functional>

#include "CTOR.h"

//

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
		RSTTM,

		INV = uint8_t(-1),
	};

	struct Instruction
	{
		OPCode opc = OPCode::NOP;
		uint8_t port = 0;
		union
		{
			uint32_t u = 0;
			float f;
		} arg;
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
		DEFAULT_CTOR(Scope);
		// DEFAULT_CP_CTOR(Scope); // illegal, cant copy vector with uniqueptrs
		DEFAULT_MV_CTOR(Scope);

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
		bool prgValid = true;

	public:
		DEFAULT_CTOR(Program);
		// DEFAULT_CP_CTOR(Program);
		DEFAULT_MV_CTOR(Program);

		bool parse(const std::string &, std::vector<std::string> &);

		InstrPtr getInstr() const;
		void reset() const;

		size_t size() const;

		bool isValid() const;
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