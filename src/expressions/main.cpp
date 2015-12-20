#define CATCH_CONFIG_MAIN
#include "catch/catch.hpp"
#include <cmath>

/*
TODO:
- logical operators - and, or
- split compiler, runtime env
*/

typedef unsigned char uint8;
typedef unsigned int uint16;
typedef unsigned int uint32;


enum class Types : uint8
{
	FLOAT,
	BOOL,

	NONE
};


enum class CompileTimeError
{
	NONE,
	UNKNOWN_IDENTIFIER,
	MISSING_LEFT_PARENTHESIS,
	MISSING_RIGHT_PARENTHESIS,
	UNEXPECTED_CHAR,
	OUT_OF_MEMORY,
	MISSING_BINARY_OPERAND,
	NOT_ENOUGH_PARAMETERS,
	INCORRECT_TYPE_ARGS
};


namespace Instruction
{
	enum Type : uint8
	{
		PUSH_FLOAT,
		POP_FLOAT,
		ADD_FLOAT,
		MUL_FLOAT,
		DIV_FLOAT,
		RET_FLOAT,
		RET_BOOL,
		SUB_FLOAT,
		UNARY_MINUS,
		CALL,
		FLOAT_LT,
		FLOAT_GT,
		AND,
		OR
	};
}


struct Token
{
	enum Type { EMPTY, NUMBER, OPERATOR, IDENTIFIER, LEFT_PARENTHESIS, RIGHT_PARENTHESIS };
	Type type;

	enum Operator {
		ADD,
		MULTIPLY,
		DIVIDE,
		SUBTRACT,
		UNARY_MINUS,
		LESS_THAN,
		GREATER_THAN,
		AND,
		OR
	};

	int offset;
	int size;

	float number;
	Operator oper;
};


class ExpressionVM
{
public:
	static const int STACK_SIZE = 50;

	struct ReturnValue
	{
		ReturnValue()
		{
			type = Types::NONE;
		}

		ReturnValue(float f) 
		{
			f_value = f;
			type = Types::FLOAT;
		}

		ReturnValue(bool b)
		{
			b_value = b;
			type = Types::BOOL;
		}

		Types type;
		union
		{
			float f_value;
			bool b_value;
		};
	};

public:
	ReturnValue evaluate(const uint8* code);
	int tokenize(const char* src, Token* tokens, int max_size);
	int compile(const char* src, const Token* tokens, int token_count, uint8* byte_code, int max_size);
	int toPostfix(const Token* input, Token* output, int count);
	ReturnValue compileAndRun(const char* src);
	CompileTimeError getCompileTimeError() const { return m_compile_time_error; }

private:
	void callFunction(uint16 idx);

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
		if (token.type == Token::IDENTIFIER) return 3;
		if (token.type == Token::LEFT_PARENTHESIS) return -1;
		if (token.type != Token::OPERATOR) DebugBreak();
		switch (token.oper)
		{
			case Token::OR: return 0;
			case Token::AND: return 1;
			case Token::LESS_THAN: return 2;
			case Token::GREATER_THAN: return 2;
			case Token::ADD: return 3;
			case Token::SUBTRACT: return 3;
			case Token::MULTIPLY: return 4;
			case Token::DIVIDE: return 4;
			case Token::UNARY_MINUS: return 4;
		}
		return -1;
	}


private:
	uint8 m_stack[STACK_SIZE];
	int m_stack_pointer;
	CompileTimeError m_compile_time_error;
	int m_compile_time_offset;
};


