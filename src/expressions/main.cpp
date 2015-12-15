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


struct Token
{
	enum Type { EMPTY, NUMBER, OPERATOR, LEFT_PARENTHESIS, RIGHT_PARENTHESIS };
	Type type;

	enum Operator {
		ADD,
		MULTIPLY,
		DIVIDE
	};

	int offset;

	float number;
	Operator oper;
};


class ExpressionVM
{
public:
	static const int STACK_SIZE = 50;


public:
	float evaluate(const uint8* code);
	int tokenize(const char* src, Token* tokens, int max_size);
	int compile(const char* src, const Token* tokens, int token_count, uint8* byte_code, int max_size);
	int toPostfix(const Token* input, Token* output, int count);
	float compileAndRun(const char* src);

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


	static int getOperatorPriority(const Token& token)
	{
		switch(token.oper)
		{
			case Token::ADD: return 0;
			case Token::MULTIPLY: return 1;
			case Token::DIVIDE: return 1;
		}
		return -1;
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
				{
					float f = pop<float>();
					push<float>(pop<float>() / f);
				}
				break;
			default:
				DebugBreak();
				break;
		}
	}
}


float ExpressionVM::compileAndRun(const char* src)
{
	static const int MAX_TOKENS_COUNT = 50;
	static const int MAX_BYTECODE_SIZE = 50;
	uint8 byte_code[MAX_BYTECODE_SIZE];
	Token tokens[MAX_TOKENS_COUNT];
	Token postfix_tokens[MAX_TOKENS_COUNT];
	int tokens_count = tokenize(src, tokens, MAX_TOKENS_COUNT);
	int postfix_tokens_count = toPostfix(tokens, postfix_tokens, tokens_count);
	compile(src, postfix_tokens, postfix_tokens_count, byte_code, MAX_BYTECODE_SIZE);
	return evaluate(byte_code);
}


int ExpressionVM::toPostfix(const Token* input, Token* output, int count)
{
	Token func_stack[64];
	int func_stack_idx = 0;
	Token* out = output;
	int out_token_count = count;
	for(int i = 0; i < count; ++i)
	{
		const Token& token = input[i];
		if(token.type == Token::NUMBER)
		{
			*out = token;
			++out;
		}
		else if (token.type == Token::LEFT_PARENTHESIS)
		{
			--out_token_count;
			func_stack[func_stack_idx] = token;
			++func_stack_idx;
		}
		else if (token.type == Token::RIGHT_PARENTHESIS)
		{
			--out_token_count;
			while (func_stack_idx > 0 && func_stack[func_stack_idx - 1].type != Token::LEFT_PARENTHESIS)
			{
				--func_stack_idx;
				*out = func_stack[func_stack_idx];
				++out;
			}

			if (func_stack_idx > 0) --func_stack_idx;
		}
		else
		{
			int prio = getOperatorPriority(token);
			while(func_stack_idx > 0 && getOperatorPriority(func_stack[func_stack_idx - 1]) > prio)
			{
				--func_stack_idx;
				*out = func_stack[func_stack_idx];
				++out;
			}

			func_stack[func_stack_idx] = token;
			++func_stack_idx;
		}
	}

	for(int i = func_stack_idx - 1; i >= 0; --i)
	{
		*out = func_stack[i];
		++out;
	}
	return out_token_count;
}



int ExpressionVM::compile(const char* src, const Token* tokens, int token_count, uint8* byte_code, int max_size)
{
	uint8* out = byte_code;
	for(int i = 0; i < token_count; ++i)
	{
		auto& token = tokens[i];
		switch(token.type)
		{
			case Token::NUMBER:
				if (max_size - (out - byte_code) < sizeof(float) + 1) return out - byte_code;
				*out = Instruction::PUSH_FLOAT;
				++out;
				*(float*)out = token.number;
				out += sizeof(float);
				break;
			case Token::OPERATOR:
				switch(token.oper)
				{
					case Token::ADD: *out = Instruction::ADD_FLOAT; ++out; break;
					case Token::MULTIPLY: *out = Instruction::MUL_FLOAT; ++out; break;
					case Token::DIVIDE: *out = Instruction::DIV_FLOAT; ++out; break;
				}
				break;
			default:
				DebugBreak();
				break;
		}
	}
	if (max_size - (out - byte_code) < 1) return out - byte_code;
	*out = Instruction::RET_FLOAT;
	++out;
	return out - byte_code;
}


