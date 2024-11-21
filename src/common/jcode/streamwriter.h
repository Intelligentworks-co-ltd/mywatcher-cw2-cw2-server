
#ifndef __streamwriter_h
#define __streamwriter_h

class streamwriter : public soramimi::jcode::writer {
private:
	FILE *_fp;
public:
	streamwriter(FILE *fp)
	{
		_fp = fp;
	}
	virtual void put(unsigned char c)
	{
		putc(c, _fp);
	}
};

#endif