ExpressionVM::ReturnValue ExpressionVM::evaluate(const uint8* code)
{
	m_stack_pointer = 0;
	const uint8* cp = code;
	for (;;)
	{
		uint8 type = *cp;
		++cp;
		switch (type)
		{
			case Instruction::CALL:
				callFunction(*(uint16*)cp);
				cp += sizeof(cp);
				break;
			case Instruction::RET_FLOAT: return pop<float>();
			case Instruction::RET_BOOL: return pop<bool>();
			case Instruction::ADD_FLOAT: push<float>(pop<float>() + pop<float>()); break;
			case Instruction::SUB_FLOAT: push<float>(-pop<float>() + pop<float>()); break;
			case Instruction::PUSH_FLOAT: cp = pushStackConst<float>(cp); break;
			case Instruction::FLOAT_LT: push<bool>(pop<float>() > pop<float>()); break;
			case Instruction::FLOAT_GT: push<bool>(pop<float>() < pop<float>()); break;
			case Instruction::MUL_FLOAT: push<float>(pop<float>() * pop<float>()); break;
			case Instruction::DIV_FLOAT:
			{
				float f = pop<float>();
				push<float>(pop<float>() / f);
			}
			break;
			case Instruction::UNARY_MINUS: push<float>(-pop<float>()); break;
			case Instruction::OR:
			{
				bool b1 = pop<bool>();
				bool b2 = pop<bool>();
				push<bool>(b1 || b2);
			}
			break;
			case Instruction::AND:
			{
				bool b1 = pop<bool>();
				bool b2 = pop<bool>();
				push<bool>(b1 && b2);
			}
			break;
			default: DebugBreak(); break;
		}
	}
}


ExpressionVM::ReturnValue ExpressionVM::compileAndRun(const char* src)
{
	static const int MAX_TOKENS_COUNT = 50;
	static const int MAX_BYTECODE_SIZE = 50;
	uint8 byte_code[MAX_BYTECODE_SIZE];
	Token tokens[MAX_TOKENS_COUNT];
	Token postfix_tokens[MAX_TOKENS_COUNT];
	int tokens_count = tokenize(src, tokens, MAX_TOKENS_COUNT);
	if (tokens_count <= 0) return ReturnValue();

	int postfix_tokens_count = toPostfix(tokens, postfix_tokens, tokens_count);
	if (postfix_tokens_count <= 0) return ReturnValue();

	int size = compile(src, postfix_tokens, postfix_tokens_count, byte_code, MAX_BYTECODE_SIZE);
	if (size <= 0) return ReturnValue();

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

			if (func_stack_idx > 0)
			{
				--func_stack_idx;
			}
			else
			{
				m_compile_time_error = CompileTimeError::MISSING_LEFT_PARENTHESIS;
				m_compile_time_offset = token.offset;
				return -1;
			}
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
		if(func_stack[i].type == Token::LEFT_PARENTHESIS)
		{
			m_compile_time_error = CompileTimeError::MISSING_RIGHT_PARENTHESIS;
			m_compile_time_offset = func_stack[i].offset;
			return -1;
		}
		*out = func_stack[i];
		++out;
	}

	return out_token_count;
}


void ExpressionVM::callFunction(uint16 idx)
{
	switch(idx)
	{
		case 0: push<float>(sin(pop<float>())); break;
		case 1: push<float>(cos(pop<float>())); break;
		default: DebugBreak(); break;
	}
}


static const uint16 getFunctionIdx(const char* src, const Token& token)
{
	static const char* functs[] = { "sin", "cos" };
	for(int i = 0; i < sizeof(functs) / sizeof(*functs); ++i)
	{
		if(strncmp(src + token.offset, functs[i], token.size) == 0) return i;
	}
	return 0xffFF;
}


static bool getConstValue(const char* src, const Token& token, float& value)
{
	static const struct { const char* name; float value; } CONSTS[] =
	{
		{ "PI", 3.14159265358979323846f }
	};
	for(const auto& i : CONSTS)
	{
		if(strncmp(i.name, src + token.offset, token.size) == 0)
		{
			value = i.value;
			return true;
		}
	}
	return false;
}


