
#include "jcode.h"

#include <stdio.h>
#include <sstream>

namespace soramimi {
	namespace jcode {

		unsigned short jmstojis(unsigned short c)
		{
			if (c >= 0xe000) {
				c -= 0x4000;
			}
			c = (((c >> 8) - 0x81) << 9) | (unsigned char)c;
			if ((unsigned char)c >= 0x80) {
				c -= 1;
			}
			if ((unsigned char)c >= 0x9e) {
				c += 0x62;
			} else {
				c -= 0x40;
			}
			c += 0x2121;
			return c;
		}

		unsigned short jistojms(unsigned short c)
		{
			c -= 0x2121;
			if (c & 0x100) {
				c += 0x9e;
			} else {
				c += 0x40;
			}
			if ((unsigned char)c >= 0x7f) {
				c++;
			}
			c = (((c >> (8 + 1)) + 0x81) << 8) | ((unsigned char)c);
			if (c >= 0xa000) {
				c += 0x4000;
			}
			return c;
		}

		extern "C" unsigned short jis_to_ucs_table[];
		extern "C" unsigned short ucs_to_jis_table[];

		unsigned short jistoucs(unsigned short c)
		{
			if (c < 0x80) {
				return c;
			}
			if (c < 0x8000) {
				return jis_to_ucs_table[c];
			}
			return 0;
		}

		unsigned short ucstojis(unsigned short c)
		{
			if (c < 0x80) {
				return c;
			}
			return ucs_to_jis_table[c];
		}

		struct eucjp_reader {
			static int get(eucjp_reader_state_t *state, unsigned char c)
			{
				if (state->a == 0) {
					if (c < 0x80) {
						return c;
					} else {
						state->a = c;
					}
				} else if (state->a == 0x8e) {
					state->a = 0;
					return jistoucs(c);
				} else if (state->a == 0x8f) {
					if (state->b == 0) {
						state->b = c;
					} else {
						//unsigned short j = ((state->b << 8) | c) & 0x7f7f;
						//state->a = 0;
						//state->b = 0;
						//return jistoucs(j);
					}
				} else {
					unsigned short j = ((state->a << 8) | c) & 0x7f7f;
					state->a = 0;
					state->b = 0;
					return jistoucs(j);
				}
				return -1;
			}
		};

		struct eucjp_writer {
			static void put(writer_state_t *state, writer *writer, int code)
			{
				code = ucstojis(code);
				if (code < 0x100) {
					if (code >= 0xa1 && code <= 0xdf) {
						writer->put(0x8e);
					}
					writer->put((unsigned char)code);
				} else {
					code |= 0x8080;
					writer->put((unsigned char)(code >> 8));
					writer->put((unsigned char)code);
				}
			}
		};

		struct sjis_reader {
			static int get(sjis_reader_state_t *state, unsigned char c)
			{
				if (state->a == 0) {
					if ((c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc)) {
						state->a = c;
					} else {
						state->a = 0;
						if (c >= 0xa1 && c <= 0xdf) {
							return c - 0xa0 + 0xff60;
						}
						return c;
					}
				} else {
					unsigned short j = jmstojis((state->a << 8) | c);
					state->a = 0;
					return jistoucs(j);
				}
				return -1;
			}
		};

		struct sjis_writer {
			static void put(writer_state_t *state, writer *writer, int code)
			{
				code = ucstojis(code);
				if (code < 0x100) {
					writer->put((unsigned char)code);
				} else {
					code = jistojms(code & 0x7f7f);
					writer->put((unsigned char)(code >> 8));
					writer->put((unsigned char)code);
				}
			}
		};

