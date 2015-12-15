#define CATCH_CONFIG_MAIN
#include "catch/catch.hpp"


typedef unsigned char uint8;
typedef unsigned int uint32;


namespace Instruction
{
	enum Type
	{
		PUSH_FLOAT,
		POP_FLOAT,
		ADD_FLOAT,
		MUL_FLOAT,
		DIV_FLOAT,
		RET_FLOAT
	};
}


class ExpressionVM
{
	public:
	static const int STACK_SIZE = 50;

	public:
	float evaluate(const uint8* code);

	private:
	template<typename T>
	T& pop()
	{
		m_stack_pointer -= sizeof(T);
		return *(T*)(m_stack + m_stack_pointer);
	}

	template <typename T>
	const uint8* pushStackConst(const uint8* cp)
	{
		*(T*)(m_stack + m_stack_pointer) = *(T*)cp;
		m_stack_pointer += sizeof(T);
		return cp + sizeof(T);
	}

	template <typename T>
	void push(T value)
	{
		*(T*)(m_stack + m_stack_pointer) = value;
		m_stack_pointer += sizeof(T);
	}

	private:
	uint8 m_stack[STACK_SIZE];
	int m_stack_pointer;
};


float ExpressionVM::evaluate(const uint8* code)
{
	m_stack_pointer = 0;
	const uint8* cp = code;
	for(;;)
	{
		uint8 type = *cp;
		++cp;
		switch(type)
		{
			case Instruction::RET_FLOAT:
				return pop<float>();
			case Instruction::ADD_FLOAT:
				push<float>(pop<float>() + pop<float>());
				break;
			case Instruction::PUSH_FLOAT:
				cp = pushStackConst<float>(cp);
				break;
			case Instruction::MUL_FLOAT:
				push<float>(pop<float>() * pop<float>());
				break;
			case Instruction::DIV_FLOAT:
				push<float>(pop<float>() / pop<float>());
				break;
			default:
				DebugBreak();
				break;
		}
	}
}


#define CLOSE_EQ(f, a, e) \
	if(!((f) > (a) - (e) && (f) < (a) + (e))) DebugBreak();

auto c = [](float f, int i) -> uint8
{
	union
	{
		float f;
		uint8 i[4];
	} u;
	u.f = f;
	return u.i[i];
};

#define FLOAT_BYTES(f) \
	Instruction::PUSH_FLOAT, c((f), 0), c((f), 1), c((f), 2), c((f), 3)

float floatBinaryOperator(float f1, float f2, Instruction::Type type)
{
	ExpressionVM vm;
	uint8 code[] = {
		FLOAT_BYTES(f1),
		FLOAT_BYTES(f2),
		type,
		Instruction::RET_FLOAT};
	 return vm.evaluate(code);
}

#undef FLOAT_BYTES2


TEST_CASE("Run", "Execute bytecode") {
	SECTION("Multiply") {
		CHECK(floatBinaryOperator(2, 4, Instruction::MUL_FLOAT) == Approx(8.0f));
		CHECK(floatBinaryOperator(5, -4, Instruction::MUL_FLOAT) == Approx(-20.0f));
		CHECK(floatBinaryOperator(3, 0, Instruction::MUL_FLOAT) == Approx(0.0f));
	}

	SECTION("Add") {
		CHECK(floatBinaryOperator(2, 4, Instruction::ADD_FLOAT) == Approx(6.0f));
		CHECK(floatBinaryOperator(5, -4, Instruction::ADD_FLOAT) == Approx(1.0f));
		CHECK(floatBinaryOperator(3, 0, Instruction::ADD_FLOAT) == Approx(3.0f));
		CHECK(floatBinaryOperator(3, -4, Instruction::ADD_FLOAT) == Approx(-1.0f));
	}
}