static const struct
{
	Token::Operator op;
	Types ret_type;
	Instruction::Type instr;
	Types args[9];
	
	int arity() const
	{
		for (int i = 0; i < sizeof(args) / sizeof(args[0]); ++i)
		{
			if (args[i] == Types::NONE) return i;
		}
		return 0;
	}

	bool checkArgTypes(const Types* stack, int idx) const
	{
		for (int i = 0; i < arity(); ++i)
		{
			if (args[i] != stack[idx - i - 1]) return false;
		}
		return true;
	}
} OPERATOR_FUNCTIONS[] = {
	{Token::ADD, Types::FLOAT, Instruction::ADD_FLOAT, {Types::FLOAT, Types::FLOAT, Types::NONE}},
	{Token::MULTIPLY,
		Types::FLOAT,
		Instruction::MUL_FLOAT,
		{Types::FLOAT, Types::FLOAT, Types::NONE}},
	{Token::DIVIDE,
		Types::FLOAT,
		Instruction::DIV_FLOAT,
		{Types::FLOAT, Types::FLOAT, Types::NONE}},
	{Token::SUBTRACT,
		Types::FLOAT,
		Instruction::SUB_FLOAT,
		{Types::FLOAT, Types::FLOAT, Types::NONE}},
	{Token::UNARY_MINUS,
		Types::FLOAT,
		Instruction::UNARY_MINUS,
		{Types::FLOAT, Types::NONE}},
	{Token::LESS_THAN,
		Types::BOOL,
		Instruction::FLOAT_LT,
		{Types::FLOAT, Types::FLOAT, Types::NONE}},
	{Token::GREATER_THAN,
		Types::BOOL,
		Instruction::FLOAT_GT,
		{Types::FLOAT, Types::FLOAT, Types::NONE}},
	{Token::AND,
		Types::BOOL,
		Instruction::AND,
		{ Types::BOOL, Types::BOOL, Types::NONE }},
	{Token::OR,
		Types::BOOL,
		Instruction::OR,
		{ Types::BOOL, Types::BOOL, Types::NONE }}
};


int ExpressionVM::compile(const char* src,
	const Token* tokens,
	int token_count,
	uint8* byte_code,
	int max_size)
{
	Types type_stack[50];
	int type_stack_idx = 0;
	uint8* out = byte_code;
	for (int i = 0; i < token_count; ++i)
	{
		auto& token = tokens[i];

		switch(token.type)
		{
			case Token::NUMBER:
				if (max_size - (out - byte_code) < sizeof(float) + 1)
				{
					m_compile_time_error = CompileTimeError::OUT_OF_MEMORY;
					return -1;
				}
				*out = Instruction::PUSH_FLOAT;
				type_stack[type_stack_idx] = Types::FLOAT;
				++type_stack_idx;
				++out;
				*(float*)out = token.number;
				out += sizeof(float);
				break;
			case Token::OPERATOR:
				for (auto& fn : OPERATOR_FUNCTIONS)
				{
					if (token.oper != fn.op) continue;

					if (type_stack_idx < fn.arity())
					{
						m_compile_time_error = CompileTimeError::NOT_ENOUGH_PARAMETERS;
						m_compile_time_offset = token.offset;
						return -1;
					}
					if (!fn.checkArgTypes(type_stack, type_stack_idx))
					{
						m_compile_time_error = CompileTimeError::INCORRECT_TYPE_ARGS;
						m_compile_time_offset = token.offset;
						return -1;
					}
					type_stack_idx -= fn.arity();
					type_stack[type_stack_idx] = fn.ret_type;
					++type_stack_idx;
					*out = fn.instr;
					++out;
					break;
				}
				break;
			case Token::IDENTIFIER:
				{
					uint16 func_idx = getFunctionIdx(src, token);
					if(func_idx != 0xffFF)
					{
						if (type_stack_idx < 1)
						{
							m_compile_time_error = CompileTimeError::NOT_ENOUGH_PARAMETERS;
							m_compile_time_offset = token.offset;
							return -1;
						}

						if (type_stack[type_stack_idx - 1] != Types::FLOAT)
						{
							m_compile_time_error = CompileTimeError::INCORRECT_TYPE_ARGS;
							m_compile_time_offset = token.offset;
							return -1;
						}

						*out = Instruction::CALL;
						++out;
						*(uint16*)out = func_idx;
						out += sizeof(uint16);
					}
					else
					{
						*out = Instruction::PUSH_FLOAT;
						type_stack[type_stack_idx] = Types::FLOAT;
						++type_stack_idx;
						++out;
						float const_value;
						if(!getConstValue(src, token, const_value))
						{
							m_compile_time_error = CompileTimeError::UNKNOWN_IDENTIFIER;
							m_compile_time_offset = token.offset;
							return -1;
						}
						*(float*)out = const_value;
						out += sizeof(float);
					}
				}
				break;
			default:
				DebugBreak();
				break;
		}
	}
	if (max_size - (out - byte_code) < 1)
	{
		m_compile_time_error = CompileTimeError::OUT_OF_MEMORY;
		return -1;
	}
	switch(type_stack[type_stack_idx - 1])
	{
		case Types::FLOAT: *out = Instruction::RET_FLOAT; break;
		case Types::BOOL: *out = Instruction::RET_BOOL; break;
		default: DebugBreak(); break;
	}
	++out;
	return out - byte_code;
}


