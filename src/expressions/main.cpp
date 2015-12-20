#define CATCH_CONFIG_MAIN
#include "catch/catch.hpp"
#include <cmath>

/*

TODO:
- runtime errors
- split compiler, runtime env
- types other than float
*/

typedef unsigned char uint8;
typedef unsigned int uint16;
typedef unsigned int uint32;


enum class CompileTimeError
{
	NONE,
	UNKNOWN_IDENTIFIER,
	MISSING_LEFT_PARENTHESIS,
	MISSING_RIGHT_PARENTHESIS,
	UNEXPECTED_CHAR,
	OUT_OF_MEMORY,
	MISSING_BINARY_OPERAND
};


namespace Instruction
{
	enum Type
	{
		PUSH_FLOAT,
		POP_FLOAT,
		ADD_FLOAT,
		MUL_FLOAT,
		DIV_FLOAT,
		RET_FLOAT,
		SUB_FLOAT,
		UNARY_MINUS,
		CALL
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
		UNARY_MINUS
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

public:
	float evaluate(const uint8* code);
	int tokenize(const char* src, Token* tokens, int max_size);
	int compile(const char* src, const Token* tokens, int token_count, uint8* byte_code, int max_size);
	int toPostfix(const Token* input, Token* output, int count);
	float compileAndRun(const char* src);
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
		if (token.type == Token::IDENTIFIER) return 2;
		switch(token.oper)
		{
			case Token::ADD: return 0;
			case Token::SUBTRACT: return 0;
			case Token::MULTIPLY: return 1;
			case Token::DIVIDE: return 1;
			case Token::UNARY_MINUS: return 1;
		}
		return -1;
	}


private:
	uint8 m_stack[STACK_SIZE];
	int m_stack_pointer;
	CompileTimeError m_compile_time_error;
	int m_compile_time_offset;
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
			case Instruction::CALL:
				callFunction(*(uint16*)cp);
				cp += sizeof(cp);
				break;
			case Instruction::RET_FLOAT:
				return pop<float>();
			case Instruction::ADD_FLOAT:
				push<float>(pop<float>() + pop<float>());
				break;
			case Instruction::SUB_FLOAT:
				push<float>(-pop<float>() + pop<float>());
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
			case Instruction::UNARY_MINUS:
				push<float>(-pop<float>());
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
	if(tokens_count <= 0 ) return 0;

	int postfix_tokens_count = toPostfix(tokens, postfix_tokens, tokens_count);
	if(postfix_tokens_count <= 0) return 0;

