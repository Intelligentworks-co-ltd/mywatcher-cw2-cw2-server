
#ifndef __jstring_h
#define __jstring_h

#include "jcode.h"
#include "memoryreader.h"
#include "widestringwriter.h"
#include "../uchar.h"
//#include <mbstring.h>
#include <string>
#include <sstream>


namespace soramimi {
	namespace jcode {

		inline ustring convert(encoding_t e, char const *ptr, char const *end)
		{
			memoryreader reader(ptr, end);
			widestringwriter writer;
			soramimi::jcode::reader_state_t istate;
			soramimi::jcode::setup_reader_state(&istate, e);
			soramimi::jcode::convert(&istate, &reader, &writer);
			return writer.str();
		}

		inline ustring convert(encoding_t e, char const *ptr, size_t len)
		{
			return convert(e, ptr, ptr + len);
		}

		inline ustring convert(encoding_t e, char const *ptr)
		{
			return convert(e, ptr, strlen(ptr));
		}

		inline ustring convert(encoding_t e, std::string const &str)
		{
			return convert(e, str.c_str(), str.size());
		}

		void convert(std::stringstream *out, encoding_t e, uchar_t const *ptr, uchar_t const *end);

		inline void convert(std::stringstream *out, encoding_t e, uchar_t const *ptr, size_t len)
		{
			convert(out, e, ptr, ptr + len);
		}

		inline void convert(std::stringstream *out, encoding_t e, uchar_t const *ptr)
		{
			convert(out, e, ptr, ptr + ucs::wcslen(ptr));
		}

		inline void convert(std::stringstream *out, encoding_t e, ustring const &str)
		{
			convert(out, e, str.c_str(), str.size());
		}

		inline std::string convert(encoding_t e, uchar_t const *ptr, uchar_t const *end)
		{
			std::stringstream out;
			convert(&out, e, ptr, end);
			return out.str();
		}

		inline std::string convert(encoding_t e, uchar_t const *ptr, size_t len)
		{
			return convert(e, ptr, ptr + len);
		}

		inline std::string convert(encoding_t e, uchar_t const *ptr)
		{
			return convert(e, ptr, ptr + ucs::wcslen(ptr));
		}

		inline std::string convert(encoding_t e, ustring const &str)
		{
			return convert(e, str.c_str(), str.size());
		}
	}
}

#endif
