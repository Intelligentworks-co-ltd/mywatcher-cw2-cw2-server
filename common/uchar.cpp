
#include "uchar.h"

namespace ucs {

	// memcmp/wmemcmp
	template <typename T> static inline int compare(T const *left, T const *right, size_t len)
	{
		size_t pos;
		for (pos = 0; pos < len; pos++) {
			int l = left[pos];
			int r = right[pos];
			int t = l - r;
			if (t < 0) {
				return -1;
			}
			if (t > 0) {
				return 1;
			}
		}
		return 0;
	}

	// strcmp/wcscmp
	template <typename T> static inline int compare(T const *left, T const *right)
	{
		while (1) {
			int l = *left;
			int r = *right;
			if (l == 0 && r == 0) {
				return 0;
			}
			int t = l - r;
			if (t < 0) {
				return -1;
			}
			if (t > 0) {
				return 1;
			}
			left++;
			right++;
		}
	}

	// stricmp/wcsicmp
	template <typename T> static inline int icompare(T const *left, T const *right)
	{
		while (1) {
			int l = *left;
			int r = *right;
			if (l == 0 && r == 0) {
				return 0;
			}
			if (l < 128 && islower((char)l)) {
				l = l + ('A' - 'a');
			}
			if (r < 128 && islower((char)r)) {
				r = r + ('A' - 'a');
			}
			if (l < r) return -1;
			if (l > r) return 1;
			left++;
			right++;
		}
	}

	// strncmp/wcsncmp
	template <typename T> static inline int comparen(T const *left, T const *right, size_t len)
	{
		while (len > 0) {
			int l = *left;
			int r = *right;
			if (l == 0 && r == 0) return 0;
			if (l < r) return -1;
			if (l > r) return 1;
			left++;
			right++;
			len--;
		}
		return 0;
	}

	uchar_t *wcsstr(uchar_t const *big, uchar_t const *little)
	{
		uchar_t const *p;
		uchar_t const *q;
		uchar_t const *r;

		if (!*little) {
			return (uchar_t *)big;
		}
		if (wcslen (big) < wcslen (little))
			return NULL;

		p = big;
		q = little;
		while (*p) {
			q = little;
			r = p;
			while (*q) {
				if (*r != *q) {
					break;
				}
				q++;
				r++;
			}
			if (!*q) {
				return (uchar_t *) p;
			}
			p++;
		}
		return NULL;
	}

	uchar_t *wmemchr(uchar_t const *s, uchar_t c, size_t n)
	{
		for (size_t i = 0; i < n; i++) {
			if (s[i] == c) {
				return (uchar_t *)(s + i);
			}
		}
		return 0;
	}

	uchar_t *wmemset(uchar_t *d, uchar_t c, size_t n)
	{
		for (size_t i = 0; i < n; i++) {
			d[i] = c;
		}
		return d;
	}


	int wmemcmp(uchar_t const *left, uchar_t const *right, size_t len)
	{
		return compare(left, right, len);
	}

	int wcscmp(uchar_t const *left, uchar_t const *right)
	{
		return compare(left, right);
	}

	uchar_t *wcschr(uchar_t const *left, uchar_t c)
	{
		for (size_t i = 0; left[i]; i++) {
			if (left[i] == c) {
				return (uchar_t *)&left[i];
			}
		}
		return 0;
	}

	int strcmp(char const *left, char const *right)
	{
		return compare(left, right);
	}

	int stricmp(char const *left, char const *right)
	{
		return icompare((unsigned char const *)left, (unsigned char const *)right);
	}

	int strncmp(char const *left, char const *right, size_t len)
	{
		return comparen(left, right, len);
	}

	int wcsncmp(uchar_t const *left, uchar_t const *right, size_t len)
	{
		return comparen(left, right, len);

	}

}