static bool isIdentifierChar(char c)
{
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_';
}


static const struct { const char* c; bool binary; Token::Operator op; } OPERATORS[] =
{
	{ "*", true, Token::MULTIPLY },
	{ "+", true, Token::ADD },
	{ "/", true, Token::DIVIDE },
	{ "<", true, Token::LESS_THAN },
	{ ">", true, Token::GREATER_THAN },
	{ "and", true, Token::AND },
	{ "or", true, Token::OR }
};


int ExpressionVM::tokenize(const char* src, Token* tokens, int max_size)
{
	m_compile_time_error = CompileTimeError::NONE;
	const char* c = src;
	int token_count = 0;
	bool binary = false;
	while (*c)
	{
		Token token = { Token::EMPTY, c - src };
		
		for (auto& i : OPERATORS)
		{
			if (strncmp(c, i.c, strlen(i.c) != 0)) continue;
			if (i.binary && !binary)
			{
				m_compile_time_error = CompileTimeError::MISSING_BINARY_OPERAND;
				m_compile_time_offset = token.offset;
				return -1;
			}

			token.type = Token::OPERATOR;
			token.oper = i.op;
			binary = false;
			c += strlen(i.c) - 1;
			break;
		}

		if (token.type == Token::EMPTY)
		{
			switch (*c)
			{
			case ' ':
			case '\n':
			case '\t': ++c; continue;
			case '-':
				token.type = Token::OPERATOR;
				token.oper = binary ? Token::SUBTRACT : Token::UNARY_MINUS;
				binary = false;
				break;
			}
		}

		if (token.type == Token::EMPTY)
		{
			if (isIdentifierChar(*c))
			{
				token.offset = c - src;
				++c;
				token.type = Token::IDENTIFIER;
				while (isIdentifierChar(*c)) ++c;
				token.size = (c - src) - token.offset;
				--c;
			}
			else if (*c == '(')
			{
				binary = false;
				token.type = Token::LEFT_PARENTHESIS;
			}
			else if (*c == ')')
			{
				binary = false;
				token.type = Token::RIGHT_PARENTHESIS;
				binary = true;
			}
			else if (*c >= '0' && *c <= '9')
			{
				token.type = Token::NUMBER;
				char* out;
				token.number = strtof(c, &out);
				c = out - 1;
				binary = true;
			}
			else
			{
				m_compile_time_error = CompileTimeError::UNEXPECTED_CHAR;
				m_compile_time_offset = token.offset;
			}
		}
		if(token.type != Token::EMPTY)
		{
			if(token_count < max_size)
			{
				tokens[token_count] = token;
			}
			else
			{
				m_compile_time_error = CompileTimeError::OUT_OF_MEMORY;
				return -1;
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

ExpressionVM::ReturnValue floatBinaryOperator(float f1, float f2, Instruction::Type type)
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


TEST_CASE("Compile time erros", "Report compile time errors") {
	ExpressionVM vm;
	vm.compileAndRun("unknown_function(10)");
	CHECK(vm.getCompileTimeError() == CompileTimeError::UNKNOWN_IDENTIFIER);

	vm.compileAndRun("sin(UKNOWN_CONST)");
	CHECK(vm.getCompileTimeError() == CompileTimeError::UNKNOWN_IDENTIFIER);

	vm.compileAndRun("sin UKNOWN_CONST)");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_LEFT_PARENTHESIS);

	vm.compileAndRun("sin (UKNOWN_CONST))");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_LEFT_PARENTHESIS);

	vm.compileAndRun("sin (UKNOWN_CONST");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_RIGHT_PARENTHESIS);

	vm.compileAndRun("sin ((UKNOWN_CONST)");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_RIGHT_PARENTHESIS);

	vm.compileAndRun("(sin ((UKNOWN_CONST)");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_RIGHT_PARENTHESIS);

	vm.compileAndRun("10 . 5");
	CHECK(vm.getCompileTimeError() == CompileTimeError::UNEXPECTED_CHAR);

	vm.compileAndRun("10 * 5;");
	CHECK(vm.getCompileTimeError() == CompileTimeError::UNEXPECTED_CHAR);

	vm.compileAndRun(".sin(0)");
	CHECK(vm.getCompileTimeError() == CompileTimeError::UNEXPECTED_CHAR);

	vm.compileAndRun("* 1");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_BINARY_OPERAND);

	vm.compileAndRun("sin");
	CHECK(vm.getCompileTimeError() == CompileTimeError::NOT_ENOUGH_PARAMETERS);

	vm.compileAndRun("sin()");
	CHECK(vm.getCompileTimeError() == CompileTimeError::NOT_ENOUGH_PARAMETERS);

	vm.compileAndRun("sin(1 < 5)");
	CHECK(vm.getCompileTimeError() == CompileTimeError::INCORRECT_TYPE_ARGS);

	vm.compileAndRun("1 + ");
	CHECK(vm.getCompileTimeError() == CompileTimeError::NOT_ENOUGH_PARAMETERS);

	vm.compileAndRun("+ 1");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_BINARY_OPERAND);

	vm.compileAndRun("1 + (+ 2)");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_BINARY_OPERAND);

	vm.compileAndRun("1 / *");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_BINARY_OPERAND);

	vm.compileAndRun("/ 1 *");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_BINARY_OPERAND);
	
	vm.compileAndRun("2 > 1 > 0");
	CHECK(vm.getCompileTimeError() == CompileTimeError::INCORRECT_TYPE_ARGS);

	vm.compileAndRun("1*1*1*1*1*1*1*1*1*1*1*1*1*1*1*1*1*1*");
	CHECK(vm.getCompileTimeError() == CompileTimeError::OUT_OF_MEMORY);
}


