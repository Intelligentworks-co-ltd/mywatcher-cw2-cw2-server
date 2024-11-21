
#ifndef __soramimi_jp__jcode_h
#define __soramimi_jp__jcode_h

#include <stdio.h>
#include "../uchar.h"

namespace soramimi {
	namespace jcode {

		enum encoding_t {
			UNKNOWN,
			EUCJP,
			SJIS,
			UTF8,
			JIS,
		};

		struct eucjp_reader_state_t {
			unsigned char a;
			unsigned char b;
		};

		struct sjis_reader_state_t {
			unsigned char a;
		};

		struct utf8_reader_state_t {
			unsigned char a;
			unsigned long b;
		};

		struct jis_reader_state_t {
			unsigned long a;
			unsigned long b;
		};

		struct reader_state_t {
			encoding_t encoding;
			union {
				struct eucjp_reader_state_t eucjp;
				struct sjis_reader_state_t  sjis;
				struct utf8_reader_state_t  utf8;
				struct jis_reader_state_t   jis;
			} state;
		};

		struct writer_state_t {
			encoding_t encoding;
			unsigned long jis_state;
		};

		class reader {
		public:
			virtual ~reader()
			{
			}
			virtual int get() = 0;
		};

		class writer {
		public:
			virtual ~writer()
			{
			}
			virtual void put(unsigned char c) = 0;
		};

		class wcswriter {
		public:
			virtual ~wcswriter()
			{
			}
			virtual void put(uchar_t c) = 0;
		};

		unsigned short jmstojis(unsigned short c);
		unsigned short jistojms(unsigned short c);
		unsigned short jistoucs(unsigned short c);
		unsigned short ucstojis(unsigned short c);

		void setup_reader_state(reader_state_t *s, encoding_t e);
		void setup_writer_state(writer_state_t *s, encoding_t e);
		int read(reader_state_t *istate, reader *reader);
		void convert(reader_state_t *istate, writer_state_t *ostate, reader *reader, writer *writer);
		void convert(reader_state_t *istate, reader *reader, wcswriter *writer);
	}
}

#endif