		struct utf8_reader {
			static int get(utf8_reader_state_t *state, unsigned char c)
			{
				if (c & 0x80) {
					if (c & 0x40) {
						state->a = c;
						if (c & 0x20) {
							if (c & 0x10) {
								if (c & 0x08) {
									if (c & 0x04) {
										state->b = c & 0x01;
									} else {
										state->b = c & 0x03;
									}
								} else {
									state->b = c & 0x07;
								}
							} else {
								state->b = c & 0x0f;
							}
						} else {
							state->b = c & 0x1f;
						}
						return -1;
					} else {
						state->a <<= 1;
						state->b = (state->b << 6) | (c & 0x3f);
						if ((state->a & 0x40) == 0 && state->b >= 0x80 && state->b < 0x10000) {
							return (unsigned short)state->b;
						}
						return -1;
					}
				} else {
					state->a = 0;
					state->b = 0;
					return c;
				}
			}
		};

		struct utf8_writer {
			static void put(writer_state_t *state, writer *writer, int code)
			{
				if (code < 0x80) {
					writer->put(code);
				} else if (code < 0x800) {
					writer->put((code >> 6) | 0xc0);
					writer->put((code & 0x3f) | 0x80);
				} else if (code < 0x10000) {
					writer->put((code >> 12) | 0xe0);
					writer->put(((code >> 6) & 0x3f) | 0x80);
					writer->put((code & 0x3f) | 0x80);
				} else if (code < 0x200000) {
					writer->put((code >> 18) | 0xf0);
					writer->put(((code >> 12) & 0x3f) | 0x80);
					writer->put(((code >> 8) & 0x3f) | 0x80);
					writer->put((code & 0x3f) | 0x80);
				} else if (code < 0x4000000) {
					writer->put((code >> 24) | 0xf8);
					writer->put(((code >> 18) & 0x3f) | 0x80);
					writer->put(((code >> 12) & 0x3f) | 0x80);
					writer->put(((code >> 8) & 0x3f) | 0x80);
					writer->put((code & 0x3f) | 0x80);
				} else {
					writer->put((code >> 30) | 0xfc);
					writer->put(((code >> 24) & 0x3f) | 0x80);
					writer->put(((code >> 18) & 0x3f) | 0x80);
					writer->put(((code >> 12) & 0x3f) | 0x80);
					writer->put(((code >> 8) & 0x3f) | 0x80);
					writer->put((code & 0x3f) | 0x80);
				}
			}
		};

		struct jis_reader {
			static int get(jis_reader_state_t *state, unsigned char c)
			{
				if (c == 0x1b) {
					state->a = 0x1b;
					state->b = 0;
				} else {
					if (state->a == 0x1b) {
						state->a = (state->a << 8) | c;
						state->b = 0;
					} else if ((state->a & 0xffffff00) == 0x1b00) {
						state->a = (state->a << 8) | c;
						state->b = 0;
					} else {
						if (state->a == 0 || state->a == 0x1b2842 || state->a == 0x1b284a) {
							state->b = 0;
							return c;
						} else if (state->a == 0x1b2849) {
							state->b = 0;
							return c | 0x80;
						} else if (state->a == 0x1b2440 || state->a == 0x1b2442) {
							if (state->b == 0) {
								state->b = 0x100 | c;						
							} else {
								unsigned short j = (unsigned short)(((state->b << 8) | c) & 0x7f7f);
								state->b = 0;
								return jistoucs(j);
							}
						}
					}
				}
				return -1;
			}
		};

		struct jis_writer {
			static void put(writer_state_t *state, writer *writer, int code)
			{
				if (code < 0x80) {
					if (state->jis_state != 0 && state->jis_state != 0x1b2842 && state->jis_state != 0x1b284a) {
						writer->put(0x1b);
						writer->put(0x28);
						writer->put(0x42);
						state->jis_state = 0x1b2842;
					}
					writer->put((unsigned char)code);
				} else {
					code = ucstojis(code);
					if (code >= 0xa1 && code <= 0xdf) {
						if (state->jis_state != 0x1b2849) {
							writer->put(0x1b);
							writer->put(0x28);
							writer->put(0x49);
							state->jis_state = 0x1b2849;
						}
						writer->put((unsigned char)code);
					} else if ((code & 0x7f00) != 0 && (code & 0x007f) != 0 && (code & 0x8080) == 0) {
						if (state->jis_state != 0x1b2440 && state->jis_state != 0x1b2442) {
							writer->put(0x1b);
							writer->put(0x24);
							writer->put(0x42);
							state->jis_state = 0x1b2442;
						}
						writer->put((unsigned char)(code >> 8));
						writer->put((unsigned char)code);
					}
				}
			}
		};

