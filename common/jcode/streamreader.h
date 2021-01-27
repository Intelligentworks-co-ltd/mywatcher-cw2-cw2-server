
#ifndef __streamreader_h
#define __streamreader_h

class streamreader : public soramimi::jcode::reader {
private:
	FILE *_fp;
public:
	streamreader(FILE *fp)
	{
		_fp = fp;
	}
	virtual int get()
	{
		return getc(_fp);
	}
};

#endif