	int size = compile(src, postfix_tokens, postfix_tokens_count, byte_code, MAX_BYTECODE_SIZE);
	if(size <=  0) return 0;

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


int ExpressionVM::compile(const char* src, const Token* tokens, int token_count, uint8* byte_code, int max_size)
{
	uint8* out = byte_code;
	for(int i = 0; i < token_count; ++i)
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
					case Token::SUBTRACT: *out = Instruction::SUB_FLOAT; ++out; break;
					case Token::UNARY_MINUS: *out = Instruction::UNARY_MINUS; ++out; break;
					default: DebugBreak(); break;
				}
				break;
			case Token::IDENTIFIER:
				{
					uint16 func_idx = getFunctionIdx(src, token);
					if(func_idx != 0xffFF)
					{
						*out = Instruction::CALL;
						++out;
						*(uint16*)out = func_idx;
						out += sizeof(uint16);
					}
					else
					{
						*out = Instruction::PUSH_FLOAT;
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
	*out = Instruction::RET_FLOAT;
	++out;
	return out - byte_code;
}


static bool isIdentifierChar(char c)
{
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_';
}


int ExpressionVM::tokenize(const char* src, Token* tokens, int max_size)
{
	m_compile_time_error = CompileTimeError::NONE;
	const char* c = src;
	int token_count = 0;
	bool binary = false;
	while (*c)
	{
		Token token = { Token::EMPTY, c - src };
		if (isIdentifierChar(*c))
		{
			token.offset = c - src;
			++c;
			token.type = Token::IDENTIFIER;
			while(isIdentifierChar(*c)) ++c;
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
			c = out-1;
			binary = true;
		}
		else
		{
			switch(*c)
			{
				case ' ':
				case '\n': break;
				case '\t': break;
				case '+':
					if(!binary)
					{
						m_compile_time_error = CompileTimeError::MISSING_BINARY_OPERAND;
						m_compile_time_offset = token.offset;
						return -1;
					}
					token.type = Token::OPERATOR;
					token.oper = Token::ADD;
					binary = false;
					break;
				case '*':
					if(!binary)
					{
						m_compile_time_error = CompileTimeError::MISSING_BINARY_OPERAND;
						m_compile_time_offset = token.offset;
						return -1;
					}
					token.type = Token::OPERATOR;
					token.oper = Token::MULTIPLY;
					binary = false;
					break;
				case '/':
					if(!binary)
					{
						m_compile_time_error = CompileTimeError::MISSING_BINARY_OPERAND;
						m_compile_time_offset = token.offset;
						return -1;
					}
					token.type = Token::OPERATOR;
					token.oper = Token::DIVIDE;
					binary = false;
					break;
				case '-':
					token.type = Token::OPERATOR;
					token.oper = binary ? Token::SUBTRACT : Token::UNARY_MINUS;
					binary = false;
					break;
				default:
					m_compile_time_error = CompileTimeError::UNEXPECTED_CHAR;
					m_compile_time_offset = token.offset;
					return -1;
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

	vm.compileAndRun("+ 1");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_BINARY_OPERAND);

	vm.compileAndRun("1 + (+ 2)");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_BINARY_OPERAND);

	vm.compileAndRun("1 / *");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_BINARY_OPERAND);

	vm.compileAndRun("/ 1 *");
	CHECK(vm.getCompileTimeError() == CompileTimeError::MISSING_BINARY_OPERAND);

	vm.compileAndRun("1*1*1*1*1*1*1*1*1*1*1*1*1*1*1*1*1*1*");
	CHECK(vm.getCompileTimeError() == CompileTimeError::OUT_OF_MEMORY);
}


TEST_CASE("Function", "Call different functions") {
	ExpressionVM vm;
	CHECK(vm.compileAndRun("sin(0)") == Approx(0.0f));
	CHECK(vm.getCompileTimeError() == CompileTimeError::NONE);
	CHECK(vm.compileAndRun("cos(0)") == Approx(1.0f));
	CHECK(vm.compileAndRun("cos 0") == Approx(1.0f));
	CHECK(vm.compileAndRun("cos(10 * 0)") == Approx(1.0f));
	CHECK(vm.compileAndRun("cos(PI)") == Approx(-1.0f));
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

	float x = vm.evaluate(byte_code);
	CHECK(x == Approx(40.0f));
}


TEST_CASE("Compile & Run", "Compile source to bytecode and run it") {
	ExpressionVM vm;
	SECTION("Multiplication & addition") {
		CHECK(vm.compileAndRun("4.5 + 10 * 3 + 5.5") == Approx(40.0f));
		CHECK(vm.compileAndRun("(4.5 + 10) * 3 + 5.5") == Approx(49.0f));
		CHECK(vm.compileAndRun("4.5 + (10 * 3) + 5.5") == Approx(40.0f));
		CHECK(vm.compileAndRun("4.5 + 10 * (3 + 5.5)") == Approx(89.5f));
		CHECK(vm.compileAndRun("(4.5 + 10 * 3 + 5.5)") == Approx(40.0f));
		CHECK(vm.compileAndRun("(4.5 + 10 * 3) + 5.5") == Approx(40.0f));
		CHECK(vm.compileAndRun("4.5 + (10 * 3 + 5.5)") == Approx(40.0f));
	}

	SECTION("Subtraction") {
		CHECK(vm.compileAndRun("4.5 - 2") == Approx(2.5f));
		CHECK(vm.compileAndRun("4.5 - 5") == Approx(-0.5f));
		CHECK(vm.compileAndRun("2 * (4.5 - 5)") == Approx(-1.0f));
	}
	
	SECTION("Unary minus") {
		CHECK(vm.compileAndRun("-1") == Approx(-1.0f));
		CHECK(vm.compileAndRun("-1 * 5") == Approx(-5.0f));
		CHECK(vm.compileAndRun("1 * -5") == Approx(-5.0f));
		CHECK(vm.compileAndRun("-1 * -5") == Approx(5.0f));
		CHECK(vm.compileAndRun("(-1) * -5") == Approx(5.0f));
		CHECK(vm.compileAndRun("2 * (-1 * -5)") == Approx(10.0f));
	}

	SECTION("Division") {
		CHECK(vm.compileAndRun("5 / 2") == Approx(2.5f));
		CHECK(vm.compileAndRun("2.5 / 2") == Approx(1.25f));
		CHECK(vm.compileAndRun("1 / 2.0") == Approx(0.5f));
	}
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