TEST_CASE("Function", "Call different functions") {
	ExpressionVM vm;
	CHECK(vm.compileAndRun("sin(0)").f_value == Approx(0.0f));
	CHECK(vm.compileAndRun("sin 0").f_value == Approx(0.0f));
	CHECK(vm.getCompileTimeError() == CompileTimeError::NONE);
	CHECK(vm.compileAndRun("cos(0)").f_value == Approx(1.0f));
	CHECK(vm.compileAndRun("cos 0").f_value == Approx(1.0f));
	CHECK(vm.compileAndRun("cos(10 * 0)").f_value == Approx(1.0f));
	CHECK(vm.compileAndRun("cos(PI)").f_value == Approx(-1.0f));
}


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

	float x = vm.evaluate(byte_code).f_value;
	CHECK(x == Approx(40.0f));
}


TEST_CASE("Booleans", "Various operations returning booleans") {
	ExpressionVM vm;

	SECTION("Comparision") {
		CHECK(vm.compileAndRun("1 < 2").b_value);
		CHECK(vm.compileAndRun("1 < (2)").b_value);
		CHECK(vm.compileAndRun("(1) < (2)").b_value);
		CHECK(vm.compileAndRun("(1 < 2)").b_value);
		CHECK(vm.compileAndRun("1 < 2").b_value);
		CHECK(!vm.compileAndRun("1 > 2").b_value);
		CHECK(vm.compileAndRun("2 > 1").b_value);
		CHECK(vm.compileAndRun("2 + 3 > 4").b_value);
		CHECK(vm.compileAndRun("4 - 1.1 < 3").b_value);
		CHECK(vm.compileAndRun("4 - 1.1 < 1.5 * 2").b_value);
		CHECK(vm.compileAndRun("-2 < -1").b_value);
		CHECK(!vm.compileAndRun("-2 < -2").b_value);
		CHECK(!vm.compileAndRun("-2 > -2").b_value);
	}

	SECTION("Simple comparision") {
		CHECK(vm.compileAndRun("-2 < -1 and 2 > 1").b_value);
		CHECK(vm.compileAndRun("-2 < -1 or 2 < 1").b_value);
		CHECK(vm.compileAndRun("-2 > -1 or 2 > 1").b_value);
		CHECK(!vm.compileAndRun("-2 > -1 or 2 < 1").b_value);
	}

	SECTION("And/Or priority") {
		CHECK(vm.compileAndRun("1 < 2 and 2 < 1 or 1 < 2").b_value);
		CHECK(vm.compileAndRun("(2 < 1 or 1 < 2) and 1 < 2").b_value);
		CHECK(vm.compileAndRun("1 < 2 and (2 < 1 or 1 < 2)").b_value);
		CHECK(!vm.compileAndRun("1 < 2 and 2 < 1 or 1 > 2").b_value);

		CHECK(vm.compileAndRun("1 < 2 or 2 < 1 and 1 < 2").b_value);
		CHECK(!vm.compileAndRun("1 > 2 or 2 < 1 and 1 < 2").b_value);

		CHECK(!vm.compileAndRun("1 > 2 or 2 < 1 and 1 < 2").b_value);
	}
}


