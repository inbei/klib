#include "KCalculator.h"
#include <sstream>
#include <cmath>
#include <regex>
#include <functional>

const std::string operand = R"(\d+(?:\.\d+)?)";

const std::string op_power = R"(\^)";
const std::string op_extract = R"(~)";
const std::string op_log = R"(')";
const std::string op_fst_binary = op_power + op_log + op_extract;

const std::string op_multiply = R"(\*)";
const std::string op_divide = R"(/)";
const std::string op_mod = R"(%)";
const std::string op_sec_binary = op_multiply + op_divide + op_mod;

const std::string op_add = R"(\+)";
const std::string op_substract = R"(\-)";
const std::string op_thr_binary = op_add + op_substract;

const std::string op_all_binary = "[" + op_fst_binary + op_sec_binary + op_thr_binary + "]";

const std::string op_unary = "asin|acos|atan|sinh|cosh|tanh|sin|cos|tan";

const std::string left_parenthesis = R"(()";
const std::string right_parenthesis = R"())";

bool KCalculator::Calculate(const std::string& exp, std::string& r)
{
	std::queue<std::string> suffixExp;
	if (!ToSuffixExp(exp, suffixExp))
		return false;

	std::stack<std::string> operandStack;
	while (!suffixExp.empty())
	{
		std::string s = suffixExp.front();
		suffixExp.pop();
		if (IsBinary(s)) // 二元操作符
		{
			if (operandStack.size() > 1) // 操作数队列元素必须大于1个
			{
				if (!CalcBinary(s, operandStack))
				{
					return false;
				}
			}
			else
			{
				m_errMsg = "binary operator's operand count is less than two.";
				return false;
			}
		}
		else if (IsUnary(s)) // 一元操作符
		{
			if (operandStack.size() > 0) // 操作数队列元素必须大于0个
			{
				std::string operand1 = operandStack.top();
				std::string r;
				if (CalcUnary(operand1, s, r))
				{
					operandStack.pop();
					operandStack.push(r);
				}
				else
				{
					return false;
				}
			}
			else
			{
				m_errMsg = "unary operator's operand count is less than one.";
				return false;
			}
		}
		else if (IsOperand(s)) // 操作数放到操作数队列里
		{
			operandStack.push(s);
		}
		else
		{
			m_errMsg = std::string("unsupported str: ") + s;
			return false;
		}
	}

	if (operandStack.size() == 1) // 操作数队列最后只有一个元素，该元素即为计算结果
	{
		r = operandStack.top();
		return true;
	}

	return false;
}

bool KCalculator::GetOperand(const std::string &s, std::string &r)
{
	std::string::const_iterator it = s.begin();
	bool point_occur = false;
	size_t sz = 0;
	while (it != s.end())
	{
		switch (*it)
		{
		case '.':
		{
			if (!point_occur)
			{
				point_occur = true;
				++sz;
				break;
			}
			else
			{
				return false;
			}
		}
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		{
			++sz;
			break;
		}
		default:
			goto done;
		}
		++it;
	}

done:
	r = s.substr(0, sz);
	return (sz > 0);
}

bool KCalculator::GetOperator(const std::string & s, std::string & op)
{
	if (s.empty())
	{
		return false;
	}

	auto getSinCosTan = [](const std::string & s, std::string & op)->bool
	{
		std::string ops = s.substr(0, 3);
		if (ops == "sin" || ops == "cos" || ops == "tan")
		{
			op = ops;
			return true;
		}
		else
			return false;
	};

	char fstCh = s.at(0);
	switch (fstCh)
	{
	case '+':
	case '-':
	case '*':
	case '/':
	case '^':
	case '~':
	case '\'':
	case '%':
	{
		op.push_back(fstCh);
		return true;
	}
	case 'a':
	{
		if (s.size() > 3)
		{
			std::string t = s.substr(1, s.length() - 1);
			return getSinCosTan(t, op);
		}
		break;
	}
	default:
	{
		if (s.size() > 2 && getSinCosTan(s, op))
		{
			if (s.size() > 3 && s.at(3) == 'h')
			{
				op.push_back('h');
			}
			return true;
		}
	}
	}
	return false;
}

