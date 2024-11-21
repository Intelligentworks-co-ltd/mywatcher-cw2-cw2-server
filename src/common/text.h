
#ifndef __TEXT_H
#define __TEXT_H

#include "uchar.h"
//#include <string>
//#include <sstream>

void swrite(std::stringstream *out, char const *str);
void utf8_write(std::stringstream *out, uchar_t const *str);
void utf8_write(std::stringstream *out, ustring const &str);
//void utf8_writef(std::stringstream *out, uchar_t const *str, ...);

std::string utf8_ustring_to_string(uchar_t const *str, size_t len);
std::string utf8_ustring_to_string(uchar_t const *str);
std::string utf8_ustring_to_string(ustring const &str);
ustring utf8_string_to_ustring(char const *str, size_t len);
ustring utf8_string_to_ustring(char const *str);
ustring utf8_string_to_ustring(std::string const &str);

ustring eucjp_string_to_ustring(char const *str, size_t len);
ustring eucjp_string_to_ustring(char const *str);
ustring eucjp_string_to_ustring(std::string const &str);










std::string ustring_to_string(uchar_t const *str, size_t len);
std::string ustring_to_string(uchar_t const *str);
std::string ustring_to_string(ustring const &str);
ustring string_to_ustring(char const *str, size_t len);
ustring string_to_ustring(char const *str);
ustring string_to_ustring(std::string const &str);

ustring trimstring(uchar_t const *left, uchar_t const *right);
ustring trimstring(uchar_t const *str, size_t len);
ustring trimstring(uchar_t const *str);
ustring trimstring(ustring const &str);

std::string trimstring(char const *left, char const *right);
std::string trimstring(char const *str, size_t len);
std::string trimstring(char const *str);
std::string trimstring(std::string const &str);

ustring remove_double_quotation(ustring &str);

#endif