TEST_CASE("Compile & Run", "Compile source to bytecode and run it") {
	ExpressionVM vm;
	SECTION("Multiplication & addition") {
		CHECK(vm.compileAndRun("4.5 + 10 * 3 + 5.5").f_value == Approx(40.0f));
		CHECK(vm.compileAndRun("(4.5 + 10) * 3 + 5.5").f_value == Approx(49.0f));
		CHECK(vm.compileAndRun("4.5 + (10 * 3) + 5.5").f_value == Approx(40.0f));
		CHECK(vm.compileAndRun("4.5 + 10 * (3 + 5.5)").f_value == Approx(89.5f));
		CHECK(vm.compileAndRun("(4.5 + 10 * 3 + 5.5)").f_value == Approx(40.0f));
		CHECK(vm.compileAndRun("(4.5 + 10 * 3) + 5.5").f_value == Approx(40.0f));
		CHECK(vm.compileAndRun("4.5 + (10 * 3 + 5.5)").f_value == Approx(40.0f));
	}

	SECTION("Subtraction") {
		CHECK(vm.compileAndRun("4.5 - 2").f_value == Approx(2.5f));
		CHECK(vm.compileAndRun("4.5 - 5").f_value == Approx(-0.5f));
		CHECK(vm.compileAndRun("2 * (4.5 - 5)").f_value == Approx(-1.0f));
	}
	
	SECTION("Unary minus") {
		CHECK(vm.compileAndRun("-1").f_value == Approx(-1.0f));
		CHECK(vm.compileAndRun("-1 * 5").f_value == Approx(-5.0f));
		CHECK(vm.compileAndRun("1 * -5").f_value == Approx(-5.0f));
		CHECK(vm.compileAndRun("-1 * -5").f_value == Approx(5.0f));
		CHECK(vm.compileAndRun("(-1) * -5").f_value == Approx(5.0f));
		CHECK(vm.compileAndRun("2 * (-1 * -5)").f_value == Approx(10.0f));
	}

	SECTION("Division") {
		CHECK(vm.compileAndRun("5 / 2").f_value == Approx(2.5f));
		CHECK(vm.compileAndRun("2.5 / 2").f_value == Approx(1.25f));
		CHECK(vm.compileAndRun("1 / 2.0").f_value == Approx(0.5f));
	}
}


TEST_CASE("Run", "Execute bytecode") {
	SECTION("Multiply") {
		CHECK(floatBinaryOperator(2, 4, Instruction::MUL_FLOAT).f_value == Approx(8.0f));
		CHECK(floatBinaryOperator(5, -4, Instruction::MUL_FLOAT).f_value == Approx(-20.0f));
		CHECK(floatBinaryOperator(3, 0, Instruction::MUL_FLOAT).f_value == Approx(0.0f));
	}

	SECTION("Add") {
		CHECK(floatBinaryOperator(2, 4, Instruction::ADD_FLOAT).f_value == Approx(6.0f));
		CHECK(floatBinaryOperator(5, -4, Instruction::ADD_FLOAT).f_value == Approx(1.0f));
		CHECK(floatBinaryOperator(3, 0, Instruction::ADD_FLOAT).f_value == Approx(3.0f));
		CHECK(floatBinaryOperator(3, -4, Instruction::ADD_FLOAT).f_value == Approx(-1.0f));
	}
}