bool KCalculator::ToSuffixExp(const std::string & exp, std::queue<std::string>& suffixExp)
{
	std::string s = exp;
	Trim(s);
	s = std::regex_replace(s, std::regex(R"(\(-)"), "(0-");
	std::stack<std::string> operatorStack;
	try
	{
		while (!s.empty())
		{
			std::string r;
			char fstCh = s.at(0);
			if(GetOperand(s, r)) // 无括号的操作数 查看操作符栈顶第一个操作符是否是一元操作符，是的话取出放在后缀表达式队列尾部				)
			{
				suffixExp.push(r);
			}
			else if (fstCh == '(') // 左括号 放到操作符栈里
			{
				r.push_back(fstCh);
				operatorStack.push(std::string(1, fstCh));
			}
			else if (fstCh == ')') // 右括号 取出操作符栈里的操作符放到后缀表达式队列尾部直到遇到左括号
			{
				std::string op;
				while (!operatorStack.empty())
				{
					op = operatorStack.top();
					operatorStack.pop();

					if (op == left_parenthesis)
					{
						break;
					}
					else
					{
						suffixExp.push(op);
					}
				}

				if (op != left_parenthesis)
				{
					m_errMsg = "unmatched ().";
					return false;
				}

				r.push_back(fstCh);
			}
			else if (GetOperator(s, r)) // 操作符
			{
				while (!operatorStack.empty()) // 取出操作符栈里的操作符放到后缀表达式队列尾部直到取出的操作符的优先级低于当前操作符的优先级
				{
					std::string op = operatorStack.top();
					if (PriorityHigher(op, r))
					{
						operatorStack.pop();
						suffixExp.push(op);
					}
					else
					{
						break;
					}
				}

				operatorStack.push(r);// 将当前的的操作符放到操作符栈里
			}
			else
			{
				m_errMsg = std::string("unsupported character: ") + s;
				return false;
			}

			s = s.substr(r.length(), s.length() - r.length()); // 匹配的后缀用来继续查找
		}
	}
	catch (const std::exception&e)
	{
		m_errMsg = e.what();
		return false;
	}

	while (!operatorStack.empty()) // 将剩余的操作符放到后缀表达式队列尾部
	{
		std::string op = operatorStack.top();
		if (op == right_parenthesis)
		{
			m_errMsg = "unmatched ().";
			return false;
		}

		suffixExp.push(op);		
		operatorStack.pop();
	}
	return true;
}

bool KCalculator::CalcBinary(const std::string &s, std::stack<std::string> &operandStack)
{
	std::string operand2 = operandStack.top();
	operandStack.pop();
	std::string operand1 = operandStack.top();
	operandStack.pop();

	double op1, op2;
	{
		std::istringstream ops(operand1);
		ops >> op1;
	}
	{
		std::istringstream ops(operand2);
		ops >> op2;
	}

	std::ostringstream os;
	switch (s.at(0))
	{
	case '+':
	{
		os << (op1 + op2);
		break;
	}
	case '-':
	{
		os << (op1 - op2);
		break;
	}
	case '*':
	{
		os << (op1 * op2);
		break;
	}
	case '/':
	{
		if (op2 < 0.000000000001 && op2 > -0.000000000001)
		{
			m_errMsg = "can not divide zero.";
			return false;
		}
		os << (op1 / op2);
		break;
	}
	case '%':
	{
		os << ((long)op1 % (long)op2);
		break;
	}
	case '^':
	{
		os << pow(op1, op2);
		break;
	}
	case '~':
	{
		os << pow(op1, 1 / op2);
		break;
	}
	case '\'':
	{
		os << (log(op2) / log(op1));		
		break;
	}
	default:
		m_errMsg = std::string("unsupported binary operator: ") + std::string(1, s.at(0));
		return false;
	}
	operandStack.push(os.str());
	return true;
}

bool KCalculator::CalcUnary(const std::string &operand1, const std::string &s, std::string &r)
{
	double op1;
	{
		std::istringstream ops(operand1);
		ops >> op1;
	}

	std::ostringstream os;
	if (s == std::string("sin"))
	{
		os << sin(op1);
	}
	else if (s == std::string("cos"))
	{
		os << cos(op1);
	}
	else if (s == std::string("tan"))
	{
		os << tan(op1);
	}
	else if (s == std::string("asin"))
	{
		os << asin(op1);
	}
	else if (s == std::string("acos"))
	{
		os << acos(op1);
	}
	else if (s == std::string("atan"))
	{
		os << atan(op1);
	}
	else if (s == std::string("sinh"))
	{
		os << sinh(op1);
	}
	else if (s == std::string("cosh"))
	{
		os << cosh(op1);
	}
	else if (s == std::string("tanh"))
	{
		os << tanh(op1);
	}
	else
	{
		m_errMsg = std::string("unsupported unary operator: ") + s;
		return false;
	}
	r = os.str();
	return true;
}

void KCalculator::Trim(std::string &s)
{
	std::string::iterator it = s.begin();
	while (it != s.end())
	{
		if (isspace(*it))
		{
			it = s.erase(it);
		}
		else
		{
			++it;
		}
	}
}

bool KCalculator::PriorityHigher(const std::string &op1, const std::string &op2)
{
	if (std::regex_match(op1, std::regex("[" + op_fst_binary + "]"))
		&& std::regex_match(op2, std::regex(op_all_binary + "|" + op_unary)))
	{
		return true;
	}
	else if (std::regex_match(op1, std::regex(op_unary))
		&& std::regex_match(op2, std::regex("[" + op_sec_binary + op_thr_binary + "]|" + op_unary)))
	{
		return true;
	}
	else if (std::regex_match(op1, std::regex("[" + op_sec_binary + "]"))
		&& std::regex_match(op2, std::regex("[" + op_sec_binary + op_thr_binary + "]")))
	{
		return true;
	}
	else if (std::regex_match(op1, std::regex("[" + op_thr_binary + "]"))
		&& std::regex_match(op2, std::regex("[" + op_thr_binary + "]")))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool KCalculator::IsUnary(const std::string &s) const
{
	return std::regex_match(s, std::regex(op_unary));
}

bool KCalculator::IsBinary(const std::string &s) const
{
	return std::regex_match(s, std::regex(op_all_binary));
}

bool KCalculator::IsOperand(const std::string &s) const
{
	return std::regex_match(s, std::regex(operand));
}