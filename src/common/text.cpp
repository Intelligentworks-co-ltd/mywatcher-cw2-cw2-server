
#ifdef WIN32
#include <windows.h>
#pragma warning(disable:4996)
#endif

#include <stdarg.h>
#include <stdio.h>
#include "text.h"
#include "misc.h"
#include "jcode/jstring.h"


void swrite(std::stringstream *out, char const *str)
{
	out->write(str, (std::streamsize)strlen(str));
}

// utf8に変換しながらstringstreamに追加する

void utf8_write(std::stringstream *out, uchar_t const *str, size_t len)
{
#if 0
	char tmp[10000];
	//int n = WideCharToMultiByte(CP_ACP, 0, str, (int)ucs::wcslen(str), tmp, sizeof(tmp), 0, 0);
	int n = WideCharToMultiByte(CP_UTF8, 0, str, (int)len, tmp, sizeof(tmp), 0, 0);	// convert to utf8
	out->write(tmp, n);
#else
	soramimi::jcode::convert(out, soramimi::jcode::UTF8, str, len);
#endif
}

void utf8_write(std::stringstream *out, uchar_t const *str)
{
	utf8_write(out, str, ucs::wcslen(str));
}

void utf8_write(std::stringstream *out, ustring const &str)
{
	utf8_write(out, str.c_str(), str.size());
}

//void utf8_writef(std::stringstream *out, uchar_t const *str, ...)
//{
//	uchar_t tmp[5000];
//	va_list ap;
//	va_start(ap, str);
//#ifdef WIN32
//	vswprintf(tmp, str, ap);
//#else
//	vswprintf(tmp, sizeof(tmp), str, ap);
//#endif
//	utf8_write(out, tmp);
//	va_end(ap);
//}

// utf8文字列変換

std::string utf8_ustring_to_string(uchar_t const *str, size_t len)
{
#if 0
	char tmp[10000];
	int n = WideCharToMultiByte(CP_UTF8, 0, str, (int)len, tmp, sizeof(tmp), 0, 0);
	return std::string(tmp, n);
#else
	return soramimi::jcode::convert(soramimi::jcode::UTF8, str, len);
#endif
}

std::string utf8_ustring_to_string(uchar_t const *str)
{
	return utf8_ustring_to_string(str, ucs::wcslen(str));
}

std::string utf8_ustring_to_string(ustring const &str)
{
	return utf8_ustring_to_string(str.c_str(), str.size());
}






// utf-8
ustring utf8_string_to_ustring(char const *str, size_t len)
{
#if 0
	if (!str || len == 0) {
		return ustring();
	}
	uchar_t tmp[10000];
	int n = MultiByteToWideChar(CP_UTF8, 0, str, (int)len, tmp, _countof(tmp));
	return ustring(tmp, n);
#else
	return soramimi::jcode::convert(soramimi::jcode::UTF8, str, len);
#endif
}
ustring utf8_string_to_ustring(char const *str)
{
	return utf8_string_to_ustring(str, strlen(str));
}
ustring utf8_string_to_ustring(std::string const &str)
{
	return utf8_string_to_ustring(str.c_str(), str.size());
}

// eucjp
ustring eucjp_string_to_ustring(char const *str, size_t len)
{
#if 0
	if (!str || len == 0) {
		return ustring();
	}
	uchar_t tmp[10000];
	int n = MultiByteToWideChar(CP_UTF8, 0, str, (int)len, tmp, _countof(tmp));
	return ustring(tmp, n);
#else
	return soramimi::jcode::convert(soramimi::jcode::EUCJP, str, len);
#endif
}
ustring eucjp_string_to_ustring(char const *str)
{
	return eucjp_string_to_ustring(str, strlen(str));
}
ustring eucjp_string_to_ustring(std::string const &str)
{
	return eucjp_string_to_ustring(str.c_str(), str.size());
}











// 文字列変換

std::string ustring_to_string(uchar_t const *str, size_t len)
{
#if 0
	char tmp[10000];
	int n = WideCharToMultiByte(CP_ACP, 0, str, (int)len, tmp, sizeof(tmp), 0, 0);
	return std::string(tmp, n);
#elif defined(WIN32)
	return soramimi::jcode::convert(soramimi::jcode::SJIS, str, len);
#else
	return soramimi::jcode::convert(soramimi::jcode::EUCJP, str, len);
#endif
}

std::string ustring_to_string(uchar_t const *str)
{
	return ustring_to_string(str, ucs::wcslen(str));
}

std::string ustring_to_string(ustring const &str)
{
	return ustring_to_string(str.c_str(), str.size());
}

ustring string_to_ustring(char const *str, size_t len)
{
#if 0
	if (!str || len == 0) {
		return ustring();
	}
	uchar_t tmp[10000];
	int n = MultiByteToWideChar(CP_ACP, 0, str, (int)len, tmp, _countof(tmp));
	return ustring(tmp, n);
#elif defined(WIN32)
	return soramimi::jcode::convert(soramimi::jcode::SJIS, str, len);
#else
	return soramimi::jcode::convert(soramimi::jcode::EUCJP, str, len);
#endif

}

ustring string_to_ustring(char const *str)
{
	return string_to_ustring(str, strlen(str));
}

ustring string_to_ustring(std::string const &str)
{
	return string_to_ustring(str.c_str(), str.size());
}

//

ustring trimstring(uchar_t const *left, uchar_t const *right)
{
	if (left && right && left < right) {
		while (left < right && iswspace(left[0])) left++;
		while (left < right && (right[-1] == 0 || iswspace(right[-1]))) right--;
		return ustring(left, right);
	}
	return ustring();
}

ustring trimstring(uchar_t const *str, size_t len)
{
	uchar_t const *left = str;
	uchar_t const *right = str + len;
	return trimstring(left, right);
}

ustring trimstring(uchar_t const *str)
{
	return trimstring(str, ucs::wcslen(str));
}

ustring trimstring(ustring const &str)
{
	return trimstring(str.c_str(), str.size());
}

//

std::string trimstring(char const *left, char const *right)
{
	if (left && right && left < right) {
		while (left < right && iswspace(left[0])) left++;
		while (left < right && (right[-1] == 0 || iswspace(right[-1]))) right--;
		return std::string(left, right);
	}
	return std::string();
}

std::string trimstring(char const *str, size_t len)
{
	char const *left = str;
	char const *right = str + len;
	return trimstring(left, right);
}

std::string trimstring(char const *str)
{
	return trimstring(str, strlen(str));
}

std::string trimstring(std::string const &str)
{
	return trimstring(str.c_str(), str.size());
}


//


ustring remove_double_quotation(ustring &str)
{
	uchar_t const *left = str.c_str();
	uchar_t const *right = left + str.size();
	while (left < right && iswspace(left[0])) left++;
	while (left < right && iswspace(right[-1])) right--;
	if (left < right && left[0] == L'\"' && right[-1] == L'\"') {
		left++;
		right--;
		return ustring(left, right);
	}
	return str;
}


