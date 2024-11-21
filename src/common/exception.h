
#ifndef __COMMON_EXCEPTION_H
#define __COMMON_EXCEPTION_H


#include <string>
#include "../common/jcode/jstring.h"


class Exception {
private:
	std::string _message;
	std::string _file;
	int _line;
public:
	Exception(std::string const &str, char const *file, int line)
	{
		_message = str;
		_file = file;
		_line = line;
	}
	Exception(ustring const &str, char const *file, int line)
	{
		_message = soramimi::jcode::convert(soramimi::jcode::SJIS, str);
		_file = _file;
		_line = line;
	}
	char const *get_message() const
	{
		return _message.c_str();
	}
	char const *get_file() const
	{
		return _file.c_str();
	}
	int get_line() const
	{
		return _line;
	}
};



#endif // __COMMON_EXCEPTION_H