int ExpressionVM::tokenize(const char* src, Token* tokens, int max_size)
{
	const char* c = src;
	int token_count = 0;
	while (*c)
	{
		Token token = { Token::EMPTY, c - src };
		if (*c == '(')
		{
			token.type = Token::LEFT_PARENTHESIS;
		}
		else if (*c == ')')
		{
			token.type = Token::RIGHT_PARENTHESIS;
		}
		else if (*c >= '0' && *c <= '9')
		{
			token.type = Token::NUMBER;
			char* out;
			token.number = strtof(c, &out);
			c = out-1;
		}
		else
		{
			switch(*c)
			{
				case '+': token.type = Token::OPERATOR; token.oper = Token::ADD; break;
				case '*': token.type = Token::OPERATOR; token.oper = Token::MULTIPLY; break;
				case '/': token.type = Token::OPERATOR; token.oper = Token::DIVIDE; break;
			}
		}
		if(token.type != Token::EMPTY)
		{
			if(token_count < max_size)
			{
				tokens[token_count] = token;
			}
			++token_count;
		}
		++c;
	}
	return token_count;
}


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


TEST_CASE("Tokenize", "Convert string to tokens") {
	ExpressionVM vm;
	static const int MAX_TOKENS = 100;
	Token tokens[MAX_TOKENS];
	CHECK(vm.tokenize("4 * 2 + 3.0", tokens, MAX_TOKENS) == 5);
	CHECK(tokens[0].type == Token::NUMBER);
	CHECK(tokens[1].type == Token::OPERATOR);
	CHECK(tokens[2].type == Token::NUMBER);
	CHECK(tokens[3].type == Token::OPERATOR);
	CHECK(tokens[4].type == Token::NUMBER);
	CHECK(vm.tokenize("2.5", tokens, MAX_TOKENS) == 1);
	CHECK(tokens[0].type == Token::NUMBER);
	CHECK(vm.tokenize("", tokens, MAX_TOKENS) == 0);
}


TEST_CASE("Compile", "Compile source to bytecode") {
	ExpressionVM vm;
	const char* src = "4.5 + 10 * 3 + 5.5";
	static const int MAX_TOKENS = 7;
	Token tokens[MAX_TOKENS];
	REQUIRE(vm.tokenize(src, tokens, MAX_TOKENS) == MAX_TOKENS);

	Token postfix_tokens[MAX_TOKENS];
	int postfix_tokens_count = vm.toPostfix(tokens, postfix_tokens, MAX_TOKENS);

	static const int BYTE_CODE_SIZE = 150;
	uint8 byte_code[BYTE_CODE_SIZE];

	int size = vm.compile(src, postfix_tokens, postfix_tokens_count, byte_code, BYTE_CODE_SIZE);
	CHECK(size == 24);

	float x = vm.evaluate(byte_code);
	CHECK(x == Approx(40.0f));
}


TEST_CASE("Compile & Run", "Compile source to bytecode and run it") {
	ExpressionVM vm;
	CHECK(vm.compileAndRun("4.5 + 10 * 3 + 5.5") == Approx(40.0f));
	CHECK(vm.compileAndRun("(4.5 + 10) * 3 + 5.5") == Approx(49.0f));
	CHECK(vm.compileAndRun("4.5 + (10 * 3) + 5.5") == Approx(40.0f));
	CHECK(vm.compileAndRun("4.5 + 10 * (3 + 5.5)") == Approx(89.5f));
	CHECK(vm.compileAndRun("(4.5 + 10 * 3 + 5.5)") == Approx(40.0f));
	CHECK(vm.compileAndRun("(4.5 + 10 * 3) + 5.5") == Approx(40.0f));
	CHECK(vm.compileAndRun("4.5 + (10 * 3 + 5.5)") == Approx(40.0f));

	CHECK(vm.compileAndRun("5 / 2") == Approx(2.5f));
	CHECK(vm.compileAndRun("2.5 / 2") == Approx(1.25f));
	CHECK(vm.compileAndRun("1 / 2.0") == Approx(0.5f));
}



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
