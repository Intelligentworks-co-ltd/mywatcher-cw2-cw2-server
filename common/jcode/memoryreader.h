
#ifndef __memoryreader_h
#define __memoryreader_h

class memoryreader : public soramimi::jcode::reader {
private:
	unsigned char const *ptr;
	unsigned char const *end;
public:
	memoryreader(void const *ptr, void const *end)
		: ptr((unsigned char const *)ptr)
		, end((unsigned char const *)end)
	{
	}
	memoryreader(void const *ptr, size_t len)
		: ptr((unsigned char const *)ptr)
		, end((unsigned char const *)ptr + len)
	{
	}
	virtual int get()
	{
		if (ptr < end) {
			return *ptr++;
		}
		return -1;
	}
};

#endif
