#pragma once
#ifndef _BASE64_HPP_
#define _BASE64_HPP_
#include <string>
/**
base64处理类
**/
namespace klib {
	class KBase64
	{
	public:
		/************************************
		* Method:    base64加密
		* Returns:   
		* Parameter: str
		* Parameter: length
		*************************************/
		static std::string Encode(const char* str, unsigned int length);

		/************************************
		* Method:    base64解密
		* Returns:   
		* Parameter: str
		*************************************/
		static std::string Decode(const std::string& str);

	private:
		static bool IsBase64Char(unsigned char c);
	};
};
#endif // !_BASE64_HPP_
