#pragma once
#include <string>
#include <queue>
#include <stack>
#include <iostream>

class KCalculator
{
public:
	bool Calculate(const std::string& exp, std::string &r);
	inline std::string ErrStr() const{ return m_errMsg; }
	void Instruction() const
	{
		std::cout << R"(
	Usage: calculator "expression"
	Supported operators as follows
	Binary operators:
	+(add),-(substract),*(multiply),/(divide),^(power),~(extract),'(log);
	Unary operators:
	sin,cos,tan,asin,acos,atan,sinh,cosh,tanh;
	)" << std::endl;
	}

private:
	bool ToSuffixExp(const std::string& exp, std::queue<std::string> &suffixExp);
	void Trim(std::string &s);
	bool PriorityHigher(const std::string &op1, const std::string &op2);
	bool CalcUnary(const std::string &operand1, const std::string &s, std::string &r);
	bool CalcBinary(const std::string &s, std::stack<std::string> &operandStack);
	bool IsUnary(const std::string &s) const;
	bool IsBinary(const std::string &s) const;
	bool IsOperand(const std::string &s) const;
	bool GetOperand(const std::string &s, std::string &r);
	bool GetOperator(const std::string &s, std::string &op);
	
private:
	std::string m_errMsg;
};