		inline void clear_state(eucjp_reader_state_t *s)
		{
			s->a = 0;
			s->b = 0;
		}

		inline void clear_state(sjis_reader_state_t *s)
		{
			s->a = 0;
		}

		inline void clear_state(utf8_reader_state_t *s)
		{
			s->a = 0;
			s->b = 0;
		}

		inline void clear_state(jis_reader_state_t *s)
		{
			s->a = 0;
			s->b = 0;
		}

		void setup_reader_state(reader_state_t *s, encoding_t e)
		{
			s->encoding = e;
			switch (s->encoding) {
			case EUCJP: clear_state(&s->state.eucjp); break;
			case SJIS:  clear_state(&s->state.sjis);  break;
			case UTF8:  clear_state(&s->state.utf8);  break;
			case JIS:   clear_state(&s->state.jis);   break;
			default:
				break;
			}
		}

		void setup_writer_state(writer_state_t *s, encoding_t e)
		{
			s->encoding = e;
			s->jis_state = 0;
		}


		int read(reader_state_t *istate, reader *reader)
		{
			int c = reader->get();
			if (c >= 0) {
				switch (istate->encoding) {
				case EUCJP: c = eucjp_reader::get(&istate->state.eucjp, c); break;
				case SJIS:  c = sjis_reader::get(&istate->state.sjis, c);   break;
				case UTF8:  c = utf8_reader::get(&istate->state.utf8, c);   break;
				case JIS:   c = jis_reader::get(&istate->state.jis, c);     break;
				default:
					break;
				}
				if (c < 0) {
					c = 0;
				}
				return c;
			}
			return -1;
		}

		void convert(reader_state_t *istate, writer_state_t *ostate, reader *reader, writer *writer)
		{
			while (1) {
				int c = read(istate, reader);
				if (c < 0) {
					break;
				}
				if (c > 0) {
					switch (ostate->encoding) {
					case EUCJP: eucjp_writer::put(ostate, writer, c); break;
					case SJIS:  sjis_writer::put(ostate, writer, c);  break;
					case UTF8:  utf8_writer::put(ostate, writer, c);  break;
					case JIS:   jis_writer::put(ostate, writer, c);   break;
					default:
						break;
					}
				}
			}
		}

		void convert(reader_state_t *istate, reader *reader, wcswriter *writer)
		{
			while (1) {
				int c = read(istate, reader);
				if (c < 0) {
					break;
				}
				if (c > 0) {
					writer->put(c);
				}
			}
		}

		static void convert(writer_state_t *ostate, writer *writer, uchar_t const *ptr, uchar_t const *end)
		{
			while (ptr < end) {
				uchar_t c = *ptr++;
				switch (ostate->encoding) {
				case EUCJP: eucjp_writer::put(ostate, writer, c); break;
				case SJIS:  sjis_writer::put(ostate, writer, c);  break;
				case UTF8:  utf8_writer::put(ostate, writer, c);  break;
				case JIS:   jis_writer::put(ostate, writer, c);   break;
				default:
					break;
				}
			}
		}

		class stringstream_writer : public writer {
		private:
			std::stringstream *_out;
		public:
			stringstream_writer(std::stringstream *out)
				: _out(out)
			{
			}
			virtual void put(unsigned char c)
			{
				_out->write((char const *)&c, 1);
			}
		};

		void convert(std::stringstream *out, encoding_t e, uchar_t const *ptr, uchar_t const *end)
		{
			writer_state_t ws;
			stringstream_writer ssw(out);
			setup_writer_state(&ws, e);
			convert(&ws, &ssw, ptr, end);
		}

	}
}

