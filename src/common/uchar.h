
// 16ビット文字列処理
//
// VC++のwchar_tは16ビットだが、gccのwchar_tは32ビットなので、互換性がない。
// gccに -fshort-wchar をつければ16ビットになるが、wcs系文字列関数が対応できない。
// これに対処するため、VC++とgccで共通に使える16ビット文字コード型uchar_tを定義し、
// 同様に、std::wstringに代わりに、ustringを実装する。
// また、std::stringstream/std::wstringstreamの代わりに、stringbuffer/ustringbufferも実装する。

#ifndef __UCHAR_H
#define __UCHAR_H

#pragma warning(disable:4996)

#include <string.h>

#include <string>
#include <sstream>
#include <iosfwd>

typedef unsigned short uchar_t;

// 標準ライブラリの代替関数

namespace ucs {

	template <typename T> static inline int length(T const *ptr)
	{
		size_t n;
		for (n = 0; ptr[n]; n++);
		return n;
	}

	static inline size_t wcslen(uchar_t const *ptr)
	{
		return length(ptr);
	}

	uchar_t *wcsstr(uchar_t const *big, uchar_t const *little);

	uchar_t *wmemchr(uchar_t const *s, uchar_t c, size_t n);
	uchar_t *wmemset(uchar_t *d, uchar_t c, size_t n);
	int wmemcmp(uchar_t const *left, uchar_t const *right, size_t len);
	int wcscmp(uchar_t const *left, uchar_t const *right);
	uchar_t *wcschr(uchar_t const *left, uchar_t c);
	int stricmp(char const *left, char const *right);
	int wcsicmp(uchar_t const *left, uchar_t const *right);
	int strncmp(char const *left, char const *right, size_t len);
	int wcsncmp(uchar_t const *left, uchar_t const *right, size_t len);
	int wtoi(uchar_t const *p);

}

// char_traits<uchar_t>

namespace std {
	template <> struct char_traits<uchar_t> {
		typedef uchar_t _Elem;
		typedef _Elem char_type;
		typedef wint_t int_type;
		typedef streampos pos_type;
		typedef streamoff off_type;

		static void assign(_Elem& _Left, const _Elem& _Right)
		{
			_Left = _Right;
		}

		static bool eq(const _Elem& _Left, const _Elem& _Right)
		{
			return (_Left == _Right);
		}

		static bool lt(const _Elem& _Left, const _Elem& _Right)
		{
			return (_Left < _Right);
		}

		static int compare(const _Elem *_First1, const _Elem *_First2, size_t _Count)
		{
			return (ucs::wmemcmp(_First1, _First2, _Count));
		}

		static size_t length(const _Elem *_First)
		{
			return ucs::wcslen(_First);
		}

		static _Elem *copy(_Elem *_First1, const _Elem *_First2, size_t _Count)
		{
			return _Copy_s(_First1, _Count, _First2, _Count);
		}

		static _Elem *_Copy_s(_Elem *_First1, size_t _Size_in_words, const _Elem *_First2, size_t _Count)
		{
			memcpy(_First1, _First2, sizeof(_Elem) * _Count);
			return _First1;
		}

		static const _Elem *find(const _Elem *_First, size_t _Count, const _Elem& _Ch)
		{
			return (const _Elem *)ucs::wmemchr(_First, _Ch, _Count);
		}

		static _Elem *move(_Elem *_First1, const _Elem *_First2, size_t _Count)
		{
			return _Move_s(_First1, _Count, _First2, _Count);
		}

		static _Elem *_Move_s(_Elem *_First1, size_t _Size_in_words, const _Elem *_First2, size_t _Count)
		{
			memmove(_First1, _First2, sizeof(_Elem) * _Count);
			return (_Elem *)_First1;
		}

		static _Elem *assign(_Elem *_First, size_t _Count, _Elem _Ch)
		{
			return (_Elem *)ucs::wmemset(_First, _Ch, _Count);
		}

		static _Elem to_char_type(const int_type& _Meta)
		{
			return _Meta;
		}

		static int_type to_int_type(const _Elem& _Ch)
		{
			return _Ch;
		}

		static bool eq_int_type(const int_type& _Left, const int_type& _Right)
		{
			return _Left == _Right;
		}

		static int_type eof()
		{
			return (WEOF);
		}

		static int_type not_eof(const int_type& _Meta)
		{
			return (_Meta != eof() ? _Meta : !eof());
		}
	};
}

// 文字列クラス

typedef std::basic_string<uchar_t> ustring;

// ヌル文字列

namespace ucs {
	template <typename T> inline T const *nullstring();
	template <> inline char const *nullstring<char>() { return ""; }
	template <> inline uchar_t const *nullstring<uchar_t>() { return (uchar_t const *)L""; }
	template <> inline unsigned char const *nullstring<unsigned char>() { return (unsigned char const *)""; }
}

