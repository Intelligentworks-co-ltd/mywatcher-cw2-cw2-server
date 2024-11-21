
#ifndef __widememorywriter_h
#define __widememorywriter_h

class widememorywriter : public soramimi::jcode::wcswriter {
private:
	uchar_t *ptr;
	uchar_t *end;
public:
	widememorywriter(void *ptr, void *end)
		: ptr((uchar_t *)ptr)
		, end((uchar_t *)end)
	{
	}
	widememorywriter(void *ptr, size_t len)
		: ptr((uchar_t *)ptr)
		, end((uchar_t *)ptr + len)
	{
	}
	virtual void put(uchar_t c)
	{
		if (ptr < end) {
			*ptr++ = c;
		}
	}
};

#endif
