
#include "combinepath.h"
#include <wctype.h>

#ifdef WIN32
#if _MSV_VER < 1400
#include <ctype.h>
#define iswspace(C) my_iswspace(C)
static inline bool iswspace(uchar_t c)
{
	return c < 0x100 && isspace(c);
}
#else
#include <wctype.h>
#endif
#endif

static bool isfileseparator(int c)
{
	return c == '\\' || c == '/';
}

// ディレクトリ名とファイル名を結合する
ustring CombinePath(uchar_t const *left, uchar_t const *right, uchar_t separator)
{
	while (iswspace(*left)) left++;
	while (iswspace(*right)) right++;
	if (isfileseparator(*right)) {
		return right;
	}
	size_t n = ucs::wcslen(left);
	if (n > 0 && isfileseparator(left[n - 1])) {
		n--;
	}
	ustring path(left, n);
	uchar_t tmp[2];
	tmp[0] = separator;
	tmp[1] = 0;
	path.append(tmp);
	path.append(right);
	return path;
}

std::string CombinePath(char const *left, char const *right, char separator)
{
	while (isspace(*left)) left++;
	while (isspace(*right)) right++;
	if (isfileseparator(*right)) {
		return right;
	}
	size_t n = strlen(left);
	if (n > 0 && isfileseparator(left[n - 1])) {
		n--;
	}
	std::string path(left, n);
	char tmp[2];
	tmp[0] = separator;
	tmp[1] = 0;
	path.append(tmp);
	path.append(right);
	return path;
}