// 文字列バッファ（stringstreamの代用）

template <typename T> class basic_stringbuffer {
private:
	struct fragment_t {
		fragment_t *chain;
		size_t capacity;
		size_t length;
		T buffer[1];
	};
	mutable fragment_t *fragment;
	static fragment_t *new_fragment(size_t len)
	{
		fragment_t *f = (fragment_t *)new char [sizeof(fragment_t) + sizeof(T) * len];
		f->chain = 0;
		f->capacity = len;
		f->length = 0;
		f->buffer[0] = 0;
		return f;
	}
	void clear_const() const
	{
		while (fragment) {
			fragment_t *p = fragment->chain;
			delete[] (char *)fragment;
			fragment = p;
		}
	}
	void build() const
	{
		if (fragment && fragment->chain) {
			size_t len = size();
			fragment_t *newfrag = new_fragment(len);
			fragment_t *p;
			size_t n = len;
			for (p = fragment; p; p = p->chain) {
				n -= p->length;
				std::copy(p->buffer, p->buffer + p->length, newfrag->buffer + n);
			}
			newfrag->length = len;
			newfrag->buffer[newfrag->length] = 0;
			clear_const();
			fragment = newfrag;
		}
	}
	basic_stringbuffer(basic_stringbuffer const &);
	void operator = (basic_stringbuffer const &);
public:
	basic_stringbuffer()
		: fragment(0)
	{
	}
	~basic_stringbuffer()
	{
		clear();
	}
	size_t size() const
	{
		size_t n = 0;
		fragment_t *p;
		for (p = fragment; p; p = p->chain) {
			n += p->length;
		}
		return n;
	}
	void clear()
	{
		clear_const();
	}
	void write(T const *ptr, size_t len)
	{
		if (ptr && len > 0) {
			if (fragment && fragment->length < fragment->capacity) {
				size_t cap = fragment->capacity - fragment->length;
				size_t n = len;
				if (n > cap) {
					n = cap;
				}
				std::copy(ptr, ptr + n, fragment->buffer + fragment->length);
				fragment->length += n;
				fragment->buffer[fragment->length] = 0;
				ptr += n;
				len -= n;
			}
			if (len > 0) {
				size_t n = 1024;
				if (n < len) {
					n = len;
				}
				fragment_t *f = new_fragment(n);
				std::copy(ptr, ptr + len, &*f->buffer);
				f->length = len;
				f->buffer[f->length] = 0;
				f->chain = fragment;
				fragment = f;
			}
		}
	}
	void write(T const *ptr)
	{
		write(ptr, std::char_traits<T>::length(ptr));
	}
	void write(std::basic_string<T> const &str)
	{
		write(str.c_str(), str.size());
	}
	void put(T c)
	{
		write(&c, 1);
	}
	void assign(T const *ptr, T const *end)
	{
		basic_stringbuffer x;
		x.write(ptr, end - ptr);
		std::swap(x.fragment, this->fragment);
	}
private:
	static void consume_(fragment_t **f, size_t *len)
	{
		if (*f) {
			if ((*f)->chain) {
				consume_(&(*f)->chain, len);
			}
			if (!(*f)->chain) {
				if (*len >= (*f)->length) {
					*len -= (*f)->length;
					delete[] (char *)(*f);
					*f = 0;
				}
			}
		}
	}
public:
	void consume(size_t len)
	{
		size_t length = size();
		if (len < length) {
			consume_(&fragment, &len);
			length = size();
			basic_stringbuffer newsb;
			newsb.write(c_str() + len, length - len);
			std::swap(fragment, newsb.fragment);
		} else {
			clear();
		}
	}
	std::basic_string<T> str() const
	{
		return std::basic_string<T>(c_str(), size());
	}
	T const *c_str() const
	{
		if (fragment) {
			build();
			return fragment->buffer;
		} else {
			return ucs::nullstring<T>();
		}
	}
	operator T const * () const
	{
		return c_str();
	}
	T const &at(size_t i) const
	{
		return c_str()[i];
	}
	bool empty() const
	{
		if (fragment && fragment->length) {
			return false;
		}
		return size() == 0;
	}
	T const *peek(size_t n) const
	{
		if (fragment) {
			fragment_t *f = fragment;
			while (f->chain) {
				f = f->chain;
			}
			if (f->length < n) {
				build();
				f = fragment;
			}
			if (f->length >= n) {
				return f->buffer;
			}
		}
		return 0;
	}
};

typedef basic_stringbuffer<char> stringbuffer;
typedef basic_stringbuffer<uchar_t> ustringbuffer;

typedef basic_stringbuffer<unsigned char> blob_t;

//

#endif


