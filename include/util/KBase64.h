#pragma once
#ifndef _BASE64_HPP_
#define _BASE64_HPP_
#include <string>

namespace klib {
	class KBase64
	{
	public:
		static std::string Encode(const char* str, unsigned int length);

		static std::string Decode(const std::string& str);

	private:
		static bool IsBase64Char(unsigned char c);
	};
};
#endif // !_BASE64_HPP_
