
#ifndef __memorywriter_h
#define __memorywriter_h

class memorywriter : public soramimi::jcode::writer {
private:
	unsigned char *ptr;
	unsigned char *end;
public:
	memorywriter(void *ptr, void *end)
		: ptr((unsigned char *)ptr)
		, end((unsigned char *)end)
	{
	}
	memorywriter(void *ptr, size_t len)
		: ptr((unsigned char *)ptr)
		, end((unsigned char *)ptr + len)
	{
	}
	virtual void put(unsigned char c)
	{
		if (ptr < end) {
			*ptr++ = c;
		}
	}
};

#endif
